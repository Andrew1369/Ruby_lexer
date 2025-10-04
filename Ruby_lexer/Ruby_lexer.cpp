#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <unordered_set>

struct Token {
    std::string type;
    std::string lexeme;
    size_t line;
    size_t column;
};

struct Rule {
    std::string type;
    std::regex re;
    bool skip;
};

static std::string escape_for_output(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

class RubyLexer {
public:
    RubyLexer() {
        const char* kws[] = {
            "__ENCODING__", "__LINE__", "__FILE__",
            "BEGIN", "END",
            "alias", "and", "begin", "break", "case", "class", "def", "defined?",
            "do", "else", "elsif", "end", "ensure", "false", "for", "if", "in",
            "module", "next", "nil", "not", "or", "redo", "rescue", "retry",
            "return", "self", "super", "then", "true", "undef", "unless", "until",
            "when", "while", "yield"
        };
        keywords.insert(kws, kws + sizeof(kws) / sizeof(kws[0]));

        // Order matters
        rules.push_back({ "COMMENT_BLOCK", std::regex("^=begin[\\s\\S]*?=end"), false });
        rules.push_back({ "COMMENT", std::regex("^#.*"), false });
        rules.push_back({ "STRING", std::regex("^\"(?:\\\\.|[\\s\\S])*?\""), false });
        rules.push_back({ "STRING", std::regex("^'(?:\\\\.|[\\s\\S])*?'"), false });
        rules.push_back({ "REGEX", std::regex("^/(?:\\\\/|\\\\.|[^/\\n])[\\s\\S]*?/[a-zA-Z]*"), false });
        rules.push_back({ "SYMBOL", std::regex("^:'(?:\\\\.|[\\s\\S])*?'"), false });
        rules.push_back({ "SYMBOL", std::regex("^:\"(?:\\\\.|[\\s\\S])*?\""), false });
        rules.push_back({ "SYMBOL", std::regex("^:[A-Za-z_][A-Za-z0-9_]*[!?=]?"), false });
        rules.push_back({ "CLASS_VAR", std::regex("^@@[A-Za-z_][A-Za-z0-9_]*"), false });
        rules.push_back({ "INSTANCE_VAR", std::regex("^@[A-Za-z_][A-Za-z0-9_]*"), false });
        rules.push_back({ "GLOBAL_VAR", std::regex("^\\$\\d+"), false });
        rules.push_back({ "GLOBAL_VAR", std::regex("^\\$[A-Za-z_][A-Za-z0-9_]*"), false });
        rules.push_back({ "NUMBER_HEX", std::regex("^0[xX][0-9A-Fa-f_]+"), false });
        rules.push_back({ "NUMBER_BIN", std::regex("^0[bB][01_]+"), false });
        rules.push_back({ "NUMBER_OCT", std::regex("^0[oO][0-7_]+"), false });
        rules.push_back({ "NUMBER_FLOAT", std::regex("^\\d[\\d_]*\\.\\d[\\d_]*(?:[eE][+-]?\\d[\\d_]*)?"), false });
        rules.push_back({ "NUMBER_FLOAT", std::regex("^\\d[\\d_]*(?:[eE][+-]?\\d[\\d_]*)"), false });
        rules.push_back({ "NUMBER_INT", std::regex("^\\d[\\d_]*"), false });
        rules.push_back({ "OP", std::regex("^(<=>|===|<<=|>>=|\\*\\*=|&&=|\\|\\|=|\\+=|-=|\\*=|/=|%=|&=|\\|=|\\^=|<=|>=|==|!=|=~|!~|\\.\\.\\.|::|=>|\\*\\*|<<|>>|&&|\\|\\||&\\.|\\.\\.)"), false });
        rules.push_back({ "OP", std::regex("^[+\\-*/%&|\\^~!=<>?:.,;()\\[\\]{}]"), false });
        rules.push_back({ "IDENT_OR_KW", std::regex("^[A-Za-z_][A-Za-z0-9_]*[!?=]?"), false });
        rules.push_back({ "WS", std::regex("^\\s+"), true });
    }

    std::vector<Token> tokenize(const std::string& input) const {
        std::vector<Token> tokens;
        size_t pos = 0;
        size_t line = 1, col = 1;
        const size_t n = input.size();

        while (pos < n) {
            bool matched = false;
            std::cmatch m;
            const char* start = input.c_str() + pos;

            for (const auto& rule : rules) {
                if (std::regex_search(start, m, rule.re, std::regex_constants::match_continuous)) {
                    std::string lex = m.str(0);
                    if (!rule.skip) {
                        Token tk;
                        tk.lexeme = lex;
                        tk.type = classify(rule.type, lex);
                        tk.line = line;
                        tk.column = col;
                        tokens.push_back(std::move(tk));
                    }
                    update_position(lex, line, col);
                    pos += lex.size();
                    matched = true;
                    break;
                }
            }

            if (!matched) {
                std::string bad(1, input[pos]);
                Token tk;
                tk.lexeme = bad;
                tk.type = "ERROR";
                tk.line = line;
                tk.column = col;
                tokens.push_back(std::move(tk));
                update_position(bad, line, col);
                ++pos;
            }
        }
        return tokens;
    }

private:
    std::vector<Rule> rules;
    std::unordered_set<std::string> keywords;

    static void update_position(const std::string& s, size_t& line, size_t& col) {
        for (char c : s) {
            if (c == '\n') {
                ++line; col = 1;
            }
            else {
                ++col;
            }
        }
    }

    std::string classify(const std::string& rawType, const std::string& lex) const {
        if (rawType == "IDENT_OR_KW") {
            if (keywords.count(lex)) return "KEYWORD";
            if (!lex.empty() && std::isupper(static_cast<unsigned char>(lex[0]))) return "CONSTANT";
            return "IDENTIFIER";
        }
        return rawType;
    }
};

static std::string read_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

static void print_tokens(const std::vector<Token>& toks) {
    for (const auto& t : toks) {
        std::cout << "< " << escape_for_output(t.lexeme) << " , " << t.type << " >\n";
    }
}

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

int main(int argc, char** argv) {
    try {
        std::string path;
        if (argc >= 2) {
            path = argv[1];
        }
        else {
            std::cout << "Enter the path to the Ruby file (.rb): ";
            std::getline(std::cin, path);
            path = trim(path);
            if (path.empty()) {
                std::cerr << "You must enter the path to the file.\n";
                return 1;
            }
        }
        std::string input = read_file(path);
        RubyLexer lexer;
        auto tokens = lexer.tokenize(input);
        print_tokens(tokens);
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
