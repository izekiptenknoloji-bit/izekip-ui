#pragma once

#include "Core/Easing.h"

namespace IzekipUI {

enum class TransitionProperty { All, BackgroundColor, Width, Height, Margin };

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

} // namespace IzekipUI
