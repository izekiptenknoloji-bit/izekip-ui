#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Core/Widget.h"

struct lxb_dom_document;
struct lxb_dom_element;
struct lxb_selectors;

namespace IzekipUI::Css {

class StyleSheet;

// One flattened, ready-to-apply declaration: a matched rule's property name + its raw
// source value text (re-tokenized by IzekipUI's own Tokenizer and existing per-property
// parse helpers -- lexbor's own property/value parser is deliberately not used, see
// plan doc), plus enough to redo the existing specificity/source-order sort.
struct MatchedDeclaration {
    std::string propertyName;
    std::string rawValueText;
    unsigned specificity = 0;
    int sourceOrder = 0;
};

// Owns a shadow lexbor DOM mirroring the real Widget tree, used only for selector
// matching (type/class/id/combinators/:hover/:active) and specificity -- lexbor never
// touches layout, paint, or property value parsing. Built once per widget tree; hover/
// active state is toggled cheaply per widget rather than rebuilding the tree.
class LexborBridge {
public:
    LexborBridge();
    ~LexborBridge();

    LexborBridge(const LexborBridge&) = delete;
    LexborBridge& operator=(const LexborBridge&) = delete;

    // Walks the real Widget tree once, allocating one shadow DOM element per Widget
    // (tag = GetTypeName(), id/class attributes mirrored), building the parent/child
    // structure to match.
    void Build(Widget& root);

    // True once Build has produced a shadow tree. Its structure reflects the widget tree at
    // BuiltGeneration(); StyleApplier::Apply rebuilds it when the tree has changed since.
    bool IsBuilt() const { return m_document != nullptr; }
    std::uint64_t BuiltGeneration() const { return m_builtGeneration; }
    // The stylesheet the widgets were last restyled against; a different sheet forces a full restyle.
    const StyleSheet* AppliedSheet() const { return m_appliedSheet; }
    void SetAppliedSheet(const StyleSheet* sheet) { m_appliedSheet = sheet; }

    // Reflects widget.IsHovered()/IsActive() onto the shadow element's "hover"/"active"
    // attributes -- lexbor's selector matcher checks for exactly these attributes when
    // evaluating :hover/:active (lxb_dom_element_attr_by_id(..., LXB_DOM_ATTR_HOVER)).
    void SyncPseudoState(Widget& widget);

    // Matches every style rule in sheet against this widget's shadow element and
    // returns the declarations of every rule that matched, each tagged with the
    // specificity lexbor computed and this rule's position in the stylesheet.
    std::vector<MatchedDeclaration> MatchedDeclarationsFor(const StyleSheet& sheet, Widget& widget);

private:
    void BuildRecursive(Widget& widget, lxb_dom_element* parentShadowElement);
    void ReleaseShadowTree();
    lxb_dom_element* ShadowElementFor(Widget& widget) const;

    lxb_dom_document* m_document = nullptr;
    lxb_selectors* m_selectors = nullptr;
    std::uint64_t m_builtGeneration = 0;
    const StyleSheet* m_appliedSheet = nullptr;
    std::unordered_map<const Widget*, lxb_dom_element*> m_widgetToShadow;
};

} // namespace IzekipUI::Css
