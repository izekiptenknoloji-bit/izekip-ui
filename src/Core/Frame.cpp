#include "Core/Frame.h"

#include <new>

#include "Platform/Diagnostics.h"

namespace IzekipUI {

FrameResult PaintFrame(Render::IRenderer& renderer, Widget& root) {
    try {
        renderer.BeginFrame();
        root.OnPaint(renderer);
        renderer.EndFrame();
        renderer.Present();
        Platform::Diagnostics::NoteHealthyFrame();
        return FrameResult::Presented;
    } catch (const std::bad_alloc&) {
        Platform::Diagnostics::EnterDegradedMode();
        // The renderer may be mid-frame: close it out so its state stays consistent, then present
        // whatever was drawn. A second allocation failure here is dropped; the next frame retries.
        try {
            renderer.EndFrame();
            renderer.Present();
        } catch (const std::bad_alloc&) {
        }
        return FrameResult::OutOfMemory;
    }
}

} // namespace IzekipUI
