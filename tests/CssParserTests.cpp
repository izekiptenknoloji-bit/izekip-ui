#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Core/Transition.h"
#include "Core/Widget.h"
#include "Widgets/Label.h"

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

void TestDisabledPseudoClassMatching() {
    auto button = std::make_shared<Widget>("Button");
    button->AddClass("action");
    auto child = std::make_shared<Widget>("Panel");
    child->AddClass("nested-action");
    button->AddChild(child);

    StyleSheet sheet = StyleSheet::LoadFromString(
        ".action { opacity: 1.0; }"
        ".action:disabled, .nested-action:disabled { opacity: 0.4; }");

    LexborBridge bridge;
    bridge.Build(*button);

    StyleApplier::Apply(bridge, sheet, *button);
    assert(button->GetStyle().opacity.has_value());
    assert(std::abs(*button->GetStyle().opacity - 1.0f) < 0.001f);

    button->SetDisabled(true);
    StyleApplier::Apply(bridge, sheet, *button);
    assert(button->GetStyle().opacity.has_value());
    assert(child->GetStyle().opacity.has_value());
    assert(std::abs(*button->GetStyle().opacity - 0.4f) < 0.001f);
    assert(std::abs(*child->GetStyle().opacity - 0.4f) < 0.001f);

    button->SetDisabled(false);
    StyleApplier::Apply(bridge, sheet, *button);
    assert(button->GetStyle().opacity.has_value());
    assert(std::abs(*button->GetStyle().opacity - 1.0f) < 0.001f);
    assert(!child->GetStyle().opacity.has_value());
    std::cout << "TestDisabledPseudoClassMatching passed\n";
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

void TestMinMaxDimensionsAndViewportHeight() {
    auto root = std::make_shared<Widget>("Panel");
    root->AddClass("viewport");
    StyleSheet sheet = StyleSheet::LoadFromString(
        ".viewport { width: 500px; min-height: 100vh; max-width: 320px; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(500.0f, 240.0f);

    assert(root->GetStyle().minHeight.has_value());
    assert(root->GetStyle().minHeight->isViewportHeight);
    assert(root->GetStyle().maxWidth.has_value());
    assert(std::abs(root->GetLocalRect().height - 240.0f) < 0.01f);
    assert(std::abs(root->GetLocalRect().width - 320.0f) < 0.01f);
    std::cout << "TestMinMaxDimensionsAndViewportHeight passed\n";
}

void TestCssGenericFontFamily() {
    auto label = std::make_shared<Widgets::Label>();
    StyleSheet sheet = StyleSheet::LoadFromString("Label { font-family: monospace; }");
    LexborBridge bridge;
    bridge.Build(*label);
    StyleApplier::Apply(bridge, sheet, *label);
    assert(label->GetStyle().fontFamily.has_value());
    assert(*label->GetStyle().fontFamily == CssFontFamily::Monospace);
    std::cout << "TestCssGenericFontFamily passed\n";
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
        "Button { transition: width 300ms ease-in-out, height 500ms linear, opacity 220ms ease-out; }");

    LexborBridge bridge;
    bridge.Build(*button);
    StyleApplier::Apply(bridge, sheet, *button);

    assert(button->GetStyle().transition.has_value());
    const auto& transitions = *button->GetStyle().transition;
    assert(transitions.size() == 3);
    assert(transitions[0].property == TransitionProperty::Width);
    assert(std::abs(transitions[0].durationSeconds - 0.3f) < 0.001f);
    assert(transitions[1].property == TransitionProperty::Height);
    assert(std::abs(transitions[1].durationSeconds - 0.5f) < 0.001f);
    assert(transitions[2].property == TransitionProperty::Opacity);
    assert(std::abs(transitions[2].durationSeconds - 0.22f) < 0.001f);

    assert(FindCoveringTransition(transitions, TransitionProperty::Width) == &transitions[0]);
    assert(FindCoveringTransition(transitions, TransitionProperty::Height) == &transitions[1]);
    assert(FindCoveringTransition(transitions, TransitionProperty::Opacity) == &transitions[2]);
    assert(FindCoveringTransition(transitions, TransitionProperty::BackgroundColor) == nullptr);

    std::cout << "TestMultiplePropertyTransitionList passed\n";
}

void TestOpacityTransitionRunsAfterClassChange() {
    Widget widget("Panel");
    widget.AddClass("loading");
    StyleSheet sheet = StyleSheet::LoadFromString(
        ".panel { opacity: 1.0; transition: opacity 220ms ease-in-out; }"
        ".loading { opacity: 0.4; }");
    widget.AddClass("panel");

    LexborBridge bridge;
    bridge.Build(widget);
    StyleApplier::Apply(bridge, sheet, widget);
    assert(widget.GetStyle().opacity.has_value());
    assert(std::abs(*widget.GetStyle().opacity - 0.4f) < 0.001f);

    widget.RemoveClass("loading");
    StyleApplier::Apply(bridge, sheet, widget);
    TickWidgetTreeAnimations(widget, 0.11f);
    assert(*widget.GetStyle().opacity > 0.4f);
    assert(*widget.GetStyle().opacity < 1.0f);

    TickWidgetTreeAnimations(widget, 0.2f);
    assert(std::abs(*widget.GetStyle().opacity - 1.0f) < 0.001f);
    std::cout << "TestOpacityTransitionRunsAfterClassChange passed\n";
}

void TestTranslateTransformAndTransition() {
    Widget widget("Panel");
    widget.AddClass("card");
    StyleSheet sheet = StyleSheet::LoadFromString(
        ".card { transform: translateY(16px); transition: transform 400ms ease-out; }"
        ".card.visible { transform: translate(0px, 0px); }");

    LexborBridge bridge;
    bridge.Build(widget);
    StyleApplier::Apply(bridge, sheet, widget);
    assert(widget.GetStyle().transform.has_value());
    assert(std::abs(widget.GetStyle().transform->y - 16.0f) < 0.001f);

    widget.AddClass("visible");
    StyleApplier::Apply(bridge, sheet, widget);
    TickWidgetTreeAnimations(widget, 0.2f);
    assert(widget.GetStyle().transform->y > 0.0f);
    assert(widget.GetStyle().transform->y < 16.0f);

    TickWidgetTreeAnimations(widget, 0.3f);
    assert(std::abs(widget.GetStyle().transform->y) < 0.001f);
    std::cout << "TestTranslateTransformAndTransition passed\n";
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
        StyleSheet::LoadFromString(
            "Panel { backdrop-filter: blur(10px) brightness(1.2) hue-rotate(90deg); }");

    LexborBridge bridge;
    bridge.Build(*panel);
    StyleApplier::Apply(bridge, sheet, *panel);

    assert(panel->GetStyle().backdropFilter.has_value());
    assert(*panel->GetStyle().backdropFilter->blurRadiusPx == 10.0f);
    assert(panel->GetStyle().backdropFilter->brightness.has_value());
    assert(std::abs(*panel->GetStyle().backdropFilter->brightness - 1.2f) < 0.001f);
    assert(panel->GetStyle().backdropFilter->hueRotateRadians.has_value());
    assert(std::abs(*panel->GetStyle().backdropFilter->hueRotateRadians -
                    1.57079632679f) < 0.001f);

    std::cout << "TestBackdropFilterMultipleFunctions passed\n";
}

void TestBackdropHueRotateAngleUnits() {
    const auto parseHue = [](const char* value) {
        auto panel = std::make_shared<Widget>("Panel");
        StyleSheet sheet = StyleSheet::LoadFromString(
            std::string("Panel { backdrop-filter: hue-rotate(") + value + "); }");
        LexborBridge bridge;
        bridge.Build(*panel);
        StyleApplier::Apply(bridge, sheet, *panel);
        assert(panel->GetStyle().backdropFilter.has_value());
        assert(panel->GetStyle().backdropFilter->hueRotateRadians.has_value());
        return *panel->GetStyle().backdropFilter->hueRotateRadians;
    };

    constexpr float kQuarterTurn = 1.57079632679f;
    assert(std::abs(parseHue("90deg") - kQuarterTurn) < 0.001f);
    assert(std::abs(parseHue("1.57079632679rad") - kQuarterTurn) < 0.001f);
    assert(std::abs(parseHue("0.25turn") - kQuarterTurn) < 0.001f);
    assert(std::abs(parseHue("100grad") - kQuarterTurn) < 0.001f);
    assert(std::abs(parseHue("0") - 0.0f) < 0.001f);

    std::cout << "TestBackdropHueRotateAngleUnits passed\n";
}

// Proves an unrecognized sub-function is skipped individually while the rest of the list
// still applies, and an entirely unsupported declaration resolves to nullopt.
void TestBackdropFilterUnknownSubFunctionIgnored() {
    auto mixedPanel = std::make_shared<Widget>("Panel");
    StyleSheet mixedSheet =
        StyleSheet::LoadFromString("Panel { backdrop-filter: blur(10px) hue-rotate(90deg); }");
    LexborBridge mixedBridge;
    mixedBridge.Build(*mixedPanel);
    StyleApplier::Apply(mixedBridge, mixedSheet, *mixedPanel);

    assert(mixedPanel->GetStyle().backdropFilter.has_value());
    assert(*mixedPanel->GetStyle().backdropFilter->blurRadiusPx == 10.0f);
    assert(std::abs(*mixedPanel->GetStyle().backdropFilter->hueRotateRadians -
                    1.57079632679f) < 0.001f);

    auto unknownPanel = std::make_shared<Widget>("Panel");
    StyleSheet unknownSheet = StyleSheet::LoadFromString("Panel { backdrop-filter: opacity(0.5); }");
    LexborBridge unknownBridge;
    unknownBridge.Build(*unknownPanel);
    StyleApplier::Apply(unknownBridge, unknownSheet, *unknownPanel);

    assert(!unknownPanel->GetStyle().backdropFilter.has_value());

    std::cout << "TestBackdropFilterUnknownSubFunctionIgnored passed\n";
}

void TestRemoveClassResyncsCascade() {
    auto root = std::make_shared<Widget>("Panel");
    auto child = std::make_shared<Widget>("Panel");
    root->AddChild(child);
    child->AddClass("tall");

    StyleSheet sheet = StyleSheet::LoadFromString(".tall { height: 40px; } .wide { width: 70px; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);
    assert(child->GetTargetStyle().height.has_value() && child->GetTargetStyle().height->value == 40.0f);

    // Class changes AFTER Build() must reach the shadow DOM on the next Apply.
    assert(child->RemoveClass("tall"));
    child->AddClass("wide");
    StyleApplier::Apply(bridge, sheet, *root);
    assert(!child->GetTargetStyle().height.has_value());
    assert(child->GetTargetStyle().width.has_value() && child->GetTargetStyle().width->value == 70.0f);

    std::cout << "TestRemoveClassResyncsCascade passed\n";
}

void TestToggleAndSetClass() {
    Widget widget("Panel");
    assert(widget.ToggleClass("a") == true && widget.HasClass("a"));
    assert(widget.ToggleClass("a") == false && !widget.HasClass("a"));
    widget.SetClass("b", true);
    widget.SetClass("b", true); // add-if-absent, no duplicate
    assert(widget.GetClasses().size() == 1);
    widget.SetClass("b", false);
    assert(widget.GetClasses().empty());
    assert(!widget.RemoveClass("missing"));
    std::cout << "TestToggleAndSetClass passed\n";
}

void TestDisplayParsing() {
    auto root = std::make_shared<Widget>("Panel");
    auto a = std::make_shared<Widget>("Panel");
    auto b = std::make_shared<Widget>("Panel");
    auto c = std::make_shared<Widget>("Panel");
    a->AddClass("a");
    b->AddClass("b");
    c->AddClass("c");
    root->AddChild(a);
    root->AddChild(b);
    root->AddChild(c);

    StyleSheet sheet = StyleSheet::LoadFromString(".a { display: none; } .b { display: flex; } .c { display: grid; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);

    assert(!a->IsDisplayed());
    assert(YGNodeStyleGetDisplay(a->GetYogaNode()) == YGDisplayNone);
    assert(b->IsDisplayed());
    assert(c->IsDisplayed()); // unknown value ignored -> default flex
    assert(root->IsDisplayed());
    std::cout << "TestDisplayParsing passed\n";
}

void TestFontWeightAndCursorParsingAndInheritance() {
    auto root = std::make_shared<Widget>("Panel");
    auto label = std::make_shared<Widget>("Label");
    auto bad = std::make_shared<Widget>("Label");
    label->AddClass("title");
    bad->AddClass("bad");
    root->AddChild(label);
    root->AddChild(bad);
    root->SetClass("pointer-root", true);

    StyleSheet sheet = StyleSheet::LoadFromString(
        ".pointer-root { cursor: pointer; } "
        ".title { font-weight: 600; } "
        ".bad { font-weight: 550; cursor: wait; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);

    assert(label->GetStyle().fontWeight == 600);
    assert(!bad->GetStyle().fontWeight.has_value());
    assert(!bad->GetStyle().cursor.has_value());
    assert(label->GetEffectiveCursor() == CssCursor::Pointer);
    assert(bad->GetEffectiveCursor() == CssCursor::Pointer);
    std::cout << "TestFontWeightAndCursorParsingAndInheritance passed\n";
}

void TestScaleRotateParsingAndTransformMath() {
    auto root = std::make_shared<Widget>("Panel");
    auto a = std::make_shared<Widget>("Panel");
    auto bad = std::make_shared<Widget>("Panel");
    a->AddClass("a");
    bad->AddClass("bad");
    root->AddChild(a);
    root->AddChild(bad);

    StyleSheet sheet = StyleSheet::LoadFromString(
        ".a { scale: 1.5; rotate: 90deg; } .bad { scale: -1; rotate: 45px; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);

    assert(a->GetStyle().scale && std::fabs(*a->GetStyle().scale - 1.5f) < 1e-5f);
    assert(a->GetStyle().rotate && std::fabs(*a->GetStyle().rotate - 1.57079632f) < 1e-5f);
    assert(!bad->GetStyle().scale.has_value());
    assert(!bad->GetStyle().rotate.has_value());

    // Scale 2 + 90deg about (10,10): (20,10) -> (10,30).
    const Transform2D t = Transform2D::AboutPivot(2.0f, 1.57079632f, 10.0f, 10.0f);
    float x = 0.0f;
    float y = 0.0f;
    t.Apply(20.0f, 10.0f, x, y);
    assert(std::fabs(x - 10.0f) < 1e-4f && std::fabs(y - 30.0f) < 1e-4f);
    t.ApplyInverse(x, y, x, y);
    assert(std::fabs(x - 20.0f) < 1e-4f && std::fabs(y - 10.0f) < 1e-4f);

    const Transform2D inner = Transform2D::AboutPivot(0.5f, 0.0f, 0.0f, 0.0f);
    const Transform2D composed = t.Compose(inner);
    composed.Apply(4.0f, 4.0f, x, y);
    float ex = 0.0f;
    float ey = 0.0f;
    t.Apply(2.0f, 2.0f, ex, ey);
    assert(std::fabs(x - ex) < 1e-4f && std::fabs(y - ey) < 1e-4f);
    std::cout << "TestScaleRotateParsingAndTransformMath passed\n";
}

void TestDisplayResetsWhenHidingClassRemoved() {
    auto root = std::make_shared<Widget>("Panel");
    auto page = std::make_shared<Widget>("Panel");
    root->AddChild(page);
    page->AddClass("hidden");

    StyleSheet sheet = StyleSheet::LoadFromString(".hidden { display: none; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);
    assert(!page->IsDisplayed());
    assert(YGNodeStyleGetDisplay(page->GetYogaNode()) == YGDisplayNone);

    // The cascade is additive -- without the write-always rule, `none` would stick here.
    page->RemoveClass("hidden");
    StyleApplier::Apply(bridge, sheet, *root);
    assert(page->IsDisplayed());
    assert(page->GetStyle().display == Display::Flex);
    assert(YGNodeStyleGetDisplay(page->GetYogaNode()) == YGDisplayFlex);
    std::cout << "TestDisplayResetsWhenHidingClassRemoved passed\n";
}

void TestHiddenChildSkippedByHitTest() {
    auto root = std::make_shared<Widget>("Panel");
    root->SetId("root");
    auto overlay = std::make_shared<Widget>("Panel");
    overlay->AddClass("overlay");
    root->AddChild(overlay);

    StyleSheet sheet = StyleSheet::LoadFromString(
        "#root { width: 100px; height: 100px; } .overlay { width: 100px; height: 100px; }"
        ".hidden { display: none; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(100.0f, 100.0f);
    assert(root->HitTest(50.0f, 50.0f) == overlay.get());

    overlay->AddClass("hidden");
    StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(100.0f, 100.0f);
    assert(root->HitTest(50.0f, 50.0f) == root.get());
    assert(root->HitTest(0.0f, 0.0f) == root.get()); // the zeroed rect sits at the origin
    std::cout << "TestHiddenChildSkippedByHitTest passed\n";
}

void TestEmptyClassListRemovesAttribute() {
    auto root = std::make_shared<Widget>("Panel");
    auto child = std::make_shared<Widget>("Panel");
    root->AddChild(child);
    child->AddClass("x");

    StyleSheet sheet = StyleSheet::LoadFromString(".x { height: 10px; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);
    assert(child->GetTargetStyle().height.has_value());

    child->RemoveClass("x");
    StyleApplier::Apply(bridge, sheet, *root);
    assert(!child->GetTargetStyle().height.has_value());
    std::cout << "TestEmptyClassListRemovesAttribute passed\n";
}

void TestKeyframeAnimationAndReducedMotion() {
    auto card = std::make_shared<Widget>("Panel");
    card->AddClass("card");
    StyleSheet sheet = StyleSheet::LoadFromString(
        "@keyframes fade-in-up { from { opacity: 0; transform: translateY(20px); }"
        " to { opacity: 1; transform: translateY(0); } }"
        ".card { opacity: 0; transform: translateY(16px);"
        " animation: fade-in-up 400ms cubic-bezier(0.22, 1, 0.36, 1) forwards; }"
        ".transient { opacity: 0.2; animation: fade-in-up 100ms linear; }"
        "@media (prefers-reduced-motion: reduce) {"
        " .card { animation: none; opacity: 1; transform: none; } }");
    assert(sheet.GetKeyframes().size() == 1);
    assert(sheet.GetKeyframes()[0].name == "fade-in-up");
    assert(sheet.GetKeyframes()[0].frames.size() == 2);

    LexborBridge bridge;
    bridge.Build(*card);
    StyleApplier::Apply(bridge, sheet, *card);
    assert(card->GetStyle().opacity && *card->GetStyle().opacity == 0.0f);
    TickWidgetTreeAnimations(*card, 0.2f);
    assert(card->GetStyle().opacity && *card->GetStyle().opacity > 0.0f &&
           *card->GetStyle().opacity < 1.0f);
    assert(card->GetStyle().transform && card->GetStyle().transform->y < 16.0f);
    StyleApplier::Apply(bridge, sheet, *card);
    assert(card->GetStyle().opacity && *card->GetStyle().opacity > 0.0f &&
           *card->GetStyle().opacity < 1.0f);
    TickWidgetTreeAnimations(*card, 0.2f);
    assert(card->GetStyle().opacity && std::abs(*card->GetStyle().opacity - 1.0f) < 0.001f);
    assert(card->GetStyle().transform && std::abs(card->GetStyle().transform->y) < 0.001f);

    sheet.SetPrefersReducedMotion(true);
    StyleApplier::Apply(bridge, sheet, *card);
    assert(card->GetStyle().opacity && std::abs(*card->GetStyle().opacity - 1.0f) < 0.001f);
    assert(card->GetStyle().transform && std::abs(card->GetStyle().transform->y) < 0.001f);
    TickWidgetTreeAnimations(*card, 1.0f);
    assert(card->GetStyle().opacity && std::abs(*card->GetStyle().opacity - 1.0f) < 0.001f);

    auto transient = std::make_shared<Widget>("Panel");
    transient->AddClass("transient");
    LexborBridge transientBridge;
    transientBridge.Build(*transient);
    StyleApplier::Apply(transientBridge, sheet, *transient);
    TickWidgetTreeAnimations(*transient, 0.1f);
    assert(transient->GetStyle().opacity &&
           std::abs(*transient->GetStyle().opacity - 0.2f) < 0.001f);
    std::cout << "TestKeyframeAnimationAndReducedMotion passed\n";
}

void TestOpacityInheritsThroughWidgetTree() {
    auto root = std::make_shared<Widget>("Panel");
    auto child = std::make_shared<Widget>("Panel");
    auto grandchild = std::make_shared<Widget>("Panel");
    root->AddChild(child);
    child->AddChild(grandchild);
    root->GetStyle().opacity = 0.5f;
    child->GetStyle().opacity = 0.8f;
    grandchild->GetStyle().opacity = 0.5f;
    assert(std::abs(root->GetEffectiveOpacity() - 0.5f) < 0.001f);
    assert(std::abs(child->GetEffectiveOpacity() - 0.4f) < 0.001f);
    assert(std::abs(grandchild->GetEffectiveOpacity() - 0.2f) < 0.001f);
    std::cout << "TestOpacityInheritsThroughWidgetTree passed\n";
}

} // namespace

int main() {
    TestTypeClassIdSelectorMatching();
    TestCommaSeparatedSelectorGroup();
    TestSpecificityOrdering();
    TestHoverActiveMatching();
    TestDisabledPseudoClassMatching();
    TestTransitionDeclarationParsing();
    TestDescendantCombinatorAndMultiValueMargin();
    TestRgbFunctionColorParsing();
    TestThreeDigitHexColorParsing();
    TestPercentWidthResolvesViaYoga();
    TestMinMaxDimensionsAndViewportHeight();
    TestCssGenericFontFamily();
    TestBoxShadowSpreadAndInset();
    TestMultipleCommaBoxShadows();
    TestThreeStopLinearGradient();
    TestBorderShorthand();
    TestMultiplePropertyTransitionList();
    TestOpacityTransitionRunsAfterClassChange();
    TestTranslateTransformAndTransition();
    TestBackdropFilterSingleFunction();
    TestBackdropFilterMultipleFunctions();
    TestBackdropHueRotateAngleUnits();
    TestBackdropFilterUnknownSubFunctionIgnored();
    TestRemoveClassResyncsCascade();
    TestToggleAndSetClass();
    TestDisplayParsing();
    TestFontWeightAndCursorParsingAndInheritance();
    TestScaleRotateParsingAndTransformMath();
    TestDisplayResetsWhenHidingClassRemoved();
    TestHiddenChildSkippedByHitTest();
    TestEmptyClassListRemovesAttribute();
    TestKeyframeAnimationAndReducedMotion();
    TestOpacityInheritsThroughWidgetTree();
    std::cout << "All CssParserTests passed.\n";
    return 0;
}
