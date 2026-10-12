#pragma once

#include "Renderer/IRenderer.h"
#include "Core/Widget.h"

namespace IzekipUI {

enum class FrameResult {
    Presented,   // the whole frame was painted and presented
    OutOfMemory  // std::bad_alloc during painting: the partial frame was presented, degraded mode is on
};

// Runs one frame (BeginFrame, OnPaint, EndFrame, Present). An out-of-memory failure during painting
// is caught here, at the frame boundary, instead of unwinding into the host application: the renderer
// is closed out and the partial frame presented, and the library enters degraded mode.
FrameResult PaintFrame(Render::IRenderer& renderer, Widget& root);

} // namespace IzekipUI
