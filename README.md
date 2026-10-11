# İzekip UI — A Modern C++ GUI Library

A from-scratch GPU-accelerated GUI library combining **real CSS styling** with **high-performance rendering** (D3D11 + Vulkan). Built as an easier-to-learn alternative to Qt, with a focus on web developers' familiar CSS paradigm.

## Key Features

- **Real CSS Styling**: Type selectors, class selectors, ID selectors, descendant combinators, pseudo-classes (`:hover`, `:active`)
- **CSS Parser**: Built on **lexbor** (standards-compliant CSS engine)
- **Flexbox Layout**: Powered by **Yoga** (Facebook's layout engine)
- **GPU Rendering**: 
  - **D3D11** (Windows) with runtime shader compilation
  - **Vulkan** (cross-platform) with SPIR-V compilation
- **SDF Text Rendering**: Smooth glyph rendering via `stb_truetype`
- **Visual Effects**: Border radius, box-shadow, linear gradients, backdrop filters
- **Smooth Animations**: CSS transitions with cubic-bezier easing
- **Keyframe Animations**: `@keyframes`, `animation`, and reduced-motion media queries
- **Opacity Support**: Full opacity property with animation support

## Supported CSS Properties

### Layout & Sizing
- `width`, `height` (px, %)
- `margin`, `padding` (1/2/3/4-value shorthands)
- `flex-direction`, `justify-content`, `align-items`

### Colors & Fills
- `background-color` (hex, rgb, rgba, named colors)
- `background` (linear-gradient only)
- `color` (text color)
- `font-family` (`sans-serif`, `serif`, `system-ui`, `monospace`; limited to bundled fonts)
- `border-color`
- `opacity` (0-1, animatable)
- `min-width`, `max-width`, `min-height`, `max-height` (px and %; `vh` for height)

### Visual Effects
- `border-radius` (SDF-based rounded corners)
- `box-shadow` (multi-shadow, with blur and spread)
- `backdrop-filter` (blur, brightness, contrast, saturate, grayscale, hue-rotate)

### Animations
- `transition` (comma-separated property list)
  - Supports: `background-color`, `width`, `height`, `margin`, `opacity`, `transform`
  - Custom easing: cubic-bezier functions
- Translation transforms: `translateX()`, `translateY()`, and `translate()`
- `@keyframes` animations: one named animation with a duration, named or
  `cubic-bezier()` easing, and optional `forwards` fill mode; animatable keyframe
  properties are `opacity` and translation-only `transform`
- `@media (prefers-reduced-motion: reduce)`: opt in to the query by setting
  `sheet.SetPrefersReducedMotion(true)` before applying the stylesheet. This is an
  application-provided preference; the library does not query the operating system.

## Building

### Requirements
- **CMake** 3.21+
- **C++20** compiler (MSVC 2026+, GCC 10+, Clang 10+)
- **Visual Studio 2026** (for D3D11 on Windows)
- **Vulkan SDK** 1.4+ (for Vulkan backend)

### D3D11 Build (Windows)
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 18 2026" -DIZEKIP_RENDER_BACKEND=D3D11
cmake --build . --config Release
```

### Vulkan Build (Cross-Platform)
```bash
mkdir build-vulkan
cd build-vulkan
cmake .. -DIZEKIP_RENDER_BACKEND=VULKAN
cmake --build . --config Release
```

## Starting a New Project

**Easiest:** download `IzekipUI-SDK-<version>-win64.zip`, extract it, run `IzekipUISetup.exe`
(a GUI installer written with İzekip UI itself), then in a new terminal:
`cssfr create myapp` → `cd myapp` → `cmake -B build` → `cmake --build build`. To build that
zip yourself: `cmake -DD3D11_BUILD_DIR=build -DVULKAN_BUILD_DIR=build-vulkan -P cmake/PackageSDK.cmake`.

**From source:** once IzekipUI is built (above), install it and scaffold a new project with `cssfr`:

```bash
cmake --install build --prefix /path/to/izekip-install
/path/to/izekip-install/bin/cssfr create myapp --izekip-path /path/to/izekip-install
cd myapp
cmake -B build
cmake --build build
```

This generates a `CMakeLists.txt` already wired for `find_package(IzekipUI)`, a minimal
`main.cpp`, and a starter `style.css` — see `cli/README.md` for details, and
`COMPATIBILITY.md` for what the installed package promises across future IzekipUI updates.

## Quick Start

```cpp
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Panel.h"
#include "CSS/StyleSheet.h"
#include "CSS/StyleApplier.h"

using namespace IzekipUI;

int main() {
    // Create window
    Platform::Window window({.title = "MyApp", .width = 800, .height = 600});
    
    // Initialize renderer
    auto renderer = Render::CreateRenderer(Render::RendererBackend::D3D11);
    Render::RendererCreateInfo info{
        .nativeWindowHandle = window.GetNativeHandle(),
        .glfwWindow = window.GetGLFWHandle(),
        .initialWidth = 800,
        .initialHeight = 600,
    };
    renderer->Initialize(info);
    
    // Build UI tree
    auto root = std::make_shared<Widgets::Panel>();
    root->SetId("root");
    root->AddClass("container");
    
    auto button = std::make_shared<Widgets::Button>();
    button->AddClass("primary");
    root->AddChild(button);
    
    // Load & apply CSS
    Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("style.css");
    Css::LexborBridge bridge;
    bridge.Build(*root);
    Css::StyleApplier::Apply(bridge, sheet, *root);
    
    // Layout & render loop
    root->CalculateLayout(800.0f, 600.0f);
    
    while (!window.ShouldClose()) {
        window.PollEvents();
        renderer->BeginFrame();
        root->OnPaint(*renderer);
        renderer->EndFrame();
        renderer->Present();
    }
    
    renderer->Shutdown();
    return 0;
}
```

### CSS Example (style.css)
```css
.container {
    background-color: #F5F5F5;
    width: 800px;
    height: 600px;
    padding: 20px;
}

Button.primary {
    background: linear-gradient(to right, #3366FF, #5588FF);
    border-radius: 8px;
    width: 120px;
    height: 40px;
    opacity: 1.0;
    transition: all 300ms ease-in-out;
}

Button.primary:hover {
    opacity: 0.8;
    box-shadow: 0px 6px 18px #00000070;
}
```

## Architecture

- **`include/Core/`** — Widget tree, style properties, animations, layout math
- **`include/CSS/`** — CSS parsing (lexbor bridge), style application, tokenizer
- **`include/Renderer/`** — Renderer abstraction, SDF drawing math, font atlasing
- **`src/Renderer/D3D11/`** — DirectX 11 backend implementation
- **`src/Renderer/Vulkan/`** — Vulkan backend implementation
- **`include/Widgets/`** — Built-in widgets (Panel, Button, Label)
- **`third_party/`** — Vendored dependencies (GLFW, Yoga, lexbor, stb_truetype)

## Roadmap

### Phase 6 ✅
- SDF text rendering via `stb_truetype`
- `Widgets::Label` with font sizing
- D3D11 + Vulkan texture/sampler support

### Phase 7 ✅ (Current)
- `Widgets::TextArea` (multi-line editable text entry)
- Real keyboard event handling (`Platform::Window::SetKeyCallback`/`SetCharCallback`)
- `:focus` pseudo-class
- Turkish character glyph support (ASCII + Latin-1 + 6 Latin Extended-A codepoints)

### Planned
- More widget types (Checkbox, Select, Slider)
- Text selection, clipboard, undo/redo in `TextArea`

### Future
- Full Unicode text support (currently ASCII + Latin-1 + Turkish-specific characters only)
- Text wrapping & multi-line for `Label` (currently explicit `\n` only)
- CSS Grid layout
- Position (absolute, relative, fixed)
- Z-index / layer ordering
- Display property (none, flex, grid)

## Examples

See `examples/01_basic_panel_button/` for a showcase of:
- Type selectors, class selectors, ID selectors
- CSS margins, padding, sizing
- Flexbox layout
- Border radius & box shadow
- Linear gradients
- Pseudo-classes (`:hover`, `:active`)
- CSS transitions & animations
- Opacity property

Run it:
```bash
cd build/examples/01_basic_panel_button/Release
./IzekipUIExample01.exe
```

## Testing

Unit tests verify CSS parsing and selector matching:
```bash
cd build/tests/Release
./IzekipUICssParserTests.exe
```

## Compatibility

İzekip UI follows semantic versioning starting at `1.0.0`. See `COMPATIBILITY.md` for what
that promises (source-level compatibility for the Tier-1 public headers listed there), what
it doesn't cover, and the automated `tools/api_diff.py` check that enforces it across
releases. See `CHANGELOG.md` for release history.

## License

This project is provided as-is for educational and commercial use. See individual dependency licenses in `third_party/`.

## Contributing

Pull requests welcome! Areas needing help:
- Additional CSS property support
- New widget types
- Performance optimization (batching, invalidation tracking)
- Cross-platform testing (Linux, macOS)

## Author

Created by [izekiptenknoloji](https://github.com/izekiptenknoloji-bit)

---

**Vision**: A single rendering layer producing pixel-identical, standards-compliant UI across Windows (D3D11) and Linux/macOS (Vulkan), with CSS familiarity lowering the barrier to entry compared to Qt or native UI toolkits.
