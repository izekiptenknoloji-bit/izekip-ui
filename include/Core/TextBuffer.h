#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace IzekipUI {

// Platform- and renderer-independent multi-line text editing model. Lines are stored as
// Unicode codepoints (not UTF-8 bytes) so cursor movement/insertion/deletion never has to
// deal with multi-byte sequences -- GLFW's char callback already delivers decoded
// codepoints directly, and the only place raw UTF-8 bytes appear is SetTextUtf8, for
// seeding initial text from a C++ string literal.
class TextBuffer {
public:
    struct Position {
        size_t line = 0;
        size_t column = 0; // codepoint index into that line, not a byte index
        bool operator==(const Position& other) const { return line == other.line && column == other.column; }
        bool operator!=(const Position& other) const { return !(*this == other); }
    };

    // Hard limits. A newline counts as one character. Input beyond a limit is dropped and counted
    // in Platform::Diagnostics (Counter::TextRejected); the edit is refused rather than growing.
    static constexpr std::size_t kMaxCharacters = 1000000;
    static constexpr std::size_t kMaxLines = 50000;
    static constexpr std::size_t kMaxInputBytes = 4 * 1024 * 1024;

    TextBuffer(); // one empty line, cursor at (0, 0)

    void SetTextUtf8(const std::string& utf8Text);
    std::string GetTextUtf8() const; // lines joined with '\n', encoded as UTF-8
    const std::vector<std::u32string>& Lines() const { return m_lines; }
    Position GetCursor() const { return m_cursor; }
    std::size_t CharacterCount() const { return m_characterCount; }

    void InsertCodepoint(char32_t codepoint);
    void InsertNewline();
    void Backspace();
    void DeleteForward();
    void MoveLeft();
    void MoveRight();
    void MoveUp();
    void MoveDown();
    void MoveHome();
    void MoveEnd();
    void MoveDocumentEnd();

private:
    void ClampCursor();
    void RecountCharacters();

    std::vector<std::u32string> m_lines;
    Position m_cursor;
    std::size_t m_characterCount = 0;
};

} // namespace IzekipUI
