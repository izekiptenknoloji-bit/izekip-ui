#include <cassert>
#include <iostream>

#include "Core/TextBuffer.h"

using namespace IzekipUI;

namespace {

void TestInitialState() {
    TextBuffer buffer;
    assert(buffer.Lines().size() == 1);
    assert(buffer.Lines()[0].empty());
    assert(buffer.GetCursor() == (TextBuffer::Position{0, 0}));
    std::cout << "TestInitialState passed\n";
}

void TestInsertCodepointIncludingTurkish() {
    TextBuffer buffer;
    buffer.InsertCodepoint(U'a');
    buffer.InsertCodepoint(U'b');
    buffer.InsertCodepoint(0x15F); // s-cedilla, ş
    assert(buffer.Lines()[0] == std::u32string({U'a', U'b', 0x15F}));
    assert(buffer.GetCursor() == (TextBuffer::Position{0, 3}));
    std::cout << "TestInsertCodepointIncludingTurkish passed\n";
}

void TestInsertNewlineSplitsMidLine() {
    TextBuffer buffer;
    buffer.InsertCodepoint(U'a');
    buffer.InsertCodepoint(U'b');
    buffer.InsertCodepoint(U'c');
    // cursor is now at column 3 (end); move it to column 1, between 'a' and 'b'.
    buffer.MoveHome();
    buffer.MoveRight();
    buffer.InsertNewline();

    assert(buffer.Lines().size() == 2);
    assert(buffer.Lines()[0] == std::u32string(U"a"));
    assert(buffer.Lines()[1] == std::u32string(U"bc"));
    assert(buffer.GetCursor() == (TextBuffer::Position{1, 0}));
    std::cout << "TestInsertNewlineSplitsMidLine passed\n";
}

void TestBackspaceAtColumnZeroMergesLines() {
    TextBuffer buffer;
    buffer.SetTextUtf8("ab\ncd");
    buffer.MoveDown();   // line 1 ("cd"), column clamped to 2 (end)
    buffer.MoveHome();   // column 0
    buffer.Backspace();  // merges "ab" + "cd" into one line, cursor after "ab"

    assert(buffer.Lines().size() == 1);
    assert(buffer.Lines()[0] == std::u32string(U"abcd"));
    assert(buffer.GetCursor() == (TextBuffer::Position{0, 2}));
    std::cout << "TestBackspaceAtColumnZeroMergesLines passed\n";
}

void TestDeleteForwardAtEndOfLineMergesLines() {
    TextBuffer buffer;
    buffer.SetTextUtf8("ab\ncd");
    buffer.MoveHome();       // line 0, column 0
    buffer.MoveEnd();        // line 0, column 2 (end of "ab")
    buffer.DeleteForward();  // merges "cd" onto "ab", cursor stays at (0, 2)

    assert(buffer.Lines().size() == 1);
    assert(buffer.Lines()[0] == std::u32string(U"abcd"));
    assert(buffer.GetCursor() == (TextBuffer::Position{0, 2}));
    std::cout << "TestDeleteForwardAtEndOfLineMergesLines passed\n";
}

void TestMovementAcrossLinesOfDifferentLengths() {
    TextBuffer buffer;
    buffer.SetTextUtf8("abcde\nxy");
    buffer.MoveDocumentEnd(); // line 1, column 2 (end of "xy")
    assert(buffer.GetCursor() == (TextBuffer::Position{1, 2}));

    buffer.MoveUp(); // column 2 fits within "abcde" (length 5), stays 2
    assert(buffer.GetCursor() == (TextBuffer::Position{0, 2}));

    buffer.MoveEnd(); // column 5, end of "abcde"
    assert(buffer.GetCursor() == (TextBuffer::Position{0, 5}));

    buffer.MoveDown(); // column 5 clamps down to "xy"'s length (2)
    assert(buffer.GetCursor() == (TextBuffer::Position{1, 2}));

    buffer.MoveRight(); // already at end of last line, no-op (no next line)
    assert(buffer.GetCursor() == (TextBuffer::Position{1, 2}));

    buffer.MoveLeft();
    assert(buffer.GetCursor() == (TextBuffer::Position{1, 1}));

    std::cout << "TestMovementAcrossLinesOfDifferentLengths passed\n";
}

void TestSetTextUtf8RoundTripsTurkishCharacters() {
    TextBuffer buffer;
    // ş ğ ı ö ü ç İ Ş Ğ Ö Ü Ç -- all 12 Turkish-specific characters in one line.
    buffer.SetTextUtf8("ş ğ ı ö ü ç İ Ş Ğ Ö Ü Ç");

    const std::u32string expected = {0x15F, U' ', 0x11F, U' ', 0x131, U' ', 0xF6,  U' ', 0xFC, U' ',
                                      0xE7,  U' ', 0x130, U' ', 0x15E, U' ', 0x11E, U' ', 0xD6, U' ',
                                      0xDC,  U' ', 0xC7};
    assert(buffer.Lines().size() == 1);
    assert(buffer.Lines()[0] == expected);
    std::cout << "TestSetTextUtf8RoundTripsTurkishCharacters passed\n";
}

} // namespace

void TestMalformedUtf8IsDroppedNotDecoded() {
    // Overlong NUL (C0 80), overlong '/' (E0 80 AF), surrogate (ED A0 80), above U+10FFFF (F4 90 80 80).
    const char* malformed[] = {"\xC0\x80", "\xE0\x80\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80"};
    for (const char* bytes : malformed) {
        TextBuffer buffer;
        buffer.SetTextUtf8(bytes);
        assert(buffer.GetTextUtf8().empty());
    }
    // Valid Turkish ğ (C4 9F) and a 4-byte character (F0 9F 98 80) round-trip unchanged.
    TextBuffer valid;
    valid.SetTextUtf8("\xC4\x9F\xF0\x9F\x98\x80");
    assert(valid.GetTextUtf8() == "\xC4\x9F\xF0\x9F\x98\x80");
    std::cout << "TestMalformedUtf8IsDroppedNotDecoded passed\n";
}

int main() {
    TestInitialState();
    TestInsertCodepointIncludingTurkish();
    TestInsertNewlineSplitsMidLine();
    TestBackspaceAtColumnZeroMergesLines();
    TestDeleteForwardAtEndOfLineMergesLines();
    TestMovementAcrossLinesOfDifferentLengths();
    TestSetTextUtf8RoundTripsTurkishCharacters();
    TestMalformedUtf8IsDroppedNotDecoded();
    std::cout << "All TextBufferTests passed.\n";
    return 0;
}
