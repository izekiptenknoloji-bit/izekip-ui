#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Core/Transition.h"
#include "Core/Widget.h"

using namespace IzekipUI;
using namespace IzekipUI::Css;

namespace {

void TestTypeClassIdSelectorMatching() {
    auto panel = std::make_shared<Widget>("Panel");
    panel->SetId("mainPanel");
    auto button = std::make_shared<Widget>("Button");
    button->AddClass("primary");
    panel->AddChild(button);

    StyleSheet sheet = StyleSheet::LoadFromString(
        "Panel { background-color: #CCCCCC; }"
        "#mainPanel { width: 400px; }"
        ".primary { height: 40px; }");

    LexborBridge bridge;
    bridge.Build(*panel);
    StyleApplier::Apply(bridge, sheet, *panel);

    assert(panel->GetStyle().backgroundColor.has_value());
    assert(panel->GetStyle().width.has_value() && *panel->GetStyle().width == 400.0f);
    assert(button->GetStyle().height.has_value() && *button->GetStyle().height == 40.0f);

    std::cout << "TestTypeClassIdSelectorMatching passed\n";
}

void TestCommaSeparatedSelectorGroup() {
    auto panel = std::make_shared<Widget>("Panel");
    auto button = std::make_shared<Widget>("Button");
    panel->AddChild(button);

    StyleSheet sheet = StyleSheet::LoadFromString("Panel, Button { color: #000000; }");

    LexborBridge bridge;
    bridge.Build(*panel);
    StyleApplier::Apply(bridge, sheet, *panel);

    assert(panel->GetStyle().textColor.has_value());
    assert(button->GetStyle().textColor.has_value());

    std::cout << "TestCommaSeparatedSelectorGroup passed\n";
}

void TestSpecificityOrdering() {
    auto panel = std::make_shared<Widget>("Panel");
    panel->SetId("mainPanel");
    panel->AddClass("primary");

    StyleSheet sheet = StyleSheet::LoadFromString(
        "Panel { background-color: #111111; }"
        ".primary { background-color: #222222; }"
        "#mainPanel { background-color: #333333; }");

    LexborBridge bridge;
    bridge.Build(*panel);
    StyleApplier::Apply(bridge, sheet, *panel);

    assert(panel->GetStyle().backgroundColor.has_value());
    assert(*panel->GetStyle().backgroundColor == Color::FromHex("#333333"));

    std::cout << "TestSpecificityOrdering passed\n";
}

void TestHoverActiveMatching() {
    auto button = std::make_shared<Widget>("Button");
    button->AddClass("primary");

    StyleSheet sheet = StyleSheet::LoadFromString(
        ".primary { background-color: #3366FF; }"
        ".primary:hover { background-color: #5588FF; }");

    LexborBridge bridge;
    bridge.Build(*button);

    StyleApplier::Apply(bridge, sheet, *button);
    assert(*button->GetStyle().backgroundColor == Color::FromHex("#3366FF"));

    button->SetHovered(true);
    StyleApplier::Apply(bridge, sheet, *button);
    assert(*button->GetStyle().backgroundColor == Color::FromHex("#5588FF"));

    std::cout << "TestHoverActiveMatching passed\n";
}

void TestTransitionDeclarationParsing() {
    auto button = std::make_shared<Widget>("Button");

    StyleSheet sheet = StyleSheet::LoadFromString("Button { transition: all 300ms ease-in-out; }");

    LexborBridge bridge;
    bridge.Build(*button);
    StyleApplier::Apply(bridge, sheet, *button);

    assert(button->GetStyle().transition.has_value());
    assert(button->GetStyle().transition->property == TransitionProperty::All);
    assert(std::abs(button->GetStyle().transition->durationSeconds - 0.3f) < 0.001f);

    std::cout << "TestTransitionDeclarationParsing passed\n";
}

// Proves the lexbor swap actually works: both constructs below were inexpressible in
// the old hand-rolled parser (single-selector-only grammar, single-value-only shorthand).
void TestDescendantCombinatorAndMultiValueMargin() {
    auto panel = std::make_shared<Widget>("Panel");
    auto button = std::make_shared<Widget>("Button");
    panel->AddChild(button);

    StyleSheet sheet = StyleSheet::LoadFromString(
        "Panel Button { border-color: #FF0000; }"
        "Button { margin: 10px 20px; }");

    LexborBridge bridge;
    bridge.Build(*panel);
    StyleApplier::Apply(bridge, sheet, *panel);

    assert(button->GetStyle().borderColor.has_value());

    assert(button->GetStyle().margin.has_value());
    const BoxEdges& margin = *button->GetStyle().margin;
    assert(margin.top == 10.0f && margin.bottom == 10.0f);
    assert(margin.left == 20.0f && margin.right == 20.0f);

    std::cout << "TestDescendantCombinatorAndMultiValueMargin passed\n";
}

} // namespace

int main() {
    TestTypeClassIdSelectorMatching();
    TestCommaSeparatedSelectorGroup();
    TestSpecificityOrdering();
    TestHoverActiveMatching();
    TestTransitionDeclarationParsing();
    TestDescendantCombinatorAndMultiValueMargin();
    std::cout << "All CssParserTests passed.\n";
    return 0;
}
