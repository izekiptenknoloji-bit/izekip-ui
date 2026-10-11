#include "CSS/StyleApplier.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "CSS/CssTokenizer.h"
#include "Core/Transition.h"

namespace IzekipUI::Css {

namespace {

// adapted from litehtml's web_color.cpp parse_hash_color() (BSD license,
// https://github.com/litehtml/litehtml) -- 3/4-digit hex doubling (each digit repeated)
// before handing off to Color::FromHex, which only understands the 6/8-digit form.
std::optional<Color> ParseHexColor(const Token& token) {
    if (token.type != TokenType::Hash) {
        return std::nullopt;
    }
    const std::string& s = token.text;
    if (s.size() == 3 || s.size() == 4) {
        std::string expanded;
        for (char c : s) {
            expanded += c;
            expanded += c;
        }
        return Color::FromHex(expanded);
    }
    if (s.size() == 6 || s.size() == 8) {
        return Color::FromHex(s);
    }
    return std::nullopt;
}

// adapted from litehtml's web_color.cpp parse_rgb_func() (BSD license,
// https://github.com/litehtml/litehtml) -- legacy comma-separated syntax only, <number>
// components (0-255). <percentage> components and the modern slash-alpha syntax are out
// of scope for this pass (see the CSS value-parser expansion plan's named scope cuts).
std::optional<Color> ParseRgbFunction(const std::vector<Token>& tokens) {
    const bool isRgba = tokens.size() == 10; // rgba( r , g , b , a )
    const bool isRgb = tokens.size() == 8;   // rgb( r , g , b )
    if (!isRgb && !isRgba) {
        return std::nullopt;
    }
    if (tokens[0].type != TokenType::Ident || tokens[0].text != (isRgb ? "rgb" : "rgba")) {
        return std::nullopt;
    }
    if (tokens[1].type != TokenType::ParenOpen) return std::nullopt;
    if (tokens[2].type != TokenType::Number) return std::nullopt;
    if (tokens[3].type != TokenType::Comma) return std::nullopt;
    if (tokens[4].type != TokenType::Number) return std::nullopt;
    if (tokens[5].type != TokenType::Comma) return std::nullopt;
    if (tokens[6].type != TokenType::Number) return std::nullopt;

    const float r = std::strtof(tokens[2].text.c_str(), nullptr) / 255.0f;
    const float g = std::strtof(tokens[4].text.c_str(), nullptr) / 255.0f;
    const float b = std::strtof(tokens[6].text.c_str(), nullptr) / 255.0f;

    if (isRgb) {
        if (tokens[7].type != TokenType::ParenClose) return std::nullopt;
        return Color{r, g, b, 1.0f};
    }

    if (tokens[7].type != TokenType::Comma) return std::nullopt;
    if (tokens[8].type != TokenType::Number) return std::nullopt;
    if (tokens[9].type != TokenType::ParenClose) return std::nullopt;
    const float a = std::strtof(tokens[8].text.c_str(), nullptr);
    return Color{r, g, b, a};
}

// Small, practical named-color table -- deliberately NOT litehtml's full 140+-entry
// g_def_colors[] list, just the names common in real-world CSS. Extend here if a specific
// name turns out to be needed.
std::optional<Color> ParseNamedColor(const std::string& name) {
    static const std::unordered_map<std::string, Color> kNamedColors = {
        {"red", Color{1.0f, 0.0f, 0.0f, 1.0f}},
        {"green", Color{0.0f, 0.5019608f, 0.0f, 1.0f}},
        {"blue", Color{0.0f, 0.0f, 1.0f, 1.0f}},
        {"black", Color{0.0f, 0.0f, 0.0f, 1.0f}},
        {"white", Color{1.0f, 1.0f, 1.0f, 1.0f}},
        {"gray", Color{0.5019608f, 0.5019608f, 0.5019608f, 1.0f}},
        {"transparent", Color{0.0f, 0.0f, 0.0f, 0.0f}},
        {"orange", Color{1.0f, 0.6470588f, 0.0f, 1.0f}},
        {"yellow", Color{1.0f, 1.0f, 0.0f, 1.0f}},
        {"purple", Color{0.5019608f, 0.0f, 0.5019608f, 1.0f}},
    };
    const auto it = kNamedColors.find(name);
    if (it == kNamedColors.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Color> ParseColor(const std::vector<Token>& tokens) {
    if (tokens.size() == 1 && tokens[0].type == TokenType::Hash) {
        return ParseHexColor(tokens[0]);
    }
    if (auto fn = ParseRgbFunction(tokens)) {
        return fn;
    }
    if (tokens.size() == 1 && tokens[0].type == TokenType::Ident) {
        return ParseNamedColor(tokens[0].text);
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

// Like ParseLength, but also accepts a <percentage> token and tags the result so
// StyleApplier knows which Yoga setter (SetWidth vs SetWidthPercent) to call -- Yoga
// itself resolves the percentage against the parent's size, no math lives here.
std::optional<Length> ParseCssLength(const std::vector<Token>& tokens) {
    if (tokens.size() != 1) {
        return std::nullopt;
    }
    if (tokens[0].type == TokenType::Percentage) {
        const std::string& text = tokens[0].text; // e.g. "50%"
        const float value = std::strtof(text.substr(0, text.size() - 1).c_str(), nullptr);
        return Length{value, true};
    }
    if (auto px = ParseLength(tokens)) {
        return Length{*px, false};
    }
    return std::nullopt;
}

// A CSS <number> or <percentage> used as a filter-function amount (e.g. brightness(1.2)
// or brightness(120%) mean the same thing) -- both forms normalize to the same float.
std::optional<float> ParseFilterAmount(const std::vector<Token>& tokens) {
    if (tokens.size() != 1) {
        return std::nullopt;
    }
    if (tokens[0].type == TokenType::Number) {
        return std::strtof(tokens[0].text.c_str(), nullptr);
    }
    if (tokens[0].type == TokenType::Percentage) {
        const std::string& text = tokens[0].text;
        return std::strtof(text.substr(0, text.size() - 1).c_str(), nullptr) / 100.0f;
    }
    return std::nullopt;
}

// backdrop-filter: <filter-function># -- a SPACE-separated list of function calls (not
// comma-separated, so SplitOnCommas doesn't apply here -- there are no top-level commas
// at all). Reads consecutive `Ident ParenOpen ... ParenClose` groups, tracking paren
// depth so a function's own nested parens (none of our supported functions have any, but
// future ones might) wouldn't be misread as the group's end.
std::optional<BackdropFilter> ParseBackdropFilter(const std::vector<Token>& tokens) {
    BackdropFilter result;
    bool anyRecognized = false;
    size_t i = 0;
    while (i < tokens.size()) {
        if (tokens[i].type != TokenType::Ident) {
            return std::nullopt;
        }
        const std::string name = tokens[i].text;
        if (i + 1 >= tokens.size() || tokens[i + 1].type != TokenType::ParenOpen) {
            return std::nullopt;
        }

        size_t depth = 1;
        size_t j = i + 2;
        const size_t argsStart = j;
        while (j < tokens.size() && depth > 0) {
            if (tokens[j].type == TokenType::ParenOpen) {
                ++depth;
            } else if (tokens[j].type == TokenType::ParenClose) {
                --depth;
            }
            if (depth > 0) {
                ++j;
            }
        }
        if (j >= tokens.size()) {
            return std::nullopt; // unterminated function call
        }
        const std::vector<Token> args(tokens.begin() + static_cast<long>(argsStart),
                                       tokens.begin() + static_cast<long>(j));

        if (name == "blur") {
            if (auto px = ParseLength(args)) {
                result.blurRadiusPx = *px;
                anyRecognized = true;
            }
        } else if (name == "brightness" || name == "contrast" || name == "saturate" ||
                   name == "grayscale") {
            if (auto amount = ParseFilterAmount(args)) {
                if (name == "brightness") result.brightness = *amount;
                else if (name == "contrast") result.contrast = *amount;
                else if (name == "saturate") result.saturate = *amount;
                else result.grayscale = *amount;
                anyRecognized = true;
            }
        }
        // Unrecognized function names (hue-rotate, invert, drop-shadow, opacity, ...) are
        // skipped individually -- the rest of the list still applies (e.g.
        // `blur(10px) hue-rotate(90deg)` still gets its blur).

        i = j + 1;
    }
    return anyRecognized ? std::optional<BackdropFilter>(result) : std::nullopt;
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

// Splits a token list on top-level commas only -- a comma nested inside a function call
// (e.g. the color-component separators in `rgb(51, 102, 255)`) doesn't count, tracked via
// paren depth. Shared by the transition list, the box-shadow list, and the gradient stop
// list.
std::vector<std::vector<Token>> SplitOnCommas(const std::vector<Token>& tokens) {
    std::vector<std::vector<Token>> segments;
    std::vector<Token> current;
    int depth = 0;
    for (const auto& token : tokens) {
        if (token.type == TokenType::ParenOpen) {
            ++depth;
        } else if (token.type == TokenType::ParenClose) {
            --depth;
        }
        if (token.type == TokenType::Comma && depth == 0) {
            segments.push_back(std::move(current));
            current.clear();
            continue;
        }
        current.push_back(token);
    }
    segments.push_back(std::move(current));
    return segments;
}

// Scope: `<prop> <duration> <easing>` per comma-separated segment (or `all ...`) -- no
// custom cubic-bezier() syntax, only the five named easing keywords. Malformed or
// unrecognized input is ignored (returns nullopt), consistent with "unknown properties
// are ignored" below.
std::optional<TransitionSpec> ParseTransitionSegment(const std::vector<Token>& tokens) {
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

// transition: <single-transition># -- a comma-separated list of transitions (e.g.
// `transition: width 300ms ease-in-out, height 300ms ease-in-out;`), previously only a
// single segment was accepted.
std::optional<std::vector<TransitionSpec>> ParseTransitionDeclaration(const std::vector<Token>& tokens) {
    std::vector<TransitionSpec> specs;
    for (auto& segment : SplitOnCommas(tokens)) {
        auto spec = ParseTransitionSegment(segment);
        if (!spec) {
            return std::nullopt;
        }
        specs.push_back(*spec);
    }
    if (specs.empty()) {
        return std::nullopt;
    }
    return specs;
}

// Tries to parse a trailing <color> off the back of `tokens`, trying the widest colors
// first (an rgb()/rgba() function call) down to the narrowest (a single hex/named-color
// token), so a function call's own tokens are never mistaken for something else.
std::optional<std::pair<Color, size_t>> ParseTrailingColor(const std::vector<Token>& tokens) {
    for (size_t len : {size_t{10}, size_t{8}, size_t{1}}) {
        if (tokens.size() < len) {
            continue;
        }
        const std::vector<Token> tail(tokens.end() - static_cast<long>(len), tokens.end());
        if (auto color = ParseColor(tail)) {
            return std::make_pair(*color, len);
        }
    }
    return std::nullopt;
}

// Single <shadow> = inset? && <length>{2,4} && <color>? -- per the CSS Backgrounds and
// Borders Level 3 grammar (not adapted from litehtml, which has no box-shadow support at
// all -- see the Phase plan). <length>{2,4} is offsetX, offsetY, [blurRadius, [spreadRadius]].
std::optional<BoxShadow> ParseBoxShadowSegment(std::vector<Token> tokens) {
    bool inset = false;
    if (!tokens.empty() && tokens.front().type == TokenType::Ident && tokens.front().text == "inset") {
        inset = true;
        tokens.erase(tokens.begin());
    }

    Color color{0.0f, 0.0f, 0.0f, 1.0f}; // CSS default is currentColor; black approximates it
    if (auto trailing = ParseTrailingColor(tokens)) {
        color = trailing->first;
        tokens.resize(tokens.size() - trailing->second);
    }

    std::vector<float> lengths;
    lengths.reserve(tokens.size());
    for (const auto& token : tokens) {
        auto value = ParseLength({token});
        if (!value) {
            return std::nullopt;
        }
        lengths.push_back(*value);
    }
    if (lengths.size() < 2 || lengths.size() > 4) {
        return std::nullopt;
    }

    BoxShadow shadow;
    shadow.offsetX = lengths[0];
    shadow.offsetY = lengths[1];
    shadow.blurRadius = lengths.size() >= 3 ? lengths[2] : 0.0f;
    shadow.spreadRadius = lengths.size() >= 4 ? lengths[3] : 0.0f;
    shadow.inset = inset;
    shadow.color = color;
    return shadow;
}

// box-shadow: <shadow>#  -- a comma-separated list of one or more shadows.
std::optional<std::vector<BoxShadow>> ParseBoxShadow(const std::vector<Token>& tokens) {
    std::vector<BoxShadow> shadows;
    for (auto& segment : SplitOnCommas(tokens)) {
        auto shadow = ParseBoxShadowSegment(std::move(segment));
        if (!shadow) {
            return std::nullopt;
        }
        shadows.push_back(*shadow);
    }
    if (shadows.empty()) {
        return std::nullopt;
    }
    return shadows;
}

// adapted from litehtml's gradient.cpp parse_color_stop() (BSD license,
// https://github.com/litehtml/litehtml), reduced to our supported subset: <color>
// <percentage>? only -- no bare <length> stop positions (litehtml supports both).
std::optional<GradientStop> ParseGradientStopSegment(const std::vector<Token>& tokens) {
    if (tokens.empty() || tokens.size() > 2) {
        return std::nullopt;
    }
    // Try widest-to-narrowest color so an rgb()/rgba() function's own tokens are never
    // mistaken for something else (same approach as ParseTrailingColor).
    for (size_t colorLen : {size_t{10}, size_t{8}, size_t{1}}) {
        if (tokens.size() < colorLen) {
            continue;
        }
        const std::vector<Token> colorTokens(tokens.begin(), tokens.begin() + static_cast<long>(colorLen));
        auto color = ParseColor(colorTokens);
        if (!color) {
            continue;
        }
        if (colorLen == tokens.size()) {
            return GradientStop{*color, std::nullopt};
        }
        if (tokens.size() - colorLen == 1 && tokens.back().type == TokenType::Percentage) {
            const std::string& text = tokens.back().text;
            const float percent = std::strtof(text.substr(0, text.size() - 1).c_str(), nullptr);
            return GradientStop{*color, percent};
        }
        return std::nullopt;
    }
    return std::nullopt;
}

// linear-gradient( to <right|left|top|bottom>, <linear-color-stop># ) -- 4 cardinal
// directions only (no angle syntax), 2 or more comma-separated stops (previously fixed
// at exactly 2). Stop position resolution (percent -> [0,1], auto-spacing, and the
// render-time cap at 3 stops) happens in Widget::OnPaint, not here -- this function only
// parses the CSS text into a direction + an ordered stop list.
std::optional<LinearGradient> ParseLinearGradient(const std::vector<Token>& tokens) {
    if (tokens.size() < 9) {
        return std::nullopt;
    }
    if (tokens[0].type != TokenType::Ident || tokens[0].text != "linear-gradient") return std::nullopt;
    if (tokens[1].type != TokenType::ParenOpen) return std::nullopt;
    if (tokens.back().type != TokenType::ParenClose) return std::nullopt;
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

    const std::vector<Token> stopTokens(tokens.begin() + 5, tokens.end() - 1);
    std::vector<GradientStop> stops;
    for (auto& segment : SplitOnCommas(stopTokens)) {
        auto stop = ParseGradientStopSegment(segment);
        if (!stop) {
            return std::nullopt;
        }
        stops.push_back(*stop);
    }
    if (stops.size() < 2) {
        return std::nullopt;
    }

    return LinearGradient{direction, std::move(stops)};
}

// border: <border-width> <border-style> <border-color> -- a fixed-order reduction of the
// real CSS shorthand (which allows any order and lets each component be omitted). Not
// adapted from litehtml, which dispatches shorthands through a declarative property
// table rather than a single copyable function -- this is written against the CSS
// Backgrounds and Borders Level 3 grammar directly. border-style has no StyleProperties
// field yet (no drawing code distinguishes solid/dashed/dotted), so the style keyword is
// only validated, not stored.
std::optional<std::pair<float, Color>> ParseBorderShorthand(const std::vector<Token>& tokens) {
    if (tokens.size() != 3) {
        return std::nullopt;
    }
    auto width = ParseLength({tokens[0]});
    if (!width) {
        return std::nullopt;
    }
    if (tokens[1].type != TokenType::Ident) {
        return std::nullopt;
    }
    static const std::unordered_map<std::string, bool> kKnownBorderStyles = {
        {"solid", true}, {"dashed", true}, {"dotted", true}, {"none", true},
    };
    if (kKnownBorderStyles.find(tokens[1].text) == kKnownBorderStyles.end()) {
        return std::nullopt;
    }
    auto color = ParseColor({tokens[2]});
    if (!color) {
        return std::nullopt;
    }
    return std::make_pair(*width, *color);
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

std::optional<Display> ParseDisplay(const std::vector<Token>& tokens) {
    if (tokens.size() != 1 || tokens[0].type != TokenType::Ident) return std::nullopt;
    const std::string& v = tokens[0].text;
    if (v == "flex") return Display::Flex;
    if (v == "none") return Display::None;
    return std::nullopt; // block/grid/inline/... are not implemented -- ignored
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
    } else if (decl.propertyName == "border") {
        if (auto parsed = ParseBorderShorthand(tokens)) {
            props.borderWidth = parsed->first;
            props.borderColor = parsed->second;
        }
    } else if (decl.propertyName == "width") {
        props.width = ParseCssLength(tokens);
    } else if (decl.propertyName == "height") {
        props.height = ParseCssLength(tokens);
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
    } else if (decl.propertyName == "backdrop-filter") {
        props.backdropFilter = ParseBackdropFilter(tokens);
    } else if (decl.propertyName == "flex-direction") {
        props.flexDirection = ParseFlexDirection(tokens);
    } else if (decl.propertyName == "justify-content") {
        props.justifyContent = ParseJustifyContent(tokens);
    } else if (decl.propertyName == "align-items") {
        props.alignItems = ParseAlignItems(tokens);
    } else if (decl.propertyName == "display") {
        props.display = ParseDisplay(tokens);
    } else if (decl.propertyName == "opacity") {
        if (auto opacity = ParseFilterAmount(tokens)) {
            props.opacity = std::clamp(*opacity, 0.0f, 1.0f);
        }
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
    // `transition` is now a comma-separated list (e.g. `width 300ms ease-in-out, height
    // 300ms ease-in-out`) -- each property below finds its OWN covering entry rather than
    // sharing a single one, since different properties can be covered by different
    // segments of the same declaration.
    static const std::vector<TransitionSpec> kNoTransitions;
    const std::vector<TransitionSpec>& transitions = resolved.transition ? *resolved.transition : kNoTransitions;

    if (resolved.backgroundColor) {
        const bool changed = !firstApply &&
                              (!previous.backgroundColor || *previous.backgroundColor != *resolved.backgroundColor);
        const TransitionSpec* transition =
            changed ? FindCoveringTransition(transitions, TransitionProperty::BackgroundColor) : nullptr;
        if (transition) {
            widget.AnimateBackgroundColorTo(*resolved.backgroundColor, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapBackgroundColor(*resolved.backgroundColor);
        }
    }

    if (resolved.width) {
        const bool changed = !firstApply && (!previous.width || *previous.width != *resolved.width);
        const TransitionSpec* transition =
            changed ? FindCoveringTransition(transitions, TransitionProperty::Width) : nullptr;
        if (transition) {
            widget.AnimateWidthTo(*resolved.width, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapWidth(*resolved.width);
        }
    }

    if (resolved.height) {
        const bool changed = !firstApply && (!previous.height || *previous.height != *resolved.height);
        const TransitionSpec* transition =
            changed ? FindCoveringTransition(transitions, TransitionProperty::Height) : nullptr;
        if (transition) {
            widget.AnimateHeightTo(*resolved.height, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapHeight(*resolved.height);
        }
    }

    if (resolved.margin) {
        const bool changed = !firstApply && (!previous.margin || *previous.margin != *resolved.margin);
        const TransitionSpec* transition =
            changed ? FindCoveringTransition(transitions, TransitionProperty::Margin) : nullptr;
        if (transition) {
            widget.AnimateMarginTo(*resolved.margin, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapMargin(*resolved.margin);
        }
    }

    if (resolved.opacity) {
        const bool changed = !firstApply && (!previous.opacity || *previous.opacity != *resolved.opacity);
        const TransitionSpec* transition =
            changed ? FindCoveringTransition(transitions, TransitionProperty::Opacity) : nullptr;
        if (transition) {
            widget.AnimateOpacityTo(*resolved.opacity, transition->durationSeconds, transition->easing);
        } else {
            widget.SnapOpacity(*resolved.opacity);
        }
    }

    // Remaining fields are never animated -- merge them straight in.
    StyleProperties nonAnimatedFields = resolved;
    nonAnimatedFields.backgroundColor.reset();
    nonAnimatedFields.width.reset();
    nonAnimatedFields.height.reset();
    nonAnimatedFields.margin.reset();
    nonAnimatedFields.opacity.reset();
    widget.GetStyle().MergeFrom(nonAnimatedFields);

    YGNodeRef node = widget.GetYogaNode();

    // `display` is ALWAYS written (resolved value or the default `flex`), unlike the
    // merge-if-set fields above: the cascade is additive, so once a `.hidden { display:
    // none }` class is removed, `resolved.display` is simply unset -- merging would leave the
    // stale `none` on both m_style and the Yoga node forever. This is the single write site.
    const Display display = resolved.display.value_or(Display::Flex);
    widget.GetStyle().display = display;
    YGNodeStyleSetDisplay(node, display == Display::None ? YGDisplayNone : YGDisplayFlex);

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
