#include "Core/Utf8.h"

namespace IzekipUI::detail {

std::u32string DecodeUtf8(const std::string& utf8Text) {
    std::u32string result;
    size_t i = 0;
    const size_t size = utf8Text.size();
    while (i < size) {
        const unsigned char b0 = static_cast<unsigned char>(utf8Text[i]);
        char32_t codepoint = 0;
        size_t extraBytes = 0;

        if (b0 < 0x80) {
            codepoint = b0;
        } else if ((b0 & 0xE0) == 0xC0) {
            codepoint = b0 & 0x1F;
            extraBytes = 1;
        } else if ((b0 & 0xF0) == 0xE0) {
            codepoint = b0 & 0x0F;
            extraBytes = 2;
        } else if ((b0 & 0xF8) == 0xF0) {
            codepoint = b0 & 0x07;
            extraBytes = 3;
        } else {
            ++i; // invalid lead byte
            continue;
        }

        if (i + extraBytes >= size) {
            break; // truncated sequence at end of string
        }

        bool valid = true;
        for (size_t k = 1; k <= extraBytes; ++k) {
            const unsigned char cont = static_cast<unsigned char>(utf8Text[i + k]);
            if ((cont & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (cont & 0x3F);
        }

        if (!valid) {
            ++i; // invalid continuation byte -- skip just the lead byte
            continue;
        }

        result.push_back(codepoint);
        i += extraBytes + 1;
    }
    return result;
}

std::string EncodeUtf8(const std::u32string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char32_t cp : text) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            if (cp >= 0xD800 && cp <= 0xDFFF) {
                continue; // lone surrogate
            }
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0x10FFFF) {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

} // namespace IzekipUI::detail
