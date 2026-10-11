#include "Widgets/Checkbox.h"

namespace IzekipUI::Widgets {

Checkbox::Checkbox(std::string text) : Widget("Checkbox") {
    // Row-with-centered-items defaults; CSS flex-direction/align-items still override them.
    YGNodeStyleSetFlexDirection(m_yogaNode, YGFlexDirectionRow);
    YGNodeStyleSetAlignItems(m_yogaNode, YGAlignCenter);

    m_box = std::make_shared<Panel>();
    m_box->AddClass("checkbox-box");
    m_mark = std::make_shared<Panel>();
    m_mark->AddClass("checkbox-mark");
    m_box->AddChild(m_mark);

    m_label = std::make_shared<Label>();
    m_label->AddClass("checkbox-label");
    m_label->SetText(std::move(text));

    AddChild(m_box);
    AddChild(m_label);
}

void Checkbox::SetChecked(bool checked) {
    m_checked = checked;
    SetClass("checked", checked);
}

void Checkbox::SetText(std::string text) {
    m_label->SetText(std::move(text));
}

void Checkbox::OnClicked() {
    SetChecked(!m_checked);
    if (m_onChanged) {
        m_onChanged(m_checked);
    }
    Widget::OnClicked();
}

} // namespace IzekipUI::Widgets
