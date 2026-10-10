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
} // namespace

void Widget::OnPaint(Render::IRenderer& renderer) {
    if (m_style.backgroundColor || m_style.background) {
        Render::RectPaint paint;
        paint.fill.color = m_style.background ? m_style.background->startColor : *m_style.backgroundColor;
        if (m_style.background) {
            paint.fill.gradientDirection = ToRenderGradientDirection(m_style.background->direction);
            paint.fill.gradientEndColor = m_style.background->endColor;
        }
        paint.cornerRadius = m_style.borderRadius.value_or(0.0f);
        if (m_style.boxShadow) {
            paint.shadow = Render::RectShadow{m_style.boxShadow->offsetX, m_style.boxShadow->offsetY,
                                               m_style.boxShadow->blurRadius, m_style.boxShadow->color};
        }
        renderer.DrawRect(m_rect, paint);
    }
    for (auto& child : m_children) {
        child->OnPaint(renderer);
    }
}

Widget* Widget::HitTest(float x, float y) {
    if (x < m_rect.x || x > m_rect.x + m_rect.width || y < m_rect.y || y > m_rect.y + m_rect.height) {
        return nullptr;
    }
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
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

void Widget::SnapWidth(float value) {
    m_style.width = value;
    YGNodeStyleSetWidth(m_yogaNode, value);
    m_widthAnim.reset();
}

void Widget::AnimateWidthTo(float target, float durationSeconds, const CubicBezier& easing) {
    if (!m_style.width) {
        SnapWidth(target);
        return;
    }
    m_widthAnim = Animation<float>{*m_style.width, target, 0.0f, durationSeconds, easing};
}

void Widget::SnapHeight(float value) {
    m_style.height = value;
    YGNodeStyleSetHeight(m_yogaNode, value);
    m_heightAnim.reset();
}

void Widget::AnimateHeightTo(float target, float durationSeconds, const CubicBezier& easing) {
    if (!m_style.height) {
        SnapHeight(target);
        return;
    }
    m_heightAnim = Animation<float>{*m_style.height, target, 0.0f, durationSeconds, easing};
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
        m_style.width = value;
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
        m_style.height = value;
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

AnimationTickResult TickWidgetTreeAnimations(Widget& root, float deltaSeconds) {
    AnimationTickResult result = root.TickAnimations(deltaSeconds);
    for (const auto& child : root.GetChildren()) {
        result |= TickWidgetTreeAnimations(*child, deltaSeconds);
    }
    return result;
}

} // namespace IzekipUI
