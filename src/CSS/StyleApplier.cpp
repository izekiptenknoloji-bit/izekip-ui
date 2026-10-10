#include "CSS/StyleApplier.h"

#include <algorithm>
#include <cstdlib>
#include <optional>

#include "CSS/CssTokenizer.h"
#include "Core/Transition.h"

namespace IzekipUI::Css {

namespace {

std::optional<Color> ParseColor(const std::vector<Token>& tokens) {
    if (tokens.size() == 1 && tokens[0].type == TokenType::Hash) {
        return Color::FromHex(tokens[0].text);
    }
    return std::nullopt;
}

std::optional<float> ParseLength(const std::vector<Token>& tokens) {
    if (tokens.size() != 1) {
        return std::nullopt;
    }
    const Token& token = tokens[0];
    if (token.type == TokenType::Dimension) {
        // Only the "px" unit is supported; strip it and parse the number.
        std::string numeric = token.text;
        if (numeric.size() > 2 && numeric.substr(numeric.size() - 2) == "px") {
            numeric = numeric.substr(0, numeric.size() - 2);
        }
        return std::strtof(numeric.c_str(), nullptr);
    }
    if (token.type == TokenType::Number) {
        return std::strtof(token.text.c_str(), nullptr);
    }
    return std::nullopt;
}

// Standard CSS box-edge shorthand expansion: 1 value (all four edges), 2 values
// (top/bottom | left/right), 3 values (top | left/right | bottom), 4 values
// (top right bottom left). Now reachable for real since lexbor hands us the full raw
// value text regardless of how many components it has -- the old hand-rolled parser
// only ever saw single-value declarations.
std::optional<BoxEdges> ParseBoxEdges(const std::vector<Token>& tokens) {
    std::vector<float> values;
    values.reserve(tokens.size());
    for (const auto& token : tokens) {
        auto value = ParseLength({token});
        if (!value) {
            return std::nullopt;
        }
        values.push_back(*value);
    }

    switch (values.size()) {
        case 1: return BoxEdges{values[0], values[0], values[0], values[0]};
        case 2: return BoxEdges{values[0], values[1], values[0], values[1]};
        case 3: return BoxEdges{values[0], values[1], values[2], values[1]};
        case 4: return BoxEdges{values[0], values[1], values[2], values[3]};
        default: return std::nullopt;
    }
}

// Accepts "300ms" or "0.3s" (check the two-char "ms" suffix before the one-char "s"
// suffix, since "ms" also ends in "s"). Returns seconds.
std::optional<float> ParseDurationSeconds(const Token& token) {
    if (token.type != TokenType::Dimension) {
        return std::nullopt;
    }
    const std::string& text = token.text;
    if (text.size() > 2 && text.substr(text.size() - 2) == "ms") {
        return std::strtof(text.substr(0, text.size() - 2).c_str(), nullptr) / 1000.0f;
    }
    if (text.size() > 1 && text.substr(text.size() - 1) == "s") {
        return std::strtof(text.substr(0, text.size() - 1).c_str(), nullptr);
    }
    return std::nullopt;
}

// Scope: single-property form only (`transition: <prop> <duration> <easing>;` or
// `all ...`) -- no comma-separated multi-property lists, no custom cubic-bezier()
// syntax, only the five named easing keywords. Malformed or unrecognized input is
// ignored (returns nullopt), consistent with "unknown properties are ignored" below.
std::optional<TransitionSpec> ParseTransitionDeclaration(const std::vector<Token>& tokens) {
    if (tokens.size() != 3 || tokens[0].type != TokenType::Ident ||
        tokens[1].type != TokenType::Dimension || tokens[2].type != TokenType::Ident) {
        return std::nullopt;
    }

    TransitionSpec spec;
    const std::string& propertyName = tokens[0].text;
    if (propertyName == "all") spec.property = TransitionProperty::All;
    else if (propertyName == "background-color") spec.property = TransitionProperty::BackgroundColor;
    else if (propertyName == "width") spec.property = TransitionProperty::Width;
    else if (propertyName == "height") spec.property = TransitionProperty::Height;
    else if (propertyName == "margin") spec.property = TransitionProperty::Margin;
    else return std::nullopt;

    auto duration = ParseDurationSeconds(tokens[1]);
    if (!duration) {
        return std::nullopt;
    }
    spec.durationSeconds = *duration;

    if (!TryGetNamedEasing(tokens[2].text, spec.easing)) {
        return std::nullopt;
    }

    return spec;
}

// Fixed 4-token form only: <offsetX> <offsetY> <blurRadius> <color>. No comma-separated
// multiple shadows, no "inset" keyword.
std::optional<BoxShadow> ParseBoxShadow(const std::vector<Token>& tokens) {
    if (tokens.size() != 4) {
        return std::nullopt;
    }
    auto offsetX = ParseLength({tokens[0]});
    auto offsetY = ParseLength({tokens[1]});
    auto blur = ParseLength({tokens[2]});
    auto color = ParseColor({tokens[3]});
    if (!offsetX || !offsetY || !blur || !color) {
        return std::nullopt;
    }
    return BoxShadow{*offsetX, *offsetY, *blur, *color};
}

// Accepts ONLY: linear-gradient '(' 'to' <right|left|top|bottom> ',' <color> ',' <color> ')'.
// Exactly 2 stops, 4 cardinal directions -- no angle syntax, no percentage stop positions.
std::optional<LinearGradient> ParseLinearGradient(const std::vector<Token>& tokens) {
    if (tokens.size() != 9) {
        return std::nullopt;
    }
    if (tokens[0].type != TokenType::Ident || tokens[0].text != "linear-gradient") return std::nullopt;
    if (tokens[1].type != TokenType::ParenOpen) return std::nullopt;
    if (tokens[2].type != TokenType::Ident || tokens[2].text != "to") return std::nullopt;
    if (tokens[3].type != TokenType::Ident) return std::nullopt;

    GradientDirection direction;
    const std::string& dir = tokens[3].text;
    if (dir == "right") direction = GradientDirection::ToRight;
    else if (dir == "left") direction = GradientDirection::ToLeft;
    else if (dir == "top") direction = GradientDirection::ToTop;
    else if (dir == "bottom") direction = GradientDirection::ToBottom;
    else return std::nullopt;

    if (tokens[4].type != TokenType::Comma) return std::nullopt;
    auto start = ParseColor({tokens[5]});
    if (!start || tokens[6].type != TokenType::Comma) return std::nullopt;
    auto end = ParseColor({tokens[7]});
    if (!end || tokens[8].type != TokenType::ParenClose) return std::nullopt;

    return LinearGradient{direction, *start, *end};
}

std::optional<FlexDirection> ParseFlexDirection(const std::vector<Token>& tokens) {
    if (tokens.size() != 1 || tokens[0].type != TokenType::Ident) return std::nullopt;
    const std::string& v = tokens[0].text;
    if (v == "row") return FlexDirection::Row;
    if (v == "column") return FlexDirection::Column;
    if (v == "row-reverse") return FlexDirection::RowReverse;
    if (v == "column-reverse") return FlexDirection::ColumnReverse;
    return std::nullopt;
}

std::optional<JustifyContent> ParseJustifyContent(const std::vector<Token>& tokens) {
    if (tokens.size() != 1 || tokens[0].type != TokenType::Ident) return std::nullopt;
    const std::string& v = tokens[0].text;
    if (v == "flex-start") return JustifyContent::FlexStart;
    if (v == "flex-end") return JustifyContent::FlexEnd;
    if (v == "center") return JustifyContent::Center;
    if (v == "space-between") return JustifyContent::SpaceBetween;
    if (v == "space-around") return JustifyContent::SpaceAround;
    return std::nullopt;
}

std::optional<AlignItems> ParseAlignItems(const std::vector<Token>& tokens) {
    if (tokens.size() != 1 || tokens[0].type != TokenType::Ident) return std::nullopt;
    const std::string& v = tokens[0].text;
    if (v == "flex-start") return AlignItems::FlexStart;
    if (v == "flex-end") return AlignItems::FlexEnd;
    if (v == "center") return AlignItems::Center;
    if (v == "stretch") return AlignItems::Stretch;
    return std::nullopt;
}

StyleProperties DeclarationToStyleProperties(const MatchedDeclaration& decl) {
    StyleProperties props;
    std::vector<Token> tokens = Tokenizer(decl.rawValueText).Tokenize();
    if (!tokens.empty() && tokens.back().type == TokenType::EndOfFile) {
        tokens.pop_back();
    }

    if (decl.propertyName == "background-color") {
        props.backgroundColor = ParseColor(tokens);
    } else if (decl.propertyName == "color") {
        props.textColor = ParseColor(tokens);
    } else if (decl.propertyName == "border-color") {
        props.borderColor = ParseColor(tokens);
    } else if (decl.propertyName == "width") {
        props.width = ParseLength(tokens);
    } else if (decl.propertyName == "height") {
        props.height = ParseLength(tokens);
    } else if (decl.propertyName == "border-width") {
        props.borderWidth = ParseLength(tokens);
    } else if (decl.propertyName == "font-size") {
        props.fontSize = ParseLength(tokens);
    } else if (decl.propertyName == "margin") {
        props.margin = ParseBoxEdges(tokens);
    } else if (decl.propertyName == "padding") {
        props.padding = ParseBoxEdges(tokens);
    } else if (decl.propertyName == "transition") {
        props.transition = ParseTransitionDeclaration(tokens);
    } else if (decl.propertyName == "border-radius") {
        props.borderRadius = ParseLength(tokens);
    } else if (decl.propertyName == "box-shadow") {
        props.boxShadow = ParseBoxShadow(tokens);
    } else if (decl.propertyName == "background") {
        props.background = ParseLinearGradient(tokens);
    } else if (decl.propertyName == "flex-direction") {
        props.flexDirection = ParseFlexDirection(tokens);
    } else if (decl.propertyName == "justify-content") {
        props.justifyContent = ParseJustifyContent(tokens);
    } else if (decl.propertyName == "align-items") {
        props.alignItems = ParseAlignItems(tokens);
    }
    // Unknown properties are ignored -- lexbor parses the full real CSS property set
    // syntactically, but IzekipUI only acts on the properties it implements above.

    return props;
}

void PushFlexPropsToYoga(YGNodeRef node, const StyleProperties& props) {
    if (props.flexDirection) {
        switch (*props.flexDirection) {
            case FlexDirection::Row: YGNodeStyleSetFlexDirection(node, YGFlexDirectionRow); break;
            case FlexDirection::Column: YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumn); break;
            case FlexDirection::RowReverse:
                YGNodeStyleSetFlexDirection(node, YGFlexDirectionRowReverse);
                break;
            case FlexDirection::ColumnReverse:
                YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumnReverse);
                break;
        }
    }
    if (props.justifyContent) {
        switch (*props.justifyContent) {
            case JustifyContent::FlexStart: YGNodeStyleSetJustifyContent(node, YGJustifyFlexStart); break;
            case JustifyContent::FlexEnd: YGNodeStyleSetJustifyContent(node, YGJustifyFlexEnd); break;
            case JustifyContent::Center: YGNodeStyleSetJustifyContent(node, YGJustifyCenter); break;
            case JustifyContent::SpaceBetween:
                YGNodeStyleSetJustifyContent(node, YGJustifySpaceBetween);
                break;
            case JustifyContent::SpaceAround:
                YGNodeStyleSetJustifyContent(node, YGJustifySpaceAround);
                break;
        }
    }
    if (props.alignItems) {
        switch (*props.alignItems) {
            case AlignItems::FlexStart: YGNodeStyleSetAlignItems(node, YGAlignFlexStart); break;
            case AlignItems::FlexEnd: YGNodeStyleSetAlignItems(node, YGAlignFlexEnd); break;
            case AlignItems::Center: YGNodeStyleSetAlignItems(node, YGAlignCenter); break;
            case AlignItems::Stretch: YGNodeStyleSetAlignItems(node, YGAlignStretch); break;
        }
    }
}

} // namespace

void StyleApplier::ApplyToSingleWidget(LexborBridge& bridge, const StyleSheet& sheet, Widget& widget) {
    bridge.SyncPseudoState(widget);
    std::vector<MatchedDeclaration> matched = bridge.MatchedDeclarationsFor(sheet, widget);

    std::stable_sort(matched.begin(), matched.end(),
                      [](const MatchedDeclaration& a, const MatchedDeclaration& b) {
                          if (a.specificity != b.specificity) return a.specificity < b.specificity;
                          return a.sourceOrder < b.sourceOrder;
                      });

    // Resolve the full current cascade into a local accumulator first -- this is the
    // diff baseline, never written straight into the widget.
    StyleProperties resolved;
    for (const auto& decl : matched) {
        resolved.MergeFrom(DeclarationToStyleProperties(decl));
    }

    const StyleProperties& previous = widget.GetTargetStyle();
    const bool firstApply = !widget.HasBeenStyled();
    const TransitionSpec* transition = resolved.transition ? &*resolved.transition : nullptr;

    if (resolved.backgroundColor) {
        const bool changed = !firstApply &&
                              (!previous.backgroundColor || *previous.backgroundColor != *resolved.backgroundColor);
        if (changed && transition && transition->CoversProperty(TransitionProperty::BackgroundColor)) {
            widget.AnimateBackgroundColorTo(*resolved.backgroundColor, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapBackgroundColor(*resolved.backgroundColor);
        }
    }

    if (resolved.width) {
        const bool changed = !firstApply && (!previous.width || *previous.width != *resolved.width);
        if (changed && transition && transition->CoversProperty(TransitionProperty::Width)) {
            widget.AnimateWidthTo(*resolved.width, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapWidth(*resolved.width);
        }
    }

    if (resolved.height) {
        const bool changed = !firstApply && (!previous.height || *previous.height != *resolved.height);
        if (changed && transition && transition->CoversProperty(TransitionProperty::Height)) {
            widget.AnimateHeightTo(*resolved.height, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapHeight(*resolved.height);
        }
    }

    if (resolved.margin) {
        const bool changed = !firstApply && (!previous.margin || *previous.margin != *resolved.margin);
        if (changed && transition && transition->CoversProperty(TransitionProperty::Margin)) {
            widget.AnimateMarginTo(*resolved.margin, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapMargin(*resolved.margin);
        }
    }

    // Remaining fields are never animated -- merge them straight in.
    StyleProperties nonAnimatedFields = resolved;
    nonAnimatedFields.backgroundColor.reset();
    nonAnimatedFields.width.reset();
    nonAnimatedFields.height.reset();
    nonAnimatedFields.margin.reset();
    widget.GetStyle().MergeFrom(nonAnimatedFields);

    YGNodeRef node = widget.GetYogaNode();
    if (resolved.padding) {
        YGNodeStyleSetPadding(node, YGEdgeTop, resolved.padding->top);
        YGNodeStyleSetPadding(node, YGEdgeRight, resolved.padding->right);
        YGNodeStyleSetPadding(node, YGEdgeBottom, resolved.padding->bottom);
        YGNodeStyleSetPadding(node, YGEdgeLeft, resolved.padding->left);
    }
    PushFlexPropsToYoga(node, resolved);

    widget.SetTargetStyle(resolved);
    widget.MarkStyled();
}

void StyleApplier::Apply(LexborBridge& bridge, const StyleSheet& sheet, Widget& root) {
    ApplyToSingleWidget(bridge, sheet, root);
    for (const auto& child : root.GetChildren()) {
        Apply(bridge, sheet, *child);
    }
}

} // namespace IzekipUI::Css
