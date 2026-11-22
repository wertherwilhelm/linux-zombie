#pragma once
#include <replxx.hxx>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include "scheme_input.hpp"
#include "scheme_engine.h"

class ReplTerminal {
    using RX = replxx::Replxx;
    RX editor_;
    scheme_engine& engine_;
    std::string history_path_;
    static int characters(std::string_view text) {
        int n = 0;
        for (unsigned char c : text) if ((c & 0xc0) != 0x80) ++n;
        return n;
    }
    static bool special(std::string_view word) {
        static const std::set<std::string_view> words = {
            "define", "define-syntax", "syntax-rules", "lambda", "if", "cond", "case",
            "let", "let*", "letrec", "begin", "set!", "quote", "quasiquote", "unquote",
            "and", "or", "do", "delay", "else", "import", "define-library", "include"
        };
        return words.count(word) != 0;
    }
public:
    explicit ReplTerminal(scheme_engine& engine) : engine_(engine) {
        editor_.set_word_break_characters(" \t\n()[]\";'`,|");
        editor_.set_max_history_size(1000);
        editor_.set_unique_history(true);
        editor_.set_indent_multiline(true);
        editor_.set_no_color(std::getenv("NO_COLOR") != nullptr);
        editor_.set_completion_callback([this](const std::string& text, int& context) {
            RX::completions_t result;
            auto analysis = scheme_input::scan(text);
            if (!analysis.tokens.empty()) {
                auto& last = analysis.tokens.back();
                if (last.end == text.size() && (last.kind == scheme_input::Kind::Comment ||
                    last.kind == scheme_input::Kind::String)) return result;
            }
            size_t start = text.size();
            while (start && !scheme_input::delimiter(text[start - 1])) --start;
            auto prefix = std::string_view(text).substr(start);
            context = characters(prefix);
            std::set<std::string> words;
            for (auto& name : engine_.get_environment()->binding_names()) words.insert(name);
            for (auto name : {"define", "define-syntax", "syntax-rules", "lambda", "if", "cond",
                 "case", "let", "let*", "letrec", "begin", "set!", "quote", "quasiquote",
                 "and", "or", "do", "delay", "import", "define-library",
                 ":help", ":q", ":quit", ":exit", ":reset", ":clear", ":format", ":fmt", ":close", ":complete"})
                words.insert(name);
            for (const auto& word : words)
                if (word.compare(0, prefix.size(), prefix) == 0) result.emplace_back(word);
            return result;
        });
        editor_.set_highlighter_callback([](const std::string& text, RX::colors_t& colors) {
            auto analysis = scheme_input::scan(text);
            for (const auto& token : analysis.tokens) {
                auto word = std::string_view(text).substr(token.begin, token.end - token.begin);
                auto color = RX::Color::DEFAULT;
                switch (token.kind) {
                case scheme_input::Kind::String: color = RX::Color::GREEN; break;
                case scheme_input::Kind::Comment: color = RX::Color::GRAY; break;
                case scheme_input::Kind::Open:
                case scheme_input::Kind::Close: color = RX::Color::CYAN; break;
                case scheme_input::Kind::Prefix: color = RX::Color::MAGENTA; break;
                default:
                    if (special(word)) color = RX::Color::BRIGHTMAGENTA;
                    else if (word.front() == '#' || std::isdigit(static_cast<unsigned char>(word.front())) ||
                             ((word.front() == '-' || word.front() == '+') && word.size() > 1 &&
                              std::isdigit(static_cast<unsigned char>(word[1])))) color = RX::Color::YELLOW;
                }
                int begin = characters(std::string_view(text).substr(0, token.begin));
                int end = begin + characters(word);
                for (int i = begin; i < end && i < static_cast<int>(colors.size()); ++i) colors[i] = color;
            }
        });
        editor_.bind_key(RX::KEY::ENTER, [this](char32_t key) {
            auto state = editor_.get_state();
            std::string text = state.text();
            auto analysis = scheme_input::scan(text);
            if (!analysis.incomplete || (!text.empty() && text.front() == ':'))
                return editor_.invoke(RX::ACTION::COMMIT_LINE, key);
            // NEW_LINE inserts at the cursor, preserving the editable multiline buffer.
            editor_.invoke(RX::ACTION::NEW_LINE, key);
            if (!analysis.in_literal && state.cursor_position() == characters(text)) {
                auto next = editor_.get_state();
                std::string indented = next.text();
                int spaces = std::min(analysis.depth, 40) * 2;
                indented.append(static_cast<size_t>(spaces), ' ');
                editor_.set_state({indented.c_str(), next.cursor_position() + spaces});
            }
            return RX::ACTION_RESULT::CONTINUE;
        });
        editor_.bind_key_internal(RX::KEY::meta('\r'), "commit_line");
        try {
            std::filesystem::path dir;
            if (auto xdg = std::getenv("XDG_STATE_HOME"); xdg && *xdg) dir = xdg;
            else if (auto home = std::getenv("HOME"); home && *home)
                dir = std::filesystem::path(home) / ".local/state";
            if (!dir.empty()) {
                dir /= "zombie-scheme";
                std::filesystem::create_directories(dir);
                history_path_ = (dir / "history").string();
                editor_.history_load(history_path_);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "warning: history unavailable: " << e.what() << '\n';
        }
    }
    // Ctrl-C cancels the current edit; Ctrl-D on an empty edit returns EOF.
    bool read(const std::string& prompt, std::string& line, bool& cancelled) {
        errno = 0;
        const char* input = editor_.input(prompt);
        cancelled = !input && errno == EAGAIN;
        if (!input) return cancelled;
        line = input;
        if (!line.empty()) {
            editor_.history_add(line);
            if (!history_path_.empty() && !editor_.history_sync(history_path_)) {
                std::cerr << "warning: could not save REPL history\n";
                history_path_.clear();
            }
        }
        return true;
    }
};
