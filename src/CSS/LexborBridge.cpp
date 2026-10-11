#include "CSS/LexborBridge.h"

#include "CSS/StyleSheet.h"

#include <algorithm>
#include <cctype>
#include <functional>

extern "C" {
#include <lexbor/css/rule.h>
#include <lexbor/css/stylesheet.h>
#include <lexbor/dom/interfaces/document.h>
#include <lexbor/dom/interfaces/element.h>
#include <lexbor/selectors/selectors.h>
}

namespace IzekipUI::Css {

namespace {

struct MatchCtx {
    bool matched = false;
    unsigned specificity = 0;
};

lxb_status_t OnSelectorMatch(lxb_dom_node_t* /*node*/, lxb_css_selector_specificity_t spec, void* ctx) {
    auto* match = static_cast<MatchCtx*>(ctx);
    match->matched = true;
    match->specificity = static_cast<unsigned>(spec);
    return LXB_STATUS_OK;
}

void SetOrClearFlagAttribute(lxb_dom_element_t* element, const char* name, size_t nameLen, bool set) {
    if (set) {
        lxb_dom_element_set_attribute(element, reinterpret_cast<const lxb_char_t*>(name), nameLen,
                                       reinterpret_cast<const lxb_char_t*>(""), 0);
    } else {
        lxb_dom_element_remove_attribute(element, reinterpret_cast<const lxb_char_t*>(name), nameLen);
    }
}

std::string JoinClasses(const Widget& widget) {
    std::string classAttr;
    for (const auto& cls : widget.GetClasses()) {
        if (!classAttr.empty()) classAttr += ' ';
        classAttr += cls;
    }
    return classAttr;
}

} // namespace

LexborBridge::LexborBridge() = default;

LexborBridge::~LexborBridge() {
    if (m_selectors) {
        lxb_selectors_destroy(m_selectors, true);
    }
    if (m_document) {
        lxb_dom_document_destroy(m_document);
    }
}

void LexborBridge::Build(Widget& root) {
    m_document = lxb_dom_document_create(nullptr);
    lxb_dom_document_init(m_document, nullptr, lxb_dom_interface_create, lxb_dom_interface_clone,
                           lxb_dom_interface_destroy, LXB_DOM_DOCUMENT_DTYPE_UNDEF, LXB_NS__UNDEF);

    m_selectors = lxb_selectors_create();
    lxb_selectors_init(m_selectors);

    m_widgetToShadow.clear();
    BuildRecursive(root, nullptr);
}

void LexborBridge::BuildRecursive(Widget& widget, lxb_dom_element* parentShadowElement) {
    const std::string& typeName = widget.GetTypeName();
    lxb_dom_element_t* element = lxb_dom_document_create_element(
        m_document, reinterpret_cast<const lxb_char_t*>(typeName.c_str()), typeName.size(), nullptr);

    if (!widget.GetId().empty()) {
        const std::string& id = widget.GetId();
        lxb_dom_element_set_attribute(element, reinterpret_cast<const lxb_char_t*>("id"), 2,
                                       reinterpret_cast<const lxb_char_t*>(id.c_str()), id.size());
    }

    if (!widget.GetClasses().empty()) {
        const std::string classAttr = JoinClasses(widget);
        lxb_dom_element_set_attribute(element, reinterpret_cast<const lxb_char_t*>("class"), 5,
                                       reinterpret_cast<const lxb_char_t*>(classAttr.c_str()),
                                       classAttr.size());
    }

    if (parentShadowElement) {
        lxb_dom_node_insert_child(lxb_dom_interface_node(parentShadowElement),
                                   lxb_dom_interface_node(element));
    }

    m_widgetToShadow.emplace_back(&widget, element);

    for (const auto& child : widget.GetChildren()) {
        BuildRecursive(*child, element);
    }
}

lxb_dom_element_t* LexborBridge::ShadowElementFor(Widget& widget) const {
    for (const auto& [w, element] : m_widgetToShadow) {
        if (w == &widget) {
            return element;
        }
    }
    return nullptr;
}

void LexborBridge::SyncPseudoState(Widget& widget) {
    lxb_dom_element_t* element = ShadowElementFor(widget);
    if (!element) {
        return;
    }
    SetOrClearFlagAttribute(element, "hover", 5, widget.IsHovered());
    SetOrClearFlagAttribute(element, "active", 6, widget.IsActive());
    SetOrClearFlagAttribute(element, "focus", 5, widget.IsFocused());
    SetOrClearFlagAttribute(element, "disabled", 8, !widget.IsEnabled());

    // Classes are live too (Widget::AddClass/RemoveClass/SetClass after Build()) -- lexbor's
    // class selector reads the attribute value directly on every match, so rewriting it here
    // each Apply is enough. The shadow tree's STRUCTURE (children, id) is still frozen at
    // Build(); only classes and pseudo-state flags are synced.
    const std::string classAttr = JoinClasses(widget);
    if (classAttr.empty()) {
        lxb_dom_element_remove_attribute(element, reinterpret_cast<const lxb_char_t*>("class"), 5);
    } else {
        lxb_dom_element_set_attribute(element, reinterpret_cast<const lxb_char_t*>("class"), 5,
                                       reinterpret_cast<const lxb_char_t*>(classAttr.c_str()),
                                       classAttr.size());
    }
}

std::vector<MatchedDeclaration> LexborBridge::MatchedDeclarationsFor(const StyleSheet& sheet,
                                                                      Widget& widget) {
    std::vector<MatchedDeclaration> result;

    lxb_dom_element_t* element = ShadowElementFor(widget);
    if (!element) {
        return result;
    }

    const std::string& source = sheet.GetSourceText();
    lxb_css_stylesheet_t* raw = sheet.GetRaw();

    int sourceOrder = 0;
    const auto mediaMatches = [&sheet, &source](const lxb_css_rule_at_t* at) {
        if (!sheet.PrefersReducedMotion() || !at ||
            at->type != LXB_CSS_AT_RULE_MEDIA) {
            return false;
        }
        const size_t begin = std::min(at->prelude_begin, source.size());
        const size_t end = std::min(at->prelude_end, source.size());
        if (end < begin) return false;
        std::string query = source.substr(begin, end - begin);
        query.erase(std::remove_if(query.begin(), query.end(), [](unsigned char c) {
            return std::isspace(c);
        }), query.end());
        std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return query.find("(prefers-reduced-motion:reduce)") != std::string::npos;
    };

    std::function<void(lxb_css_rule_list_t*)> collect = [&](lxb_css_rule_list_t* list) {
        if (!list) return;
        for (lxb_css_rule_t* rule = list->first; rule; rule = rule->next, ++sourceOrder) {
            if (rule->type == LXB_CSS_RULE_STYLE) {
                auto* style = lxb_css_rule_style(rule);
                MatchCtx ctx;
                lxb_selectors_match_node(m_selectors, lxb_dom_interface_node(element),
                                         style->selector, OnSelectorMatch, &ctx);
                if (ctx.matched) {
                    for (lxb_css_rule_t* declNode = style->declarations->first; declNode;
                         declNode = declNode->next) {
                        auto* decl = lxb_css_rule_declaration(declNode);
                        MatchedDeclaration md;
                        md.propertyName.assign(source.data() + decl->offset.name_begin,
                                               decl->offset.name_end - decl->offset.name_begin);
                        md.rawValueText.assign(source.data() + decl->offset.value_begin,
                                               decl->offset.value_end - decl->offset.value_begin);
                        md.specificity = ctx.specificity;
                        md.sourceOrder = sourceOrder;
                        result.push_back(std::move(md));
                    }
                }
            } else if (rule->type == LXB_CSS_RULE_AT_RULE) {
                auto* at = lxb_css_rule_at(rule);
                if (mediaMatches(at) && at->u.media) {
                    collect(at->u.media->block);
                }
            }
        }
    };
    collect(lxb_css_rule_list(raw->root));

    return result;
}

} // namespace IzekipUI::Css
