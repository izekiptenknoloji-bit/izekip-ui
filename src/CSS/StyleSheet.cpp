#include "CSS/StyleSheet.h"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C" {
#include <lexbor/css/css.h>
#include <lexbor/css/rule.h>
#include <lexbor/css/stylesheet.h>
}

namespace IzekipUI::Css {

namespace {

std::string Trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    return first < last ? std::string(first, last) : std::string{};
}

} // namespace

namespace {

// lxb_css_parser_destroy does not free the selector pool that lxb_css_parser_selectors_init
// allocates (lexbor 3.0.1), so release that pool first or every parsed sheet leaks it.
void DestroyParser(lxb_css_parser_t* parser) {
    if (!parser) {
        return;
    }
    if (parser->selectors) {
        parser->selectors = lxb_css_selectors_destroy(parser->selectors, true);
    }
    lxb_css_parser_destroy(parser, true);
}

} // namespace

StyleSheet::~StyleSheet() {
    if (m_sheet) {
        lxb_css_stylesheet_destroy(m_sheet, true);
    }
    DestroyParser(m_parser);
}

StyleSheet::StyleSheet(StyleSheet&& other) noexcept
    : m_sourceText(std::move(other.m_sourceText)), m_keyframes(std::move(other.m_keyframes)),
      m_prefersReducedMotion(other.m_prefersReducedMotion), m_parser(other.m_parser),
      m_sheet(other.m_sheet) {
    other.m_parser = nullptr;
    other.m_sheet = nullptr;
}

StyleSheet& StyleSheet::operator=(StyleSheet&& other) noexcept {
    if (this != &other) {
        if (m_sheet) lxb_css_stylesheet_destroy(m_sheet, true);
        DestroyParser(m_parser);

        m_sourceText = std::move(other.m_sourceText);
        m_keyframes = std::move(other.m_keyframes);
        m_prefersReducedMotion = other.m_prefersReducedMotion;
        m_parser = other.m_parser;
        m_sheet = other.m_sheet;
        other.m_parser = nullptr;
        other.m_sheet = nullptr;
    }
    return *this;
}

StyleSheet StyleSheet::LoadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("CSS: could not open file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return LoadFromString(buffer.str());
}

StyleSheet StyleSheet::LoadFromString(std::string_view css) {
    StyleSheet sheet;
    sheet.m_sourceText.assign(css);

    sheet.m_parser = lxb_css_parser_create();
    lxb_css_parser_init(sheet.m_parser, nullptr);
    lxb_css_parser_selectors_init(sheet.m_parser);

    sheet.m_sheet = lxb_css_stylesheet_create(nullptr);
    lxb_status_t status = lxb_css_stylesheet_parse(
        sheet.m_sheet, sheet.m_parser, reinterpret_cast<const lxb_char_t*>(sheet.m_sourceText.data()),
        sheet.m_sourceText.size());
    if (status != LXB_STATUS_OK) {
        throw std::runtime_error("CSS: lexbor failed to parse stylesheet");
    }

    lxb_css_rule_t* rule = lxb_css_rule_list(sheet.m_sheet->root)->first;
    while (rule != nullptr) {
        if (rule->type == LXB_CSS_RULE_AT_RULE) {
            auto* at = lxb_css_rule_at(rule);
            std::string atName;
            if (at->type == LXB_CSS_AT_RULE__CUSTOM && at->u.custom) {
                atName.assign(reinterpret_cast<const char*>(at->u.custom->name.data),
                              at->u.custom->name.length);
            } else {
                const size_t nameStart = std::min(at->name_begin, sheet.m_sourceText.size());
                size_t nameEnd = nameStart;
                while (nameEnd < sheet.m_sourceText.size() &&
                       (std::isalnum(static_cast<unsigned char>(sheet.m_sourceText[nameEnd])) ||
                        sheet.m_sourceText[nameEnd] == '-' || sheet.m_sourceText[nameEnd] == '_')) {
                    ++nameEnd;
                }
                atName = sheet.m_sourceText.substr(nameStart, nameEnd - nameStart);
            }
            std::transform(atName.begin(), atName.end(), atName.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            lxb_css_rule_list_t* keyframeBlock = nullptr;
            if (at->type == LXB_CSS_AT_RULE__CUSTOM && at->u.custom) {
                keyframeBlock = at->u.custom->block;
            } else if (at->type == LXB_CSS_AT_RULE__UNDEF && at->u.undef) {
                keyframeBlock = at->u.undef->block;
            }
            if (atName == "keyframes" && keyframeBlock) {
                const size_t preludeBegin = std::min(at->prelude_begin, sheet.m_sourceText.size());
                const size_t preludeEnd = std::min(at->prelude_end, sheet.m_sourceText.size());
                KeyframesDefinition definition;
                definition.name = Trim(sheet.m_sourceText.substr(preludeBegin, preludeEnd - preludeBegin));
                for (lxb_css_rule_t* frameRule = keyframeBlock->first; frameRule;
                     frameRule = frameRule->next) {
                    size_t selectorBegin = 0;
                    size_t selectorEnd = 0;
                    lxb_css_rule_declaration_list_t* declarations = nullptr;
                    if (frameRule->type == LXB_CSS_RULE_STYLE) {
                        auto* style = lxb_css_rule_style(frameRule);
                        selectorBegin = style->prelude_begin;
                        selectorEnd = style->prelude_end;
                        declarations = style->declarations;
                    } else if (frameRule->type == LXB_CSS_RULE_BAD_STYLE) {
                        auto* style = lxb_css_rule_bad_style(frameRule);
                        selectorBegin = style->prelude_begin;
                        selectorEnd = style->prelude_end;
                        declarations = style->declarations;
                    }
                    if (!declarations || selectorBegin > sheet.m_sourceText.size() ||
                        selectorEnd > sheet.m_sourceText.size() || selectorEnd < selectorBegin) {
                        continue;
                    }
                    const std::string selector =
                        Trim(sheet.m_sourceText.substr(selectorBegin, selectorEnd - selectorBegin));
                    float offset = -1.0f;
                    if (selector == "from") offset = 0.0f;
                    else if (selector == "to") offset = 1.0f;
                    else if (!selector.empty() && selector.back() == '%') {
                        char* end = nullptr;
                        const float percent = std::strtof(selector.c_str(), &end);
                        if (end == selector.c_str() + selector.size() && std::isfinite(percent) &&
                            percent >= 0.0f && percent <= 100.0f) {
                            offset = percent / 100.0f;
                        }
                    }
                    if (offset < 0.0f) continue;

                    RawKeyframe frame;
                    frame.offset = offset;
                    for (lxb_css_rule_t* declRule = declarations->first; declRule;
                         declRule = declRule->next) {
                        if (declRule->type != LXB_CSS_RULE_DECLARATION) continue;
                        const auto* decl = lxb_css_rule_declaration(declRule);
                        const size_t nameStart = decl->offset.name_begin;
                        const size_t nameEnd = decl->offset.name_end;
                        const size_t valueStart = decl->offset.value_begin;
                        const size_t valueEnd = decl->offset.value_end;
                        if (nameStart > nameEnd || nameEnd > sheet.m_sourceText.size() ||
                            valueStart > valueEnd || valueEnd > sheet.m_sourceText.size()) continue;
                        frame.declarations.push_back({
                            sheet.m_sourceText.substr(nameStart, nameEnd - nameStart),
                            sheet.m_sourceText.substr(valueStart, valueEnd - valueStart)});
                    }
                    definition.frames.push_back(std::move(frame));
                }
                std::stable_sort(definition.frames.begin(), definition.frames.end(),
                                 [](const RawKeyframe& a, const RawKeyframe& b) {
                                     return a.offset < b.offset;
                                 });
                if (!definition.name.empty() && !definition.frames.empty()) {
                    sheet.m_keyframes.push_back(std::move(definition));
                }
            }
        }
        rule = rule->next;
    }

    return sheet;
}

size_t StyleSheet::RuleCount() const {
    size_t count = 0;
    lxb_css_rule_t* rule = lxb_css_rule_list(m_sheet->root)->first;
    while (rule != nullptr) {
        ++count;
        rule = rule->next;
    }
    return count;
}

} // namespace IzekipUI::Css
