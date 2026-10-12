#pragma once

#include <optional>
#include <string>
#include <vector>

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
// CSS `display`. Only `flex` (the default) and `none` are recognized -- `none` removes the
// widget from layout AND from painting/hit-testing (parents skip hidden children).
enum class Display { Flex, None };

// A CSS <length-percentage>: either a definite pixel value or a percentage of the
// parent's resolved size. Yoga resolves the percentage case natively (YGNodeStyleSet
// {Width,Height}Percent) -- isPercent just tells StyleApplier which Yoga setter to call,
// no percentage-resolution math lives on the IzekipUI side.
struct Length {
    float value = 0.0f;
    bool isPercent = false;
    bool isViewportHeight = false;

    bool operator==(const Length& other) const {
        return value == other.value && isPercent == other.isPercent &&
               isViewportHeight == other.isViewportHeight;
    }
    bool operator!=(const Length& other) const { return !(*this == other); }
};

// Named faces: sans-serif/system-ui -> Proportional (Inter), monospace -> Monospace (SpaceMono),
// arial/helvetica -> Arial (Arimo), serif/times -> Serif (Tinos), courier -> Courier (Cousine).
enum class CssFontFamily { Proportional, Monospace, Arial, Serif, Courier };

// Mouse cursor shape requested by CSS `cursor`. Applied by the app via Platform::Window::SetCursor
// for whichever widget is hovered; inherited from ancestors when unset (as in CSS).
enum class CssCursor { Default, Pointer, Text, Crosshair, NotAllowed };

struct CssAnimation {
    std::string name;
    float durationSeconds = 0.0f;
    CubicBezier easing{};
    bool forwards = false;

    bool operator==(const CssAnimation& other) const {
        return name == other.name && durationSeconds == other.durationSeconds &&
               easing.x1 == other.easing.x1 && easing.y1 == other.easing.y1 &&
               easing.x2 == other.easing.x2 && easing.y2 == other.easing.y2 &&
               forwards == other.forwards;
    }
    bool operator!=(const CssAnimation& other) const { return !(*this == other); }
};

struct Translation {
    float x = 0.0f;
    float y = 0.0f;

    bool operator==(const Translation& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const Translation& other) const { return !(*this == other); }
};

struct BoxShadow {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float blurRadius = 0.0f;
    float spreadRadius = 0.0f;
    // Parsed but not yet rendered differently -- the shader's shadow pass always draws a
    // drop shadow; true inset (shadow painted inward from the element's inner edge) needs
    // its own smoothstep direction and is a deferred follow-up (named scope cut).
    bool inset = false;
    Color color;

    bool operator==(const BoxShadow& other) const {
        return offsetX == other.offsetX && offsetY == other.offsetY &&
               blurRadius == other.blurRadius && spreadRadius == other.spreadRadius &&
               inset == other.inset && color == other.color;
    }
    bool operator!=(const BoxShadow& other) const { return !(*this == other); }
};

// <linear-color-stop> = <color> <percentage>? -- percentPosition is the raw, unresolved
// CSS percent (0-100); nullopt means "auto-spaced", resolved later by Widget::OnPaint
// (the same CSS-to-render translation seam as everywhere else), not here.
struct GradientStop {
    Color color;
    std::optional<float> percentPosition;

    bool operator==(const GradientStop& other) const {
        return color == other.color && percentPosition == other.percentPosition;
    }
    bool operator!=(const GradientStop& other) const { return !(*this == other); }
};

struct LinearGradient {
    GradientDirection direction = GradientDirection::ToRight;
    std::vector<GradientStop> stops; // always >= 2

    bool operator==(const LinearGradient& other) const {
        return direction == other.direction && stops == other.stops;
    }
    bool operator!=(const LinearGradient& other) const { return !(*this == other); }
};

// backdrop-filter: an effect list applied to whatever has already been painted BEHIND
// this widget, sampled via a GPU-to-GPU render-target copy (see Widget::OnPaint and
// IRenderer::DrawBackdropBlurRect) -- not a per-pixel paint property like the others
// above. Each field is independently optional; unset means "no-op" for that function
// (brightness/contrast/saturate default to 1.0 = unchanged, grayscale to 0.0 = unchanged).
// Named scope cut: the CSS *order* the functions are written in is not preserved -- blur
// is always applied first (it commutes with the affine color adjustments below: blur is a
// linear spatial operator, brightness/contrast/saturate/grayscale are all linear
// per-pixel color transforms, and a linear spatial operator commutes with any
// position-independent linear color transform), then brightness -> contrast -> saturate
// -> grayscale -> hue-rotate in that fixed order. invert()/drop-shadow()/opacity() are
// unsupported and silently ignored per-function (not the whole declaration).
struct BackdropFilter {
    std::optional<float> blurRadiusPx;
    std::optional<float> brightness;
    std::optional<float> contrast;
    std::optional<float> saturate;
    std::optional<float> grayscale;
    std::optional<float> hueRotateRadians;

    bool operator==(const BackdropFilter& other) const {
        return blurRadiusPx == other.blurRadiusPx && brightness == other.brightness &&
               contrast == other.contrast && saturate == other.saturate &&
               grayscale == other.grayscale && hueRotateRadians == other.hueRotateRadians;
    }
    bool operator!=(const BackdropFilter& other) const { return !(*this == other); }
};

// Visual properties a widget can receive from CSS. Every field is optional because
// a widget only has what some matched rule actually set — MergeFrom only overwrites
// fields the higher-priority side actually set, leaving the rest untouched.
struct StyleProperties {
    std::optional<Color> backgroundColor;
    std::optional<Color> textColor;
    std::optional<Color> borderColor;

    std::optional<Length> width;
    std::optional<Length> height;
    std::optional<Length> minWidth;
    std::optional<Length> maxWidth;
    std::optional<Length> minHeight;
    std::optional<Length> maxHeight;
    std::optional<float> borderWidth;
    std::optional<float> fontSize;
    std::optional<CssFontFamily> fontFamily;
    // CSS numeric weight (100..900). Only the 400-vs-bold distinction is rendered, via a
    // synthetic emboldening pass in Label (no bold font asset ships with IzekipUI).
    std::optional<int> fontWeight;
    std::optional<CssCursor> cursor;
    // CSS individual transform properties. `scale` is a uniform factor (> 0), `rotate` is in
    // radians. Both pivot about the widget's own center; layout is never affected.
    std::optional<float> scale;
    std::optional<float> rotate;
    std::optional<float> opacity;
    std::optional<Translation> transform;
    std::optional<CssAnimation> animation;

    // margin/padding stay pixel-only for now (percent support deferred -- BoxEdges would
    // need to become a Length-per-edge struct, a bigger change than this pass covers).
    std::optional<BoxEdges> margin;
    std::optional<BoxEdges> padding;

    // Describes how future resolved-value changes to this widget should be animated.
    // Not itself a value to paint -- just read by StyleApplier when deciding animate vs
    // snap. A comma-separated transition list (e.g. `width 300ms ease-in-out, height
    // 300ms ease-in-out`) parses into more than one entry.
    std::optional<std::vector<TransitionSpec>> transition;

    // Visual-effects properties (Phase: SDF render pass). Snap-only -- intentionally never
    // wired into TransitionProperty, so they structurally can't be animated yet.
    std::optional<float> borderRadius;
    // Comma-separated multiple shadows (CSS allows a list); each is drawn as its own
    // shadow-only DrawRect call, see Widget::OnPaint.
    std::optional<std::vector<BoxShadow>> boxShadow;
    std::optional<LinearGradient> background; // distinct from backgroundColor; wins over it if set
    std::optional<BackdropFilter> backdropFilter;

    // Flexbox layout keywords -- pushed straight into the widget's Yoga node (not painted,
    // so not part of the animation system, same as padding).
    std::optional<FlexDirection> flexDirection;
    std::optional<JustifyContent> justifyContent;
    std::optional<AlignItems> alignItems;
    std::optional<Display> display;

    void MergeFrom(const StyleProperties& higherPriority);
};

} // namespace IzekipUI
