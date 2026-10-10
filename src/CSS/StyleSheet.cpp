#include "CSS/StyleSheet.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

extern "C" {
#include <lexbor/css/css.h>
#include <lexbor/css/rule.h>
#include <lexbor/css/stylesheet.h>
}

namespace IzekipUI::Css {

StyleSheet::~StyleSheet() {
    if (m_sheet) {
        lxb_css_stylesheet_destroy(m_sheet, true);
    }
    if (m_parser) {
        lxb_css_parser_destroy(m_parser, true);
    }
}

StyleSheet::StyleSheet(StyleSheet&& other) noexcept
    : m_sourceText(std::move(other.m_sourceText)), m_parser(other.m_parser), m_sheet(other.m_sheet) {
    other.m_parser = nullptr;
    other.m_sheet = nullptr;
}

StyleSheet& StyleSheet::operator=(StyleSheet&& other) noexcept {
    if (this != &other) {
        if (m_sheet) lxb_css_stylesheet_destroy(m_sheet, true);
        if (m_parser) lxb_css_parser_destroy(m_parser, true);

        m_sourceText = std::move(other.m_sourceText);
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
