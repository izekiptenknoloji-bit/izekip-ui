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

// Offsets come from lexbor's parser over the same source text. They are clamped rather than
// trusted, so a malformed span can never read outside the stylesheet.
std::string SourceSlice(const std::string& source, size_t begin, size_t end) {
    begin = std::min(begin, source.size());
    end = std::min(end, source.size());
    if (end <= begin) {
        return {};
    }
    return source.substr(begin, end - begin);
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
    ReleaseShadowTree();
}

void LexborBridge::ReleaseShadowTree() {
    if (m_selectors) {
        lxb_selectors_destroy(m_selectors, true);
        m_selectors = nullptr;
    }
    if (m_document) {
        lxb_dom_document_destroy(m_document);
        m_document = nullptr;
    }
    m_widgetToShadow.clear();
}

void LexborBridge::Build(Widget& root) {
    // A second Build replaces the first tree; without this the old document and selectors leak.
    ReleaseShadowTree();

    m_document = lxb_dom_document_create(nullptr);
    if (!m_document) {
        return; // no shadow tree: every match reports nothing rather than dereferencing null
    }
    if (lxb_dom_document_init(m_document, nullptr, lxb_dom_interface_create, lxb_dom_interface_clone,
                              lxb_dom_interface_destroy, LXB_DOM_DOCUMENT_DTYPE_UNDEF,
                              LXB_NS__UNDEF) != LXB_STATUS_OK) {
        ReleaseShadowTree();
        return;
    }

    m_selectors = lxb_selectors_create();
    if (!m_selectors || lxb_selectors_init(m_selectors) != LXB_STATUS_OK) {
        ReleaseShadowTree();
        return;
    }

    BuildRecursive(root, nullptr);
    m_builtGeneration = WidgetTreeGeneration();
}

void LexborBridge::BuildRecursive(Widget& widget, lxb_dom_element* parentShadowElement) {
    const std::string& typeName = widget.GetTypeName();
    lxb_dom_element_t* element = lxb_dom_document_create_element(
        m_document, reinterpret_cast<const lxb_char_t*>(typeName.c_str()), typeName.size(), nullptr);
    if (!element) {
        return; // out of memory: this widget and its subtree get no shadow element
    }

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

    m_widgetToShadow[&widget] = element;

    for (const auto& child : widget.GetChildren()) {
        BuildRecursive(*child, element);
    }
}

lxb_dom_element_t* LexborBridge::ShadowElementFor(Widget& widget) const {
    const auto found = m_widgetToShadow.find(&widget);
    return found == m_widgetToShadow.end() ? nullptr : found->second;
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
                    for (lxb_css_rule_t* declNode = style->declarations ? style->declarations->first : nullptr;
                         declNode; declNode = declNode->next) {
                        auto* decl = lxb_css_rule_declaration(declNode);
                        if (!decl) {
                            continue;
                        }
                        MatchedDeclaration md;
                        md.propertyName = SourceSlice(source, decl->offset.name_begin, decl->offset.name_end);
                        md.rawValueText = SourceSlice(source, decl->offset.value_begin, decl->offset.value_end);
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
