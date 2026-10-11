#include "Core/Widget.h"

#include <algorithm>

#include "Core/Interpolate.h"
#include "Renderer/IRenderer.h"

namespace IzekipUI {

Widget::Widget(std::string typeName) : m_typeName(std::move(typeName)) {
    m_yogaNode = YGNodeNew();
}

Widget::~Widget() {
    if (m_yogaNode) {
        YGNodeFree(m_yogaNode);
    }
}

void Widget::AddChild(std::shared_ptr<Widget> child) {
    child->m_parent = this;
    YGNodeInsertChild(m_yogaNode, child->m_yogaNode, YGNodeGetChildCount(m_yogaNode));
    m_children.push_back(std::move(child));
}

bool Widget::HasClass(const std::string& className) const {
    return std::find(m_classes.begin(), m_classes.end(), className) != m_classes.end();
}

bool Widget::RemoveClass(const std::string& className) {
    const auto newEnd = std::remove(m_classes.begin(), m_classes.end(), className);
    const bool removed = newEnd != m_classes.end();
    m_classes.erase(newEnd, m_classes.end());
    return removed;
}

void Widget::SetClass(const std::string& className, bool enabled) {
    if (enabled) {
        if (!HasClass(className)) {
            m_classes.push_back(className);
        }
    } else {
        RemoveClass(className);
    }
}

bool Widget::ToggleClass(const std::string& className) {
    const bool enable = !HasClass(className);
    SetClass(className, enable);
    return enable;
}

void Widget::CalculateLayout(float availableWidth, float availableHeight) {
    YGNodeCalculateLayout(m_yogaNode, availableWidth, availableHeight, YGDirectionLTR);
    ResolveAbsoluteRects(0.0f, 0.0f);
}

void Widget::ResolveAbsoluteRects(float parentAbsoluteX, float parentAbsoluteY) {
    const float left = YGNodeLayoutGetLeft(m_yogaNode);
    const float top = YGNodeLayoutGetTop(m_yogaNode);
    const float width = YGNodeLayoutGetWidth(m_yogaNode);
    const float height = YGNodeLayoutGetHeight(m_yogaNode);

    m_rect.x = parentAbsoluteX + left;
    m_rect.y = parentAbsoluteY + top;
    m_rect.width = width;
    m_rect.height = height;

    for (auto& child : m_children) {
        child->ResolveAbsoluteRects(m_rect.x, m_rect.y);
    }
}

namespace {
Render::GradientDirection ToRenderGradientDirection(GradientDirection direction) {
    switch (direction) {
        case GradientDirection::ToRight: return Render::GradientDirection::ToRight;
        case GradientDirection::ToLeft: return Render::GradientDirection::ToLeft;
        case GradientDirection::ToTop: return Render::GradientDirection::ToTop;
        case GradientDirection::ToBottom: return Render::GradientDirection::ToBottom;
    }
    return Render::GradientDirection::ToRight;
}

// Resolves CSS-side gradient stops (percent positions, some possibly unset) into
// IRenderer's normalized-[0,1]-position stops. A CSS gradient with more than 3 stops is
// capped to its first 3 here -- the shader only carries 3 color slots (named scope cut,
// see the CSS value-parser expansion plan's gradient section).
std::vector<Render::GradientStop> ResolveGradientStops(const std::vector<GradientStop>& cssStops) {
    std::vector<GradientStop> capped(cssStops.begin(),
                                      cssStops.begin() + std::min<size_t>(cssStops.size(), 3));

    std::vector<Render::GradientStop> resolved;
    resolved.reserve(capped.size());
    const size_t lastIndex = capped.size() - 1;
    for (size_t i = 0; i < capped.size(); ++i) {
        // Auto-spacing (no explicit percent): evenly distribute by index. This is a
        // simplified version of CSS's full "interpolate between explicit neighbors"
        // algorithm -- fine for the common case of a gradient with no mixed explicit/
        // implicit positions.
        const float position = capped[i].percentPosition ? (*capped[i].percentPosition / 100.0f)
                                                            : (static_cast<float>(i) / static_cast<float>(lastIndex));
        resolved.push_back(Render::GradientStop{capped[i].color, position});
    }
    return resolved;
}
} // namespace

void Widget::OnPaint(Render::IRenderer& renderer) {
    if (m_style.backgroundColor || m_style.background || m_style.boxShadow || m_style.backdropFilter) {
        const float cornerRadius = m_style.borderRadius.value_or(0.0f);

        // Shadows are drawn first (so the element's own fill composites on top of them),
        // back-to-front in reverse CSS list order -- CSS's first-listed shadow is the one
        // nearest the element, i.e. the last one drawn. Each is its own DrawRect call with
        // a fully transparent fill: ComputeRectDrawData's "fill OVER shadow" composite then
        // reduces to just the shadow, so no shader change was needed for multiple shadows.
        if (m_style.boxShadow) {
            const auto& shadows = *m_style.boxShadow;
            const float opacity = m_style.opacity.value_or(1.0f);
            for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
                Render::RectPaint shadowOnlyPaint;
                shadowOnlyPaint.fill.color = Color{0.0f, 0.0f, 0.0f, 0.0f};
                shadowOnlyPaint.cornerRadius = cornerRadius;
                shadowOnlyPaint.shadow = Render::RectShadow{it->offsetX, it->offsetY, it->blurRadius,
                                                             it->spreadRadius, it->color};
                shadowOnlyPaint.opacity = opacity;
                renderer.DrawRect(m_rect, shadowOnlyPaint);
            }
        }

        // backdrop-filter next -- it samples/blurs whatever's already been painted behind
        // this element (shadows included), then the element's own (usually translucent)
        // fill right below composites on top of that blurred result, which is what
        // actually produces the "glass" look.
        if (m_style.backdropFilter) {
            Render::BackdropBlurPaint blurPaint;
            blurPaint.cornerRadius = cornerRadius;
            blurPaint.filter.blurRadiusPx = m_style.backdropFilter->blurRadiusPx.value_or(0.0f);
            blurPaint.filter.brightness = m_style.backdropFilter->brightness.value_or(1.0f);
            blurPaint.filter.contrast = m_style.backdropFilter->contrast.value_or(1.0f);
            blurPaint.filter.saturate = m_style.backdropFilter->saturate.value_or(1.0f);
            blurPaint.filter.grayscale = m_style.backdropFilter->grayscale.value_or(0.0f);
            renderer.DrawBackdropBlurRect(m_rect, blurPaint);
        }

        if (m_style.backgroundColor || m_style.background) {
            Render::RectPaint paint;
            if (m_style.background) {
                paint.fill.color = m_style.background->stops.front().color;
                paint.fill.gradientDirection = ToRenderGradientDirection(m_style.background->direction);
                paint.fill.gradientStops = ResolveGradientStops(m_style.background->stops);
            } else {
                paint.fill.color = *m_style.backgroundColor;
            }
            paint.cornerRadius = cornerRadius;
            paint.opacity = m_style.opacity.value_or(1.0f);
            renderer.DrawRect(m_rect, paint);
        }
    }
    // Hidden (display: none) children are skipped HERE, by the parent, rather than inside
    // each child's own OnPaint -- Label/TextArea override OnPaint and draw glyphs after
    // calling the base, so a child-side check would still let their text through.
    for (auto& child : m_children) {
        if (child->IsDisplayed()) {
            child->OnPaint(renderer);
        }
    }
}

Widget* Widget::HitTest(float x, float y) {
    if (x < m_rect.x || x > m_rect.x + m_rect.width || y < m_rect.y || y > m_rect.y + m_rect.height) {
        return nullptr;
    }
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        if (!(*it)->IsDisplayed()) {
            continue; // Yoga zeroes a hidden node's rect, but the bounds check is inclusive
        }
        if (Widget* hit = (*it)->HitTest(x, y)) {
            return hit;
        }
    }
    return this;
}

void Widget::SnapBackgroundColor(const Color& value) {
    m_style.backgroundColor = value;
    m_backgroundColorAnim.reset();
}

void Widget::AnimateBackgroundColorTo(const Color& target, float durationSeconds, const CubicBezier& easing) {
    if (!m_style.backgroundColor) {
        SnapBackgroundColor(target);
        return;
    }
    m_backgroundColorAnim = Animation<Color>{*m_style.backgroundColor, target, 0.0f, durationSeconds, easing};
}

void Widget::SnapWidth(Length value) {
    m_style.width = value;
    if (value.isPercent) {
        YGNodeStyleSetWidthPercent(m_yogaNode, value.value);
    } else {
        YGNodeStyleSetWidth(m_yogaNode, value.value);
    }
    m_widthAnim.reset();
}

void Widget::AnimateWidthTo(Length target, float durationSeconds, const CubicBezier& easing) {
    // Percent widths aren't animated -- interpolating would require re-resolving against
    // the parent's size every tick, which this float-lerp animation system doesn't do.
    // Snap instead (named scope cut).
    if (!m_style.width || target.isPercent || m_style.width->isPercent) {
        SnapWidth(target);
        return;
    }
    m_widthAnim = Animation<float>{m_style.width->value, target.value, 0.0f, durationSeconds, easing};
}

void Widget::SnapHeight(Length value) {
    m_style.height = value;
    if (value.isPercent) {
        YGNodeStyleSetHeightPercent(m_yogaNode, value.value);
    } else {
        YGNodeStyleSetHeight(m_yogaNode, value.value);
    }
    m_heightAnim.reset();
}

void Widget::AnimateHeightTo(Length target, float durationSeconds, const CubicBezier& easing) {
    if (!m_style.height || target.isPercent || m_style.height->isPercent) {
        SnapHeight(target);
        return;
    }
    m_heightAnim = Animation<float>{m_style.height->value, target.value, 0.0f, durationSeconds, easing};
}

void Widget::SnapMargin(const BoxEdges& value) {
    m_style.margin = value;
    YGNodeStyleSetMargin(m_yogaNode, YGEdgeTop, value.top);
    YGNodeStyleSetMargin(m_yogaNode, YGEdgeRight, value.right);
    YGNodeStyleSetMargin(m_yogaNode, YGEdgeBottom, value.bottom);
    YGNodeStyleSetMargin(m_yogaNode, YGEdgeLeft, value.left);
    m_marginAnim.reset();
}

void Widget::AnimateMarginTo(const BoxEdges& target, float durationSeconds, const CubicBezier& easing) {
    if (!m_style.margin) {
        SnapMargin(target);
        return;
    }
    m_marginAnim = Animation<BoxEdges>{*m_style.margin, target, 0.0f, durationSeconds, easing};
}

void Widget::SnapOpacity(float value) {
    m_style.opacity = value;
    m_opacityAnim.reset();
}

void Widget::AnimateOpacityTo(float target, float durationSeconds, const CubicBezier& easing) {
    if (!m_style.opacity) {
        SnapOpacity(target);
        return;
    }
    m_opacityAnim = Animation<float>{*m_style.opacity, target, 0.0f, durationSeconds, easing};
}

AnimationTickResult Widget::TickAnimations(float deltaSeconds) {
    AnimationTickResult result;

    if (m_backgroundColorAnim) {
        auto& anim = *m_backgroundColorAnim;
        anim.elapsed += deltaSeconds;
        const float t = anim.duration > 0.0f ? std::clamp(anim.elapsed / anim.duration, 0.0f, 1.0f) : 1.0f;
        const float eased = anim.easing.Evaluate(t);
        if (t >= 1.0f) {
            m_style.backgroundColor = anim.to;
            m_backgroundColorAnim.reset();
        } else {
            m_style.backgroundColor = LerpColor(anim.from, anim.to, eased);
            result.stillAnimating = true;
        }
    }

    if (m_widthAnim) {
        auto& anim = *m_widthAnim;
        anim.elapsed += deltaSeconds;
        const float t = anim.duration > 0.0f ? std::clamp(anim.elapsed / anim.duration, 0.0f, 1.0f) : 1.0f;
        const float eased = anim.easing.Evaluate(t);
        const float value = t >= 1.0f ? anim.to : Lerp(anim.from, anim.to, eased);
        m_style.width = Length{value, false}; // animated widths are always a definite px value
        YGNodeStyleSetWidth(m_yogaNode, value);
        result.layoutAffected = true;
        if (t >= 1.0f) {
            m_widthAnim.reset();
        } else {
            result.stillAnimating = true;
        }
    }

    if (m_heightAnim) {
        auto& anim = *m_heightAnim;
        anim.elapsed += deltaSeconds;
        const float t = anim.duration > 0.0f ? std::clamp(anim.elapsed / anim.duration, 0.0f, 1.0f) : 1.0f;
        const float eased = anim.easing.Evaluate(t);
        const float value = t >= 1.0f ? anim.to : Lerp(anim.from, anim.to, eased);
        m_style.height = Length{value, false}; // animated heights are always a definite px value
        YGNodeStyleSetHeight(m_yogaNode, value);
        result.layoutAffected = true;
        if (t >= 1.0f) {
            m_heightAnim.reset();
        } else {
            result.stillAnimating = true;
        }
    }

    if (m_marginAnim) {
        auto& anim = *m_marginAnim;
        anim.elapsed += deltaSeconds;
        const float t = anim.duration > 0.0f ? std::clamp(anim.elapsed / anim.duration, 0.0f, 1.0f) : 1.0f;
        const float eased = anim.easing.Evaluate(t);
        const BoxEdges value = t >= 1.0f ? anim.to : LerpBoxEdges(anim.from, anim.to, eased);
        m_style.margin = value;
        YGNodeStyleSetMargin(m_yogaNode, YGEdgeTop, value.top);
        YGNodeStyleSetMargin(m_yogaNode, YGEdgeRight, value.right);
        YGNodeStyleSetMargin(m_yogaNode, YGEdgeBottom, value.bottom);
        YGNodeStyleSetMargin(m_yogaNode, YGEdgeLeft, value.left);
        result.layoutAffected = true;
        if (t >= 1.0f) {
            m_marginAnim.reset();
        } else {
            result.stillAnimating = true;
        }
    }

    if (m_opacityAnim) {
        auto& anim = *m_opacityAnim;
        anim.elapsed += deltaSeconds;
        const float t = anim.duration > 0.0f ? std::clamp(anim.elapsed / anim.duration, 0.0f, 1.0f) : 1.0f;
        const float eased = anim.easing.Evaluate(t);
        const float value = t >= 1.0f ? anim.to : Lerp(anim.from, anim.to, eased);
        m_style.opacity = std::clamp(value, 0.0f, 1.0f);
        if (t >= 1.0f) {
            m_opacityAnim.reset();
        } else {
            result.stillAnimating = true;
        }
    }

    return result;
}

void ClearHoveredRecursive(Widget& root) {
    root.SetHovered(false);
    for (const auto& child : root.GetChildren()) {
        ClearHoveredRecursive(*child);
    }
}

void ClearActiveRecursive(Widget& root) {
    root.SetActive(false);
    for (const auto& child : root.GetChildren()) {
        ClearActiveRecursive(*child);
    }
}

void ClearFocusedRecursive(Widget& root) {
    root.SetFocused(false);
    for (const auto& child : root.GetChildren()) {
        ClearFocusedRecursive(*child);
    }
}

AnimationTickResult TickWidgetTreeAnimations(Widget& root, float deltaSeconds) {
    AnimationTickResult result = root.TickAnimations(deltaSeconds);
    for (const auto& child : root.GetChildren()) {
        result |= TickWidgetTreeAnimations(*child, deltaSeconds);
    }
    return result;
}

} // namespace IzekipUI
