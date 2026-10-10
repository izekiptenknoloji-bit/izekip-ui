#pragma once

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

    const std::string& GetTypeName() const { return m_typeName; }

    StyleProperties& GetStyle() { return m_style; }
    const StyleProperties& GetStyle() const { return m_style; }

    YGNodeRef GetYogaNode() const { return m_yogaNode; }

    // Only meaningful when called on the root widget.
    void CalculateLayout(float availableWidth, float availableHeight);

    const Rect& GetLocalRect() const { return m_rect; }

    virtual void OnPaint(Render::IRenderer& renderer);

    // Deepest, topmost (last-added child wins) widget whose rect contains (x, y) in the
    // same absolute pixel space as GetLocalRect(). Returns nullptr on a total miss.
    Widget* HitTest(float x, float y);

    void SetHovered(bool hovered) { m_hovered = hovered; }
    bool IsHovered() const { return m_hovered; }
    void SetActive(bool active) { m_active = active; }
    bool IsActive() const { return m_active; }

    // Diff baseline for StyleApplier -- the last CSS-resolved target style, used to detect
    // "this property's value just changed" so it knows whether to animate or snap. Not
    // meant to be painted from; application code should use GetStyle() instead.
    const StyleProperties& GetTargetStyle() const { return m_targetStyle; }
    void SetTargetStyle(StyleProperties style) { m_targetStyle = std::move(style); }
    bool HasBeenStyled() const { return m_hasBeenStyled; }
    void MarkStyled() { m_hasBeenStyled = true; }

    void SnapBackgroundColor(const Color& value);
    void AnimateBackgroundColorTo(const Color& target, float durationSeconds, const CubicBezier& easing);

    void SnapWidth(float value);
    void AnimateWidthTo(float target, float durationSeconds, const CubicBezier& easing);

    void SnapHeight(float value);
    void AnimateHeightTo(float target, float durationSeconds, const CubicBezier& easing);

    void SnapMargin(const BoxEdges& value);
    void AnimateMarginTo(const BoxEdges& target, float durationSeconds, const CubicBezier& easing);

    // Advances this widget's in-flight animations by deltaSeconds, writing interpolated
    // results into m_style (and, for width/height/margin, into the Yoga node). Does not
    // recurse into children -- see the free function TickWidgetTreeAnimations for that.
    AnimationTickResult TickAnimations(float deltaSeconds);

protected:
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
    bool m_hasBeenStyled = false;
    StyleProperties m_targetStyle;

    std::optional<Animation<Color>> m_backgroundColorAnim;
    std::optional<Animation<float>> m_widthAnim;
    std::optional<Animation<float>> m_heightAnim;
    std::optional<Animation<BoxEdges>> m_marginAnim;
};

// Clears IsHovered()/IsActive() across the whole subtree -- used before re-marking the
// new hover/active ancestor chain each time the hit-tested widget changes.
void ClearHoveredRecursive(Widget& root);
void ClearActiveRecursive(Widget& root);

// Ticks every widget in the subtree and ORs their results together.
AnimationTickResult TickWidgetTreeAnimations(Widget& root, float deltaSeconds);

} // namespace IzekipUI
