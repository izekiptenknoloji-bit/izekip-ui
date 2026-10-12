# Changelog

All notable changes to İzekip UI are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.0.0/); versioning follows
[Semantic Versioning](https://semver.org/) as defined in `COMPATIBILITY.md`.

## [Unreleased]

### Added
- Crash-loop safe mode (`Platform/SafeMode.h`). Each renderer start is counted per application
  in the user state directory; after the first presented frame the count is cleared. Three
  consecutive launches without a presented frame make the next launch use the WARP software
  rasterizer (D3D11) or a software Vulkan device (CPU device type, e.g. Mesa lavapipe). Without
  a software device the hardware device is used and a message says so. `IZEKIP_SAFE_MODE=1`
  forces it.

- Diagnostics (`Platform/Diagnostics.h`): always-on counters (live widgets, style applies,
  dropped draws, device recoveries) and process memory readers (Windows working set and
  private bytes, Linux VmRSS and RssAnon). `IZEKIP_DIAG=1` prints one line per minute from the
  presentation path.

### Changed
- D3D11 falls back to WARP when the hardware device cannot be created.
- The `IzekipUI` library is built with warnings as errors (`-Wall -Wextra -Wpedantic -Wshadow
  -Werror`, MSVC `/W4 /WX`).
- Vulkan: an unknown acquire, submit, or command-recording result skips the frame or rebuilds
  the device instead of recording with a stale image index or throwing.

### Fixed
- Glyphs wider than the atlas row are skipped instead of overrunning the atlas buffer; font
  files are size-checked before reading.
- Windows: asset paths containing non-ASCII directory names (for example a Turkish user
  profile) are opened as UTF-8 instead of the ANSI code page.

## [1.3.0] - 2026-10-12

Additive for Tier-1 consumers. `IRenderer` gained two virtual test hooks with default bodies
(`SimulateDeviceLossForTesting`, `IsDeviceLostForTesting`) and a `FontId` enumerator set
extended from 2 to 5 values.

### Added
- CSS `font-family` names for open-licensed stand-ins of the classic web fonts (SIL OFL,
  shipped in `share/IzekipUI/fonts`): `arial`/`helvetica` -> Arimo, `serif`/`times`/`times new
  roman`/`georgia` -> Tinos, `courier`/`courier new` -> Cousine. All five faces share one
  `Texture2DArray` in both backends; a missing stand-in file falls back to Inter.
- Device-loss recovery for D3D11 and Vulkan (`DEVICE_REMOVED` / `VK_ERROR_DEVICE_LOST`):
  the device and every GPU resource are rebuilt; drawing is skipped while lost.
- GPU tests (`IZEKIP_TEST_GPU=ON`): recovery under simulated device loss, and a font-family
  sample window.

### Fixed
- Vulkan `Shutdown` now resets every destroyed handle; recovery previously destroyed stale
  handles a second time.
- Linux builds: Win32 native headers are guarded; the default backend follows the platform.

## [1.2.0] - 2026-10-11

Additive for Tier-1 consumers. `IRenderer` gained a pure virtual (`SetWorldTransform`), so
out-of-tree renderer implementations must add it. `Widget::HitTest` now respects transforms;
untransformed widgets behave as before.

### Added
- CSS `scale` (uniform factor) and `rotate` (any CSS angle unit), pivoting about each
  widget's own center. Nested transforms compose through `Widget::GetWorldTransform()`,
  and hit-testing uses the inverse. Backed by the new `Core/Transform2D.h` and
  `IRenderer::SetWorldTransform`; both D3D11 and Vulkan shaders apply it.
- CSS `cursor` (`default`, `pointer`, `text`, `crosshair`, `not-allowed`), inherited from
  ancestors, applied through `Platform::Window::SetCursorShape`.
- CSS `font-weight`: values 600 and up render with a synthetic emboldening pass in `Label`
  (no bold font asset ships with IzekipUI).
- Installer: "Yapay Zeka Yardımı" option that installs `share/doc/IzekipUI/AI_GUIDE.md`,
  a teaching text for AI coding assistants.

### Fixed
- Backdrop blur scratch texture format now matches the backbuffer (`CopySubresourceRegion`
  silently failed on mismatched formats, leaving stale pixels behind the blur).
- Backdrop blur kernel replaced with a single-pass 2D Gaussian spanning the full radius.
- Installer text glyphs were invisible after the transform pipeline change.

### Known limitations
- `scale` and `rotate` are not animatable through `transition` yet.
- `backdrop-filter` and shadow blur do not follow a scaled or rotated element's transform.

## [1.1.0] - 2026-10-11

Additive only -- `tools/api_diff.py --check --against 1.0.0` reports ADDED entries and
nothing removed or changed.

### Added
- **SDK installer** `IzekipUISetup.exe`, written with İzekip UI itself (`installer/`, built
  with `-DIZEKIP_BUILD_INSTALLER=ON`, Windows + D3D11 build only). Per-user (no admin) or
  all-users (UAC) install to a folder of your choice; optional PATH entry, `IzekipUI_DIR`,
  Apps & features uninstaller and Start Menu shortcut. Uninstall reverses exactly what the
  install recorded in `<prefix>\uninstall\install.manifest` and never deletes files it
  didn't install. `--headless` / `--no-system` modes for scripting and tests.
- `cmake -P cmake/PackageSDK.cmake` builds the distributable zip (installer + Debug and
  Release payload per backend). No Python involved.
- `Widget::RemoveClass` / `SetClass` / `ToggleClass` -- class changes after
  `LexborBridge::Build()` now reach the CSS cascade on the next `StyleApplier::Apply()`.
- CSS `display: flex | none`. Hidden widgets are removed from layout, painting and
  hit-testing.
- `Widget::SetOnClick` / `Click` / `SetFocusable`, and `InteractionController`: hover,
  active, focus and click handling from raw mouse events in one reusable class.
- Widgets: `Checkbox`, `RadioButton` + `RadioGroup`, `ProgressBar` -- all visuals CSS-driven.
- `TextBuffer::GetTextUtf8()`.
- `Label` now decodes its text as UTF-8, so Turkish characters (and the rest of the glyph
  atlas's Latin-1 coverage) render in labels, not just in `TextArea`.
- `cssfr` project-scaffolding CLI (native C++, installed to `bin/`), CMake
  `install()`/`find_package(IzekipUI)` support, `examples/04_glassmorphism`.
- `tests/InteractionTests.cpp` and an installer install/uninstall round-trip in CTest.

### Fixed
- Debug consumers of an installed SDK: Debug libraries now carry a `d` suffix and both
  configurations can be installed side by side, so a Debug build no longer fails to link
  against a Release-only install (LNK2038).

### Known limitations
- The CSS cascade is additive: only `display` is reset when the class that set it is
  removed. For any other property a state class overrides, give it a base value too.

## [1.0.0] - 2026-10-11

First version covered by a compatibility policy (see `COMPATIBILITY.md` for what "1.0.0"
means here — source-level compatibility, Tier-1 headers only). Retroactive summary of
everything built up to this point:

### Added
- Widget tree (`Panel`, `Button`, `Label`, `TextArea`) laid out via Yoga flexbox.
- Real CSS cascade via lexbor: type/`.class`/`#id` selectors, descendant combinators,
  specificity ordering, `:hover`/`:active`/`:focus` pseudo-classes.
- Backend-agnostic `IRenderer` with two implementations sharing one HLSL shader source:
  D3D11 (Windows) and Vulkan (cross-platform).
- CSS-driven visual properties: `background-color`, `background` (linear-gradient, 2-3
  stops), `border-radius`, `box-shadow` (blur + spread), `opacity`, `backdrop-filter`
  (`blur()`, `brightness()`, `contrast()`, `saturate()`, `grayscale()` — real GPU-to-GPU
  framebuffer-copy rendering, not just parsing).
- `transition` (cubic-bezier easing, Newton-Raphson solved) on `background-color`, `width`,
  `height`, `margin`.
- SDF-based text rendering (`GlyphAtlas`, stb_truetype) for `Label`, baked from ASCII +
  Latin-1 Supplement + 6 Latin Extended-A codepoints for Turkish (ş Ş ğ Ğ ı İ).
- Real keyboard input (`Platform::Window::SetKeyCallback`/`SetCharCallback`) and a
  `Widget::IsFocused()` concept, exercised by the new multi-line editable `TextArea` widget
  (backed by a platform/renderer-independent `TextBuffer`) — the first widget with
  keyboard-driven behavior.
- `tests/CssParserTests.cpp` (CSS cascade correctness) and `tests/TextBufferTests.cpp`
  (`TextBuffer` editing logic, including a 12-Turkish-character UTF-8 round-trip).
- Three example apps under `examples/`: a CSS-property showcase, a static "document" layout
  stress-test, and a notepad demonstrating keyboard input.

### Known limitations
See `ARCHITECTURE.md`'s "Known Limitations" and "Named, deliberate scope cuts" sections —
not repeated here to avoid the two documents drifting out of sync.
