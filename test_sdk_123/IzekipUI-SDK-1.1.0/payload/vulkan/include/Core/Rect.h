#pragma once

namespace IzekipUI {

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct BoxEdges {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    bool operator==(const BoxEdges& other) const {
        return top == other.top && right == other.right && bottom == other.bottom &&
               left == other.left;
    }
    bool operator!=(const BoxEdges& other) const { return !(*this == other); }
};

} // namespace IzekipUI
