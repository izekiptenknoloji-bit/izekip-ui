#pragma once

#include <filesystem>
#include <string>
#include <string_view>

struct lxb_css_parser;
struct lxb_css_stylesheet;

namespace IzekipUI::Css {

// Wraps a lexbor-parsed stylesheet. LexborBridge is the only consumer that reaches into
// GetRaw()/GetSourceText() (to walk the rule tree and extract declaration value text via
// byte offsets into the original source) -- everything else goes through StyleApplier.
class StyleSheet {
public:
    static StyleSheet LoadFromFile(const std::filesystem::path& path);
    static StyleSheet LoadFromString(std::string_view css);

    ~StyleSheet();
    StyleSheet(StyleSheet&& other) noexcept;
    StyleSheet& operator=(StyleSheet&& other) noexcept;
    StyleSheet(const StyleSheet&) = delete;
    StyleSheet& operator=(const StyleSheet&) = delete;

    lxb_css_stylesheet* GetRaw() const { return m_sheet; }

    // Declaration offsets in the parsed tree index into this exact string -- it must
    // outlive every MatchedDeclaration extracted from this stylesheet.
    const std::string& GetSourceText() const { return m_sourceText; }

    // Diagnostic only, not used for matching -- counts top-level style rules.
    size_t RuleCount() const;

private:
    StyleSheet() = default;

    std::string m_sourceText;
    lxb_css_parser* m_parser = nullptr;
    lxb_css_stylesheet* m_sheet = nullptr;
};

} // namespace IzekipUI::Css
