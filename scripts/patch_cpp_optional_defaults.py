"""Default generated cpp-restsdk optional parameters so callers can omit them.

The generator emits every optional parameter as a bare ``boost::optional<T>``
with no default, so a call site must pass one argument per parameter the spec it
was generated from happens to carry. Since the client is regenerated per target
-- including from a deployed backend -- adding an optional parameter to an
endpoint breaks every C++ caller of it until that backend is live everywhere the
SDK builds. Defaulting the trailing optional run to ``boost::none`` lets callers
pass only the parameters they actually set, which compiles against either spec.

Declarations only: C++ rejects a default argument repeated on the definition.
"""

from __future__ import annotations

import argparse
from pathlib import Path

DECLARATION_END = ") const;"
DEFAULT = " = boost::none"


def matching_open(text: str, close: int) -> int | None:
    """Index of the ``(`` matching the ``)`` at ``close``."""
    depth = 0
    for index in range(close, -1, -1):
        if text[index] == ")":
            depth += 1
        elif text[index] == "(":
            depth -= 1
            if depth == 0:
                return index
    return None


def param_spans(params: str) -> list[tuple[int, int]]:
    """Bounds of each top-level parameter, trimmed; templates stay intact."""
    bounds, depth, start = [], 0, 0
    for index, char in enumerate(params):
        if char in "<(":
            depth += 1
        elif char in ">)":
            depth -= 1
        elif char == "," and depth == 0:
            bounds.append((start, index))
            start = index + 1
    bounds.append((start, len(params)))
    spans = []
    for begin, end in bounds:
        text = params[begin:end]
        if not text.strip():
            continue
        lead = len(text) - len(text.lstrip())
        trail = len(text) - len(text.rstrip())
        spans.append((begin + lead, end - trail))
    return spans


def patch_declaration(params: str) -> str:
    spans = param_spans(params)
    optional = ["boost::optional" in params[begin:end] for begin, end in spans]
    if not any(optional):
        return params
    first = optional.index(True)
    # A default argument binds every parameter after it, so only a trailing run
    # can take one. The generator orders required parameters first, but a spec
    # that breaks that ordering must be left alone rather than made uncompilable.
    if not all(optional[first:]):
        return params
    patched = params
    for begin, end in reversed(spans[first:]):
        if DEFAULT.strip() in patched[begin:end]:
            continue  # Safe to run the post-generation stage more than once.
        patched = patched[:end] + DEFAULT + patched[end:]
    return patched


def patch_optional_defaults(header: str) -> str:
    # Walk backwards so an insertion never shifts a declaration not yet visited.
    result, search = header, len(header)
    while True:
        close = result.rfind(DECLARATION_END, 0, search)
        if close == -1:
            return result
        search = close
        open_paren = matching_open(result, close)
        if open_paren is None:
            continue
        params = result[open_paren + 1 : close]
        patched = patch_declaration(params)
        if patched != params:
            result = result[: open_paren + 1] + patched + result[close:]


def patch_tree(root: Path) -> int:
    patched = 0
    for header_path in sorted((root / "include/CppRestOpenAPIClient/api").glob("*.h")):
        header = header_path.read_text()
        updated = patch_optional_defaults(header)
        if updated != header:
            header_path.write_text(updated)
            patched += 1
    return patched


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rest_root", type=Path)
    args = parser.parse_args()
    count = patch_tree(args.rest_root)
    print(f"Defaulted optional parameters in {count} generated C++ API headers")
