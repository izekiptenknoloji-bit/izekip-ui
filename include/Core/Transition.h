#pragma once

#include <vector>

#include "Core/Easing.h"

namespace IzekipUI {

enum class TransitionProperty { All, BackgroundColor, Width, Height, Margin, Opacity };

// Describes how a widget should animate future changes to its resolved style -- this is
// not itself a trigger, it is read whenever StyleApplier notices a resolved value changed
// (e.g. because :hover started matching) to decide whether to animate or snap.
struct TransitionSpec {
    TransitionProperty property = TransitionProperty::All;
    float durationSeconds = 0.0f;
    CubicBezier easing{};

    bool CoversProperty(TransitionProperty prop) const {
        return durationSeconds > 0.0f && (property == TransitionProperty::All || property == prop);
    }
};

// `transition` is a comma-separated list (e.g. `width 300ms ease-in-out, height 300ms
// ease-in-out`), each entry covering its own property -- this finds the first entry (if
// any) that applies to `prop` and would therefore animate a resolved-value change on it.
inline const TransitionSpec* FindCoveringTransition(const std::vector<TransitionSpec>& specs,
                                                      TransitionProperty prop) {
    for (const auto& spec : specs) {
        if (spec.CoversProperty(prop)) {
            return &spec;
        }
    }
    return nullptr;
}

} // namespace IzekipUI
