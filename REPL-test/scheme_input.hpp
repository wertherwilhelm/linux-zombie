#pragma once
#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

// Lightweight lexical analysis for the editor; evaluation still uses scheme_reader.
namespace scheme_input {
enum class Kind { Atom, String, Comment, Open, Close, Prefix };
struct Token { size_t begin, end; Kind kind; };
struct Analysis {
    std::vector<Token> tokens;
    int depth = 0;
    bool incomplete = false;
    bool error = false;
    bool has_datum = false;
    bool in_literal = false;
};
inline bool delimiter(char c) {
    return std::isspace(static_cast<unsigned char>(c)) ||
        std::string_view("()[]\";'`,|").find(c) != std::string_view::npos;
}
inline Analysis scan(std::string_view s) {
    Analysis a;
    std::vector<size_t> opens;
    bool expecting_datum = false;
    for (size_t i = 0; i < s.size();) {
        size_t begin = i;
        char c = s[i++];
        Kind kind = Kind::Atom;
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        if (c == ';') {
            while (i < s.size() && s[i] != '\n') ++i;
            kind = Kind::Comment;
        } else if (c == '#' && i < s.size() && s[i] == '|') {
            ++i;
            int nesting = 1;
            while (i < s.size() && nesting) {
                if (s.substr(i, 2) == "#|") { ++nesting; i += 2; }
                else if (s.substr(i, 2) == "|#") { --nesting; i += 2; }
                else ++i;
            }
            a.incomplete |= nesting != 0;
            a.in_literal = nesting != 0;
            kind = Kind::Comment;
        } else if (c == '"' || c == '|') {
            bool closed = false;
            while (i < s.size()) {
                char next = s[i++];
                if (next == '\\' && i < s.size()) ++i;
                else if (next == c) { closed = true; break; }
            }
            a.incomplete |= !closed;
            a.in_literal = !closed;
            a.has_datum = true;
            expecting_datum = false;
            kind = Kind::String;
        } else if (c == '#' && i < s.size() && s[i] == '\\') {
            ++i;
            if (i == s.size()) a.incomplete = true;
            else { ++i; while (i < s.size() && !delimiter(s[i])) ++i; }
            a.has_datum = true;
            expecting_datum = false;
        } else if (c == '(' || c == '[') {
            opens.push_back(begin);
            a.has_datum = true;
            expecting_datum = false;
            kind = Kind::Open;
        } else if (c == ')' || c == ']') {
            if (opens.empty() || expecting_datum) a.error = true;
            else {
                char opening = s[opens.back()];
                if ((opening == '(') != (c == ')')) a.error = true;
                opens.pop_back();
            }
            expecting_datum = false;
            kind = Kind::Close;
        } else if (c == '\'' || c == '`' || c == ',') {
            if (c == ',' && i < s.size() && s[i] == '@') ++i;
            expecting_datum = true;
            a.has_datum = true;
            kind = Kind::Prefix;
        } else {
            while (i < s.size() && !delimiter(s[i])) ++i;
            // Vector prefixes belong to their following opening delimiter.
            auto word = s.substr(begin, i - begin);
            if (!((word == "#" || word == "#u8") && i < s.size() && s[i] == '(')) {
                a.has_datum = true;
                expecting_datum = false;
            }
        }
        a.tokens.push_back({begin, i, kind});
    }
    a.depth = static_cast<int>(opens.size());
    a.incomplete |= a.depth > 0 || expecting_datum;
    if (a.error) a.incomplete = false; // Let the reader report malformed input promptly.
    return a;
}
} // namespace scheme_input
