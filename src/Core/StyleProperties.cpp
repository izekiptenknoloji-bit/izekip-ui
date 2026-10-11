#include "Core/StyleProperties.h"

namespace IzekipUI {

void StyleProperties::MergeFrom(const StyleProperties& higherPriority) {
    if (higherPriority.backgroundColor) backgroundColor = higherPriority.backgroundColor;
    if (higherPriority.textColor) textColor = higherPriority.textColor;
    if (higherPriority.borderColor) borderColor = higherPriority.borderColor;

    if (higherPriority.width) width = higherPriority.width;
    if (higherPriority.height) height = higherPriority.height;
    if (higherPriority.minWidth) minWidth = higherPriority.minWidth;
    if (higherPriority.maxWidth) maxWidth = higherPriority.maxWidth;
    if (higherPriority.minHeight) minHeight = higherPriority.minHeight;
    if (higherPriority.maxHeight) maxHeight = higherPriority.maxHeight;
    if (higherPriority.borderWidth) borderWidth = higherPriority.borderWidth;
    if (higherPriority.fontSize) fontSize = higherPriority.fontSize;
    if (higherPriority.fontFamily) fontFamily = higherPriority.fontFamily;
    if (higherPriority.fontWeight) fontWeight = higherPriority.fontWeight;
    if (higherPriority.cursor) cursor = higherPriority.cursor;
    if (higherPriority.scale) scale = higherPriority.scale;
    if (higherPriority.rotate) rotate = higherPriority.rotate;
    if (higherPriority.opacity) opacity = higherPriority.opacity;
    if (higherPriority.transform) transform = higherPriority.transform;
    if (higherPriority.animation) animation = higherPriority.animation;

    if (higherPriority.margin) margin = higherPriority.margin;
    if (higherPriority.padding) padding = higherPriority.padding;

    if (higherPriority.transition) transition = higherPriority.transition;

    if (higherPriority.borderRadius) borderRadius = higherPriority.borderRadius;
    if (higherPriority.boxShadow) boxShadow = higherPriority.boxShadow;
    if (higherPriority.background) background = higherPriority.background;
    if (higherPriority.backdropFilter) backdropFilter = higherPriority.backdropFilter;

    if (higherPriority.flexDirection) flexDirection = higherPriority.flexDirection;
    if (higherPriority.justifyContent) justifyContent = higherPriority.justifyContent;
    if (higherPriority.alignItems) alignItems = higherPriority.alignItems;
    if (higherPriority.display) display = higherPriority.display;
}

} // namespace IzekipUI
