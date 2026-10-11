#pragma once

#include <string>

// Internal (lives under src/, never installed, not part of the public API): shared by
// Core/TextBuffer.cpp and Widgets/Label.cpp.
namespace IzekipUI::detail {

// Decodes UTF-8 into Unicode codepoints. Malformed/truncated sequences are skipped rather
// than throwing -- inputs are C++ string literals and widget text, not untrusted files.
std::u32string DecodeUtf8(const std::string& utf8Text);

// Inverse of DecodeUtf8. Codepoints outside the Unicode range (or surrogates) are skipped.
std::string EncodeUtf8(const std::u32string& text);

} // namespace IzekipUI::detail
