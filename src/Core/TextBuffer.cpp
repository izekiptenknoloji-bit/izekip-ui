#include "Core/TextBuffer.h"

#include <algorithm>

#include "Core/Utf8.h"
#include "Platform/Diagnostics.h"

namespace IzekipUI {

TextBuffer::TextBuffer() {
    m_lines.emplace_back();
}

void TextBuffer::SetTextUtf8(const std::string& utf8Text) {
    std::string input = utf8Text;
    if (input.size() > kMaxInputBytes) {
        input.resize(kMaxInputBytes); // the decoder drops a cut-off trailing sequence
        Platform::Diagnostics::Increment(Platform::Diagnostics::Counter::TextRejected);
    }
    std::u32string decoded = detail::DecodeUtf8(input);
    if (decoded.size() > kMaxCharacters) {
        decoded.resize(kMaxCharacters);
        Platform::Diagnostics::Increment(Platform::Diagnostics::Counter::TextRejected);
    }

    m_lines.clear();
    size_t start = 0;
    while (true) {
        if (m_lines.size() >= kMaxLines) {
            Platform::Diagnostics::Increment(Platform::Diagnostics::Counter::TextRejected);
            break;
        }
        const size_t newlinePos = decoded.find(U'\n', start);
        if (newlinePos == std::u32string::npos) {
            m_lines.push_back(decoded.substr(start));
            break;
        }
        m_lines.push_back(decoded.substr(start, newlinePos - start));
        start = newlinePos + 1;
    }
    m_cursor = Position{};
    RecountCharacters();
    ClampCursor();
}

std::string TextBuffer::GetTextUtf8() const {
    std::u32string joined;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        if (i > 0) {
            joined.push_back(U'\n');
        }
        joined += m_lines[i];
    }
    return detail::EncodeUtf8(joined);
}

void TextBuffer::InsertCodepoint(char32_t codepoint) {
    if (m_characterCount >= kMaxCharacters) {
        Platform::Diagnostics::Increment(Platform::Diagnostics::Counter::TextRejected);
        return;
    }
    std::u32string& line = m_lines[m_cursor.line];
    line.insert(line.begin() + static_cast<ptrdiff_t>(m_cursor.column), codepoint);
    ++m_cursor.column;
    ++m_characterCount;
    ClampCursor();
}

void TextBuffer::InsertNewline() {
    if (m_characterCount >= kMaxCharacters || m_lines.size() >= kMaxLines) {
        Platform::Diagnostics::Increment(Platform::Diagnostics::Counter::TextRejected);
        return;
    }
    std::u32string& line = m_lines[m_cursor.line];
    std::u32string tail = line.substr(m_cursor.column);
    line.resize(m_cursor.column);
    m_lines.insert(m_lines.begin() + static_cast<ptrdiff_t>(m_cursor.line) + 1, std::move(tail));
    ++m_cursor.line;
    m_cursor.column = 0;
    ++m_characterCount;
    ClampCursor();
}

void TextBuffer::Backspace() {
    if (m_cursor.column > 0) {
        std::u32string& line = m_lines[m_cursor.line];
        line.erase(line.begin() + static_cast<ptrdiff_t>(m_cursor.column) - 1);
        --m_cursor.column;
        --m_characterCount;
    } else if (m_cursor.line > 0) {
        const size_t prevLineLength = m_lines[m_cursor.line - 1].size();
        m_lines[m_cursor.line - 1] += m_lines[m_cursor.line];
        m_lines.erase(m_lines.begin() + static_cast<ptrdiff_t>(m_cursor.line));
        --m_cursor.line;
        m_cursor.column = prevLineLength;
        --m_characterCount; // the removed newline
    }
    ClampCursor();
}

void TextBuffer::DeleteForward() {
    std::u32string& line = m_lines[m_cursor.line];
    if (m_cursor.column < line.size()) {
        line.erase(line.begin() + static_cast<ptrdiff_t>(m_cursor.column));
        --m_characterCount;
    } else if (m_cursor.line + 1 < m_lines.size()) {
        line += m_lines[m_cursor.line + 1];
        m_lines.erase(m_lines.begin() + static_cast<ptrdiff_t>(m_cursor.line) + 1);
        --m_characterCount; // the removed newline
    }
    ClampCursor();
}

void TextBuffer::RecountCharacters() {
    m_characterCount = 0;
    for (const auto& line : m_lines) {
        m_characterCount += line.size();
    }
    if (!m_lines.empty()) {
        m_characterCount += m_lines.size() - 1; // one newline between consecutive lines
    }
}

void TextBuffer::MoveLeft() {
    if (m_cursor.column > 0) {
        --m_cursor.column;
    } else if (m_cursor.line > 0) {
        --m_cursor.line;
        m_cursor.column = m_lines[m_cursor.line].size();
    }
    ClampCursor();
}

void TextBuffer::MoveRight() {
    if (m_cursor.column < m_lines[m_cursor.line].size()) {
        ++m_cursor.column;
    } else if (m_cursor.line + 1 < m_lines.size()) {
        ++m_cursor.line;
        m_cursor.column = 0;
    }
    ClampCursor();
}

void TextBuffer::MoveUp() {
    if (m_cursor.line > 0) {
        --m_cursor.line;
        m_cursor.column = std::min(m_cursor.column, m_lines[m_cursor.line].size());
    }
    ClampCursor();
}

void TextBuffer::MoveDown() {
    if (m_cursor.line + 1 < m_lines.size()) {
        ++m_cursor.line;
        m_cursor.column = std::min(m_cursor.column, m_lines[m_cursor.line].size());
    }
    ClampCursor();
}

void TextBuffer::MoveHome() {
    m_cursor.column = 0;
}

void TextBuffer::MoveEnd() {
    m_cursor.column = m_lines[m_cursor.line].size();
}

void TextBuffer::MoveDocumentEnd() {
    m_cursor.line = m_lines.size() - 1;
    m_cursor.column = m_lines[m_cursor.line].size();
}

void TextBuffer::ClampCursor() {
    m_cursor.line = std::min(m_cursor.line, m_lines.size() - 1);
    m_cursor.column = std::min(m_cursor.column, m_lines[m_cursor.line].size());
}

} // namespace IzekipUI
