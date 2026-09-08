#!/usr/bin/env python3
"""Prepare and validate the archive-ready House Rules FOMOD package."""

from __future__ import annotations

import argparse
import configparser
import hashlib
import pathlib
import re
import shutil
import tempfile
import xml.etree.ElementTree as ET


ROOT = pathlib.Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "package"
BUILD = ROOT / "build"

DLL_RELATIVE = pathlib.Path("core/F4SE/Plugins/HouseRules.dll")
PDB_RELATIVE = pathlib.Path("core/F4SE/Plugins/HouseRules.pdb")
STATIC_FILES = (
    pathlib.Path("fomod/ModuleConfig.xml"),
    pathlib.Path("fomod/info.xml"),
    pathlib.Path("core/MCM/Config/HouseRules/settings.ini"),
    pathlib.Path(
        "frontends/mcm/F4SE/Plugins/HouseRules.frontend.ini"
    ),
    pathlib.Path(
        "frontends/mcm/MCM/Config/HouseRules/config.json"
    ),
    pathlib.Path(
        "frontends/mcm/MCM/Config/HouseRules/lib.swf"
    ),
    pathlib.Path(
        "frontends/dmui/F4SE/Plugins/HouseRules.frontend.ini"
    ),
)
EXPECTED_FILES = frozenset((*STATIC_FILES, DLL_RELATIVE, PDB_RELATIVE))
OPTIONAL_FILES = frozenset((pathlib.Path("core/HouseRules.esp"),))
FRONTEND_CHOICES = (
    ("Mod Configuration Menu", "mcm"),
    ("Dear Modding UI", "dmui"),
)


def source_files(package_root: pathlib.Path) -> tuple[pathlib.Path, ...]:
    return (
        *STATIC_FILES,
        *(relative for relative in sorted(OPTIONAL_FILES)
          if (package_root / relative).is_file()),
    )


def project_version() -> str:
    versions = re.findall(
        r"""^\s*set_version\(\s*["']([^"']+)["']\s*\)""",
        (ROOT / "xmake.lua").read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    if len(versions) != 1:
        raise ValueError("xmake.lua must declare exactly one project version")
    return versions[0]


def copy_file(source: pathlib.Path, destination: pathlib.Path) -> None:
    if not source.is_file():
        raise FileNotFoundError(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_selector(path: pathlib.Path) -> str:
    text = path.read_text(encoding="utf-8")
    parser = configparser.ConfigParser(interpolation=None)
    parser.read_string(text)
    if parser.sections() != ["Interface"]:
        raise ValueError(f"selector must contain only [Interface]: {path}")
    if set(parser["Interface"]) != {"frontend"}:
        raise ValueError(f"selector must contain only Frontend: {path}")
    frontend = parser["Interface"]["frontend"].strip()
    if frontend not in {"mcm", "dmui"}:
        raise ValueError(f"selector has unsupported frontend {frontend}: {path}")
    return frontend


def resolved_repo_child(path: pathlib.Path, description: str) -> pathlib.Path:
    resolved = path.resolve()
    resolved_root = ROOT.resolve()
    if resolved == resolved_root or resolved_root not in resolved.parents:
        raise ValueError(f"{description} must stay inside the repository: {path}")
    return resolved


def safe_child(root: pathlib.Path, relative: str) -> pathlib.Path:
    normalized = pathlib.PureWindowsPath(relative)
    if normalized.is_absolute() or ".." in normalized.parts:
        raise ValueError(f"path escapes staging root: {relative}")
    candidate = root.joinpath(*normalized.parts).resolve()
    resolved_root = root.resolve()
    if candidate != resolved_root and resolved_root not in candidate.parents:
        raise ValueError(f"path escapes staging root: {relative}")
    return candidate


def copy_folder_contents(source: pathlib.Path, destination: pathlib.Path) -> None:
    if not source.is_dir():
        raise ValueError(f"installer source folder is missing: {source}")
    destination.mkdir(parents=True, exist_ok=True)
    for child in source.iterdir():
        target = destination / child.name
        if child.is_dir():
            shutil.copytree(child, target, dirs_exist_ok=True)
        else:
            copy_file(child, target)


def materialize_installer_choice(
    installer: pathlib.Path,
    plugin_name: str,
    destination: pathlib.Path,
) -> None:
    module_config = ET.parse(installer / "fomod" / "ModuleConfig.xml")
    groups = module_config.findall(".//group")
    if len(groups) != 1 or groups[0].attrib.get("type") != "SelectExactlyOne":
        raise ValueError("installer must contain one SelectExactlyOne frontend group")
    plugins = groups[0].findall("./plugins/plugin")
    selected = [plugin for plugin in plugins if plugin.attrib.get("name") == plugin_name]
    if len(selected) != 1:
        raise ValueError(f"expected exactly one installer choice named {plugin_name}")
    if destination.exists() and any(destination.iterdir()):
        raise ValueError(f"materialized destination must be empty: {destination}")
    destination.mkdir(parents=True, exist_ok=True)

    folder_nodes = [
        *module_config.findall("./requiredInstallFiles/folder"),
        *selected[0].findall("./files/folder"),
    ]
    for folder in folder_nodes:
        if "source" not in folder.attrib:
            raise ValueError("installer folder is missing its source")
        source = safe_child(installer, folder.attrib["source"])
        target = safe_child(destination, folder.attrib.get("destination", ""))
        copy_folder_contents(source, target)


def validate_materialized_choice(
    root: pathlib.Path,
    frontend: str,
    dll_hash: str,
    pdb_hash: str,
    expected_selector: str,
) -> None:
    plugins = root / "F4SE" / "Plugins"
    if sha256(plugins / "HouseRules.dll") != dll_hash:
        raise ValueError(f"{frontend} installer choice has the wrong DLL")
    if sha256(plugins / "HouseRules.pdb") != pdb_hash:
        raise ValueError(f"{frontend} installer choice has the wrong PDB")
    if (root / "Data").exists():
        raise ValueError(f"{frontend} installer choice contains nested Data")
    if (root / "MCM" / "Settings" / "HouseRules.ini").exists():
        raise ValueError("user override INI must never be packaged")
    selector_path = plugins / "HouseRules.frontend.ini"
    selector_text = selector_path.read_text(
        encoding="utf-8"
    )
    if read_selector(selector_path) != frontend or selector_text != expected_selector:
        raise ValueError(f"{frontend} installer choice has the wrong selector")

    config = root / "MCM" / "Config" / "HouseRules"
    if not (config / "settings.ini").is_file():
        raise ValueError(f"{frontend} installer choice is missing shared defaults")
    if frontend == "mcm":
        for required in ("config.json", "lib.swf"):
            if not (config / required).is_file():
                raise ValueError(f"MCM installer choice is missing {required}")
    else:
        for forbidden in ("config.json", "lib.swf"):
            if (config / forbidden).exists():
                raise ValueError(
                    f"native installer choice contains forbidden {forbidden}"
                )


def validate_metadata(package_root: pathlib.Path) -> None:
    info = ET.parse(package_root / "fomod" / "info.xml").getroot()
    version = info.findtext("Version")
    expected_version = project_version()
    if version != expected_version:
        raise ValueError(
            f"FOMOD metadata version {version!r} does not match "
            f"xmake.lua version {expected_version!r}"
        )

    module_config = ET.parse(
        package_root / "fomod" / "ModuleConfig.xml"
    ).getroot()
    groups = module_config.findall(".//group")
    if len(groups) != 1 or groups[0].attrib.get("type") != "SelectExactlyOne":
        raise ValueError("installer must contain one SelectExactlyOne frontend group")
    plugins = groups[0].findall("./plugins/plugin")
    if [plugin.attrib.get("name") for plugin in plugins] != [
        choice[0] for choice in FRONTEND_CHOICES
    ]:
        raise ValueError("installer frontend choices or order changed")

    folders = module_config.findall("./requiredInstallFiles/folder")
    if len(folders) != 1 or folders[0].attrib != {
        "source": "core",
        "destination": "",
        "priority": "0",
    }:
        raise ValueError("installer core folder mapping is invalid")
    expected_frontend_sources = [
        r"frontends\mcm",
        r"frontends\dmui",
    ]
    frontend_sources = [
        plugin.find("./files/folder").attrib
        for plugin in plugins
        if plugin.find("./files/folder") is not None
    ]
    if frontend_sources != [
        {"source": source, "destination": "", "priority": "10"}
        for source in expected_frontend_sources
    ]:
        raise ValueError("installer frontend folder mappings are invalid")


def validate_package(
    package_root: pathlib.Path = PACKAGE,
    work_root: pathlib.Path | None = None,
) -> None:
    package_root = package_root.resolve()
    actual_files = frozenset(
        path.relative_to(package_root)
        for path in package_root.rglob("*")
        if path.is_file()
    )
    missing = EXPECTED_FILES - actual_files
    unexpected = actual_files - EXPECTED_FILES - OPTIONAL_FILES
    if missing or unexpected:
        raise ValueError(
            "package file layout mismatch: "
            f"missing={sorted(map(str, missing))}, "
            f"unexpected={sorted(map(str, unexpected))}"
        )
    if (package_root / "Data").exists():
        raise ValueError("package archive root must not contain a top-level Data")
    if any(
        path.as_posix().lower() == "mcm/settings/houserules.ini"
        for path in actual_files
    ):
        raise ValueError("user override INI must never be packaged")

    selectors = {}
    for frontend, relative in (
        (
            "mcm",
            pathlib.Path(
                "frontends/mcm/F4SE/Plugins/HouseRules.frontend.ini"
            ),
        ),
        (
            "dmui",
            pathlib.Path(
                "frontends/dmui/F4SE/Plugins/HouseRules.frontend.ini"
            ),
        ),
    ):
        selector_path = package_root / relative
        if read_selector(selector_path) != frontend:
            raise ValueError(f"tracked {frontend} selector is invalid")
        selectors[frontend] = selector_path.read_text(encoding="utf-8")

    validate_metadata(package_root)
    dll_hash = sha256(package_root / DLL_RELATIVE)
    pdb_hash = sha256(package_root / PDB_RELATIVE)

    if work_root is None:
        BUILD.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(
            prefix="package-validation-",
            dir=BUILD,
        )
    else:
        work_root.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(
            prefix="package-validation-",
            dir=work_root,
        )
    with temporary as temporary_path:
        validation_root = pathlib.Path(temporary_path)
        for plugin_name, frontend in FRONTEND_CHOICES:
            choice = validation_root / frontend
            materialize_installer_choice(package_root, plugin_name, choice)
            validate_materialized_choice(
                choice,
                frontend,
                dll_hash,
                pdb_hash,
                selectors[frontend],
            )
            for relative in actual_files & OPTIONAL_FILES:
                installed = choice / relative.relative_to("core")
                if sha256(package_root / relative) != sha256(installed):
                    raise ValueError(
                        f"{frontend} installer choice has the wrong {relative.name}"
                    )


def prepare_package(
    dll: pathlib.Path,
    pdb: pathlib.Path,
    output: pathlib.Path = PACKAGE,
) -> pathlib.Path:
    dll = resolved_repo_child(dll, "DLL source")
    pdb = resolved_repo_child(pdb, "PDB source")
    if not dll.is_file():
        raise FileNotFoundError(dll)
    if not pdb.is_file():
        raise FileNotFoundError(pdb)

    output = resolved_repo_child(output, "package output")
    package_root = PACKAGE.resolve()
    assets = source_files(package_root)
    if output == package_root:
        for relative in assets:
            if not (package_root / relative).is_file():
                raise FileNotFoundError(package_root / relative)
    else:
        if output in package_root.parents or package_root in output.parents:
            raise ValueError("alternate package output cannot overlap package source")
        build_root = BUILD.resolve()
        if build_root not in output.parents:
            raise ValueError("alternate package output must stay under build")
        if output.exists():
            if not output.is_dir():
                raise ValueError(
                    f"alternate package output is not a directory: {output}"
                )
            if any(output.iterdir()):
                raise ValueError(
                    f"alternate package output must not contain stale files: {output}"
                )
        for relative in assets:
            source = package_root / relative
            if not source.is_file():
                raise FileNotFoundError(source)
        output.mkdir(parents=True, exist_ok=True)
        for relative in assets:
            copy_file(package_root / relative, output / relative)

    copy_file(dll, output / DLL_RELATIVE)
    copy_file(pdb, output / PDB_RELATIVE)
    validate_package(output)
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dll", type=pathlib.Path, required=True)
    parser.add_argument("--pdb", type=pathlib.Path, required=True)
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=PACKAGE,
        help=(
            "archive root to prepare (default: tracked package source; "
            "alternate roots must be empty directories under build)"
        ),
    )
    args = parser.parse_args()
    output = prepare_package(args.dll, args.pdb, args.output)
    print(f"prepared and validated archive-ready package at {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
