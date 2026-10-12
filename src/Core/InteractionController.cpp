#include "Core/InteractionController.h"

namespace IzekipUI {

namespace {

void MarkChain(Widget* widget, void (Widget::*setter)(bool)) {
    for (Widget* w = widget; w != nullptr; w = w->GetParent()) {
        (w->*setter)(true);
    }
}

bool IsWithin(const Widget* widget, const Widget* ancestor) {
    for (const Widget* w = widget; w != nullptr; w = w->GetParent()) {
        if (w == ancestor) return true;
    }
    return false;
}

// A widget is effectively visible only if it and every ancestor are displayed.
bool IsEffectivelyDisplayed(const Widget* widget) {
    for (const Widget* w = widget; w != nullptr; w = w->GetParent()) {
        if (!w->IsDisplayed()) return false;
    }
    return true;
}

Widget* NearestClickable(Widget* widget) {
    for (Widget* w = widget; w != nullptr; w = w->GetParent()) {
        if (w->IsClickable() && w->IsEnabled()) return w;
    }
    return nullptr;
}

Widget* NearestFocusable(Widget* widget) {
    for (Widget* w = widget; w != nullptr; w = w->GetParent()) {
        if (w->IsFocusable() && w->IsEnabled()) return w;
    }
    return nullptr;
}

} // namespace

InteractionController::InteractionController(Widget& root) : m_root(root) {}

void InteractionController::OnMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

void InteractionController::OnMouseButton(bool pressed) {
    m_pendingButtonEvents.push_back(pressed);
}

void InteractionController::SetFocus(Widget* widget) {
    if (widget == m_focused) {
        return;
    }
    ClearFocusedRecursive(m_root);
    if (widget) {
        widget->SetFocused(true);
    }
    m_focused = widget;
    m_restyleRequested = true;
}

void InteractionController::HandlePress() {
    m_buttonDown = true;
    m_clickTarget = NearestClickable(m_hovered);
    SetFocus(NearestFocusable(m_hovered)); // pressing on nothing focusable blurs
}

void InteractionController::HandleRelease(std::vector<Widget*>& clicksToFire) {
    m_buttonDown = false;
    if (m_clickTarget && m_hovered && IsWithin(m_hovered, m_clickTarget)) {
        clicksToFire.push_back(m_clickTarget);
    }
    m_clickTarget = nullptr;
}

bool InteractionController::Update() {
    // A structural change may have destroyed widgets the controller points at: forget them first.
    if (m_treeGeneration != WidgetTreeGeneration()) {
        m_treeGeneration = WidgetTreeGeneration();
        m_hovered = nullptr;
        m_clickTarget = nullptr;
        m_focused = nullptr;
        m_restyleRequested = true;
    }
    bool changed = m_restyleRequested;
    m_restyleRequested = false;

    // Hover -- hit-tested every frame, not just on mouse movement, so a page switch under a
    // stationary cursor still updates :hover.
    Widget* hit = m_root.HitTest(m_mouseX, m_mouseY);
    if (hit != m_hovered) {
        ClearHoveredRecursive(m_root);
        if (hit) {
            MarkChain(hit, &Widget::SetHovered);
        }
        m_hovered = hit;
        changed = true;
    }

    std::vector<Widget*> clicksToFire;
    for (const bool pressed : m_pendingButtonEvents) {
        if (pressed) {
            HandlePress();
        } else {
            HandleRelease(clicksToFire);
        }
    }
    m_pendingButtonEvents.clear();

    // :active = the hovered chain while the button is held (same rule the examples use).
    const bool wantActive = m_buttonDown && m_hovered != nullptr && m_hovered->IsEnabled();
    if (wantActive != m_activeState || (wantActive && changed)) {
        ClearActiveRecursive(m_root);
        if (wantActive) {
            MarkChain(m_hovered, &Widget::SetActive);
        }
        m_activeState = wantActive;
        changed = true;
    }

    if (m_focused && (!IsEffectivelyDisplayed(m_focused) || !m_focused->IsEnabled())) {
        SetFocus(nullptr);
    }

    for (Widget* target : clicksToFire) {
        target->Click();
        changed = true;
    }

    if (m_restyleRequested) { // SetFocus() calls above
        changed = true;
        m_restyleRequested = false;
    }
    return changed;
}

} // namespace IzekipUI
