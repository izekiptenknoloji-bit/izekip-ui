#include "Widgets/ProgressBar.h"

#include <algorithm>

namespace IzekipUI::Widgets {

ProgressBar::ProgressBar() : Widget("ProgressBar") {
    // Row direction so the default cross-axis stretch makes the fill full-height.
    YGNodeStyleSetFlexDirection(m_yogaNode, YGFlexDirectionRow);

    m_fill = std::make_shared<Panel>();
    m_fill->AddClass("progress-fill");
    AddChild(m_fill);
    SetValue(0.0f);
}

void ProgressBar::SetValue(float value01) {
    m_value = std::clamp(value01, 0.0f, 1.0f);
    m_fill->SnapWidth(Length{m_value * 100.0f, true});
}

} // namespace IzekipUI::Widgets
