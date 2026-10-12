#pragma once

#include <cstdint>
#include <vector>

#include "Core/Widget.h"

namespace IzekipUI {

// Owns the hover / active / focus / click bookkeeping every app otherwise hand-rolls in its
// main loop. Platform- and renderer-independent: feed it raw mouse events (from
// Platform::Window callbacks, or synthetic ones in tests) and call Update() once per frame.
//
//   controller.OnMouseMove(x, y);          // from the cursor-pos callback
//   controller.OnMouseButton(pressed);     // from the mouse-button callback (left button)
//   ...
//   if (controller.Update()) {             // once per frame, BEFORE painting
//       Css::StyleApplier::Apply(bridge, sheet, root);
//       root.CalculateLayout(w, h);
//   }
//
// Click rule: a click fires when the left button is pressed over a clickable widget (the
// nearest ancestor-or-self with IsClickable()) and released anywhere inside that widget's
// subtree. Clicks are queued and fired from inside Update(), never from inside the mouse
// callbacks, so onClick handlers can freely change classes / text / pages.
//
// Widget pointers are held raw: widgets are never removed from a tree in this library (no
// RemoveChild), so they outlive the controller's use of them as long as the tree does.
class InteractionController {
public:
    explicit InteractionController(Widget& root);

    void OnMouseMove(float x, float y);
    void OnMouseButton(bool pressed); // left button only; filter other buttons in the app

    // Hit-tests (every call, so a page switch updates hover even without mouse movement),
    // updates hover/active chains, applies click-to-focus/blur, drops focus from a widget
    // that is no longer displayed, then fires queued clicks. Returns true if anything that
    // affects styling changed or any click fired -- the caller must then re-run
    // StyleApplier::Apply and CalculateLayout.
    bool Update();

    // For class/text changes made outside click handlers (which already count).
    void RequestRestyle() { m_restyleRequested = true; }

    // nullptr = blur. Clears focus across the whole tree first.
    void SetFocus(Widget* widget);

    Widget* GetHovered() const { return m_hovered; }
    Widget* GetFocused() const { return m_focused; }

private:
    void HandlePress();
    void HandleRelease(std::vector<Widget*>& clicksToFire);

    Widget& m_root;
    float m_mouseX = -1.0f;
    float m_mouseY = -1.0f;
    bool m_buttonDown = false;
    std::vector<bool> m_pendingButtonEvents; // in arrival order, true = press
    Widget* m_hovered = nullptr;
    Widget* m_clickTarget = nullptr;
    Widget* m_focused = nullptr;
    std::uint64_t m_treeGeneration = 0;
    bool m_activeState = false;
    bool m_restyleRequested = false;
};

} // namespace IzekipUI
