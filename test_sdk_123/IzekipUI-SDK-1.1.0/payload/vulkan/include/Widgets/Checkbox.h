#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Core/Widget.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"

namespace IzekipUI::Widgets {

// A labeled checkbox. Composite: Panel.checkbox-box (containing Panel.checkbox-mark), then
// Label.checkbox-label. The checked state is expressed as a `checked` CSS class on the
// Checkbox itself, so every visual is CSS-driven, e.g.:
//
//   .checkbox-box  { width: 18px; height: 18px; border-radius: 4px; background-color: #2A2F36;
//                    justify-content: center; align-items: center; margin: 0px 10px 0px 0px; }
//   .checkbox-mark { width: 10px; height: 10px; border-radius: 2px; background-color: #3366FF;
//                    display: none; }
//   Checkbox.checked .checkbox-mark { display: flex; }
//
// Clicking anywhere on it (box or label) toggles it -- requires an InteractionController
// (or a manual Click() call). Lays out as a row with centered items unless CSS overrides it.
class Checkbox : public Widget {
public:
    explicit Checkbox(std::string text = {});

    // Sets/clears the `checked` class. Does NOT fire onChanged (programmatic change).
    void SetChecked(bool checked);
    bool IsChecked() const { return m_checked; }

    void SetText(std::string text);
    Label& GetLabel() { return *m_label; }

    // Fired after a user (or Click()) toggle, with the new state.
    void SetOnChanged(std::function<void(bool checked)> onChanged) { m_onChanged = std::move(onChanged); }

protected:
    bool HandlesClick() const override { return true; }
    void OnClicked() override;

private:
    std::shared_ptr<Panel> m_box;
    std::shared_ptr<Panel> m_mark;
    std::shared_ptr<Label> m_label;
    std::function<void(bool)> m_onChanged;
    bool m_checked = false;
};

} // namespace IzekipUI::Widgets
