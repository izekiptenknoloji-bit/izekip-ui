# cssfr -- IzekipUI project scaffolding CLI

Generates a new, ready-to-build project that uses IzekipUI via `find_package()` -- a
pre-wired `CMakeLists.txt`, a minimal `main.cpp`, and a starter `style.css`. A native C++
tool (`cssfr.cpp`, built as part of IzekipUI's own build via `IZEKIP_BUILD_CLI`, default
ON) -- no Python, no runtime dependency beyond the C++ standard library. The four template
files are embedded as string literals directly in `cssfr.cpp`, so the built `cssfr.exe` is
fully self-contained.

## Prerequisite

IzekipUI must already be installed somewhere (`cmake --install build --prefix <path>` --
this also installs `cssfr.exe` itself into `<path>/bin/`). `cssfr` scaffolds a project that
*uses* an installed IzekipUI; it does not build/install IzekipUI itself.

## Usage

```
cssfr create <name> [--izekip-path <install-prefix>] [--output-dir <dir>]
```

```
cmake --install build --prefix C:\izekip-install
C:\izekip-install\bin\cssfr.exe create myapp --izekip-path C:\izekip-install
cd myapp
cmake -B build
cmake --build build
```

Without `--izekip-path`, the generated `CMakeLists.txt` has a commented-out
`CMAKE_PREFIX_PATH` line and you pass `-DCMAKE_PREFIX_PATH=...` (or set `IzekipUI_DIR`)
yourself when configuring.
