#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace IzekipUI::Css {

// Phase 1 only needs enough CSS to express: type/.class/#id selectors (no combinators,
// no compound selectors) and a flat list of `property: value;` declarations.
enum class TokenType {
    Ident,      // Panel, background-color, solid identifiers...
    Hash,       // #mainPanel (selector) or #CCCCCC (color value) -- text excludes the '#'
    Dot,        // .
    Colon,      // :
    Semicolon,  // ;
    BraceOpen,  // {
    BraceClose, // }
    ParenOpen,  // (
    ParenClose, // )
    Comma,      // ,
    Number,     // 10
    Dimension,  // 10px -- text includes the unit, e.g. "10px"
    Percentage, // 10% -- text includes the '%', e.g. "10%"
    EndOfFile,
};

struct Token {
    TokenType type;
    std::string text;
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view source);

    std::vector<Token> Tokenize();

private:
    char Peek() const;
    char Advance();
    bool IsAtEnd() const;
    void SkipWhitespaceAndComments();

    std::string_view m_source;
    size_t m_pos = 0;
};

} // namespace IzekipUI::Css
