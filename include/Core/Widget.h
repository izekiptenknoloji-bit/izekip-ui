#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <yoga/Yoga.h>

#include "Core/Easing.h"
#include "Core/Rect.h"
#include "Core/StyleProperties.h"

namespace IzekipUI::Render {
class IRenderer;
}

namespace IzekipUI {

struct AnimationTickResult {
    bool stillAnimating = false;
    bool layoutAffected = false; // true if width/height/margin changed and a relayout is needed

    AnimationTickResult& operator|=(const AnimationTickResult& other) {
        stillAnimating = stillAnimating || other.stillAnimating;
        layoutAffected = layoutAffected || other.layoutAffected;
        return *this;
    }
};

struct KeyframeAnimationPoint {
    float offset = 0.0f;
    std::optional<float> opacity;
    std::optional<Translation> transform;
};

// Base class for every UI element. Layout is delegated to Yoga: each Widget owns a
// YGNodeRef, and CSS-driven width/height/margin/padding are written into that node
// (see StyleApplier). CalculateLayout() runs Yoga's algorithm once from the root and
// GetLocalRect() reads back the resulting absolute rect.
class Widget : public std::enable_shared_from_this<Widget> {
public:
    explicit Widget(std::string typeName);
    virtual ~Widget();

    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    void AddChild(std::shared_ptr<Widget> child);
    const std::vector<std::shared_ptr<Widget>>& GetChildren() const { return m_children; }
    Widget* GetParent() const { return m_parent; }

    void SetId(std::string id) { m_id = std::move(id); }
    const std::string& GetId() const { return m_id; }

    void AddClass(std::string className) { m_classes.push_back(std::move(className)); }
    bool HasClass(const std::string& className) const;
    const std::vector<std::string>& GetClasses() const { return m_classes; }

    // Class mutation after LexborBridge::Build() is fine -- the bridge re-syncs classes on
    // every StyleApplier::Apply(). The caller still has to trigger that Apply (e.g. via
    // InteractionController::RequestRestyle(), or by doing it from inside an onClick handler,
    // whose firing already makes InteractionController::Update() return true).
    bool RemoveClass(const std::string& className);          // removes every occurrence; true if any
    void SetClass(const std::string& className, bool enabled); // add-if-absent / remove
    bool ToggleClass(const std::string& className);          // returns the new state

    // False once CSS resolved `display: none` for this widget. Parents skip hidden children
    // in OnPaint and HitTest. Hiding the root widget itself is unsupported.
    bool IsDisplayed() const { return m_style.display.value_or(Display::Flex) != Display::None; }

    // Click handling. Clicks are detected and fired by InteractionController (press +
    // release over the same clickable widget's subtree); Click() fires one programmatically.
    void SetOnClick(std::function<void()> onClick) { m_onClick = std::move(onClick); }
    void Click() {
        if (IsEnabled()) {
            OnClicked();
        }
    }
    bool IsClickable() const { return HandlesClick(); }

    // Disabled widgets remain visible and hoverable, but cannot be focused or activated.
    // A disabled ancestor disables its entire subtree; request a restyle after changing it.
    void SetDisabled(bool disabled) { m_disabled = disabled; }
    bool IsDisabled() const { return m_disabled; }
    bool IsEnabled() const {
        for (const Widget* widget = this; widget != nullptr; widget = widget->m_parent) {
            if (widget->m_disabled) {
                return false;
            }
        }
        return true;
    }

    // Whether pressing this widget gives it keyboard focus (TextArea is focusable by default).
    void SetFocusable(bool focusable) { m_focusable = focusable; }
    bool IsFocusable() const { return m_focusable; }

    // Cursor shape for this widget: the nearest ancestor-or-self with `cursor` set, else Default.
    CssCursor GetEffectiveCursor() const;

    const std::string& GetTypeName() const { return m_typeName; }

    StyleProperties& GetStyle() { return m_style; }
    const StyleProperties& GetStyle() const { return m_style; }

    YGNodeRef GetYogaNode() const { return m_yogaNode; }

    // Only meaningful when called on the root widget.
    void CalculateLayout(float availableWidth, float availableHeight);
    void ApplyViewportUnits(float viewportHeight);

    const Rect& GetLocalRect() const { return m_rect; }
    Rect GetVisualRect() const;
    float GetEffectiveOpacity() const;

    virtual void OnPaint(Render::IRenderer& renderer);

    // Deepest, topmost (last-added child wins) widget whose rect contains (x, y) in the
    // same absolute pixel space as GetLocalRect(). Returns nullptr on a total miss.
    Widget* HitTest(float x, float y);

    void SetHovered(bool hovered) { m_hovered = hovered; }
    bool IsHovered() const { return m_hovered; }
    void SetActive(bool active) { m_active = active; }
    bool IsActive() const { return m_active; }
    void SetFocused(bool focused) { m_focused = focused; }
    bool IsFocused() const { return m_focused; }

    // Diff baseline for StyleApplier -- the last CSS-resolved target style, used to detect
    // "this property's value just changed" so it knows whether to animate or snap. Not
    // meant to be painted from; application code should use GetStyle() instead.
    const StyleProperties& GetTargetStyle() const { return m_targetStyle; }
    void SetTargetStyle(StyleProperties style) { m_targetStyle = std::move(style); }
    bool HasBeenStyled() const { return m_hasBeenStyled; }
    void MarkStyled() { m_hasBeenStyled = true; }

    void SnapBackgroundColor(const Color& value);
    void AnimateBackgroundColorTo(const Color& target, float durationSeconds, const CubicBezier& easing);

    void SnapWidth(Length value);
    void AnimateWidthTo(Length target, float durationSeconds, const CubicBezier& easing);

    void SnapHeight(Length value);
    void AnimateHeightTo(Length target, float durationSeconds, const CubicBezier& easing);

    void SnapMargin(const BoxEdges& value);
    void AnimateMarginTo(const BoxEdges& target, float durationSeconds, const CubicBezier& easing);

    void SnapOpacity(float value);
    void AnimateOpacityTo(float target, float durationSeconds, const CubicBezier& easing);

    void SnapTransform(Translation value);
    void AnimateTransformTo(Translation target, float durationSeconds, const CubicBezier& easing);
    void StartCssAnimation(const CssAnimation& animation,
                           std::vector<KeyframeAnimationPoint> points);
    void CancelCssAnimation();
    bool CssAnimationAffectsOpacity() const;
    bool CssAnimationAffectsTransform() const;

    // Advances this widget's in-flight animations by deltaSeconds, writing interpolated
    // results into m_style (and, for width/height/margin, into the Yoga node). Does not
    // recurse into children -- see the free function TickWidgetTreeAnimations for that.
    AnimationTickResult TickAnimations(float deltaSeconds);

protected:
    // Subclassing hooks for click handling (Checkbox, RadioButton). The default "is
    // clickable" answer is "has an onClick callback".
    virtual bool HandlesClick() const { return static_cast<bool>(m_onClick); }
    virtual void OnClicked() {
        if (m_onClick) m_onClick();
    }

    std::function<void()> m_onClick;
    bool m_focusable = false;

    // Reads this node's Yoga-computed layout, offsets it by parentAbsoluteX/Y to produce
    // an absolute Rect, stores it in m_rect, and recurses into children.
    void ResolveAbsoluteRects(float parentAbsoluteX, float parentAbsoluteY);

    template <typename T>
    struct Animation {
        T from{};
        T to{};
        float elapsed = 0.0f;
        float duration = 0.0f;
        CubicBezier easing{};
    };

    YGNodeRef m_yogaNode = nullptr;
    std::string m_typeName;
    std::string m_id;
    std::vector<std::string> m_classes;
    std::vector<std::shared_ptr<Widget>> m_children;
    Widget* m_parent = nullptr;
    StyleProperties m_style;
    Rect m_rect;

    bool m_hovered = false;
    bool m_active = false;
    bool m_focused = false;
    bool m_disabled = false;
    bool m_hasBeenStyled = false;
    StyleProperties m_targetStyle;

    std::optional<Animation<Color>> m_backgroundColorAnim;
    std::optional<Animation<float>> m_widthAnim;
    std::optional<Animation<float>> m_heightAnim;
    std::optional<Animation<BoxEdges>> m_marginAnim;
    std::optional<Animation<float>> m_opacityAnim;
    std::optional<Animation<Translation>> m_transformAnim;
    struct CssAnimationState {
        CssAnimation spec;
        std::vector<KeyframeAnimationPoint> points;
        float elapsed = 0.0f;
        std::optional<float> baseOpacity;
        std::optional<Translation> baseTransform;
        bool finished = false;
    };
    std::optional<CssAnimationState> m_cssAnimation;
};

// Clears IsHovered()/IsActive() across the whole subtree -- used before re-marking the
// new hover/active ancestor chain each time the hit-tested widget changes.
void ClearHoveredRecursive(Widget& root);
void ClearActiveRecursive(Widget& root);
void ClearFocusedRecursive(Widget& root);

// Ticks every widget in the subtree and ORs their results together.
AnimationTickResult TickWidgetTreeAnimations(Widget& root, float deltaSeconds);

} // namespace IzekipUI
