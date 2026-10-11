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
    ApplyViewportUnits(availableHeight);
    YGNodeCalculateLayout(m_yogaNode, availableWidth, availableHeight, YGDirectionLTR);
    ResolveAbsoluteRects(0.0f, 0.0f);
}

void Widget::ApplyViewportUnits(float viewportHeight) {
    const auto dimension = [viewportHeight](YGNodeRef node, const std::optional<Length>& value,
                                             bool width, bool minimum, bool maximum) {
        if (!value || !value->isViewportHeight) {
            return;
        }
        const float pixels = viewportHeight * value->value / 100.0f;
        if (width) {
            if (minimum) YGNodeStyleSetMinWidth(node, pixels);
            else if (maximum) YGNodeStyleSetMaxWidth(node, pixels);
            else YGNodeStyleSetWidth(node, pixels);
        } else {
            if (minimum) YGNodeStyleSetMinHeight(node, pixels);
            else if (maximum) YGNodeStyleSetMaxHeight(node, pixels);
            else YGNodeStyleSetHeight(node, pixels);
        }
    };
    dimension(m_yogaNode, m_style.width, true, false, false);
    dimension(m_yogaNode, m_style.height, false, false, false);
    dimension(m_yogaNode, m_style.minWidth, true, true, false);
    dimension(m_yogaNode, m_style.maxWidth, true, false, true);
    dimension(m_yogaNode, m_style.minHeight, false, true, false);
    dimension(m_yogaNode, m_style.maxHeight, false, false, true);
    for (const auto& child : m_children) {
        child->ApplyViewportUnits(viewportHeight);
    }
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

Rect Widget::GetVisualRect() const {
    Rect rect = m_rect;
    for (const Widget* widget = this; widget != nullptr; widget = widget->m_parent) {
        if (widget->m_style.transform) {
            rect.x += widget->m_style.transform->x;
            rect.y += widget->m_style.transform->y;
        }
    }
    return rect;
}

CssCursor Widget::GetEffectiveCursor() const {
    for (const Widget* widget = this; widget != nullptr; widget = widget->m_parent) {
        if (widget->m_style.cursor) {
            return *widget->m_style.cursor;
        }
    }
    return CssCursor::Default;
}

float Widget::GetEffectiveOpacity() const {
    float opacity = 1.0f;
    for (const Widget* widget = this; widget != nullptr; widget = widget->m_parent) {
        opacity *= widget->m_style.opacity.value_or(1.0f);
    }
    return std::clamp(opacity, 0.0f, 1.0f);
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
    const Rect visualRect = GetVisualRect();
    if (m_style.backgroundColor || m_style.background || m_style.boxShadow || m_style.backdropFilter) {
        const float cornerRadius = m_style.borderRadius.value_or(0.0f);

        // Shadows are drawn first (so the element's own fill composites on top of them),
        // back-to-front in reverse CSS list order -- CSS's first-listed shadow is the one
        // nearest the element, i.e. the last one drawn. Each is its own DrawRect call with
        // a fully transparent fill: ComputeRectDrawData's "fill OVER shadow" composite then
        // reduces to just the shadow, so no shader change was needed for multiple shadows.
        if (m_style.boxShadow) {
            const auto& shadows = *m_style.boxShadow;
            const float opacity = GetEffectiveOpacity();
            for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
                Render::RectPaint shadowOnlyPaint;
                shadowOnlyPaint.fill.color = Color{0.0f, 0.0f, 0.0f, 0.0f};
                shadowOnlyPaint.cornerRadius = cornerRadius;
                shadowOnlyPaint.shadow = Render::RectShadow{it->offsetX, it->offsetY, it->blurRadius,
                                                             it->spreadRadius, it->color};
                shadowOnlyPaint.opacity = opacity;
                renderer.DrawRect(visualRect, shadowOnlyPaint);
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
            blurPaint.filter.hueRotateRadians =
                m_style.backdropFilter->hueRotateRadians.value_or(0.0f);
            renderer.DrawBackdropBlurRect(visualRect, blurPaint);
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
            paint.opacity = GetEffectiveOpacity();
            renderer.DrawRect(visualRect, paint);
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
    const Rect visualRect = GetVisualRect();
    if (x < visualRect.x || x > visualRect.x + visualRect.width ||
        y < visualRect.y || y > visualRect.y + visualRect.height) {
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

void Widget::SnapTransform(Translation value) {
    m_style.transform = value;
    m_transformAnim.reset();
}

void Widget::AnimateTransformTo(Translation target, float durationSeconds,
                                const CubicBezier& easing) {
    if (!m_style.transform) {
        SnapTransform(target);
        return;
    }
    m_transformAnim = Animation<Translation>{*m_style.transform, target, 0.0f,
                                              durationSeconds, easing};
}

void Widget::StartCssAnimation(const CssAnimation& animation,
                               std::vector<KeyframeAnimationPoint> points) {
    if (animation.name.empty() || animation.durationSeconds <= 0.0f || points.empty()) {
        m_cssAnimation.reset();
        return;
    }
    std::stable_sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
        return a.offset < b.offset;
    });
    m_cssAnimation = CssAnimationState{animation, std::move(points), 0.0f,
                                       m_style.opacity, m_style.transform, false};
}

void Widget::CancelCssAnimation() {
    if (m_cssAnimation) {
        m_style.opacity = m_cssAnimation->baseOpacity;
        m_style.transform = m_cssAnimation->baseTransform;
        m_cssAnimation.reset();
    }
}

bool Widget::CssAnimationAffectsOpacity() const {
    return m_cssAnimation && std::any_of(
        m_cssAnimation->points.begin(), m_cssAnimation->points.end(),
        [](const KeyframeAnimationPoint& point) { return point.opacity.has_value(); });
}

bool Widget::CssAnimationAffectsTransform() const {
    return m_cssAnimation && std::any_of(
        m_cssAnimation->points.begin(), m_cssAnimation->points.end(),
        [](const KeyframeAnimationPoint& point) { return point.transform.has_value(); });
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

    if (m_transformAnim) {
        auto& anim = *m_transformAnim;
        anim.elapsed += deltaSeconds;
        const float t = anim.duration > 0.0f
                            ? std::clamp(anim.elapsed / anim.duration, 0.0f, 1.0f)
                            : 1.0f;
        const float eased = anim.easing.Evaluate(t);
        if (t >= 1.0f) {
            m_style.transform = anim.to;
            m_transformAnim.reset();
        } else {
            m_style.transform = Translation{
                Lerp(anim.from.x, anim.to.x, eased),
                Lerp(anim.from.y, anim.to.y, eased)
            };
            result.stillAnimating = true;
        }
    }

    if (m_cssAnimation) {
        auto& anim = *m_cssAnimation;
        if (anim.finished) return result;
        anim.elapsed += std::max(deltaSeconds, 0.0f);
        const float progress = anim.spec.durationSeconds > 0.0f
                                   ? std::clamp(anim.elapsed / anim.spec.durationSeconds, 0.0f, 1.0f)
                                   : 1.0f;
        const bool finished = progress >= 1.0f;
        const float eased = anim.spec.easing.Evaluate(progress);

        const auto sample = [eased](auto base, const auto& member, auto interpolate)
            -> std::optional<decltype(base)> {
            using Value = decltype(base);
            std::vector<std::pair<float, Value>> values;
            bool hasAtStart = false;
            bool hasAtEnd = false;
            for (const auto& point : member) {
                if (point.second) {
                    values.emplace_back(point.first, *point.second);
                    hasAtStart = hasAtStart || point.first == 0.0f;
                    hasAtEnd = hasAtEnd || point.first == 1.0f;
                }
            }
            if (!hasAtStart) values.emplace_back(0.0f, base);
            if (!hasAtEnd) values.emplace_back(1.0f, base);
            std::stable_sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });
            if (values.size() < 2) return std::nullopt;
            auto upper = std::upper_bound(values.begin(), values.end(), eased,
                                          [](float position, const auto& entry) {
                                              return position < entry.first;
                                          });
            if (upper == values.begin()) return upper->second;
            if (upper == values.end()) return values.back().second;
            const auto& right = *upper;
            const auto& left = *(upper - 1);
            const float span = right.first - left.first;
            const float local = span > 0.0f ? (eased - left.first) / span : 1.0f;
            return interpolate(left.second, right.second, local);
        };

        std::vector<std::pair<float, std::optional<float>>> opacityPoints;
        std::vector<std::pair<float, std::optional<Translation>>> transformPoints;
        opacityPoints.reserve(anim.points.size());
        transformPoints.reserve(anim.points.size());
        for (const auto& point : anim.points) {
            opacityPoints.emplace_back(point.offset, point.opacity);
            transformPoints.emplace_back(point.offset, point.transform);
        }
        const float baseOpacity = anim.baseOpacity.value_or(1.0f);
        if (auto value = sample(baseOpacity, opacityPoints,
                                [](float from, float to, float t) { return Lerp(from, to, t); })) {
            m_style.opacity = std::clamp(*value, 0.0f, 1.0f);
        }
        const Translation baseTransform = anim.baseTransform.value_or(Translation{});
        if (auto value = sample(baseTransform, transformPoints, [](Translation from, Translation to, float t) {
                return Translation{Lerp(from.x, to.x, t), Lerp(from.y, to.y, t)};
            })) {
            m_style.transform = *value;
        }
        if (finished) {
            if (!anim.spec.forwards) {
                m_style.opacity = anim.baseOpacity;
                m_style.transform = anim.baseTransform;
                m_cssAnimation.reset();
            } else {
                anim.finished = true;
            }
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
