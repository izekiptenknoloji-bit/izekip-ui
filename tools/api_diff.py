#!/usr/bin/env python3
"""
tools/api_diff.py -- source-level API-diff tool for IzekipUI's Tier-1 (frozen,
consumer-facing) public headers. See COMPATIBILITY.md for what Tier-1 means and why the
Tier-2 headers below are deliberately excluded from this scan.

This is a regex/text-based extractor, NOT a real C++ parser (no libclang dependency, since
this toolchain has none -- the D3D11 build uses MSVC's D3DCompile, the Vulkan build uses
dxc.exe, neither is libclang/LLVM). It is reliable for THIS codebase's public API today
because that API was verified, by direct reading during the session that wrote this tool,
to contain no class/function templates, no macro-wrapped declarations, and a consistent
single-statement-per-line style in every Tier-1 header.

Each declaration is keyed by its qualified name, its parameter count (for callables), and a
short hash of its normalized parameter-list text PLUS whatever trailing qualifier text
follows the closing ')' (const/noexcept/override/&/&&/...). The hash is what lets e.g. a
real constructor (`Window(const WindowDesc&)`) and a `= delete`d copy constructor
(`Window(const Window&)`) -- same name, same arity, genuinely different parameter types --
resolve to different keys instead of silently colliding; including the trailing-qualifier
text is what distinguishes a const/non-const overload pair like
`TextArea::GetBuffer()`/`TextArea::GetBuffer() const` (an actual pair in this codebase,
found by testing this exact tool against it -- the first version of this script hashed only
the parameter list and silently dropped the non-const overload). It is still not real
overload resolution (it compares literal text, not resolved types), but it is materially
better than name+arity alone and costs nothing extra to compute.

Named blind spots (each a deliberate, acknowledged simplification -- false positives here
are fine, false negatives are not):
  - No real preprocessor/template/macro handling. Upgrade trigger: the day a Tier-1 header
    gains a class/function template, a macro-wrapped declaration, or a conditionally
    compiled (#ifdef) block.
  - True overload resolution is not performed (see above) -- two overloads with literally
    identical parameter-list AND trailing-qualifier text would still collide. No known
    instance in Tier-1 headers today (the one real case, TextArea::GetBuffer()'s const/
    non-const pair, is exactly what motivated including trailing qualifiers in the hash).
  - A default-member-initializer VALUE change (e.g. `opacity = 1.0f` -> `= 0.5f`) is
    reported as CHANGED even though it doesn't break compilation for existing callers --
    deliberate over-reporting, not a bug.
  - `Widget`'s `protected:` surface (subclassing extension points) is not scanned -- no
    current example subclasses Widget directly, but this is a real, named gap.
  - Comment-stripping is line/state-based, not a real preprocessor, and would mis-handle a
    `//` inside a string literal. None exist in any Tier-1 declaration today.
  - Forward declarations (e.g. `class GlyphAtlas;`) are captured as a plain entry rather
    than specially tagged. Harmless (they diff identically across snapshots unless the
    forward-declared name itself changes, which would correctly show up anyway).

Usage:
    python tools/api_diff.py --snapshot <version>            write a new snapshot
    python tools/api_diff.py --snapshot <version> --force    overwrite an existing snapshot
    python tools/api_diff.py --check [--against <version>]   diff current headers vs. a snapshot
    python tools/api_diff.py --list-tier1                    print the headers this tool reads
"""
import argparse
import hashlib
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SNAPSHOT_DIR = Path(__file__).resolve().parent / "api_snapshots"

# Tier 1: frozen, consumer-facing (see COMPATIBILITY.md). Tier 2 -- CSS/CssTokenizer.h,
# Core/Interpolate.h, Renderer/RectDrawData.h, Renderer/GlyphAtlas.h, and the generated
# (not committed) RenderBackendConfig.h -- is deliberately NOT scanned: each is
# internal-only plumbing that happens to live under include/ only because no narrower
# include scope exists for "visible to specific internal .cpp files but not consumers".
# This split is load-bearing, not a loophole -- see COMPATIBILITY.md for the concrete
# history (GlyphAtlas::FindGlyph's signature and RectDrawData.h's PSConstants struct both
# changed in the same development cycle this tool was written in).
TIER1_HEADERS = [
    "include/Core/Color.h",
    "include/Core/Rect.h",
    "include/Core/Easing.h",
    "include/Core/Transition.h",
    "include/Core/StyleProperties.h",
    "include/Core/TextBuffer.h",
    "include/Core/Widget.h",
    "include/Core/InteractionController.h",
    "include/Platform/Window.h",
    "include/Renderer/IRenderer.h",
    "include/CSS/StyleSheet.h",
    "include/CSS/LexborBridge.h",
    "include/CSS/StyleApplier.h",
    "include/Widgets/Panel.h",
    "include/Widgets/Button.h",
    "include/Widgets/Label.h",
    "include/Widgets/TextArea.h",
    "include/Widgets/Checkbox.h",
    "include/Widgets/RadioButton.h",
    "include/Widgets/ProgressBar.h",
]


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    lines = []
    for line in text.split("\n"):
        idx = line.find("//")
        lines.append(line if idx == -1 else line[:idx])
    return "\n".join(lines)


def strip_preprocessor(text: str) -> str:
    lines = [line for line in text.split("\n") if not line.lstrip().startswith("#")]
    return "\n".join(lines)


def normalize(text: str) -> str:
    return " ".join(text.split())


def split_signature(decl: str):
    """Returns (param_text, trailing_text_after_closing_paren) for the FIRST top-level
    '(' ... ')' pair in decl, or (None, None) if decl has no top-level '('. trailing_text
    captures const/noexcept/override/&/&&/'= 0'/etc. -- needed to distinguish a
    const/non-const overload pair (same name, same params, different trailing qualifier)
    from colliding onto the same key."""
    paren = decl.find("(")
    if paren == -1:
        return None, None
    depth = 0
    close_idx = -1
    for idx in range(paren, len(decl)):
        ch = decl[idx]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                close_idx = idx
                break
    if close_idx == -1:
        return decl[paren + 1:].strip(), ""
    inner = decl[paren + 1:close_idx].strip()
    trailing = decl[close_idx + 1:].strip()
    return inner, trailing


def param_count(inner_text: str) -> int:
    if not inner_text:
        return 0
    count = 1
    nest = 0
    for c in inner_text:
        if c in "(<":
            nest += 1
        elif c in ")>":
            nest = max(0, nest - 1)
        elif c == "," and nest == 0:
            count += 1
    return count


def declaration_name(decl: str) -> str:
    """Best-effort identifier extraction for keying a declaration (see module docstring's
    named blind spots -- this is not real C++ parsing)."""
    decl = decl.strip().rstrip(";").strip()
    if decl.startswith("using "):
        rest = decl[len("using "):].strip()
        m = re.match(r"([A-Za-z_]\w*)", rest)
        return m.group(1) if m else rest
    paren = decl.find("(")
    if paren != -1:
        before = decl[:paren].strip()
        tokens = before.split()
        return tokens[-1] if tokens else before
    eq = decl.find("=")
    head = decl[:eq] if eq != -1 else decl
    tokens = head.split()
    return tokens[-1] if tokens else head


def make_key(scope_path, decl: str) -> str:
    name = declaration_name(decl)
    qualified = "::".join(list(scope_path) + [name]) if scope_path else name
    if decl.strip().startswith("using "):
        return qualified
    inner, trailing = split_signature(decl)
    if inner is None:
        return qualified
    pc = param_count(inner)
    fingerprint = normalize(inner) + "|" + normalize(trailing)
    digest = hashlib.sha1(fingerprint.encode("utf-8")).hexdigest()[:8]
    return f"{qualified}/{pc}/{digest}"


def extract(filepath: Path) -> dict:
    """Returns {key: normalized_declaration_text} for one Tier-1 header."""
    raw = filepath.read_text(encoding="utf-8")
    text = strip_preprocessor(strip_comments(raw))

    entries = {}
    scope_path = []       # namespace/class/struct names, for qualifying keys
    access_stack = []     # one entry per OPEN class/struct frame (not namespace)
    frame_kind_stack = []  # one entry per OPEN class/struct frame
    pop_counts = []        # how many scope_path levels one '}' should pop (namespaces can
                            # push >1 level via the "namespace A::B {" shorthand but share
                            # a single brace pair)
    pending = []
    paren_depth = 0
    i = 0
    n = len(text)

    def emittable():
        return all(a == "public" for a in access_stack)

    while i < n:
        c = text[i]

        if c == "(":
            paren_depth += 1
            pending.append(c)
            i += 1
            continue
        if c == ")":
            paren_depth = max(0, paren_depth - 1)
            pending.append(c)
            i += 1
            continue

        if paren_depth == 0 and c == ":" and (i + 1 >= n or text[i + 1] != ":"):
            label = normalize("".join(pending))
            if label in ("public", "private", "protected") and access_stack:
                access_stack[-1] = label
                pending = []
                i += 1
                continue
            pending.append(c)
            i += 1
            continue

        if paren_depth == 0 and c in "{};":
            buf = normalize("".join(pending))

            if c == ";":
                if buf and emittable():
                    entries[make_key(scope_path, buf)] = buf
                pending = []
                i += 1
                continue

            if c == "{":
                ns_match = re.match(r"^namespace\s+([\w:]+)$", buf)
                cls_match = re.match(r"^(class|struct)\s+([A-Za-z_]\w*)\b.*$", buf)
                enum_match = re.match(r"^enum\s+class\s+([A-Za-z_]\w*)\b.*$", buf)

                if ns_match:
                    parts = ns_match.group(1).split("::")
                    scope_path.extend(parts)
                    frame_kind_stack.extend(["namespace"] * len(parts))
                    pop_counts.append(len(parts))
                    pending = []
                    i += 1
                    continue

                if enum_match:
                    enum_name = enum_match.group(1)
                    if emittable():
                        entries[
                            "::".join(list(scope_path) + [f"{enum_name} [enum]"])
                        ] = buf
                    depth = 1
                    j = i + 1
                    body = []
                    while j < n and depth > 0:
                        if text[j] == "{":
                            depth += 1
                        elif text[j] == "}":
                            depth -= 1
                            if depth == 0:
                                break
                        body.append(text[j])
                        j += 1
                    if emittable():
                        for enumerator in "".join(body).split(","):
                            ename = enumerator.strip().split("=")[0].strip()
                            if ename:
                                key = "::".join(list(scope_path) + [enum_name, ename])
                                entries[key] = "present"
                    i = j + 1
                    while i < n and text[i] in " \t\r\n":
                        i += 1
                    if i < n and text[i] == ";":
                        i += 1
                    pending = []
                    continue

                if cls_match:
                    kind, cname = cls_match.group(1), cls_match.group(2)
                    if emittable():
                        key = "::".join(list(scope_path) + [f"{cname} [{kind}]"])
                        entries[key] = buf
                    scope_path.append(cname)
                    frame_kind_stack.append(kind)
                    access_stack.append("private" if kind == "class" else "public")
                    pop_counts.append(1)
                    pending = []
                    i += 1
                    continue

                # Inline-bodied function/method (e.g. `Widget* GetParent() const { ... }`,
                # or `bool operator==(...) const { ... }`): keep the signature, skip the
                # body without pushing any scope.
                if emittable() and buf:
                    entries[make_key(scope_path, buf)] = buf
                depth = 1
                j = i + 1
                while j < n and depth > 0:
                    if text[j] == "{":
                        depth += 1
                    elif text[j] == "}":
                        depth -= 1
                    j += 1
                i = j
                pending = []
                continue

            if c == "}":
                cnt = pop_counts.pop() if pop_counts else 0
                for _ in range(cnt):
                    if scope_path:
                        scope_path.pop()
                    kind = frame_kind_stack.pop() if frame_kind_stack else None
                    if kind in ("class", "struct") and access_stack:
                        access_stack.pop()
                pending = []
                i += 1
                k = i
                while k < n and text[k] in " \t\r\n":
                    k += 1
                if k < n and text[k] == ";":
                    i = k + 1
                continue

        pending.append(c)
        i += 1

    return entries


def extract_all(headers) -> dict:
    combined = {}
    for rel in headers:
        path = REPO_ROOT / rel
        for key, value in extract(path).items():
            combined[f"{rel}:{key}"] = value
    return combined


def write_snapshot(path: Path, entries: dict) -> None:
    with path.open("w", encoding="utf-8") as f:
        for key in sorted(entries):
            f.write(f"{key}\t{entries[key]}\n")


def read_snapshot(path: Path) -> dict:
    entries = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        key, _, value = line.partition("\t")
        entries[key] = value
    return entries


def version_sort_key(v: str):
    try:
        return tuple(int(x) for x in v.split("."))
    except ValueError:
        return (0,)


def latest_snapshot_version(snapshot_dir: Path):
    versions = [p.stem for p in snapshot_dir.glob("*.txt")] if snapshot_dir.exists() else []
    versions.sort(key=version_sort_key)
    return versions[-1] if versions else None


def diff_entries(old: dict, new: dict):
    added = sorted(k for k in new if k not in old)
    removed = sorted(k for k in old if k not in new)
    changed = sorted(k for k in new if k in old and new[k] != old[k])
    return added, removed, changed


def cmd_snapshot(version: str, force: bool) -> int:
    SNAPSHOT_DIR.mkdir(parents=True, exist_ok=True)
    out = SNAPSHOT_DIR / f"{version}.txt"
    if out.exists() and not force:
        print(f"error: {out} already exists (use --force to overwrite)", file=sys.stderr)
        return 2
    entries = extract_all(TIER1_HEADERS)
    write_snapshot(out, entries)
    print(f"wrote {out} ({len(entries)} entries)")
    return 0


def cmd_check(against: str) -> int:
    version = against or latest_snapshot_version(SNAPSHOT_DIR)
    if not version:
        print("error: no snapshot found under tools/api_snapshots/ -- run --snapshot first",
              file=sys.stderr)
        return 2
    snap_path = SNAPSHOT_DIR / f"{version}.txt"
    if not snap_path.exists():
        print(f"error: no snapshot for version {version} at {snap_path}", file=sys.stderr)
        return 2

    old = read_snapshot(snap_path)
    new = extract_all(TIER1_HEADERS)
    added, removed, changed = diff_entries(old, new)

    print(f"Comparing current Tier-1 headers against snapshot {version}:")
    if added:
        print(f"\nADDED ({len(added)}):")
        for k in added:
            print(f"  + {k}")
    if removed:
        print(f"\nREMOVED ({len(removed)}):")
        for k in removed:
            print(f"  - {k}")
    if changed:
        print(f"\nCHANGED ({len(changed)}):")
        for k in changed:
            print(f"  ~ {k}")
            print(f"    old: {old[k]}")
            print(f"    new: {new[k]}")
    if not (added or removed or changed):
        print("\nNo differences.")

    if removed or changed:
        print(f"\n{len(removed)} removed, {len(changed)} changed -- "
              f"review against COMPATIBILITY.md before accepting.")
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--snapshot", metavar="VERSION", help="write a new snapshot for VERSION")
    parser.add_argument("--force", action="store_true", help="overwrite an existing snapshot file")
    parser.add_argument("--check", action="store_true", help="diff current headers against a snapshot")
    parser.add_argument("--against", metavar="VERSION", help="snapshot version to diff against (default: latest)")
    parser.add_argument("--list-tier1", action="store_true", help="print the Tier-1 headers this tool reads")
    args = parser.parse_args()

    if args.list_tier1:
        for h in TIER1_HEADERS:
            print(h)
        return 0
    if args.snapshot:
        return cmd_snapshot(args.snapshot, args.force)
    if args.check:
        return cmd_check(args.against)

    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
