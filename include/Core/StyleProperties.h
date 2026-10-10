#pragma once

#include <optional>

#include "Core/Color.h"
#include "Core/Rect.h"
#include "Core/Transition.h"

namespace IzekipUI {

enum class GradientDirection { ToRight, ToLeft, ToTop, ToBottom };

// Mirrors the subset of Yoga's own flexbox enums IzekipUI exposes through CSS. Yoga
// already implements the real flexbox layout math -- these just forward the user's CSS
// keyword choice into YGNodeStyleSet{FlexDirection,JustifyContent,AlignItems}.
enum class FlexDirection { Row, Column, RowReverse, ColumnReverse };
enum class JustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround };
enum class AlignItems { FlexStart, FlexEnd, Center, Stretch };

struct BoxShadow {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float blurRadius = 0.0f;
    Color color;

    bool operator==(const BoxShadow& other) const {
        return offsetX == other.offsetX && offsetY == other.offsetY &&
               blurRadius == other.blurRadius && color == other.color;
    }
    bool operator!=(const BoxShadow& other) const { return !(*this == other); }
};

struct LinearGradient {
    GradientDirection direction = GradientDirection::ToRight;
    Color startColor;
    Color endColor;

    bool operator==(const LinearGradient& other) const {
        return direction == other.direction && startColor == other.startColor &&
               endColor == other.endColor;
    }
    bool operator!=(const LinearGradient& other) const { return !(*this == other); }
};

// Visual properties a widget can receive from CSS. Every field is optional because
// a widget only has what some matched rule actually set — MergeFrom only overwrites
// fields the higher-priority side actually set, leaving the rest untouched.
struct StyleProperties {
    std::optional<Color> backgroundColor;
    std::optional<Color> textColor;
    std::optional<Color> borderColor;

    std::optional<float> width;
    std::optional<float> height;
    std::optional<float> borderWidth;
    std::optional<float> fontSize;

    std::optional<BoxEdges> margin;
    std::optional<BoxEdges> padding;

    // Describes how future resolved-value changes to this widget should be animated.
    // Not itself a value to paint -- just read by StyleApplier when deciding animate vs snap.
    std::optional<TransitionSpec> transition;

    // Visual-effects properties (Phase: SDF render pass). Snap-only -- intentionally never
    // wired into TransitionProperty, so they structurally can't be animated yet.
    std::optional<float> borderRadius;
    std::optional<BoxShadow> boxShadow;
    std::optional<LinearGradient> background; // distinct from backgroundColor; wins over it if set

    // Flexbox layout keywords -- pushed straight into the widget's Yoga node (not painted,
    // so not part of the animation system, same as padding).
    std::optional<FlexDirection> flexDirection;
    std::optional<JustifyContent> justifyContent;
    std::optional<AlignItems> alignItems;

    void MergeFrom(const StyleProperties& higherPriority);
};

} // namespace IzekipUI
