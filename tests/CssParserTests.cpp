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
    assert(panel->GetStyle().width.has_value() && panel->GetStyle().width->value == 400.0f);
    assert(button->GetStyle().height.has_value() && button->GetStyle().height->value == 40.0f);

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
    assert(button->GetStyle().transition->size() == 1);
    assert((*button->GetStyle().transition)[0].property == TransitionProperty::All);
    assert(std::abs((*button->GetStyle().transition)[0].durationSeconds - 0.3f) < 0.001f);

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

// Proves the rgb()/rgba() legacy comma-separated function syntax (adapted from
// litehtml's web_color.cpp parse_rgb_func()) resolves to the same Color as the
// equivalent hex literal.
void TestRgbFunctionColorParsing() {
    auto button = std::make_shared<Widget>("Button");

    StyleSheet sheet = StyleSheet::LoadFromString(
        "Button { background-color: rgb(51, 102, 255); color: rgba(0, 0, 0, 0.5); }");

    LexborBridge bridge;
    bridge.Build(*button);
    StyleApplier::Apply(bridge, sheet, *button);

    assert(button->GetStyle().backgroundColor.has_value());
    assert(*button->GetStyle().backgroundColor == Color::FromHex("#3366FF"));

    assert(button->GetStyle().textColor.has_value());
    assert(std::abs(button->GetStyle().textColor->a - 0.5f) < 0.001f);

    std::cout << "TestRgbFunctionColorParsing passed\n";
}

// Proves 3/4-digit hex colors (per-digit doubling, adapted from litehtml's
// web_color.cpp parse_hash_color()) resolve identically to their 6/8-digit expansion.
void TestThreeDigitHexColorParsing() {
    auto button = std::make_shared<Widget>("Button");

    StyleSheet sheet = StyleSheet::LoadFromString("Button { background-color: #F00; }");

    LexborBridge bridge;
    bridge.Build(*button);
    StyleApplier::Apply(bridge, sheet, *button);

    assert(button->GetStyle().backgroundColor.has_value());
    assert(*button->GetStyle().backgroundColor == Color::FromHex("#FF0000"));

    std::cout << "TestThreeDigitHexColorParsing passed\n";
}

// Proves `width: 50%` is tagged as a percentage (not misread as 50px) and that Yoga
// resolves it natively against the parent's size -- no percentage-resolution math on
// the IzekipUI side, just picking YGNodeStyleSetWidthPercent over YGNodeStyleSetWidth.
void TestPercentWidthResolvesViaYoga() {
    auto panel = std::make_shared<Widget>("Panel");
    auto button = std::make_shared<Widget>("Button");
    panel->AddChild(button);

    StyleSheet sheet = StyleSheet::LoadFromString("Button { width: 50%; }");

    LexborBridge bridge;
    bridge.Build(*panel);
    StyleApplier::Apply(bridge, sheet, *panel);

    assert(button->GetStyle().width.has_value());
    assert(button->GetStyle().width->isPercent);
    assert(button->GetStyle().width->value == 50.0f);

    panel->CalculateLayout(400.0f, 100.0f);
    assert(std::abs(YGNodeLayoutGetWidth(button->GetYogaNode()) - 200.0f) < 0.01f);

    std::cout << "TestPercentWidthResolvesViaYoga passed\n";
}

// Proves the 5-value (spread) and `inset` box-shadow variants parse correctly.
void TestBoxShadowSpreadAndInset() {
    auto spreadButton = std::make_shared<Widget>("Button");
    StyleSheet spreadSheet =
        StyleSheet::LoadFromString("Button { box-shadow: 0px 4px 12px 6px #00000055; }");
    LexborBridge spreadBridge;
    spreadBridge.Build(*spreadButton);
    StyleApplier::Apply(spreadBridge, spreadSheet, *spreadButton);

    assert(spreadButton->GetStyle().boxShadow.has_value());
    assert(spreadButton->GetStyle().boxShadow->size() == 1);
    const BoxShadow& spread = (*spreadButton->GetStyle().boxShadow)[0];
    assert(spread.offsetY == 4.0f && spread.blurRadius == 12.0f && spread.spreadRadius == 6.0f);
    assert(!spread.inset);

    auto insetButton = std::make_shared<Widget>("Button");
    StyleSheet insetSheet = StyleSheet::LoadFromString("Button { box-shadow: inset 0px 0px 4px #000; }");
    LexborBridge insetBridge;
    insetBridge.Build(*insetButton);
    StyleApplier::Apply(insetBridge, insetSheet, *insetButton);

    assert(insetButton->GetStyle().boxShadow.has_value());
    assert(insetButton->GetStyle().boxShadow->size() == 1);
    assert((*insetButton->GetStyle().boxShadow)[0].inset);

    std::cout << "TestBoxShadowSpreadAndInset passed\n";
}

// Proves a comma-separated box-shadow list parses into multiple independent shadows.
void TestMultipleCommaBoxShadows() {
    auto button = std::make_shared<Widget>("Button");
    StyleSheet sheet = StyleSheet::LoadFromString(
        "Button { box-shadow: 0px 2px 4px #000000, 0px 0px 0px 2px #FFFFFF; }");

    LexborBridge bridge;
    bridge.Build(*button);
    StyleApplier::Apply(bridge, sheet, *button);

    assert(button->GetStyle().boxShadow.has_value());
    assert(button->GetStyle().boxShadow->size() == 2);
    assert((*button->GetStyle().boxShadow)[0].color == Color::FromHex("#000000"));
    assert((*button->GetStyle().boxShadow)[1].color == Color::FromHex("#FFFFFF"));
    assert((*button->GetStyle().boxShadow)[1].spreadRadius == 2.0f);

    std::cout << "TestMultipleCommaBoxShadows passed\n";
}

// Proves a 3-stop gradient with explicit percentage positions parses correctly (the old
// parser only ever accepted exactly 2 stops with no position).
void TestThreeStopLinearGradient() {
    auto button = std::make_shared<Widget>("Button");
    StyleSheet sheet = StyleSheet::LoadFromString(
        "Button { background: linear-gradient(to right, red 0%, blue 50%, green 100%); }");

    LexborBridge bridge;
    bridge.Build(*button);
    StyleApplier::Apply(bridge, sheet, *button);

    assert(button->GetStyle().background.has_value());
    const auto& stops = button->GetStyle().background->stops;
    assert(stops.size() == 3);
    assert(stops[0].percentPosition.has_value() && *stops[0].percentPosition == 0.0f);
    assert(stops[1].percentPosition.has_value() && *stops[1].percentPosition == 50.0f);
    assert(stops[2].percentPosition.has_value() && *stops[2].percentPosition == 100.0f);

    std::cout << "TestThreeStopLinearGradient passed\n";
}

// Proves the `border: <width> <style> <color>` shorthand dispatches to border-width and
// border-color (border-style is validated but has no field to store into yet).
void TestBorderShorthand() {
    auto button = std::make_shared<Widget>("Button");
    StyleSheet sheet = StyleSheet::LoadFromString("Button { border: 2px solid #000000; }");

    LexborBridge bridge;
    bridge.Build(*button);
    StyleApplier::Apply(bridge, sheet, *button);

    assert(button->GetStyle().borderWidth.has_value() && *button->GetStyle().borderWidth == 2.0f);
    assert(button->GetStyle().borderColor.has_value() &&
           *button->GetStyle().borderColor == Color::FromHex("#000000"));

    std::cout << "TestBorderShorthand passed\n";
}

// Proves a comma-separated transition list parses into independent entries, each
// covering its own property (the old parser only ever accepted a single segment).
void TestMultiplePropertyTransitionList() {
    auto button = std::make_shared<Widget>("Button");
    StyleSheet sheet = StyleSheet::LoadFromString(
        "Button { transition: width 300ms ease-in-out, height 500ms linear; }");

    LexborBridge bridge;
    bridge.Build(*button);
    StyleApplier::Apply(bridge, sheet, *button);

    assert(button->GetStyle().transition.has_value());
    const auto& transitions = *button->GetStyle().transition;
    assert(transitions.size() == 2);
    assert(transitions[0].property == TransitionProperty::Width);
    assert(std::abs(transitions[0].durationSeconds - 0.3f) < 0.001f);
    assert(transitions[1].property == TransitionProperty::Height);
    assert(std::abs(transitions[1].durationSeconds - 0.5f) < 0.001f);

    assert(FindCoveringTransition(transitions, TransitionProperty::Width) == &transitions[0]);
    assert(FindCoveringTransition(transitions, TransitionProperty::Height) == &transitions[1]);
    assert(FindCoveringTransition(transitions, TransitionProperty::BackgroundColor) == nullptr);

    std::cout << "TestMultiplePropertyTransitionList passed\n";
}

// Proves a single backdrop-filter function (blur) parses.
void TestBackdropFilterSingleFunction() {
    auto panel = std::make_shared<Widget>("Panel");
    StyleSheet sheet = StyleSheet::LoadFromString("Panel { backdrop-filter: blur(10px); }");

    LexborBridge bridge;
    bridge.Build(*panel);
    StyleApplier::Apply(bridge, sheet, *panel);

    assert(panel->GetStyle().backdropFilter.has_value());
    assert(panel->GetStyle().backdropFilter->blurRadiusPx.has_value());
    assert(*panel->GetStyle().backdropFilter->blurRadiusPx == 10.0f);
    assert(!panel->GetStyle().backdropFilter->brightness.has_value());

    std::cout << "TestBackdropFilterSingleFunction passed\n";
}

// Proves a space-separated multi-function backdrop-filter list parses (not comma-separated).
void TestBackdropFilterMultipleFunctions() {
    auto panel = std::make_shared<Widget>("Panel");
    StyleSheet sheet =
        StyleSheet::LoadFromString("Panel { backdrop-filter: blur(10px) brightness(1.2); }");

    LexborBridge bridge;
    bridge.Build(*panel);
    StyleApplier::Apply(bridge, sheet, *panel);

    assert(panel->GetStyle().backdropFilter.has_value());
    assert(*panel->GetStyle().backdropFilter->blurRadiusPx == 10.0f);
    assert(panel->GetStyle().backdropFilter->brightness.has_value());
    assert(std::abs(*panel->GetStyle().backdropFilter->brightness - 1.2f) < 0.001f);

    std::cout << "TestBackdropFilterMultipleFunctions passed\n";
}

// Proves an unrecognized sub-function (hue-rotate) is skipped individually -- the rest of
// the list still applies -- while a declaration made ENTIRELY of unrecognized functions
// resolves to nullopt (consistent with "unknown properties are ignored").
void TestBackdropFilterUnknownSubFunctionIgnored() {
    auto mixedPanel = std::make_shared<Widget>("Panel");
    StyleSheet mixedSheet =
        StyleSheet::LoadFromString("Panel { backdrop-filter: blur(10px) hue-rotate(90deg); }");
    LexborBridge mixedBridge;
    mixedBridge.Build(*mixedPanel);
    StyleApplier::Apply(mixedBridge, mixedSheet, *mixedPanel);

    assert(mixedPanel->GetStyle().backdropFilter.has_value());
    assert(*mixedPanel->GetStyle().backdropFilter->blurRadiusPx == 10.0f);

    auto unknownPanel = std::make_shared<Widget>("Panel");
    StyleSheet unknownSheet = StyleSheet::LoadFromString("Panel { backdrop-filter: opacity(0.5); }");
    LexborBridge unknownBridge;
    unknownBridge.Build(*unknownPanel);
    StyleApplier::Apply(unknownBridge, unknownSheet, *unknownPanel);

    assert(!unknownPanel->GetStyle().backdropFilter.has_value());

    std::cout << "TestBackdropFilterUnknownSubFunctionIgnored passed\n";
}

} // namespace

int main() {
    TestTypeClassIdSelectorMatching();
    TestCommaSeparatedSelectorGroup();
    TestSpecificityOrdering();
    TestHoverActiveMatching();
    TestTransitionDeclarationParsing();
    TestDescendantCombinatorAndMultiValueMargin();
    TestRgbFunctionColorParsing();
    TestThreeDigitHexColorParsing();
    TestPercentWidthResolvesViaYoga();
    TestBoxShadowSpreadAndInset();
    TestMultipleCommaBoxShadows();
    TestThreeStopLinearGradient();
    TestBorderShorthand();
    TestMultiplePropertyTransitionList();
    TestBackdropFilterSingleFunction();
    TestBackdropFilterMultipleFunctions();
    TestBackdropFilterUnknownSubFunctionIgnored();
    std::cout << "All CssParserTests passed.\n";
    return 0;
}
