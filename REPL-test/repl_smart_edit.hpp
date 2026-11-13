#pragma once
#include <string>
#include <string_view>
#include <cctype>
#include "repl_core.hpp"

class LispSmartEditPlugin : public IReplInputPlugin {
public:
    explicit LispSmartEditPlugin(int indent_width = 2)
        : indent_width_(indent_width > 0 ? indent_width : 2) {}

    void register_meta_commands(ReplSession& session) override {
        session.register_meta_command("format", "Reformat current input buffer",
            [this](std::string_view, const ReplBufferState& state, const ReplConfig&) {
                return format_command(state);
            });
        session.register_meta_command("fmt", "Alias for format",
            [this](std::string_view, const ReplBufferState& state, const ReplConfig&) {
                return format_command(state);
            });
        session.register_meta_command("close", "Auto-close open parens in buffer",
            [this](std::string_view, const ReplBufferState& state, const ReplConfig&) {
                return close_command(state);
            });
        session.register_meta_command("complete", "Alias for close",
            [this](std::string_view, const ReplBufferState& state, const ReplConfig&) {
                return close_command(state);
            });
    }

    std::string continuation_prompt(const ReplConfig& cfg,
                                    const ReplBufferState& state) override {
        int depth = state.paren_balance;
        if (depth < 0) depth = 0;
        std::string prompt = cfg.cont_prompt;
        prompt.append(static_cast<size_t>(depth * indent_width_), ' ');
        return prompt;
    }

private:
    int indent_width_ = 2;

    ReplEditResult format_command(const ReplBufferState& state) const {
        ReplEditResult result;
        result.handled = true;

        if (is_effectively_empty(state.buffer)) {
            result.events.push_back({ ReplEvent::Kind::Info, "buffer is empty" });
            return result;
        }

        auto formatted = format_buffer(state.buffer, indent_width_);
        if (formatted == state.buffer) {
            result.events.push_back({ ReplEvent::Kind::Info, "buffer already formatted" });
            return result;
        }

        result.new_buffer = formatted;
        result.new_paren_balance = paren_balance_for_buffer(formatted);
        result.events.push_back({ ReplEvent::Kind::Info, "buffer formatted:" });
        result.events.push_back({ ReplEvent::Kind::Info, strip_trailing_newline(formatted) });
        return result;
    }

    ReplEditResult close_command(const ReplBufferState& state) const {
        ReplEditResult result;
        result.handled = true;

        if (state.paren_balance <= 0) {
            result.events.push_back({ ReplEvent::Kind::Info, "no open parens to close" });
            return result;
        }

        std::string new_buffer(state.buffer);
        if (new_buffer.empty() || new_buffer.back() != '\n') new_buffer.push_back('\n');
        new_buffer.append(static_cast<size_t>(state.paren_balance), ')');
        new_buffer.push_back('\n');

        result.new_buffer = std::move(new_buffer);
        result.new_paren_balance = 0;
        result.events.push_back({ ReplEvent::Kind::Info,
            "closed " + std::to_string(state.paren_balance) + " paren(s)" });
        return result;
    }

    static std::string_view trim_left(std::string_view s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
            s.remove_prefix(1);
        return s;
    }

    static bool is_effectively_empty(std::string_view s) {
        bool in_string = false;
        bool escape = false;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];

            if (in_string) {
                if (escape) { escape = false; continue; }
                if (c == '\\') { escape = true; continue; }
                if (c == '"') { in_string = false; continue; }
                continue;
            }

            if (c == ';') {
                while (i < s.size() && s[i] != '\n') ++i;
                continue;
            }

            if (c == '"') { in_string = true; continue; }
            if (!std::isspace(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }

    static int paren_delta_line(std::string_view line) {
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

            if (c == ';') break;

            if (c == '"') { in_string = true; continue; }
            if (c == '(') ++delta;
            if (c == ')') --delta;
        }
        return delta;
    }

    static int paren_balance_for_buffer(std::string_view buffer) {
        int balance = 0;
        size_t start = 0;
        while (start < buffer.size()) {
            size_t end = buffer.find('\n', start);
            if (end == std::string_view::npos) end = buffer.size();
            balance += paren_delta_line(buffer.substr(start, end - start));
            if (end == buffer.size()) break;
            start = end + 1;
        }
        return balance;
    }

    static void scan_line_for_balance(std::string_view line,
                                      int* depth,
                                      bool* in_string,
                                      bool* escape) {
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];

            if (*in_string) {
                if (*escape) { *escape = false; continue; }
                if (c == '\\') { *escape = true; continue; }
                if (c == '"') { *in_string = false; continue; }
                continue;
            }

            if (c == ';') break;

            if (c == '"') { *in_string = true; continue; }
            if (c == '(') ++(*depth);
            if (c == ')') --(*depth);
        }
    }

    static std::string format_buffer(std::string_view buffer, int indent_width) {
        std::string out;
        out.reserve(buffer.size());

        int depth = 0;
        bool in_string = false;
        bool escape = false;

        size_t start = 0;
        while (start <= buffer.size()) {
            size_t end = buffer.find('\n', start);
            bool has_newline = true;
            if (end == std::string_view::npos) {
                end = buffer.size();
                has_newline = false;
            }
            std::string_view line = buffer.substr(start, end - start);

            if (in_string) {
                out.append(line);
                if (has_newline) out.push_back('\n');
                scan_line_for_balance(line, &depth, &in_string, &escape);
            } else {
                auto trimmed = trim_left(line);
                if (trimmed.empty()) {
                    if (has_newline) out.push_back('\n');
                } else if (trimmed.front() == ';') {
                    int indent_depth = depth;
                    if (indent_depth < 0) indent_depth = 0;
                    out.append(static_cast<size_t>(indent_depth * indent_width), ' ');
                    out.append(trimmed);
                    if (has_newline) out.push_back('\n');
                    scan_line_for_balance(trimmed, &depth, &in_string, &escape);
                } else {
                    size_t close_prefix = 0;
                    while (close_prefix < trimmed.size() && trimmed[close_prefix] == ')') {
                        ++close_prefix;
                    }
                    int indent_depth = depth - static_cast<int>(close_prefix);
                    if (indent_depth < 0) indent_depth = 0;
                    out.append(static_cast<size_t>(indent_depth * indent_width), ' ');
                    out.append(trimmed);
                    if (has_newline) out.push_back('\n');
                    scan_line_for_balance(trimmed, &depth, &in_string, &escape);
                }
            }

            if (!has_newline) break;
            start = end + 1;
        }

        return out;
    }

    static std::string strip_trailing_newline(std::string_view text) {
        if (!text.empty() && text.back() == '\n') {
            text.remove_suffix(1);
        }
        return std::string(text);
    }
};
