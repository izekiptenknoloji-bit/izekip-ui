#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Core/Widget.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"

namespace IzekipUI::Widgets {

class RadioGroup;

// One option of a mutually-exclusive RadioGroup. Composite: Panel.radio-dot-outer
// (containing Panel.radio-dot), then Label.radio-label. The selected state is a `checked`
// CSS class (same name Checkbox uses), e.g.:
//
//   .radio-dot-outer { width: 18px; height: 18px; border-radius: 9px; background-color: #2A2F36;
//                      justify-content: center; align-items: center; margin: 0px 10px 0px 0px; }
//   .radio-dot       { width: 8px; height: 8px; border-radius: 4px; background-color: #3366FF;
//                      display: none; }
//   RadioButton.checked .radio-dot { display: flex; }
//
// Clicking selects it within its group (requires InteractionController or Click()).
class RadioButton : public Widget {
public:
    explicit RadioButton(std::string text = {});

    bool IsSelected() const { return m_selected; }
    void SetText(std::string text);
    Label& GetLabel() { return *m_label; }

protected:
    bool HandlesClick() const override { return true; }
    void OnClicked() override;

private:
    friend class RadioGroup;
    void SetSelectedInternal(bool selected);

    std::shared_ptr<Panel> m_outer;
    std::shared_ptr<Panel> m_dot;
    std::shared_ptr<Label> m_label;
    RadioGroup* m_group = nullptr;
    bool m_selected = false;
};

// Not a widget -- an app-owned selection model. It must outlive any click on its buttons
// (it detaches them in its destructor). The first button added becomes selected.
class RadioGroup {
public:
    RadioGroup() = default;
    ~RadioGroup();
    RadioGroup(const RadioGroup&) = delete;
    RadioGroup& operator=(const RadioGroup&) = delete;

    void Add(std::shared_ptr<RadioButton> button);
    void Select(int index);              // no-op if already selected; fires onChanged otherwise
    void Select(RadioButton* button);
    int GetSelectedIndex() const { return m_selected; }
    RadioButton* GetButton(int index) const;
    int GetCount() const { return static_cast<int>(m_buttons.size()); }

    void SetOnChanged(std::function<void(int index)> onChanged) { m_onChanged = std::move(onChanged); }

private:
    std::vector<std::shared_ptr<RadioButton>> m_buttons;
    int m_selected = -1;
    std::function<void(int)> m_onChanged;
};

} // namespace IzekipUI::Widgets
