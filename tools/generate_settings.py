#!/usr/bin/env python3
"""Generate House Rules settings bindings and the MCM document.

The checked-in catalog is the UI-neutral source of truth. ``--import-existing``
re-imports Settings.h and MCM JSON while preserving catalog navigation.
Normal development uses the default write mode or ``--check``.
"""

from __future__ import annotations

import argparse
import html
import json
import math
import pathlib
import re
import sys
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parents[1]
CATALOG = ROOT / "settings" / "catalog.json"
SETTINGS_H = ROOT / "src" / "Settings.h"
GENERATED_H = ROOT / "src" / "SettingsCatalog.generated.h"
GENERATED_CPP = ROOT / "src" / "SettingsCatalog.generated.cpp"
MCM_JSON = (
    ROOT
    / "package"
    / "frontends"
    / "mcm"
    / "MCM"
    / "Config"
    / "HouseRules"
    / "config.json"
)
DEFAULTS_INI = (
    ROOT
    / "package"
    / "core"
    / "MCM"
    / "Config"
    / "HouseRules"
    / "settings.ini"
)

SETTING_RE = re.compile(
    r'inline\s+static\s+REX::INI::(Bool|F32|I32|Str)<>\s+(\w+)\s*'
    r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(.*?)\s*\};',
    re.S,
)


def parse_default(kind: str, expression: str) -> Any:
    expression = expression.strip()
    if kind == "Bool":
        if expression not in {"true", "false"}:
            raise ValueError(f"unsupported bool default: {expression}")
        return expression == "true"
    if kind == "I32":
        return int(expression)
    if kind == "F32":
        return float(expression.removesuffix("f"))
    match = re.fullmatch(
        r'std::string\s*\{\s*"((?:[^"\\]|\\.)*)"\s*\}',
        expression,
        re.S,
    )
    if not match:
        raise ValueError(f"unsupported string default: {expression}")
    return bytes(match.group(1), "utf-8").decode("unicode_escape")


def parse_settings_header() -> list[dict[str, Any]]:
    source = SETTINGS_H.read_text(encoding="utf-8")
    settings: list[dict[str, Any]] = []
    for match in SETTING_RE.finditer(source):
        rex_type, member, section, key, expression = match.groups()
        value_type = {
            "Bool": "bool",
            "F32": "float",
            "I32": "int",
            "Str": "string",
        }[rex_type]
        default = parse_default(rex_type, expression)
        if value_type == "bool":
            ini_default = "1" if default else "0"
        elif value_type == "string":
            ini_default = default
        else:
            ini_default = expression.strip().removesuffix("f")
        settings.append(
            {
                "id": f"{key}:{section}",
                "section": section,
                "key": key,
                "cpp": f"{section}::{member}",
                "type": value_type,
                "default": default,
                "iniDefault": ini_default,
            }
        )
    if len(settings) != 255:
        raise ValueError(f"expected 255 Settings.h entries, found {len(settings)}")
    ids = [entry["id"] for entry in settings]
    if len(ids) != len(set(ids)):
        raise ValueError("Settings.h contains duplicate section/key pairs")
    return settings


def strip_html(text: str) -> str:
    return html.unescape(re.sub(r"<[^>]*>", "", text)).strip()


def import_existing() -> dict[str, Any]:
    navigation = json.loads(CATALOG.read_text(encoding="utf-8"))
    page_categories = {page["id"]: page["category"] for page in navigation["pages"]}
    settings = parse_settings_header()
    by_id = {entry["id"]: entry for entry in settings}
    mcm = json.loads(MCM_JSON.read_text(encoding="utf-8-sig"))
    pages: list[dict[str, Any]] = []
    source_pages = [
        {
            "pageDisplayName": "General",
            "content": mcm["content"],
            "_root": True,
        },
        *mcm["pages"],
    ]
    seen: set[str] = set()
    for page_index, source_page in enumerate(source_pages):
        name = source_page["pageDisplayName"]
        page_id = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
        if page_id not in page_categories:
            raise ValueError(f"assign a catalog category before importing page {page_id}")
        summary = ""
        content: list[dict[str, Any]] = []
        group = "General"
        setting_order = 0
        for item in source_page["content"]:
            if "id" not in item:
                content.append(item)
                if item.get("type") == "section":
                    group = item.get("text", group)
                elif item.get("type") == "text" and item.get("html") and not summary:
                    candidate = strip_html(item.get("text", ""))
                    if candidate and candidate not in {name, mcm["displayName"]}:
                        summary = candidate
                continue

            setting_id = item["id"]
            if setting_id not in by_id:
                raise ValueError(f"MCM references unknown setting {setting_id}")
            if setting_id in seen:
                raise ValueError(f"MCM references setting twice: {setting_id}")
            seen.add(setting_id)
            options = item.get("valueOptions", {})
            entry = by_id[setting_id]
            expected_source = {
                "bool": "ModSettingBool",
                "float": "ModSettingFloat",
                "int": "ModSettingInt",
            }.get(entry["type"])
            if options.get("sourceType") != expected_source:
                raise ValueError(
                    f"{setting_id}: {options.get('sourceType')} does not match {entry['type']}"
                )
            ui: dict[str, Any] = {
                "page": page_id,
                "group": group,
                "label": item["text"],
                "help": item.get("help", ""),
                "control": item["type"],
                "order": setting_order,
            }
            setting_order += 1
            for field in ("min", "max", "step", "options"):
                if field in options:
                    ui[field] = options[field]
            entry["ui"] = ui
            content.append({"setting": setting_id})

        pages.append(
            {
                "id": page_id,
                "category": page_categories[page_id],
                "name": name,
                "summary": summary or f"{name} settings.",
                "root": page_index == 0,
                "content": content,
            }
        )

    if len(seen) != 226:
        raise ValueError(f"expected 226 MCM settings, found {len(seen)}")
    return {
        "schemaVersion": 1,
        "product": {
            "modName": mcm["modName"],
            "displayName": mcm["displayName"],
            "mcmSchema": mcm["$schema"],
            "minimumMcmVersion": mcm["minMcmVersion"],
            "pluginRequirements": mcm["pluginRequirements"],
        },
        "categories": navigation["categories"],
        "pages": pages,
        "settings": settings,
    }


def validate(catalog: dict[str, Any]) -> None:
    if set(catalog) != {"schemaVersion", "product", "categories", "pages", "settings"}:
        raise ValueError(f"unrecognized catalog fields: {set(catalog)}")
    if catalog.get("schemaVersion") != 1:
        raise ValueError("unsupported catalog schemaVersion")
    if set(catalog["product"]) != {
        "modName",
        "displayName",
        "mcmSchema",
        "minimumMcmVersion",
        "pluginRequirements",
    }:
        raise ValueError("unrecognized product metadata")
    settings = catalog.get("settings")
    pages = catalog.get("pages")
    if not isinstance(settings, list) or not isinstance(pages, list):
        raise ValueError("catalog settings/pages must be arrays")
    if len(settings) != 255:
        raise ValueError(f"expected 255 settings, found {len(settings)}")
    page_ids = {page["id"] for page in pages}
    if len(page_ids) != len(pages) or len(pages) != 16:
        raise ValueError("catalog must contain 16 uniquely identified pages")
    categories = catalog["categories"]
    if not isinstance(categories, list) or not categories:
        raise ValueError("catalog categories must be a nonempty array")
    category_ids: set[str] = set()
    for category in categories:
        if not isinstance(category, dict) or set(category) != {"id", "name"}:
            raise ValueError("invalid category fields")
        if (
            not isinstance(category["id"], str)
            or not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", category["id"])
            or not isinstance(category["name"], str)
            or not category["name"].strip()
            or category["id"] in category_ids
        ):
            raise ValueError(f"invalid or duplicate category: {category}")
        category_ids.add(category["id"])
    ids: set[str] = set()
    ui_count = 0
    counts: dict[str, int] = {"bool": 0, "float": 0, "int": 0, "string": 0}
    for entry in settings:
        required = {"id", "section", "key", "cpp", "type", "default", "iniDefault"}
        unknown = set(entry) - (required | {"ui"})
        missing = required - set(entry)
        if unknown or missing:
            raise ValueError(f"{entry.get('id')}: unknown={unknown}, missing={missing}")
        setting_id = entry["id"]
        if setting_id != f'{entry["key"]}:{entry["section"]}':
            raise ValueError(f"{setting_id}: id does not match key:section")
        if setting_id in ids:
            raise ValueError(f"duplicate setting id {setting_id}")
        ids.add(setting_id)
        value_type = entry["type"]
        if value_type not in counts:
            raise ValueError(f"{setting_id}: unknown type {value_type}")
        counts[value_type] += 1
        default = entry["default"]
        if value_type == "bool" and type(default) is not bool:
            raise ValueError(f"{setting_id}: invalid bool default")
        if value_type == "int" and (type(default) is not int):
            raise ValueError(f"{setting_id}: invalid int default")
        if value_type == "float" and (
            type(default) not in {int, float} or not math.isfinite(float(default))
        ):
            raise ValueError(f"{setting_id}: invalid float default")
        if value_type == "string" and not isinstance(default, str):
            raise ValueError(f"{setting_id}: invalid string default")
        ui = entry.get("ui")
        if not ui:
            continue
        ui_count += 1
        allowed = {
            "page",
            "group",
            "label",
            "help",
            "control",
            "order",
            "min",
            "max",
            "step",
            "options",
        }
        if set(ui) - allowed:
            raise ValueError(f"{setting_id}: unrecognized UI fields {set(ui) - allowed}")
        required_ui = {"page", "group", "label", "help", "control", "order"}
        if required_ui - set(ui):
            raise ValueError(f"{setting_id}: missing UI fields {required_ui - set(ui)}")
        if ui.get("page") not in page_ids:
            raise ValueError(f"{setting_id}: unknown page {ui.get('page')}")
        control = ui.get("control")
        expected = {"bool": "switcher", "float": "slider", "int": "slider"}.get(value_type)
        if control == "stepper":
            if value_type != "int" or not ui.get("options"):
                raise ValueError(f"{setting_id}: invalid stepper")
            if default < 0 or default >= len(ui["options"]):
                raise ValueError(f"{setting_id}: default is outside stepper options")
        elif control != expected:
            raise ValueError(f"{setting_id}: {control} does not match {value_type}")
        if control == "slider":
            for field in ("min", "max", "step"):
                if field not in ui or type(ui[field]) not in {int, float}:
                    raise ValueError(f"{setting_id}: slider missing numeric {field}")
            if not all(math.isfinite(float(ui[field])) for field in ("min", "max", "step")):
                raise ValueError(f"{setting_id}: non-finite slider range")
            if ui["min"] > ui["max"] or ui["step"] <= 0:
                raise ValueError(f"{setting_id}: invalid slider range")
            if not (ui["min"] <= default <= ui["max"]):
                raise ValueError(f"{setting_id}: default outside slider range")
    if ui_count != 226:
        raise ValueError(f"expected 226 UI settings, found {ui_count}")
    if counts != {"bool": 39, "float": 175, "int": 34, "string": 7}:
        raise ValueError(f"unexpected settings type counts: {counts}")

    placed: list[str] = []
    for page in pages:
        allowed = {"id", "category", "name", "summary", "root", "content"}
        if set(page) != allowed:
            raise ValueError(f"{page.get('id')}: invalid page fields")
        if page["category"] not in category_ids:
            raise ValueError(f"{page['id']}: unknown category {page['category']}")
        for item in page["content"]:
            if "setting" in item:
                if set(item) != {"setting"} or item["setting"] not in ids:
                    raise ValueError(f"{page['id']}: invalid setting reference {item}")
                placed.append(item["setting"])
            else:
                item_type = item.get("type")
                allowed_fields = {
                    "text": {"type", "text", "html"},
                    "section": {"type", "text"},
                    "image": {"type", "libName", "className"},
                    "spacer": {"type"},
                }.get(item_type)
                if allowed_fields is None or set(item) - allowed_fields:
                    raise ValueError(
                        f"{page['id']}: unrecognized presentation item {item}"
                    )
    ui_ids = [entry["id"] for entry in settings if entry.get("ui")]
    if placed != ui_ids and set(placed) != set(ui_ids):
        raise ValueError("page placement and UI settings differ")
    if len(placed) != len(set(placed)):
        raise ValueError("a UI setting is placed more than once")
    if {page["category"] for page in pages} != category_ids:
        raise ValueError("each category must contain at least one page")


def mcm_document(catalog: dict[str, Any]) -> dict[str, Any]:
    by_id = {entry["id"]: entry for entry in catalog["settings"]}

    def materialize(items: list[dict[str, Any]]) -> list[dict[str, Any]]:
        output: list[dict[str, Any]] = []
        for item in items:
            if "setting" not in item:
                output.append(item)
                continue
            entry = by_id[item["setting"]]
            ui = entry["ui"]
            value_options: dict[str, Any] = {}
            for field in ("min", "max", "step"):
                if field in ui:
                    value_options[field] = ui[field]
            value_options["sourceType"] = {
                "bool": "ModSettingBool",
                "float": "ModSettingFloat",
                "int": "ModSettingInt",
            }[entry["type"]]
            if "options" in ui:
                value_options["options"] = ui["options"]
            output.append(
                {
                    "id": entry["id"],
                    "text": ui["label"],
                    "type": ui["control"],
                    "help": ui["help"],
                    "valueOptions": value_options,
                }
            )
        return output

    product = catalog["product"]
    root = next(page for page in catalog["pages"] if page["root"])
    return {
        "$schema": product["mcmSchema"],
        "minMcmVersion": product["minimumMcmVersion"],
        "modName": product["modName"],
        "displayName": product["displayName"],
        "pluginRequirements": product["pluginRequirements"],
        "content": materialize(root["content"]),
        "pages": [
            {
                "pageDisplayName": page["name"],
                "content": materialize(page["content"]),
            }
            for page in catalog["pages"]
            if not page["root"]
        ],
    }


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def cpp_value(entry: dict[str, Any], default: bool = False) -> str:
    value = entry["default"] if default else None
    value_type = entry["type"]
    if value_type == "bool":
        return "true" if value else "false"
    if value_type == "float":
        return f"double{{ {float(value):.17g} }}"
    if value_type == "int":
        return f"std::int64_t{{ {int(value)} }}"
    return f"std::string{{ {cpp_string(value)} }}"


def generated_header() -> str:
    return """// Generated by tools/generate_settings.py. Do not edit.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace HouseRules::SettingsCatalog
{
\tenum class ValueType : std::uint8_t { kBool, kFloat, kInt, kString };
\tusing Value = std::variant<bool, double, std::int64_t, std::string>;

\tstruct Descriptor
\t{
\t\tstd::string_view id;
\t\tstd::string_view section;
\t\tstd::string_view key;
\t\tValueType type;
\t\tValue defaultValue;
\t\tbool exposed;
\t\tstd::string_view pageId;
\t\tstd::string_view pageName;
\t\tstd::string_view pageSummary;
\t\tstd::string_view group;
\t\tstd::string_view label;
\t\tstd::string_view help;
\t\tstd::string_view control;
\t\tdouble minimum;
\t\tdouble maximum;
\t\tdouble step;
\t\tbool hasRange;
\t\tstd::string_view options;
\t\tstd::int32_t sortKey;
\t\tValue (*read)();
\t\tbool (*write)(const Value&);
\t};

\tstruct Page
\t{
\t\tstd::string_view id;
\t\tstd::string_view name;
\t\tstd::string_view summary;
\t\tstd::int32_t sortKey;
\t\tstd::string_view categoryId;
\t};

\tstruct Category
\t{
\t\tstd::string_view id;
\t\tstd::string_view name;
\t\tstd::int32_t sortKey;
\t};

\t[[nodiscard]] std::span<const Descriptor> All() noexcept;
\t[[nodiscard]] std::span<const Page> Pages() noexcept;
\t[[nodiscard]] std::span<const Category> Categories() noexcept;
\t[[nodiscard]] const Descriptor* Find(std::string_view a_id) noexcept;
}
"""


def generated_cpp(catalog: dict[str, Any]) -> str:
    pages_by_id = {page["id"]: page for page in catalog["pages"]}
    rows: list[str] = []
    for entry in catalog["settings"]:
        ui = entry.get("ui", {})
        page = pages_by_id.get(ui.get("page"), {})
        has_range = "min" in ui
        options = "|".join(ui.get("options", []))
        value_type = {
            "bool": "ValueType::kBool",
            "float": "ValueType::kFloat",
            "int": "ValueType::kInt",
            "string": "ValueType::kString",
        }[entry["type"]]
        read_conversion = {
            "bool": f"Settings::{entry['cpp']}.GetValue()",
            "float": f"double{{ Settings::{entry['cpp']}.GetValue() }}",
            "int": f"std::int64_t{{ Settings::{entry['cpp']}.GetValue() }}",
            "string": f"Settings::{entry['cpp']}.GetValue()",
        }[entry["type"]]
        variant_type = {
            "bool": "bool",
            "float": "double",
            "int": "std::int64_t",
            "string": "std::string",
        }[entry["type"]]
        set_conversion = {
            "bool": "*value",
            "float": "static_cast<float>(*value)",
            "int": "static_cast<std::int32_t>(*value)",
            "string": "*value",
        }[entry["type"]]
        rows.append(
            "\t\tDescriptor{ "
            + ", ".join(
                [
                    cpp_string(entry["id"]),
                    cpp_string(entry["section"]),
                    cpp_string(entry["key"]),
                    value_type,
                    cpp_value(entry, True),
                    "true" if ui else "false",
                    cpp_string(ui.get("page", "")),
                    cpp_string(page.get("name", "")),
                    cpp_string(page.get("summary", "")),
                    cpp_string(ui.get("group", "")),
                    cpp_string(ui.get("label", "")),
                    cpp_string(ui.get("help", "")),
                    cpp_string(ui.get("control", "")),
                    f"{float(ui.get('min', 0.0)):.17g}",
                    f"{float(ui.get('max', 0.0)):.17g}",
                    f"{float(ui.get('step', 0.0)):.17g}",
                    "true" if has_range else "false",
                    cpp_string(options),
                    str(ui.get("order", 0)),
                    f"[]() -> Value {{ return {read_conversion}; }}",
                    (
                        "[](const Value& a_value) { "
                        f"if (const auto* value = std::get_if<{variant_type}>(&a_value)) {{ "
                        f"Settings::{entry['cpp']}.SetValue({set_conversion}); return true; "
                        "} return false; }"
                    ),
                ]
            )
            + " },"
        )
    page_rows = [
        f'\t\tPage{{ {cpp_string(page["id"])}, {cpp_string(page["name"])}, '
        f'{cpp_string(page["summary"])}, {index * 100}, {cpp_string(page["category"])} }},'
        for index, page in enumerate(catalog["pages"])
    ]
    category_rows = [
        f'\t\tCategory{{ {cpp_string(category["id"])}, {cpp_string(category["name"])}, {index * 100} }},'
        for index, category in enumerate(catalog["categories"])
    ]
    return (
        """// Generated by tools/generate_settings.py. Do not edit.
#include "PCH.h"

#include "SettingsCatalog.generated.h"
#include "Settings.h"

#include <array>

namespace HouseRules::SettingsCatalog
{
\tnamespace
\t{
\t\tconst std::array<Descriptor, 255> kDescriptors{
"""
        + "\n".join(rows)
        + """
\n\t\t};
\n\t\tconstexpr std::array<Page, 16> kPages{
"""
        + "\n".join(page_rows)
        + """
\n\t\t};
"""
        + f'\n\t\tconstexpr std::array<Category, {len(category_rows)}> kCategories{{\n'
        + "\n".join(category_rows)
        + """
\n\t\t};
\t}

\tstd::span<const Descriptor> All() noexcept
\t{
\t\treturn kDescriptors;
\t}

\tstd::span<const Page> Pages() noexcept
\t{
\t\treturn kPages;
\t}

\tstd::span<const Category> Categories() noexcept
\t{
\t\treturn kCategories;
\t}

\tconst Descriptor* Find(std::string_view a_id) noexcept
\t{
\t\tfor (const auto& descriptor : kDescriptors) {
\t\t\tif (descriptor.id == a_id) {
\t\t\t\treturn std::addressof(descriptor);
\t\t\t}
\t\t}
\t\treturn nullptr;
\t}
}
"""
    )


def defaults_ini(catalog: dict[str, Any]) -> str:
    sections: dict[str, list[dict[str, Any]]] = {}
    for entry in catalog["settings"]:
        sections.setdefault(entry["section"], []).append(entry)
    lines: list[str] = []
    for section, entries in sections.items():
        if lines:
            lines.append("")
        lines.append(f"[{section}]")
        for entry in entries:
            lines.append(f'{entry["key"]}={entry["iniDefault"]}')
    return "\n".join(lines) + "\n"


def render_outputs(catalog: dict[str, Any]) -> dict[pathlib.Path, str]:
    return {
        GENERATED_H: generated_header(),
        GENERATED_CPP: generated_cpp(catalog),
        MCM_JSON: json.dumps(mcm_document(catalog), indent=2, ensure_ascii=False) + "\n",
        DEFAULTS_INI: defaults_ini(catalog),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if generated files drift")
    parser.add_argument(
        "--import-existing",
        action="store_true",
        help="re-import Settings.h and MCM JSON while preserving catalog categories",
    )
    args = parser.parse_args()

    if args.check and args.import_existing:
        parser.error("--check and --import-existing are mutually exclusive")
    if args.import_existing:
        catalog = import_existing()
        CATALOG.parent.mkdir(parents=True, exist_ok=True)
        CATALOG.write_text(
            json.dumps(catalog, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    else:
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))

    validate(catalog)
    current_settings = parse_settings_header()
    comparable = [
        {key: entry[key] for key in ("id", "section", "key", "cpp", "type", "default")}
        for entry in catalog["settings"]
    ]
    parsed = [
        {key: entry[key] for key in ("id", "section", "key", "cpp", "type", "default")}
        for entry in current_settings
    ]
    if comparable != parsed:
        raise ValueError("catalog settings/defaults drift from src/Settings.h")

    failed = False
    for path, content in render_outputs(catalog).items():
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8-sig") != content:
                print(f"drift: {path.relative_to(ROOT)}", file=sys.stderr)
                failed = True
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
            print(f"generated {path.relative_to(ROOT)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
