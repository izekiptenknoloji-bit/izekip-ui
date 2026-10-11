#include <cassert>
#include <iostream>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Core/InteractionController.h"
#include "Core/TextBuffer.h"
#include "Widgets/Checkbox.h"
#include "Widgets/Panel.h"
#include "Widgets/ProgressBar.h"
#include "Widgets/RadioButton.h"

// Renderer-free tests: hit-testing scenes use plain Panels with fixed CSS sizes (a Label in
// the laid-out tree would need Inter.ttf in the working directory to measure itself), and
// the composite widgets are exercised through Click() without any layout.

using namespace IzekipUI;
using namespace IzekipUI::Css;

namespace {

// root (200x100, row) -> a (0..100) | b (100..200, contains bChild)
struct Scene {
    std::shared_ptr<Widgets::Panel> root = std::make_shared<Widgets::Panel>();
    std::shared_ptr<Widgets::Panel> a = std::make_shared<Widgets::Panel>();
    std::shared_ptr<Widgets::Panel> b = std::make_shared<Widgets::Panel>();
    std::shared_ptr<Widgets::Panel> bChild = std::make_shared<Widgets::Panel>();
    StyleSheet sheet = StyleSheet::LoadFromString(
        "#root { width: 200px; height: 100px; flex-direction: row; }"
        ".cell { width: 100px; height: 100px; }"
        ".inner { width: 50px; height: 50px; }"
        ".hidden { display: none; }");
    LexborBridge bridge;

    Scene() {
        root->SetId("root");
        a->AddClass("cell");
        b->AddClass("cell");
        bChild->AddClass("inner");
        b->AddChild(bChild);
        root->AddChild(a);
        root->AddChild(b);
        bridge.Build(*root);
        Restyle();
    }

    void Restyle() {
        StyleApplier::Apply(bridge, sheet, *root);
        root->CalculateLayout(200.0f, 100.0f);
    }
};

void Pump(InteractionController& controller, Scene& scene) {
    if (controller.Update()) {
        scene.Restyle();
    }
}

void TestClickFiresOnPressAndReleaseOverSameWidget() {
    Scene scene;
    int clicks = 0;
    scene.a->SetOnClick([&] { ++clicks; });
    InteractionController controller(*scene.root);

    controller.OnMouseMove(50.0f, 50.0f);
    controller.OnMouseButton(true);
    controller.OnMouseButton(false); // same frame: queued in order
    Pump(controller, scene);
    assert(clicks == 1);
    std::cout << "TestClickFiresOnPressAndReleaseOverSameWidget passed\n";
}

void TestNoClickWhenReleasedOutside() {
    Scene scene;
    int clicks = 0;
    scene.a->SetOnClick([&] { ++clicks; });
    InteractionController controller(*scene.root);

    controller.OnMouseMove(50.0f, 50.0f);
    controller.OnMouseButton(true);
    Pump(controller, scene);
    controller.OnMouseMove(150.0f, 50.0f); // over b
    controller.OnMouseButton(false);
    Pump(controller, scene);
    assert(clicks == 0);
    std::cout << "TestNoClickWhenReleasedOutside passed\n";
}

void TestPressOnChildBubblesToClickableAncestor() {
    Scene scene;
    int clicks = 0;
    scene.b->SetOnClick([&] { ++clicks; });
    InteractionController controller(*scene.root);

    controller.OnMouseMove(110.0f, 10.0f); // inside bChild (100..150, 0..50)
    Pump(controller, scene);
    assert(controller.GetHovered() == scene.bChild.get());
    assert(scene.b->IsHovered() && scene.root->IsHovered()); // hover chain
    controller.OnMouseButton(true);
    Pump(controller, scene);
    assert(scene.b->IsActive());
    controller.OnMouseMove(180.0f, 80.0f); // still inside b, outside bChild
    controller.OnMouseButton(false);
    Pump(controller, scene);
    assert(clicks == 1);
    assert(!scene.b->IsActive());
    std::cout << "TestPressOnChildBubblesToClickableAncestor passed\n";
}

void TestFocusAndBlur() {
    Scene scene;
    scene.a->SetFocusable(true);
    InteractionController controller(*scene.root);

    controller.OnMouseMove(50.0f, 50.0f);
    controller.OnMouseButton(true);
    controller.OnMouseButton(false);
    Pump(controller, scene);
    assert(controller.GetFocused() == scene.a.get() && scene.a->IsFocused());

    controller.OnMouseMove(150.0f, 50.0f); // press on non-focusable b -> blur
    controller.OnMouseButton(true);
    controller.OnMouseButton(false);
    Pump(controller, scene);
    assert(controller.GetFocused() == nullptr && !scene.a->IsFocused());

    controller.SetFocus(scene.a.get());
    scene.a->AddClass("hidden"); // hiding a focused widget drops focus
    scene.Restyle();
    Pump(controller, scene);
    assert(controller.GetFocused() == nullptr);
    std::cout << "TestFocusAndBlur passed\n";
}

void TestDisabledWidgetsCannotBeFocusedOrClicked() {
    Scene scene;
    int clicks = 0;
    scene.a->SetOnClick([&] { ++clicks; });
    scene.a->SetFocusable(true);
    scene.a->SetDisabled(true);
    InteractionController controller(*scene.root);

    controller.OnMouseMove(50.0f, 50.0f);
    controller.OnMouseButton(true);
    controller.OnMouseButton(false);
    Pump(controller, scene);
    assert(clicks == 0);
    assert(controller.GetFocused() == nullptr);

    scene.a->Click();
    assert(clicks == 0);

    scene.a->SetDisabled(false);
    controller.OnMouseButton(true);
    controller.OnMouseButton(false);
    Pump(controller, scene);
    assert(clicks == 1);
    assert(controller.GetFocused() == scene.a.get());

    scene.a->SetDisabled(true);
    Pump(controller, scene);
    assert(controller.GetFocused() == nullptr);
    std::cout << "TestDisabledWidgetsCannotBeFocusedOrClicked passed\n";
}

void TestHoverUpdatesAfterHideWithoutMouseMove() {
    Scene scene;
    InteractionController controller(*scene.root);
    controller.OnMouseMove(110.0f, 10.0f);
    Pump(controller, scene);
    assert(controller.GetHovered() == scene.bChild.get());

    scene.bChild->AddClass("hidden");
    scene.Restyle();
    Pump(controller, scene); // no mouse movement
    assert(controller.GetHovered() == scene.b.get());
    assert(!scene.bChild->IsHovered());
    std::cout << "TestHoverUpdatesAfterHideWithoutMouseMove passed\n";
}

void TestTranslatedWidgetHitTestingUsesPaintedPosition() {
    auto root = std::make_shared<Widgets::Panel>();
    root->SetId("root");
    auto child = std::make_shared<Widgets::Panel>();
    child->AddClass("target");
    root->AddChild(child);

    StyleSheet sheet = StyleSheet::LoadFromString(
        "#root { width: 100px; height: 100px; transform: translate(20px, 10px); }"
        ".target { width: 50px; height: 50px; }");
    LexborBridge bridge;
    bridge.Build(*root);
    StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(100.0f, 100.0f);

    const Rect childRect = child->GetVisualRect();
    assert(std::abs(childRect.x - 20.0f) < 0.001f);
    assert(std::abs(childRect.y - 10.0f) < 0.001f);
    assert(root->HitTest(25.0f, 15.0f) == child.get());
    assert(root->HitTest(5.0f, 5.0f) == nullptr);
    std::cout << "TestTranslatedWidgetHitTestingUsesPaintedPosition passed\n";
}

void TestCheckboxToggle() {
    Widgets::Checkbox box("Seçenek");
    int changes = 0;
    bool lastState = false;
    box.SetOnChanged([&](bool checked) {
        ++changes;
        lastState = checked;
    });
    assert(box.IsClickable());
    assert(!box.IsChecked() && !box.HasClass("checked"));

    box.Click();
    assert(box.IsChecked() && box.HasClass("checked") && changes == 1 && lastState);
    box.Click();
    assert(!box.IsChecked() && !box.HasClass("checked") && changes == 2 && !lastState);

    box.SetChecked(true); // programmatic: no callback
    assert(box.IsChecked() && changes == 2);
    std::cout << "TestCheckboxToggle passed\n";
}

void TestRadioGroupExclusivity() {
    auto first = std::make_shared<Widgets::RadioButton>("Sadece benim için");
    auto second = std::make_shared<Widgets::RadioButton>("Tüm kullanıcılar");
    Widgets::RadioGroup group;
    int changes = 0;
    group.SetOnChanged([&](int) { ++changes; });
    group.Add(first);
    group.Add(second);

    assert(group.GetSelectedIndex() == 0 && first->IsSelected() && !second->IsSelected());
    assert(first->HasClass("checked") && !second->HasClass("checked"));

    second->Click();
    assert(group.GetSelectedIndex() == 1 && !first->IsSelected() && second->IsSelected());
    assert(changes == 1);

    second->Click(); // already selected -> no change event
    assert(changes == 1);
    group.Select(0);
    assert(first->IsSelected() && changes == 2);
    std::cout << "TestRadioGroupExclusivity passed\n";
}

void TestProgressBarClamps() {
    Widgets::ProgressBar bar;
    bar.SetValue(0.5f);
    assert(bar.GetValue() == 0.5f);
    bar.SetValue(2.0f);
    assert(bar.GetValue() == 1.0f);
    bar.SetValue(-1.0f);
    assert(bar.GetValue() == 0.0f);
    std::cout << "TestProgressBarClamps passed\n";
}

void TestTextBufferGetTextUtf8RoundTrip() {
    TextBuffer buffer;
    const std::string text = "C:\\Users\\Furkan\\İzekip ş ğ ı\nikinci satır";
    buffer.SetTextUtf8(text);
    assert(buffer.GetTextUtf8() == text);
    buffer.InsertCodepoint(0x15F); // ş at the cursor (0,0)
    assert(buffer.GetTextUtf8() == "ş" + text);
    std::cout << "TestTextBufferGetTextUtf8RoundTrip passed\n";
}

} // namespace

int main() {
    TestClickFiresOnPressAndReleaseOverSameWidget();
    TestNoClickWhenReleasedOutside();
    TestPressOnChildBubblesToClickableAncestor();
    TestFocusAndBlur();
    TestDisabledWidgetsCannotBeFocusedOrClicked();
    TestHoverUpdatesAfterHideWithoutMouseMove();
    TestTranslatedWidgetHitTestingUsesPaintedPosition();
    TestCheckboxToggle();
    TestRadioGroupExclusivity();
    TestProgressBarClamps();
    TestTextBufferGetTextUtf8RoundTrip();
    std::cout << "All InteractionTests passed.\n";
    return 0;
}
