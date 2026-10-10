#include "CSS/CssTokenizer.h"

#include <cctype>

namespace IzekipUI::Css {

namespace {
bool IsIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IsIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
}
} // namespace

Tokenizer::Tokenizer(std::string_view source) : m_source(source) {}

bool Tokenizer::IsAtEnd() const {
    return m_pos >= m_source.size();
}

char Tokenizer::Peek() const {
    return IsAtEnd() ? '\0' : m_source[m_pos];
}

char Tokenizer::Advance() {
    return m_source[m_pos++];
}

void Tokenizer::SkipWhitespaceAndComments() {
    while (!IsAtEnd()) {
        char c = Peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            Advance();
            continue;
        }
        if (c == '/' && m_pos + 1 < m_source.size() && m_source[m_pos + 1] == '*') {
            m_pos += 2;
            while (!IsAtEnd() && !(Peek() == '*' && m_pos + 1 < m_source.size() &&
                                    m_source[m_pos + 1] == '/')) {
                Advance();
            }
            if (!IsAtEnd()) {
                m_pos += 2; // consume "*/"
            }
            continue;
        }
        break;
    }
}

std::vector<Token> Tokenizer::Tokenize() {
    std::vector<Token> tokens;

    while (true) {
        SkipWhitespaceAndComments();
        if (IsAtEnd()) {
            tokens.push_back({TokenType::EndOfFile, ""});
            break;
        }

        char c = Peek();

        if (c == '{') { Advance(); tokens.push_back({TokenType::BraceOpen, "{"}); continue; }
        if (c == '}') { Advance(); tokens.push_back({TokenType::BraceClose, "}"}); continue; }
        if (c == '(') { Advance(); tokens.push_back({TokenType::ParenOpen, "("}); continue; }
        if (c == ')') { Advance(); tokens.push_back({TokenType::ParenClose, ")"}); continue; }
        if (c == ':') { Advance(); tokens.push_back({TokenType::Colon, ":"}); continue; }
        if (c == ';') { Advance(); tokens.push_back({TokenType::Semicolon, ";"}); continue; }
        if (c == ',') { Advance(); tokens.push_back({TokenType::Comma, ","}); continue; }
        if (c == '.') { Advance(); tokens.push_back({TokenType::Dot, "."}); continue; }

        if (c == '#') {
            Advance();
            std::string text;
            while (!IsAtEnd() && IsIdentChar(Peek())) {
                text += Advance();
            }
            tokens.push_back({TokenType::Hash, text});
            continue;
        }

        if (IsIdentStart(c)) {
            std::string text;
            while (!IsAtEnd() && IsIdentChar(Peek())) {
                text += Advance();
            }
            tokens.push_back({TokenType::Ident, text});
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
            std::string text;
            text += Advance();
            while (!IsAtEnd() && (std::isdigit(static_cast<unsigned char>(Peek())) || Peek() == '.')) {
                text += Advance();
            }
            if (!IsAtEnd() && IsIdentStart(Peek())) {
                // Unit suffix, e.g. "px" -> Dimension token.
                while (!IsAtEnd() && IsIdentChar(Peek())) {
                    text += Advance();
                }
                tokens.push_back({TokenType::Dimension, text});
            } else if (!IsAtEnd() && Peek() == '%') {
                text += Advance();
                tokens.push_back({TokenType::Percentage, text});
            } else {
                tokens.push_back({TokenType::Number, text});
            }
            continue;
        }

        // Unknown character: skip it rather than failing the whole parse.
        Advance();
    }

    return tokens;
}

} // namespace IzekipUI::Css
