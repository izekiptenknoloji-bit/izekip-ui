#pragma once

#include "CSS/LexborBridge.h"
#include "CSS/StyleSheet.h"
#include "Core/Widget.h"

namespace IzekipUI::Css {

// Applies a StyleSheet to a widget tree. Selector matching, combinators, and specificity
// are delegated to lexbor via LexborBridge (bridge.MatchedDeclarationsFor); this class
// only turns the matched declarations' raw value text back into typed StyleProperties
// (via IzekipUI's own Tokenizer + per-property parse helpers, unchanged from before the
// lexbor swap) and decides what happens to each one.
//
// For each widget, matched declarations are sorted by (specificity, source order)
// ascending and merged into a local "resolved" style in that order -- so higher
// specificity wins, and on a tie the later rule in the file wins.
//
// The resolved style is then diffed against the widget's previous target style
// (Widget::GetTargetStyle()): on first apply, or for any property with no active
// transition covering it, the new value snaps immediately. For animatable properties
// (background-color/width/height/margin) that changed AND are covered by a resolved
// `transition`, the widget instead starts (or redirects) an animation from its current
// rendered value to the new target -- see Widget::Animate*To / Widget::TickAnimations.
// The same pass also pushes non-animated width/height/margin/padding/flex-* directly
// into the widget's Yoga node, since those drive layout rather than just paint.
class StyleApplier {
public:
    static void Apply(LexborBridge& bridge, const StyleSheet& sheet, Widget& root);

private:
    static void ApplyToSingleWidget(LexborBridge& bridge, const StyleSheet& sheet, Widget& widget);
};

} // namespace IzekipUI::Css
