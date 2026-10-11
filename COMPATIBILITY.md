# İzekip UI — API Compatibility Policy

This document defines what "breaking İzekip UI" means for this project, so that updating
the library does not silently break downstream projects that consume it.

## Scope: source-level only

İzekip UI is consumed as **source** — a downstream project adds this repository as a git
submodule (or copies it in) and calls `add_subdirectory()` from its own `CMakeLists.txt`.
There is no precompiled `.lib`/`.dll` distributed today. This policy therefore covers
**source compatibility** (a consumer's existing `.cpp` files keep compiling and behaving the
same after pulling an update) — it does **not** cover binary/ABI compatibility (struct
layout stability, calling-convention stability, etc.). If this project ever starts shipping
precompiled binaries, ABI compatibility needs its own, stricter policy at that time.

## Versioning

İzekip UI follows [Semantic Versioning](https://semver.org/): `MAJOR.MINOR.PATCH`.

- **PATCH** — bug fixes only, no API surface change.
- **MINOR** — new, additive Tier-1 API (see below), and/or a `[[deprecated]]` annotation
  added to something that will be removed in a future MAJOR. Never removes or changes the
  meaning of existing Tier-1 API.
- **MAJOR** — may remove anything that was `[[deprecated]]` for at least one full MINOR
  version, or make any other breaking change.

Starting point: **1.0.0**, as of 2026-10-11. The current Tier-1 API (defined below) is the
frozen baseline — not a "0.x, anything can still change" stance.

The version is available to consumers at compile time via the generated
`generated/Core/Version.h` (`IZEKIP_VERSION_MAJOR`/`MINOR`/`PATCH`/`IZEKIP_VERSION_STRING`,
configured from `cmake/Version.h.in` by the top-level `CMakeLists.txt`'s `project(... VERSION
...)` call).

## Tier 1 vs. Tier 2: what's actually frozen

Not every header under `include/` is consumer-facing. Some live there only because two
*internal* translation units (e.g. both render backends, or `StyleApplier.cpp`'s own value
parsers) need to share them, and `include/` is the only scope broad enough for that — not
because a consumer is meant to use them directly. Freezing those too would be dishonest:
**`GlyphAtlas::FindGlyph`'s signature (`char` → `char32_t`) and `RectDrawData.h`'s
`PSConstants` struct (new `backdropParams`/`backdropColorAdjust` fields) both changed in the
same development cycle this policy was written in**, to add Turkish-character support and
backdrop-filter rendering respectively. If Tier 2 were covered by this policy, "frozen as of
today" would already be retroactively false. The Tier 2 list below is load-bearing, not a
loophole.

### Tier 1 — frozen, covered by this policy (20 headers)

```
include/Core/Color.h
include/Core/Rect.h
include/Core/Easing.h          (transitively public: CubicBezier appears in Widget::Animate*To signatures)
include/Core/Transition.h      (transitively public: TransitionSpec appears in StyleProperties::transition)
include/Core/StyleProperties.h
include/Core/TextBuffer.h
include/Core/Widget.h
include/Core/InteractionController.h   (since 1.1.0)
include/Platform/Window.h
include/Renderer/IRenderer.h
include/CSS/StyleSheet.h
include/CSS/LexborBridge.h
include/CSS/StyleApplier.h
include/Widgets/Panel.h
include/Widgets/Button.h
include/Widgets/Label.h
include/Widgets/TextArea.h
include/Widgets/Checkbox.h            (since 1.1.0)
include/Widgets/RadioButton.h         (since 1.1.0, also declares RadioGroup)
include/Widgets/ProgressBar.h         (since 1.1.0)
```

Plus, once it exists: `generated/Core/Version.h` (unlike the Tier-2 generated header below,
this one IS meant for consumers to read, e.g. to log or `#if`-check against
`IZEKIP_VERSION_MAJOR`).

Only the **`public:`** members of each class/struct are covered. `Widget`'s `protected:`
surface (`m_rect`, the nested `Animation<T>` template, etc.) is a **named, acknowledged gap**
— no current example subclasses `Widget` directly (`Panel`/`Button`/`Label`/`TextArea` are
this library's own thin subclasses), but it is structurally possible, and this policy does
not yet protect that extension point. Revisit if/when that becomes a real use case.

Since 1.1.0 `Widget` also has two **protected virtual hooks**, `HandlesClick()` and
`OnClicked()`, which `Checkbox` and `RadioButton` override. They are part of the same
subclassing surface and are likewise not scanned by `tools/api_diff.py`; their signatures
should still be treated as stable.

### Tier 2 — exempt, free to change at any time

```
include/CSS/CssTokenizer.h          -- internal to StyleApplier.cpp's own value-parsing helpers
include/Core/Interpolate.h          -- internal to Widget.cpp only; never named in a Tier-1 signature
include/Renderer/RectDrawData.h     -- shared D3D11/Vulkan backend plumbing, not consumer-facing
include/Renderer/GlyphAtlas.h       -- internal to Label.cpp/TextArea.cpp
generated/Renderer/RenderBackendConfig.h  -- internal to RHI.cpp only; not committed to git
src/Renderer/D3D11/*, src/Renderer/Vulkan/*  -- already PRIVATE include scope, unreachable anyway
```

## Safe (additive) vs. breaking changes

**Safe — no version-policy concern, PATCH or MINOR:**
- A new `std::optional` field on `StyleProperties` (with the matching line added to
  `StyleProperties::MergeFrom` — see `ARCHITECTURE.md`'s CSS-pipeline section; this is the
  established convention every CSS property addition has followed so far).
- A new CSS property name/value recognized by `StyleApplier`.
- A new enumerator **appended** to an existing `enum class` (none of today's Tier-1 enums
  assign explicit numeric values, so appending is always safe; inserting in the middle or
  assigning explicit values later would not be).
- A new free function, a new widget type, a new public method.

**Breaking — requires a deprecation cycle (see below) or a MAJOR bump:**
- Removing or renaming a public method, free function, or struct field.
- Changing a parameter's type or order, or a return type.
- Removing or retyping a public struct field.
- Removing an enumerator.
- Changing any `IRenderer` pure-virtual method's signature (breaks every backend AND any
  consumer code that calls it, since it's the one interface designed to be called directly
  by consumer `main.cpp` files).
- Changing a public base class.

## Deprecation convention

Mark the old API `[[deprecated("why; will be removed in vX.Y")]]` and keep it working for
**at least one full MINOR version** before actually removing it in a MAJOR release.

Note: `tools/api_diff.py` (below) does not parse `[[deprecated]]` — when the grace period
ends and the deprecated API is actually removed, the tool will report it as a plain
`REMOVED` entry, exactly as it would for an accidental break. Confirming "yes, this one was
properly deprecated for a full MINOR version first" is a manual step in the release
checklist below, not something the tool verifies for you.

## Release checklist

1. Bump the version in the top-level `CMakeLists.txt`'s `project(IzekipUI VERSION ...)`.
2. Add an entry to `CHANGELOG.md`.
3. Run `python tools/api_diff.py --check` and review the report against the safe/breaking
   table above. For any `REMOVED`/`CHANGED` entry, confirm it was either (a) properly
   deprecated for a full prior MINOR version, or (b) this is a deliberate MAJOR bump.
4. Re-run the SDK verification steps: `cmake --install` both backend configs to scratch
   prefixes, run the installed `bin/cssfr create` against each to scaffold a scratch
   project, configure+build (via `find_package(IzekipUI)`, not `add_subdirectory`) and run
   it. See the plan this was built from, or just redo it by hand -- the point is confirming
   the installed package still actually works, not just that it still compiles in-tree.
5. Build the distributable SDK: `cmake -DD3D11_BUILD_DIR=build -DVULKAN_BUILD_DIR=build-vulkan
   -P cmake/PackageSDK.cmake` (writes `dist/IzekipUI-SDK-<version>-win64.zip`), then
   install it once with the GUI installer and uninstall it again from Apps & features.
6. Run `python tools/api_diff.py --snapshot <new version>` and commit the resulting file
   under `tools/api_snapshots/`.
7. `git tag v<new version>`.

See `tools/api_diff.py`'s own top-of-file comment for the tool's exact scanning algorithm
and its named blind spots (template/macro/overload handling, etc.).
