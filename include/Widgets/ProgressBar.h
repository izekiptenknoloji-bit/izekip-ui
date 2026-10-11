#pragma once

#include <memory>

#include "Core/Widget.h"
#include "Widgets/Panel.h"

namespace IzekipUI::Widgets {

// A horizontal progress bar: the ProgressBar itself is the track, its single child
// Panel.progress-fill is the filled part, sized as a percentage width. Style both with CSS:
//
//   ProgressBar    { width: 400px; height: 10px; border-radius: 5px; background-color: #2A2F36; }
//   .progress-fill { border-radius: 5px; background-color: #3366FF; }
//
// Do NOT set `width` on .progress-fill in CSS -- the next StyleApplier::Apply would
// overwrite the value SetValue() wrote. The fill stretches to the bar's full height.
class ProgressBar : public Widget {
public:
    ProgressBar();

    // Clamped to [0, 1]. Only touches the Yoga node -- the caller must run
    // CalculateLayout() on the root afterwards for the new width to show.
    void SetValue(float value01);
    float GetValue() const { return m_value; }

private:
    std::shared_ptr<Panel> m_fill;
    float m_value = 0.0f;
};

} // namespace IzekipUI::Widgets
