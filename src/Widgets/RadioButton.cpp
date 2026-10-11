#include "Widgets/RadioButton.h"

namespace IzekipUI::Widgets {

RadioButton::RadioButton(std::string text) : Widget("RadioButton") {
    YGNodeStyleSetFlexDirection(m_yogaNode, YGFlexDirectionRow);
    YGNodeStyleSetAlignItems(m_yogaNode, YGAlignCenter);

    m_outer = std::make_shared<Panel>();
    m_outer->AddClass("radio-dot-outer");
    m_dot = std::make_shared<Panel>();
    m_dot->AddClass("radio-dot");
    m_outer->AddChild(m_dot);

    m_label = std::make_shared<Label>();
    m_label->AddClass("radio-label");
    m_label->SetText(std::move(text));

    AddChild(m_outer);
    AddChild(m_label);
}

void RadioButton::SetText(std::string text) {
    m_label->SetText(std::move(text));
}

void RadioButton::SetSelectedInternal(bool selected) {
    m_selected = selected;
    SetClass("checked", selected);
}

void RadioButton::OnClicked() {
    if (m_group) {
        m_group->Select(this);
    }
    Widget::OnClicked();
}

RadioGroup::~RadioGroup() {
    for (auto& button : m_buttons) {
        button->m_group = nullptr;
    }
}

void RadioGroup::Add(std::shared_ptr<RadioButton> button) {
    button->m_group = this;
    const bool first = m_buttons.empty();
    button->SetSelectedInternal(first);
    if (first) {
        m_selected = 0;
    }
    m_buttons.push_back(std::move(button));
}

void RadioGroup::Select(int index) {
    if (index < 0 || index >= GetCount() || index == m_selected) {
        return;
    }
    for (int i = 0; i < GetCount(); ++i) {
        m_buttons[static_cast<size_t>(i)]->SetSelectedInternal(i == index);
    }
    m_selected = index;
    if (m_onChanged) {
        m_onChanged(index);
    }
}

void RadioGroup::Select(RadioButton* button) {
    for (int i = 0; i < GetCount(); ++i) {
        if (m_buttons[static_cast<size_t>(i)].get() == button) {
            Select(i);
            return;
        }
    }
}

RadioButton* RadioGroup::GetButton(int index) const {
    if (index < 0 || index >= GetCount()) {
        return nullptr;
    }
    return m_buttons[static_cast<size_t>(index)].get();
}

} // namespace IzekipUI::Widgets
