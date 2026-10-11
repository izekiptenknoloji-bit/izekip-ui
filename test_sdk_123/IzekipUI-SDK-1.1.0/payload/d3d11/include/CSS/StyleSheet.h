#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct lxb_css_parser;
struct lxb_css_stylesheet;

namespace IzekipUI::Css {

struct RawKeyframeDeclaration {
    std::string propertyName;
    std::string value;
};

struct RawKeyframe {
    float offset = 0.0f;
    std::vector<RawKeyframeDeclaration> declarations;
};

struct KeyframesDefinition {
    std::string name;
    std::vector<RawKeyframe> frames;
};

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
    const std::vector<KeyframesDefinition>& GetKeyframes() const { return m_keyframes; }
    bool PrefersReducedMotion() const { return m_prefersReducedMotion; }
    void SetPrefersReducedMotion(bool value) { m_prefersReducedMotion = value; }

    // Diagnostic only, not used for matching -- counts top-level style rules.
    size_t RuleCount() const;

private:
    StyleSheet() = default;

    std::string m_sourceText;
    std::vector<KeyframesDefinition> m_keyframes;
    bool m_prefersReducedMotion = false;
    lxb_css_parser* m_parser = nullptr;
    lxb_css_stylesheet* m_sheet = nullptr;
};

} // namespace IzekipUI::Css
