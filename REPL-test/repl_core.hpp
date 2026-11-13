#pragma once
/**
 * @file repl_core.hpp
 * @brief Core REPL session logic, buffering, and meta-command support.
 */

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <cctype>
#include <utility>
#include "i_eval_engine.h"
#include "scheme_engine.h"

/**
 * @brief Configuration for REPL prompts and behaviors.
 */
struct ReplConfig {
    std::string prompt = "scheme> ";
    std::string cont_prompt = "....> ";
    bool echo_result = true;
    bool allow_meta_commands = true; // :q, :reset, etc.
    std::vector<std::string> meta_prefixes = { ":" };
};

/**
 * @brief Event emitted by the REPL for the frontend to render.
 */
struct ReplEvent {
    enum class Kind { Prompt, Stdout, Stderr, Result, Info };
    Kind kind;
    std::string text;
};

/**
 * @brief Snapshot of current input buffer state.
 */
struct ReplBufferState {
    std::string_view buffer;
    int paren_balance = 0;
};

/**
 * @brief Result of handling a meta command or input edit.
 */
struct ReplEditResult {
    bool handled = false;
    std::optional<std::string> new_buffer;
    std::optional<int> new_paren_balance;
    bool emit_prompt = true;
    std::optional<std::string> prompt_override;
    std::vector<ReplEvent> events;
};

class ReplSession;

/**
 * @brief Plugin interface for custom REPL input handling.
 *
 * Plugins can register meta commands, transform lines, and supply custom
 * Continuation prompts.
 */
class IReplInputPlugin {
public:
    virtual ~IReplInputPlugin() = default;

    /**
     * @brief Register plugin-specific meta commands.
     * @param session REPL session to register against.
     */
    virtual void register_meta_commands(ReplSession& session) { (void)session; }

    /**
     * @brief Handle a meta command line.
     * @param line Raw input line.
     * @param state Current buffer state.
     * @param cfg Session config.
     * @return Edit result (handled=false to defer to others).
     */
    virtual ReplEditResult handle_meta_command(std::string_view line,
                                               const ReplBufferState& state,
                                               const ReplConfig& cfg) {
        (void)line;
        (void)state;
        (void)cfg;
        return {};
    }

    /**
     * @brief Transform a line before buffering.
     * @param line Raw input line.
     * @param state Current buffer state.
     * @param cfg Session config.
     * @return Transformed line.
     */
    virtual std::string transform_line(std::string_view line,
                                       const ReplBufferState& state,
                                       const ReplConfig& cfg) {
        (void)state;
        (void)cfg;
        return std::string(line);
    }

    /**
     * @brief Provide a Continuation prompt when buffer is incomplete.
     * @param cfg Session config.
     * @param state Current buffer state.
     * @return Prompt string.
     */
    virtual std::string continuation_prompt(const ReplConfig& cfg,
                                            const ReplBufferState& state) {
        (void)state;
        return cfg.cont_prompt;
    }

    /**
     * @brief Notification that the session reset.
     */
    virtual void on_reset() {}

    /**
     * @brief Notification that the input buffer was cleared.
     */
    virtual void on_clear() {}
};

/**
 * @brief REPL session driver with buffering and meta command support.
 *
 * The session is frontend-agnostic: submit lines and consume events.
 */
class ReplSession {
public:
    /**
     * @brief Meta command handler signature.
     */
    using MetaCommandHandler = std::function<ReplEditResult(
        std::string_view args,
        const ReplBufferState& state,
        const ReplConfig& cfg)>;

    /**
     * @brief Meta command entry with description and handler.
     */
    struct MetaCommandEntry {
        std::string description;
        MetaCommandHandler handler;
    };

    /**
     * @brief Construct a session with the given engine and config.
     */
    ReplSession(std::unique_ptr<IEvalEngine> engine, ReplConfig cfg = {})
        : engine_(std::move(engine)), cfg_(std::move(cfg)) {
        normalize_meta_prefixes();
        register_builtin_meta_commands();
        engine_->warmup_prelude();
    }

    /**
     * @brief Add an input plugin.
     */
    void add_input_plugin(std::unique_ptr<IReplInputPlugin> plugin) {
        if (!plugin) return;
        input_plugins_.push_back(std::move(plugin));
        input_plugins_.back()->register_meta_commands(*this);
    }

    /**
     * @brief Register a meta command with no description.
     */
    void register_meta_command(std::string name, MetaCommandHandler handler) {
        register_meta_command(std::move(name), std::string{}, std::move(handler));
    }

    /**
     * @brief Register a meta command.
     */
    void register_meta_command(std::string name,
                               std::string description,
                               MetaCommandHandler handler) {
        if (!is_valid_meta_command_name(name)) return;
        if (!handler) return;
        meta_commands_[std::move(name)] = { std::move(description), std::move(handler) };
    }

    /**
     * @brief Replace meta command prefixes.
     */
    void set_meta_command_prefixes(std::vector<std::string> prefixes) {
        cfg_.meta_prefixes = std::move(prefixes);
        normalize_meta_prefixes();
    }

    /**
     * @brief Start a session and emit the initial prompt.
     */
    std::vector<ReplEvent> start() {
        return { {ReplEvent::Kind::Prompt, cfg_.prompt} };
    }

    /**
     * @brief Submit a line of input and receive events.
     * @param line Input line.
     * @return Events to render.
     */
    std::vector<ReplEvent> submit_line(std::string_view line) {
        std::vector<ReplEvent> ev;

        // Check for simple exit/quit commands (without colon)
        if (cfg_.allow_meta_commands && is_simple_exit_command(line)) {
            ev.push_back({ ReplEvent::Kind::Info, "bye" });
            ev.push_back({ ReplEvent::Kind::Prompt, "" });
            return ev;
        }

        // Meta commands (optional). Keep these in the session so frontends are trivial.
        if (cfg_.allow_meta_commands) {
            auto meta = parse_meta_command(line);
            if (meta) {
                if (try_handle_plugin_meta_command(line, ev)) return ev;
                return handle_meta_command(*meta);
            }
        }

        // Accumulate input and track completeness.
        auto transformed = apply_line_transforms(line);
        append_line(transformed);
        paren_balance_ += paren_delta(transformed);

        if (should_wait_for_more_input()) {
            ev.push_back({ ReplEvent::Kind::Prompt, continuation_prompt() });
            return ev;
        }

        // If buffer is empty or only contains whitespace/comments, just show new prompt
        if (is_effectively_empty(buffer_)) {
            buffer_.clear();
            paren_balance_ = 0;
            ev.push_back({ ReplEvent::Kind::Prompt, cfg_.prompt });
            return ev;
        }

        // We believe we have a complete form(s) in buffer_. Evaluate.
        auto src = std::exchange(buffer_, std::string{});
        paren_balance_ = 0;

        try {
            engine_->set_capture_stdout(true);
            engine_->clear_captured_stdout();
            engine_->begin_eval();

            void* val = engine_->eval_string(src);

            engine_->end_eval();
            auto out = engine_->take_captured_stdout();
            if (!out.empty()) ev.push_back({ ReplEvent::Kind::Stdout, out });

            // Check for errors even if no exception was thrown
            // (scheme_engine catches exceptions and returns nullptr)
            if (val == nullptr) {
                // Try to get error message from engine
                auto* scheme_eng = dynamic_cast<scheme_engine*>(engine_.get());
                if (scheme_eng) {
                    std::string err_msg = scheme_eng->get_last_error();
                    if (!err_msg.empty()) {
                        ev.push_back({ ReplEvent::Kind::Stderr, std::string("error: ") + err_msg });
                    }
                }
            } else if (cfg_.echo_result) {
                ev.push_back({ ReplEvent::Kind::Result, engine_->print_value(val) });
            }
        }
        catch (const std::exception& e) {
            ev.push_back({ ReplEvent::Kind::Stderr, std::string("error: ") + e.what() });
        }
        catch (...) {
            ev.push_back({ ReplEvent::Kind::Stderr, "error: unknown exception" });
        }

        ev.push_back({ ReplEvent::Kind::Prompt, cfg_.prompt });
        return ev;
    }

    /**
     * @brief Clear the input buffer.
     */
    void clear_buffer() {
        buffer_.clear();
        paren_balance_ = 0;
    }

private:
    // ---- simple exit commands (without colon prefix)
    bool is_simple_exit_command(std::string_view line) const {
        auto s = std::string(trim_left(line));
        return s == "exit" || s == "quit" || s == "(exit)" || s == "(quit)";
    }

    // ---- meta commands
    struct ParsedMetaCommand {
        std::string_view raw;
        std::string_view prefix;
        std::string_view name;
        std::string_view args;
    };

    std::optional<ParsedMetaCommand> parse_meta_command(std::string_view line) const {
        auto trimmed = trim_left(line);
        auto prefix = match_meta_prefix(trimmed);
        if (prefix.empty()) return std::nullopt;

        auto after_prefix = trimmed.substr(prefix.size());
        auto after_prefix_trimmed = trim_left(after_prefix);

        size_t name_end = 0;
        while (name_end < after_prefix_trimmed.size() &&
               !std::isspace(static_cast<unsigned char>(after_prefix_trimmed[name_end]))) {
            ++name_end;
        }
        auto name = after_prefix_trimmed.substr(0, name_end);
        auto args = trim_left(after_prefix_trimmed.substr(name_end));

        return ParsedMetaCommand{ trimmed, prefix, name, args };
    }

    std::vector<ReplEvent> handle_meta_command(const ParsedMetaCommand& meta) {
        std::vector<ReplEvent> ev;
        auto result = dispatch_meta_command(meta);
        apply_plugin_result(result, ev);
        return ev;
    }

    ReplEditResult dispatch_meta_command(const ParsedMetaCommand& meta) {
        ReplEditResult result;
        result.handled = true;

        if (meta.name.empty()) {
            result.events.push_back({ ReplEvent::Kind::Stderr,
                                      "unknown command: " + std::string(meta.raw) });
            return result;
        }

        auto it = meta_commands_.find(std::string(meta.name));
        if (it == meta_commands_.end()) {
            result.events.push_back({ ReplEvent::Kind::Stderr,
                                      "unknown command: " + std::string(meta.raw) });
            return result;
        }

        result = it->second.handler(meta.args, buffer_state(), cfg_);
        result.handled = true;
        return result;
    }

    void register_builtin_meta_commands() {
        register_meta_command("help", "List meta commands or details for one command",
            [this](std::string_view args, const ReplBufferState&, const ReplConfig&) {
                return make_help_result(args);
            });
        register_meta_command("q", "Exit the REPL",
            [this](std::string_view, const ReplBufferState&, const ReplConfig&) {
            return make_quit_result();
        });
        register_meta_command("quit", "Exit the REPL",
            [this](std::string_view, const ReplBufferState&, const ReplConfig&) {
            return make_quit_result();
        });
        register_meta_command("exit", "Exit the REPL",
            [this](std::string_view, const ReplBufferState&, const ReplConfig&) {
            return make_quit_result();
        });
        register_meta_command("reset", "Reset interpreter state",
            [this](std::string_view, const ReplBufferState&, const ReplConfig&) {
            clear_buffer();
            engine_->reset();
            engine_->warmup_prelude();
            notify_plugins_reset();
            ReplEditResult result;
            result.handled = true;
            result.events.push_back({ ReplEvent::Kind::Info, "session reset" });
            return result;
        });
        register_meta_command("clear", "Clear input buffer",
            [this](std::string_view, const ReplBufferState&, const ReplConfig&) {
            clear_buffer();
            notify_plugins_clear();
            ReplEditResult result;
            result.handled = true;
            result.events.push_back({ ReplEvent::Kind::Info, "input buffer cleared" });
            return result;
        });
    }

    ReplEditResult make_quit_result() const {
        ReplEditResult result;
        result.handled = true;
        result.prompt_override = std::string{};
        result.events.push_back({ ReplEvent::Kind::Info, "bye" });
        return result;
    }

    ReplEditResult make_help_result(std::string_view args) const {
        ReplEditResult result;
        result.handled = true;

        auto trimmed_args = trim_left(args);
        if (!trimmed_args.empty()) {
            size_t name_end = 0;
            while (name_end < trimmed_args.size() &&
                   !std::isspace(static_cast<unsigned char>(trimmed_args[name_end]))) {
                ++name_end;
            }
            auto token = trimmed_args.substr(0, name_end);
            auto name = token;
            auto prefix = match_meta_prefix(name);
            if (!prefix.empty()) {
                name = name.substr(prefix.size());
            }

            if (name.empty()) {
                result.events.push_back({ ReplEvent::Kind::Stderr, "unknown command: " + std::string(token) });
                return result;
            }

            auto it = meta_commands_.find(std::string(name));
            if (it == meta_commands_.end()) {
                result.events.push_back({ ReplEvent::Kind::Stderr, "unknown command: " + std::string(token) });
                return result;
            }

            std::string line = "Meta command: ";
            line.append(primary_meta_prefix());
            line.append(name);
            result.events.push_back({ ReplEvent::Kind::Info, line });

            if (!it->second.description.empty()) {
                result.events.push_back({ ReplEvent::Kind::Info, "  " + it->second.description });
            } else {
                result.events.push_back({ ReplEvent::Kind::Info, "  (no description)" });
            }
            return result;
        }

        std::string header = "Meta commands";
        auto prefix_list = format_meta_prefixes();
        if (!prefix_list.empty()) {
            header.append(" (prefixes: ");
            header.append(prefix_list);
            header.append(")");
        }
        result.events.push_back({ ReplEvent::Kind::Info, header });

        if (meta_commands_.empty()) {
            result.events.push_back({ ReplEvent::Kind::Info, "  (none)" });
            return result;
        }

        std::vector<std::string> names;
        names.reserve(meta_commands_.size());
        size_t name_width = 0;
        for (const auto& entry : meta_commands_) {
            names.push_back(entry.first);
            name_width = std::max(name_width, entry.first.size());
        }
        std::sort(names.begin(), names.end());

        auto prefix = primary_meta_prefix();
        size_t prefix_width = prefix.size();

        for (const auto& name : names) {
            auto it = meta_commands_.find(name);
            if (it == meta_commands_.end()) continue;

            std::string line = "  ";
            line.append(prefix);
            line.append(name);

            const auto& description = it->second.description;
            if (!description.empty()) {
                size_t total_name = prefix_width + name.size();
                size_t target = prefix_width + name_width;
                size_t padding = 1;
                if (target > total_name) padding += target - total_name;
                line.append(padding, ' ');
                line.append(description);
            }

            result.events.push_back({ ReplEvent::Kind::Info, line });
        }

        return result;
    }

    void normalize_meta_prefixes() {
        std::vector<std::string> normalized;
        normalized.reserve(cfg_.meta_prefixes.size());
        for (const auto& prefix : cfg_.meta_prefixes) {
            if (prefix.empty()) continue;
            bool exists = false;
            for (const auto& existing : normalized) {
                if (existing == prefix) { exists = true; break; }
            }
            if (!exists) normalized.push_back(prefix);
        }
        cfg_.meta_prefixes = std::move(normalized);
    }

    static bool is_valid_meta_command_name(std::string_view name) {
        if (name.empty()) return false;
        for (char c : name) {
            if (std::isspace(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }

    std::string_view match_meta_prefix(std::string_view trimmed) const {
        std::string_view best;
        for (const auto& prefix : cfg_.meta_prefixes) {
            if (prefix.empty() || trimmed.size() < prefix.size()) continue;
            if (trimmed.compare(0, prefix.size(), prefix) == 0) {
                if (prefix.size() > best.size()) best = prefix;
            }
        }
        return best;
    }

    std::string primary_meta_prefix() const {
        if (cfg_.meta_prefixes.empty()) return {};
        return cfg_.meta_prefixes.front();
    }

    std::string format_meta_prefixes() const {
        if (cfg_.meta_prefixes.empty()) return {};
        std::string out;
        for (size_t i = 0; i < cfg_.meta_prefixes.size(); ++i) {
            if (i > 0) out.append(", ");
            out.append(cfg_.meta_prefixes[i]);
        }
        return out;
    }

    // ---- input buffering rules
    void append_line(std::string_view line) {
        buffer_.append(line.data(), line.size());
        buffer_.push_back('\n');
    }

    bool should_wait_for_more_input() const {
        // Only wait if we have unclosed parens (incomplete expression).
        // Empty lines and comment-only lines should show a new prompt, not Continuation.
        return paren_balance_ > 0;
    }

    // Very small �paren balance delta� scanner:
    // - ignores parentheses inside "...", respects backslash escapes
    // - ignores everything after ';' (line comment) when not in string
    static int paren_delta(std::string_view line) {
        bool in_string = false;
        bool escape = false;

        int delta = 0;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];

            if (in_string) {
                if (escape) { escape = false; continue; }
                if (c == '\\') { escape = true; continue; }
                if (c == '"') { in_string = false; continue; }
                continue;
            }

            // comment begins
            if (c == ';') break;

            if (c == '"') { in_string = true; continue; }
            if (c == '(') ++delta;
            if (c == ')') --delta;
        }
        return delta;
    }

    static bool is_effectively_empty(const std::string& s) {
        bool in_string = false, escape = false;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];

            if (in_string) {
                if (escape) { escape = false; continue; }
                if (c == '\\') { escape = true; continue; }
                if (c == '"') { in_string = false; continue; }
                continue;
            }

            if (c == ';') {
                // skip to end of line
                while (i < s.size() && s[i] != '\n') ++i;
                continue;
            }

            if (c == '"') { in_string = true; continue; }
            if (!std::isspace(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }

    static std::string_view trim_left(std::string_view s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
            s.remove_prefix(1);
        return s;
    }

    ReplBufferState buffer_state() const {
        return { buffer_, paren_balance_ };
    }

    bool try_handle_plugin_meta_command(std::string_view line, std::vector<ReplEvent>& ev) {
        if (input_plugins_.empty()) return false;
        auto state = buffer_state();
        for (const auto& plugin : input_plugins_) {
            auto result = plugin->handle_meta_command(line, state, cfg_);
            if (!result.handled) continue;
            apply_plugin_result(result, ev);
            return true;
        }
        return false;
    }

    void apply_plugin_result(const ReplEditResult& result, std::vector<ReplEvent>& ev) {
        if (result.new_buffer) {
            buffer_ = *result.new_buffer;
            if (result.new_paren_balance) {
                paren_balance_ = *result.new_paren_balance;
            } else {
                paren_balance_ = paren_balance_for_buffer(buffer_);
            }
        }
        ev.insert(ev.end(), result.events.begin(), result.events.end());
        if (result.emit_prompt) {
            if (result.prompt_override) {
                ev.push_back({ ReplEvent::Kind::Prompt, *result.prompt_override });
            } else {
                ev.push_back({ ReplEvent::Kind::Prompt, prompt_for_state() });
            }
        }
    }

    std::string apply_line_transforms(std::string_view line) const {
        if (input_plugins_.empty()) return std::string(line);
        std::string transformed(line);
        auto state = buffer_state();
        for (const auto& plugin : input_plugins_) {
            transformed = plugin->transform_line(transformed, state, cfg_);
        }
        return transformed;
    }

    std::string continuation_prompt() const {
        if (input_plugins_.empty()) return cfg_.cont_prompt;
        return input_plugins_.back()->continuation_prompt(cfg_, buffer_state());
    }

    std::string prompt_for_state() const {
        if (should_wait_for_more_input()) return continuation_prompt();
        return cfg_.prompt;
    }

    void notify_plugins_reset() {
        for (const auto& plugin : input_plugins_) plugin->on_reset();
    }

    void notify_plugins_clear() {
        for (const auto& plugin : input_plugins_) plugin->on_clear();
    }

    static int paren_balance_for_buffer(std::string_view buffer) {
        int balance = 0;
        size_t start = 0;
        while (start < buffer.size()) {
            size_t end = buffer.find('\n', start);
            if (end == std::string_view::npos) end = buffer.size();
            balance += paren_delta(buffer.substr(start, end - start));
            if (end == buffer.size()) break;
            start = end + 1;
        }
        return balance;
    }

    std::unique_ptr<IEvalEngine> engine_;
    ReplConfig cfg_;
    std::string buffer_;
    int paren_balance_ = 0;
    std::vector<std::unique_ptr<IReplInputPlugin>> input_plugins_;
    std::unordered_map<std::string, MetaCommandEntry> meta_commands_;
};
