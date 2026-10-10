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
- `border-color`
- `opacity` (0-1, animatable)

### Visual Effects
- `border-radius` (SDF-based rounded corners)
- `box-shadow` (multi-shadow, with blur and spread)
- `backdrop-filter` (blur, brightness, contrast, saturate, grayscale)

### Animations
- `transition` (comma-separated property list)
  - Supports: `background-color`, `width`, `height`, `margin`, `opacity`
  - Custom easing: cubic-bezier functions

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

### Phase 6 ✅ (Current)
- SDF text rendering via `stb_truetype`
- `Widgets::Label` with font sizing
- D3D11 + Vulkan texture/sampler support

### Phase 7 (Planned)
- Input widget (text entry)
- Keyboard event handling
- Focus/blur pseudo-classes
- More widget types (Checkbox, Select, Slider)

### Future
- Unicode text support (currently ASCII 32-126)
- Text wrapping & multi-line
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
