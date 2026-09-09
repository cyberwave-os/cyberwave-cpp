"""Shared repair for cpp-restsdk numeric enums in both SDK generation paths.

The generator emits numeric converter definitions, but string declarations,
string serialization temporaries/literals, and body-less numeric setters. Keep
the generated enum names and wire types; limit repairs to those converter pairs.
"""

from __future__ import annotations

import argparse
from decimal import Decimal
from pathlib import Path
import re


NUMERIC_CONVERTER = re.compile(
    r"(?P<model>\w+)::(?P<enum>\w+Enum) (?P=model)::to(?P=enum)"
    r"\(const (?P<type>int32_t|int64_t|float|double)& value\) const\n"
    r"\{(?P<body>.*?)^\}",
    re.MULTILINE | re.DOTALL,
)
PATCH_MARKER = "// cpp-restsdk numeric-enum wire-type repair"


def numeric_literal(expression: str, wire_type: str) -> str:
    wrapped = re.fullmatch(
        r'utility::conversions::to_string_t\("([^"\\]+)"\)', expression
    )
    value = wrapped.group(1) if wrapped else expression
    if not re.fullmatch(r"-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?", value):
        raise ValueError(f"Unsupported generated numeric enum literal: {expression}")
    number = Decimal(value)
    if wire_type in {"int32_t", "int64_t"}:
        bits = 32 if wire_type == "int32_t" else 64
        if number != number.to_integral_value() or not -(
            2 ** (bits - 1)
        ) <= number < 2 ** (bits - 1):
            raise ValueError(f"Enum literal is outside {wire_type}: {value}")
        integer = int(number)
        if wire_type == "int64_t":
            value = (
                "(-9223372036854775807LL - 1)"
                if integer == -(2**63)
                else f"{integer}LL"
            )
        else:
            value = str(integer)
    return f"static_cast<{wire_type}>({value})"


def patch_numeric_enums(header: str, source: str) -> tuple[str, str]:
    for converter in list(NUMERIC_CONVERTER.finditer(source)):
        if PATCH_MARKER in converter.group("body"):
            continue  # Safe to run the post-generation stage more than once.
        model, enum, wire_type = converter.group("model", "enum", "type")
        # Enum names retain underscores while generated setter names may not.
        setter = re.search(
            rf"void {model}::set(?P<stem>\w+)\(const {enum} value\)", source
        )
        if not setter:
            raise ValueError(f"Missing numeric enum setter: {model}::{enum}")
        stem = setter.group("stem")
        mappings = re.findall(
            rf"if \(value == (.*?)\)\s*\{{\s*return {enum}::(\w+);\s*\}}",
            converter.group("body"),
            flags=re.DOTALL,
        )
        if not mappings:
            raise ValueError(f"Unrecognized numeric enum converter: {model}::{enum}")
        mappings = [
            (numeric_literal(value.strip(), wire_type), key) for value, key in mappings
        ]
        from_pattern = re.compile(
            rf"const {wire_type} {model}::from{enum}\(const {enum} value\) const\n\{{.*?^\}}",
            re.MULTILINE | re.DOTALL,
        )
        from_converter = from_pattern.search(source)
        if not from_converter:
            raise ValueError(f"Missing numeric enum reverse converter: {model}::{enum}")
        for old, new in (
            (
                f"{enum} to{enum}(const utility::string_t& value) const;",
                f"{enum} to{enum}(const {wire_type}& value) const;",
            ),
            (
                f"const utility::string_t from{enum}(const {enum} value) const;",
                f"const {wire_type} from{enum}(const {enum} value) const;",
            ),
        ):
            if old not in header and new not in header:
                raise ValueError(f"Missing numeric enum declaration: {model}::{enum}")
            header = header.replace(old, new)
        to_body = "\n".join(
            f"    if (value == {value}) return {enum}::{key};"
            for value, key in mappings
        )
        source = source.replace(
            converter.group(0),
            f"{model}::{enum} {model}::to{enum}(const {wire_type}& value) const\n{{\n"
            f"    {PATCH_MARKER}\n{to_body}\n"
            f'    throw std::invalid_argument("Invalid value for conversion to {enum}");\n}}',
            1,
        )
        cases = "\n".join(
            f"        case {enum}::{key}: return {value};" for value, key in mappings
        )
        source = source.replace(
            from_converter.group(0),
            f"const {wire_type} {model}::from{enum}(const {enum} value) const\n{{\n"
            f"    switch(value)\n    {{\n{cases}\n"
            f'        default: throw std::invalid_argument("Invalid {enum} value");\n    }}\n}}',
            1,
        )
        source = source.replace(
            f"utility::string_t refVal = from{enum}(",
            f"{wire_type} refVal = from{enum}(",
        )
        # Delegate numeric setters through the same membership validation as JSON.
        source = re.sub(
            rf"void {model}::set{stem}\({wire_type} value\)\s*(?=\nvoid {model}::set{stem}\(const {enum} value\))",
            f"void {model}::set{stem}({wire_type} value)\n{{\n    set{stem}(to{enum}(value));\n}}\n",
            source,
        )
        # ModelBase's int64 conversion accepts/truncates floating-point numbers;
        # reject non-integral/out-of-range wire values before enum conversion.
        guard = ""
        if wire_type in {"int32_t", "int64_t"}:
            range_check = "is_int32" if wire_type == "int32_t" else "is_int64"
            guard = f"if (!fieldValue.is_number() || !fieldValue.as_number().{range_check}()) return false;\n            "
        source = source.replace(
            f"ok &= ModelBase::fromJson(fieldValue, refVal_set{stem});",
            guard
            + f"if (!ModelBase::fromJson(fieldValue, refVal_set{stem})) return false;",
        )
    return header, source


def patch_tree(root: Path) -> int:
    changes = []
    for source_path in sorted((root / "src/model").glob("*.cpp")):
        source = source_path.read_text()
        if not NUMERIC_CONVERTER.search(source):
            continue
        header_path = (
            root / "include/CppRestOpenAPIClient/model" / f"{source_path.stem}.h"
        )
        header = header_path.read_text()
        patched_header, patched_source = patch_numeric_enums(header, source)
        if (patched_header, patched_source) != (header, source):
            changes.append((header_path, patched_header, source_path, patched_source))
    # Validate every candidate before mutating any generated file.
    for header_path, header, source_path, source in changes:
        header_path.write_text(header)
        source_path.write_text(source)
    return len(changes)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rest_root", type=Path)
    args = parser.parse_args()
    print(f"Patched numeric enums in {patch_tree(args.rest_root)} generated C++ models")
