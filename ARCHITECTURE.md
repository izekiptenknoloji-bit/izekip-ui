# İzekip UI — API & Architecture Reference

Complete API reference for building applications with İzekip UI. This document is optimized for AI-assisted code generation (GitHub Copilot, Claude).

## Core Concepts

### 1. Widget Tree
Every UI is a tree of widgets. Root widget contains children, which can contain their own children.

```cpp
auto root = std::make_shared<Widgets::Panel>();
auto button = std::make_shared<Widgets::Button>();
root->AddChild(button);
```

### 2. CSS Styling
Widgets are styled via CSS files loaded at runtime. CSS cascades and applies via selectors.

```css
/* style.css */
Button {
    background-color: #FF6B6B;
    width: 120px;
    height: 40px;
}

Button.primary {
    background: linear-gradient(to right, #3366FF, #5588FF);
}

Button:hover {
    opacity: 0.8;
}
```

### 3. Layout System (Yoga)
Flexbox layout is automatic. Set flex properties in CSS.

```css
Panel {
    flex-direction: row;      /* or column */
    justify-content: center;  /* flex-start, flex-end, space-between, space-around */
    align-items: center;      /* flex-start, flex-end, stretch */
    padding: 20px;
}
```

### 4. Event Handling
Mouse events are handled via hover/active pseudo-classes.

```cpp
// In C++ code, update :hover/:active by checking mouse position
Widget* hitWidget = root->HitTest(mouseX, mouseY);
if (hitWidget != previousHitWidget) {
    ClearHoveredRecursive(*root);
    if (hitWidget) {
        MarkHoveredChain(hitWidget, true);
    }
    Css::StyleApplier::Apply(bridge, sheet, *root);
}
```

## Widget Types

### Panel
Basic container widget. No visual behavior except styling.

```cpp
auto panel = std::make_shared<Widgets::Panel>();
panel->SetId("mainPanel");
panel->AddClass("container");
root->AddChild(panel);
```

**CSS Properties:**
- All layout: `width`, `height`, `margin`, `padding`, `flex-*`
- All visual: `background-color`, `border-radius`, `box-shadow`, `opacity`

---

### Button
Interactive button widget. Responds to :hover and :active pseudo-classes.

```cpp
auto button = std::make_shared<Widgets::Button>();
button->AddClass("primary");
button->AddClass("large");
root->AddChild(button);
```

**CSS Properties:**
- Same as Panel
- Best with transitions for smooth hover effects

**CSS Example:**
```css
Button.primary {
    background: linear-gradient(to right, #3366FF, #5588FF);
    width: 120px;
    height: 40px;
    transition: all 300ms ease-in-out;
}

Button.primary:hover {
    opacity: 0.8;
    box-shadow: 0px 6px 18px #00000070;
}

Button.primary:active {
    box-shadow: 0px 2px 6px #00000055;
}
```

---

### Label
Text rendering widget. Displays a single line of text with font styling.

```cpp
auto label = std::make_shared<Widgets::Label>();
label->SetText("Hello, İzekip UI!");
label->AddClass("title");
root->AddChild(label);
```

**CSS Properties:**
- `color` — text color
- `font-size` — text size in pixels
- `opacity` — text transparency

**C++ API:**
```cpp
label->SetText(std::string text);
label->SetFontFamily(Render::FontId::Proportional);  // or Monospace
```

**CSS Example:**
```css
Label.title {
    color: #222222;
    font-size: 24px;
    font-weight: bold;  /* Not yet supported */
}

Label.code {
    color: #7FDBAA;
    font-size: 13px;
}
```

---

## CSS Property Reference

### Sizing
| Property | Values | Example |
|----------|--------|---------|
| `width` | `<length>` or `<percentage>` | `width: 400px;` or `width: 50%;` |
| `height` | `<length>` or `<percentage>` | `height: 300px;` or `height: 100%;` |

### Spacing
| Property | Values | Example |
|----------|--------|---------|
| `margin` | 1-4 values | `margin: 10px;` or `margin: 10px 20px;` or `margin: 10px 20px 30px 40px;` |
| `padding` | 1-4 values | `padding: 20px;` or `padding: 10px 15px;` |

### Colors & Fills
| Property | Values | Example |
|----------|--------|---------|
| `background-color` | hex, rgb, rgba, named | `background-color: #FF6B6B;` or `rgba(255, 0, 0, 0.5)` |
| `background` | linear-gradient | `background: linear-gradient(to right, #3366FF, #5588FF);` |
| `color` | (text color) | `color: #222222;` |
| `border-color` | hex, rgb, rgba, named | `border-color: #FF0000;` |
| `opacity` | 0-1 | `opacity: 0.8;` |

### Visual Effects
| Property | Values | Example |
|----------|--------|---------|
| `border-radius` | `<length>` | `border-radius: 8px;` |
| `box-shadow` | `offsetX offsetY blurRadius spreadRadius color` | `box-shadow: 0px 4px 12px #00000055;` |
| `backdrop-filter` | blur, brightness, contrast, saturate, grayscale | `backdrop-filter: blur(10px) brightness(1.1);` |

### Flexbox Layout
| Property | Values | Example |
|----------|--------|---------|
| `flex-direction` | row, column, row-reverse, column-reverse | `flex-direction: row;` |
| `justify-content` | flex-start, flex-end, center, space-between, space-around | `justify-content: center;` |
| `align-items` | flex-start, flex-end, center, stretch | `align-items: center;` |

### Animations
| Property | Values | Example |
|----------|--------|---------|
| `transition` | `<property> <duration> <easing>` | `transition: opacity 300ms ease-in-out;` |

**Animatable Properties:**
- `background-color`
- `width`
- `height`
- `margin`
- `opacity`

**Easing Functions:**
- `linear`
- `ease-in`, `ease-out`, `ease-in-out`
- `cubic-bezier(x1, y1, x2, y2)`

---

## Complete Application Template

```cpp
#include <chrono>
#include <iostream>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Button.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"

using namespace IzekipUI;

int main() {
    // === WINDOW SETUP ===
    Platform::WindowDesc windowDesc;
    windowDesc.title = "My Application";
    windowDesc.width = 1000;
    windowDesc.height = 700;
    Platform::Window window(windowDesc);

    // === RENDERER SETUP ===
    auto renderer = Render::CreateRenderer(Render::GetCompiledInBackend());
    
    Render::RendererCreateInfo rendererInfo;
    rendererInfo.nativeWindowHandle = window.GetNativeHandle();
    rendererInfo.glfwWindow = window.GetGLFWHandle();
    rendererInfo.initialWidth = window.GetWidth();
    rendererInfo.initialHeight = window.GetHeight();
    renderer->Initialize(rendererInfo);

    window.SetResizeCallback([&](uint32_t width, uint32_t height) {
        renderer->Resize(width, height);
    });

    // === MOUSE INPUT ===
    double mouseX = 0.0, mouseY = 0.0;
    window.SetCursorPosCallback([&](double x, double y) {
        mouseX = x;
        mouseY = y;
    });

    bool leftButtonDown = false;
    window.SetMouseButtonCallback([&](Platform::MouseButton button, Platform::MouseAction action) {
        if (button == Platform::MouseButton::Left) {
            leftButtonDown = (action == Platform::MouseAction::Press);
        }
    });

    // === BUILD WIDGET TREE ===
    auto root = std::make_shared<Widgets::Panel>();
    root->SetId("root");
    root->AddClass("container");

    // Add your widgets here
    auto title = std::make_shared<Widgets::Label>();
    title->SetText("Welcome to İzekip UI");
    title->AddClass("title");
    root->AddChild(title);

    auto button = std::make_shared<Widgets::Button>();
    button->AddClass("primary");
    root->AddChild(button);

    // === LOAD CSS ===
    Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("style.css");
    std::cout << "[App] Loaded " << sheet.RuleCount() << " CSS rules\n";

    // === APPLY STYLES ===
    Css::LexborBridge bridge;
    bridge.Build(*root);
    Css::StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(static_cast<float>(window.GetWidth()),
                          static_cast<float>(window.GetHeight()));

    // === MAIN LOOP ===
    Widget* hoveredWidget = nullptr;
    bool activeState = false;
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (!window.ShouldClose()) {
        window.PollEvents();

        // Update hover state
        Widget* hitWidget = root->HitTest(static_cast<float>(mouseX), static_cast<float>(mouseY));
        bool needsReapply = false;

        if (hitWidget != hoveredWidget) {
            ClearHoveredRecursive(*root);
            if (hitWidget) {
                MarkHoveredChain(hitWidget, true);
            }
            hoveredWidget = hitWidget;
            needsReapply = true;
        }

        // Update active state
        const bool wantActive = leftButtonDown && hoveredWidget != nullptr;
        if (wantActive != activeState) {
            ClearActiveRecursive(*root);
            if (wantActive) {
                MarkActiveChain(hoveredWidget, true);
            }
            activeState = wantActive;
            needsReapply = true;
        }

        // Reapply CSS if needed
        if (needsReapply) {
            Css::StyleApplier::Apply(bridge, sheet, *root);
        }

        // Tick animations
        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        const AnimationTickResult tickResult = TickWidgetTreeAnimations(*root, deltaSeconds);
        if (needsReapply || tickResult.layoutAffected) {
            root->CalculateLayout(static_cast<float>(window.GetWidth()),
                                  static_cast<float>(window.GetHeight()));
        }

        // Render
        renderer->BeginFrame();
        root->OnPaint(*renderer);
        renderer->EndFrame();
        renderer->Present();
    }

    renderer->Shutdown();
    return 0;
}
```

## Common Patterns

### Pattern 1: Responsive Grid
```css
Panel.grid {
    flex-direction: row;
    flex-wrap: wrap;  /* Not yet supported */
}

Panel.grid-item {
    width: 200px;
    height: 200px;
    margin: 10px;
}
```

### Pattern 2: Animated Button
```css
Button {
    opacity: 1.0;
    transition: all 200ms ease-in-out;
}

Button:hover {
    opacity: 0.9;
    transform: scale(1.05);  /* Not yet supported */
}

Button:active {
    opacity: 0.7;
}
```

### Pattern 3: Form Layout
```css
.form {
    flex-direction: column;
    padding: 20px;
}

.form-group {
    flex-direction: column;
    margin: 0px 0px 16px 0px;
}

Label.form-label {
    color: #222222;
    font-size: 14px;
    margin: 0px 0px 8px 0px;
}

Input.form-input {
    width: 100%;
    height: 36px;
    padding: 8px 12px;
    border: 1px solid #CCCCCC;
    border-radius: 4px;
}
```

### Pattern 4: Color Transitions
```css
Panel.card {
    background-color: #FFFFFF;
    transition: all 300ms ease-in-out;
}

Panel.card:hover {
    background-color: #F5F5F5;
    box-shadow: 0px 4px 12px #00000020;
}
```

## Performance Tips

1. **Minimize Reapplies** — Only call `StyleApplier::Apply()` when needed (hover/active changes)
2. **Batch Animations** — Multiple properties animate in one pass via `TickWidgetTreeAnimations()`
3. **Use Gradients Sparingly** — Multiple stops are capped at 3, only 2-stop is ideal
4. **Box-shadow Blur Radius** — Clamped to 20px max (expensive GPU operation)

## Known Limitations

- **Keyboard input exists, but only inside `Widgets::TextArea`** — `Platform::Window` has
  real `SetKeyCallback`/`SetCharCallback` plumbing and `Widget` has a real `SetFocused`/
  `IsFocused`/`:focus` concept (see `examples/03_notepad/`), but no other widget reads
  keyboard input — there's no keyboard-driven focus traversal (Tab does nothing), no
  keyboard activation of `Button`, etc.
- **Single-line Text** — still true for `Label` (no word-wrap; `\n`-only line breaks).
  `TextArea` is multi-line but has no scroll/clip (overflowing text is drawn anyway) and no
  word-wrap either — same `\n`-only convention.
- **Mostly ASCII** — the glyph atlas now also bakes Latin-1 Supplement (32-0xFF, so é à ñ ç ö ü
  etc. render) plus 6 explicit Latin Extended-A codepoints for Turkish (ş Ş ğ Ğ ı İ,
  `GlyphAtlas::kExtendedCodepoints`) — but no other non-ASCII script/alphabet, and no other
  Latin Extended-A letter (e.g. Czech/Polish/Croatian-specific ones) is baked. Both `Label`
  and `TextArea` decode UTF-8 to codepoints (shared internal `src/Core/Utf8.h`), so
  everything the atlas bakes renders in either widget.
- **No text selection, clipboard, undo/redo, or click-to-position-cursor** in `TextArea` —
  v1 is type/Backspace/Delete/Enter/arrows + click-to-focus only (cursor jumps to
  end-of-document on focus, not to the clicked character). Named scope cuts, not bugs.
- **No Z-index** — Widgets render in tree order
- **No Absolute Positioning** — Flexbox only
- **No CSS Grid** — Flexbox only
- **Static Fonts** — Only Inter (proportional) and Space Mono (monospace)

## Debugging

```cpp
// Print CSS rule count
std::cout << "CSS Rules: " << sheet.RuleCount() << std::endl;

// Check widget properties
std::cout << "Widget ID: " << widget->GetId() << std::endl;
std::cout << "Widget Classes: ";
for (const auto& cls : widget->GetClasses()) {
    std::cout << cls << " ";
}
std::cout << std::endl;

// Check computed style
const auto& style = widget->GetStyle();
if (style.backgroundColor) {
    std::cout << "BG Color: " << style.backgroundColor->a << std::endl;
}
```

## Next Steps

- See `examples/01_basic_panel_button/` for a complete working showcase
- Read `README.md` for build instructions
- Check individual header files in `include/` for detailed API docs

---

## Internal Architecture (for engine contributors)

Everything above this line is the *consumer* API (how to build an app with İzekip UI).
This section is for an agent that will modify İzekip UI itself — adding CSS properties,
hardening existing ones, or touching the renderer.

### Module layout

```
include/Core/        Color, Rect, StyleProperties, Widget, Easing, Transition, Interpolate,
                      TextBuffer (platform/renderer-independent multi-line edit model)
include/CSS/         CssTokenizer, LexborBridge, StyleApplier, StyleSheet
include/Renderer/    IRenderer, RectDrawData, GlyphAtlas
include/Platform/    Window (GLFW wrapper; mouse + keyboard callbacks)
include/Widgets/     Panel, Button, Label (thin Widget subclasses), TextArea (editable,
                      owns a TextBuffer, the one widget with real keyboard-driven behavior)

src/Core/, src/CSS/, src/Platform/, src/Widgets/   -- mirror include/, implementation only
src/Renderer/
  RectDrawData.cpp        CPU-side draw-data math, SHARED by both backends
  RHI.cpp                 CreateRenderer()/GetCompiledInBackend() factory
  GlyphAtlas.cpp          stb_truetype glyph baking (Inter + Space Mono; ASCII + Latin-1 +
                          6 Turkish Latin Extended-A codepoints, see kExtendedCodepoints)
  Shaders/RectShader.hlsl SINGLE shader source for both backends (see below)
  D3D11/D3D11Renderer.*   D3D11 backend (Windows-only)
  Vulkan/VulkanRenderer.* Vulkan backend (cross-platform)

third_party/   glfw, yoga (flexbox), lexbor (CSS grammar parser), stb (stb_truetype), fonts
tests/CssParserTests.cpp     CSS parsing correctness, no renderer tests
tests/TextBufferTests.cpp    TextBuffer logic (insert/delete/move/UTF-8 decode), no
                              renderer/GLFW dependency -- the only other unit-testable layer
```

### Build system

- CMake 3.21+, C++20. `cmake/Options.cmake` defines `IZEKIP_RENDER_BACKEND` (`D3D11` or
  `VULKAN`, cache var) → generates `generated/Renderer/RenderBackendConfig.h` with
  `IZEKIP_RENDER_BACKEND_D3D11`/`IZEKIP_RENDER_BACKEND_VULKAN` macros. Only ONE backend is
  compiled into a given build — there is no runtime backend switch.
- Vulkan builds additionally `find_package(Vulkan REQUIRED COMPONENTS dxc)` — `dxc.exe`
  compiles `RectShader.hlsl` → SPIR-V at **build time** (CMake custom command), D3D11 instead
  compiles it at **runtime** via `D3DCompile`. Any shader edit must stay valid for both compilers.
- Conventionally two parallel build trees exist side by side: `build/` (D3D11) and
  `build-vulkan/` (Vulkan), each configured once with `-DIZEKIP_RENDER_BACKEND=...`. Always
  rebuild+retest BOTH after any renderer or shader change — the backends diverge in real,
  easy-to-miss ways (see "Known backend asymmetries" below).
- `third_party/{glfw,yoga,lexbor}` are built as CMake subdirectories (not package-manager
  deps); `stb` and `fonts` are header/data-only.

### CSS pipeline (text → applied style)

`CssTokenizer` (hand-rolled, used only for sub-parsing already-extracted declaration values,
e.g. inside `StyleApplier`'s function-argument parsers) + **lexbor** (real CSS grammar parser,
handles selectors/cascade/at-rules at the document level) → `StyleSheet` (parsed rule list) →
`LexborBridge` (maps lexbor's DOM-like tree onto the `Widget` tree so lexbor's selector engine
can match against it) → `StyleApplier::Apply(bridge, sheet, widget)`:
1. For each widget, find all rules whose selector matches (type/`.class`/`#id`, descendant
   combinator, `:hover`/`:active` pseudo-classes — no other combinators/pseudo-classes yet).
2. Sort by CSS specificity, low → high.
3. For each matching rule's declarations, call `DeclarationToStyleProperties` to build a
   `StyleProperties` with only the fields that rule set (everything else `std::nullopt`).
4. `StyleProperties::MergeFrom(higherPriority)` folds each in turn — a set field always
   overwrites, an unset (`nullopt`) field never clobbers a lower-priority value already merged.
   **Every new CSS property added to `StyleProperties` MUST get a `MergeFrom` line** — this is
   the single most common way to silently add a property that parses but never cascades right.
5. Recognized-but-malformed or entirely-unknown property names/values are silently ignored —
   there is no error/warning channel for authoring mistakes. (Candidate hardening item — see
   below.)

Adding a new CSS property means touching exactly 3 places: a field in `StyleProperties`
(+ `MergeFrom` line), a parse branch in `StyleApplier.cpp`'s `DeclarationToStyleProperties`,
and whatever consumes the field (`Widget::OnPaint` for most visual properties, `Widget`'s Yoga
node setup for layout properties).

### Rendering pipeline

`IRenderer` is the only renderer-facing interface `Widget::OnPaint` calls through — it knows
nothing about D3D11/Vulkan. Both backends share:
- **One HLSL source** (`RectShader.hlsl`) with mode flags in the constant buffer
  (`textParams.x`, `backdropParams.x`) selecting which `PSMain` branch runs — there is
  deliberately only ONE pipeline/PSO per backend, not one per draw-call kind.
- **One CPU-side draw-data layer** (`RectDrawData.cpp`) that turns an `IRenderer`-level paint
  struct (`RectPaint`, `BackdropBlurPaint`, etc.) into the exact `VSConstants`/`PSConstants`
  byte layout the shader expects. If you add a shader feature, the matching `Compute*DrawData`
  function is where its C++-side math lives — never duplicate that math inside a backend's
  `.cpp`.
- Three texture/sampler slots exist today: font atlas (proportional), font atlas (monospace),
  backdrop-filter scratch texture. A 4th would need a new descriptor-set-layout binding pair on
  Vulkan (`CreateDescriptorSetLayoutAndPool`) and a new `t`/`s` register pair on D3D11.

**D3D11 specifics**: immediate-context command ordering is the only synchronization that
exists — no explicit barriers. Any GPU resource a draw call reads/writes must be a real member
variable if it needs to survive past the function that created it (see `m_backBufferTexture`,
added specifically so `CopySubresourceRegion` could read it).

**Vulkan specifics** — the harder backend, has constraints D3D11 doesn't:
- `vkCmdCopyImage`/transfer commands cannot run inside an active render pass. The backdrop-
  filter feature solved this with a second `VkRenderPass` (`m_renderPassResume`, `loadOp=LOAD`)
  and an end→copy→barrier→resume sequence, reusing the same framebuffers and pipeline (render-
  pass *compatibility* only depends on attachment format/sample-count/layout-in-reference, not
  load/store ops — this is the trick to remember if another copy-from-framebuffer feature is
  ever needed).
- Pipeline/vertex-buffer/viewport/scissor bindings do **not** survive an end+begin-render-pass
  pair — `BindFrameDrawState()` exists specifically to be re-callable after any render-pass
  resume, and any future render-pass split must call it again.
- `m_swapchainExtent` (queried live from `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`) is the
  only authoritative surface size — see "Known backend asymmetries" below for why `m_width`/
  `m_height` are NOT safe to use for anything that must byte-match a real image's dimensions.

### Interaction, class mutation and `display` (since 1.1.0)

- **Live classes.** `LexborBridge::Build()` mirrors the widget tree into a lexbor shadow DOM
  once. `SyncPseudoState` (called per widget on every `StyleApplier::Apply()`) rewrites the
  hover/active/focus flags AND, since 1.1.0, the `class` attribute -- so
  `AddClass`/`RemoveClass`/`SetClass` after `Build()` take effect on the next Apply. The
  shadow tree's *structure* (children, ids) is still frozen at `Build()`.
- **The cascade is additive.** Apply only writes properties some matching rule sets; when a
  class that set a property is removed, nothing un-sets it. `display` is the one exception:
  it is written on every Apply (resolved value or default `flex`) to both `m_style` and the
  Yoga node -- single write site in `ApplyToSingleWidget`. For everything else, CSS authors
  must give every state-overridden property a base value (`installer/assets/setup.css`
  follows this rule).
- **Hidden widgets.** Yoga zeroes a `display: none` subtree's layout, but `Widget::OnPaint`
  and `HitTest` additionally skip hidden *children* in the parent's loop. It has to be the
  parent: `Label`/`TextArea` override `OnPaint` and draw glyphs after calling the base, so a
  check inside the child would still let their text through. Hiding the root itself is
  unsupported.
- **InteractionController** (`include/Core/InteractionController.h`) replaces the per-example
  hover/active boilerplate. It hit-tests every `Update()` (so a page switch updates hover
  without mouse movement), keeps press/release events in arrival order, fires a click when the
  release lands inside the pressed clickable widget's subtree (nearest ancestor-or-self with
  `IsClickable()`), gives focus to the nearest focusable ancestor on press, and fires queued
  clicks from `Update()` -- never from inside GLFW callbacks -- so handlers can change classes
  and text freely. `Update()` returning true means: re-run Apply + CalculateLayout. Examples
  01-04 still use their hand-rolled loops (not migrated).
- **Composite widgets** (`Checkbox`, `RadioButton`, `ProgressBar`) build their own child
  widgets in the constructor and express state as a `checked` class / a percentage width, so
  their look is entirely CSS. Click behavior comes from the protected `HandlesClick()` /
  `OnClicked()` hooks on `Widget`.

### The SDK installer (`installer/`)

Not part of the library: a separate executable target that consumes İzekip UI like any app,
built only when `IZEKIP_BUILD_INSTALLER=ON` on Windows with the D3D11 backend. All Win32
calls (registry, environment + `WM_SETTINGCHANGE`, `SHGetKnownFolderPath`, `IShellLinkW`,
`ShellExecuteExW "runas"`, `IFileOpenDialog`) live in `installer/WinSys.cpp` and nowhere
else. `InstallEngine` is UI- and thread-free (the GUI steps it with a ~12 ms budget per
frame; `--headless` loops it). The manifest it writes drives uninstall. Because the library
loads fonts/shaders/CSS relative to the current directory, the installer pins its CWD to its
own folder at startup (an uninstaller launched from Apps & features otherwise runs with
CWD = System32). An exe-relative asset lookup inside the library itself is still a scope cut.

### Known backend asymmetries / unfixed issues (candidates for "hardening")

- ~~`Platform::Window::GetWidth()/GetHeight()` can lie on Vulkan~~ — **fixed.**
  `Window`'s constructor now calls `glfwGetFramebufferSize()` right after
  `glfwCreateWindow()` and stores the *actual* (possibly OS-clamped) size into
  `m_width`/`m_height`, instead of trusting `desc.width/height` outright. Previously, if the
  OS/compositor clamped the window at creation time (e.g. requested height taller than the
  screen), `FramebufferSizeCallback` — the only other thing that ever corrected
  `m_width`/`m_height` — wasn't registered yet to catch it, since it's installed after
  `glfwCreateWindow()` returns. D3D11 never showed this (DXGI swap chains honor the requested
  size regardless of monitor bounds in windowed mode), which is why it only surfaced as a
  Vulkan `vkCmdCopyImage` dimension-mismatch validation error during the backdrop-filter work.
  The Vulkan backend's own `m_swapchainExtent` (queried independently via
  `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`) remains the authoritative size for anything
  swapchain-image-related and was intentionally left as-is — this fix just makes
  `Window::GetWidth()/GetHeight()` honest too, for every other caller (layout, hit-testing,
  `RendererCreateInfo::initialWidth/Height`, etc.) that was never wrong on Vulkan's swapchain
  specifically but was still reading a value that could diverge from reality.
- Unknown/malformed CSS declarations are silently dropped with no diagnostic — fine for a
  showcase, a liability for anyone debugging their own stylesheet. A hardening pass could add
  an optional parse-warning callback/log without changing default (silent) behavior.
- `tests/CssParserTests.cpp` only covers CSS parsing. There is no automated test of renderer
  output (pixel comparison, etc.) — regressions in `RectDrawData.cpp`/the shaders are currently
  only caught by eyeballing the example app on both backends.
- **`Widget` has no `RemoveClass()`/`RemoveChild()`.** `AddClass`/`AddChild` are append-only
  (`GetClasses()` even returns a `const&`, so the caller can't mutate the vector itself).
  Found while building `examples/02_text_document/`: a runtime theme toggle (e.g. a "dark
  mode" button) can't be done cleanly with the current API — poking `GetStyle()` directly
  works once, but the next `StyleApplier::Apply()` call (triggered by any hover/active change
  elsewhere) recomputes the widget's style from the CSS cascade from scratch and silently
  overwrites the manual change. Candidate hardening: either a real `RemoveClass`/`ToggleClass`
  pair, or a documented "sticky" style layer `Apply()` is guaranteed not to clobber.
- **No absolute positioning or z-index** means the only way for one widget to visually
  overlap another is a negative margin combined with paint (tree) order — there is no
  explicit "draw this on top of that" mechanism. Works (see `examples/02_text_document/`'s
  `.callout`, deliberately overlapped this way to test backdrop-filter against real painted
  text), but it is fragile: the overlap amount has to be hand-tuned against sibling content
  height, and any later content-length change silently changes how much overlap happens.
- ~~Glyph atlas is ASCII-only (32-126)~~ — **mostly fixed.** `GlyphAtlas::kLastChar` is now
  0xFF (bakes Latin-1 Supplement too: ç ö ü Ç Ö Ü é à ñ etc.), plus 6 explicit Latin
  Extended-A codepoints (`kExtendedCodepoints`: ş Ş ğ Ğ ı İ) baked in a second pass and
  looked up via a short linear scan in `FindGlyph` when a codepoint falls outside
  `[kFirstChar, kLastChar]` — deliberately NOT a wide contiguous bake up to that range (see
  `GlyphAtlas.h`'s doc comment: a comprehensive font like `Inter.ttf` likely has real glyphs
  for most of Latin Extended-A, serving languages nobody asked for, risking the atlas
  overflow). Atlas widened 512→1024px wide to fit the extra Latin-1 glyphs. `FindGlyph`/
  `GetKerning` now take `char32_t`, not `char`; `Label` was later switched from byte
  iteration to UTF-8-decoded codepoints too, so Turkish text renders in labels, not just
  `TextArea`. `Widgets::TextArea` (see `examples/03_notepad/`) is the first widget
  that actually exercises this — typed/seeded Turkish text renders correctly. Still not
  fixed: any OTHER non-ASCII script, and any other Latin Extended-A letter (Czech/Polish/
  Croatian-specific ones, etc.) — those remain silently dropped, same as before.

### Named, deliberate scope cuts (NOT bugs — documented decisions, revisit only if asked)

CSS: `hue-rotate()` (needs a 3×3 color rotation matrix, separate math family from the other
backdrop-filter functions), `invert()`, `drop-shadow()` (redundant with existing `box-shadow`),
`opacity()` filter function (redundant with `rgba()`), percent-based `margin`/`padding`
(would require `BoxEdges` to become length-per-edge instead of px-only), angle/radial/conic
gradients, CSS Grid, `@media`, `@keyframes`, `var()`, `calc()`, pseudo-elements, `em`/`rem`,
`flex-wrap`, `transform`, `font-weight`.
Renderer: full 2-pass separable Gaussian blur (current backdrop-filter blur is single-pass
NxN, capped at 20px radius — a 2-pass version needs render-to-texture infra that doesn't exist
yet), word-wrap (explicit `\n` only, in both `Label` and `TextArea`), full Unicode (ASCII +
Latin-1 + 6 Turkish codepoints only, see the glyph atlas entry above), z-index (paint order
= tree order), absolute positioning (flexbox only), text selection/clipboard/undo-redo/IME
in `TextArea`, keyboard-driven focus traversal (Tab is received, does nothing).
