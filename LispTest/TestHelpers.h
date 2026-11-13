#pragma once
std::list<std::string> Tokenize4(const std::string& str)
{
    std::list<std::string> tokens;
    std::string::const_iterator it = str.begin();

    while (it != str.end()) {
        while (it != str.end() && *it == ' ') ++it;

        if (it == str.end()) break;

        if (*it == '(' || *it == ')') {
            tokens.emplace_back((*it == '(') ? "(" : ")");
            ++it;
        }
        else if (*it == '"') {
            // String literal: "hello world"
            auto start = it;
            ++it; // Skip opening quote
            while (it != str.end() && *it != '"') {
                if (*it == '\\' && (it + 1) != str.end()) {
                    ++it; // Skip escape character
                }
                ++it;
            }
            if (it != str.end()) ++it; // Skip closing quote
            tokens.emplace_back(start, it);
        }
        else if (*it == ';') {
            if ((it + 1) != str.end() && *(it + 1) == ';') {
                it = std::find_if(it, str.end(), [](char c) { return c == '\n' || c == '\r'; });
                if (it != str.end()) ++it;
            }
        }
        else if (*it == '#' && (it + 1) != str.end() && *(it + 1) == '\\') {
            // Character literal: #\c or #\space, etc.
            auto start = it;
            it += 2; // Skip #\

            // Check for named characters
            auto t = std::find_if(it, str.end(), [](char c) { return c == ' ' || c == '(' || c == ')'; });
            tokens.emplace_back(start, t);
            it = t;
        }
        else {
            auto t = std::find_if(it, str.end(), [](char c) { return c == ' ' || c == '(' || c == ')'; });
            tokens.emplace_back(it, t);
            it = t;
        }
    }

    return tokens;
}

void PrintList4(Object* o)
{
    if (o->is_atom()) {
        std::cout << o << std::endl;
    }
    else {
        if (!cdr(o)->is_atom() && !(cdr(o)->is_nil())) {
            std::cout << car(o);
            PrintList4(cdr(o));
        }
    }
}

Object* ReadFrom4(std::list<std::string>& tokens)
{
    static int depth = 0;
    depth++;

    const std::string token(tokens.front());
    tokens.pop_front();

    if (token == "(") {
        // Check if this is an empty list
        if (tokens.front() == ")") {
            tokens.pop_front();
            return nil;
        }

        // Build list by maintaining head and tail pointers
        ConsCell* result = nullptr;
        ConsCell* tail = nullptr;

        while (tokens.front() != ")") {
            Object* l = ReadFrom4(tokens);
            ConsCell* new_cell = new ConsCell(l, nil);

            if (result == nullptr) {
                // First element
                result = new_cell;
                tail = new_cell;
            }
            else {
                // Add to end of list
                tail->set_cdr_noclone(new_cell);
                tail = new_cell;
            }
        }
        tokens.pop_front();
        return result;
    }
    else
    {
        // Check for character literal: #\c or #\space, #\tab, #\newline
        if (token.size() >= 2 && token[0] == '#' && token[1] == '\\') {
            if (token.size() == 3) {
                // Single character: #\a
                return new a_char(token[2]);
            }
            else {
                // Named characters
                std::string charName = token.substr(2);
                if (charName == "space") return new a_char(' ');
                else if (charName == "tab") return new a_char('\t');
                else if (charName == "newline") return new a_char('\n');
                else if (charName == "return") return new a_char('\r');
                else return new a_char(token[2]); // fallback to first char
            }
        }

        // Check for string literal: "hello"
        if (token.size() >= 2 && token[0] == '"' && token[token.size() - 1] == '"') {
            // Strip the quotes and create string
            std::string str_value = token.substr(1, token.size() - 2);
            return new a_string(str_value);
        }

        std::istringstream token_stream(token);
        double double_value;

        // Try to parse as double (handles both integers and floats)
        if (token_stream >> double_value) {
            // Make sure we consumed the entire token
            std::string remaining;
            token_stream >> remaining;
            if (remaining.empty()) {
                return new a_double(double_value);
            }
        }

        // Not a valid number, treat as string/symbol
        return new a_string(token);
    }
}

///////////////////////////////////////////////////////////////

// convert given string to list of tokens
std::list<std::string> tokenize(const std::string& str)
{
    std::list<std::string> tokens;
    const char* s = str.c_str();
    while (*s)
    {
        while (*s == ' ')
            ++s;
        if (*s == '(' || *s == ')') {
            tokens.push_back(*s++ == '(' ? "(" : ")");
        }
        else if (*s == '+') {
            tokens.push_back("+");
            ++s;
        }
        else if (*s == ';') {
            const char* p = s;
            if (*(++p) == ';') {
                while (*p && *p != '\n' && *p != '\r') {
                    ++p;
                }
                ++p;
                s = p;
            }
        }
        else {
            const char* t = s;
            while (*t && *t != ' ' && *t != '(' && *t != ')') {
                ++t;
            }
            tokens.push_back(std::string(s, t));
            s = t;
        }
    }
    return tokens;
}

void printlist(Object* o)
{
    if (o->is_atom()) {
        std::cout << o << std::endl;
    }
    else {
        if (!cdr(o)->is_atom() && !(cdr(o)->is_nil())) {
            std::cout << car(o);
            printlist(cdr(o));
        }
    }
}
