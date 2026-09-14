from __future__ import annotations

import hashlib
import importlib.util
import pathlib
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET
import zipfile


ROOT = pathlib.Path(__file__).resolve().parents[1]


def load_packager():
    path = ROOT / "tools" / "package_release.py"
    spec = importlib.util.spec_from_file_location("package_release", path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    spec.loader.exec_module(module)
    return module


def file_hash(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class PackagingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package = load_packager()

    def source_hashes(self):
        return {
            relative: file_hash(self.package.PACKAGE / relative)
            for relative in self.package.source_files(self.package.PACKAGE)
        }

    def test_archive_ready_package_and_frontend_invariants(self):
        before = self.source_hashes()
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            root = pathlib.Path(temporary)
            dll = root / "HouseRules.dll"
            pdb = root / "HouseRules.pdb"
            dll.write_bytes(b"same dll")
            pdb.write_bytes(b"same pdb")

            archives = [root / "archive-one", root / "archive-two"]
            for archive in archives:
                prepared = self.package.prepare_package(dll, pdb, archive)
                self.assertEqual(archive.resolve(), prepared)
                self.assertEqual(
                    frozenset((
                        *self.package.source_files(self.package.PACKAGE),
                        self.package.DLL_RELATIVE,
                        self.package.PDB_RELATIVE,
                    )),
                    frozenset(
                        path.relative_to(archive)
                        for path in archive.rglob("*")
                        if path.is_file()
                    ),
                )
                self.assertFalse((archive / "Data").exists())
                self.assertEqual(file_hash(dll), file_hash(archive / self.package.DLL_RELATIVE))
                self.assertEqual(file_hash(pdb), file_hash(archive / self.package.PDB_RELATIVE))

                for plugin_name, frontend in self.package.FRONTEND_CHOICES:
                    materialized = root / f"materialized-{archive.name}-{frontend}"
                    self.package.materialize_installer_choice(
                        archive,
                        plugin_name,
                        materialized,
                    )
                    plugins = materialized / "F4SE" / "Plugins"
                    self.assertEqual(file_hash(dll), file_hash(plugins / "HouseRules.dll"))
                    self.assertEqual(file_hash(pdb), file_hash(plugins / "HouseRules.pdb"))
                    self.assertEqual(
                        (
                            archive
                            / "frontends"
                            / frontend
                            / "F4SE"
                            / "Plugins"
                            / "HouseRules.frontend.ini"
                        ).read_text(encoding="utf-8"),
                        (plugins / "HouseRules.frontend.ini").read_text(
                            encoding="utf-8"
                        ),
                    )
                    self.assertFalse((materialized / "Data").exists())
                    self.assertFalse(
                        (
                            materialized
                            / "MCM"
                            / "Settings"
                            / "HouseRules.ini"
                        ).exists()
                    )
                    config = materialized / "MCM" / "Config" / "HouseRules"
                    self.assertTrue((config / "settings.ini").is_file())
                    if frontend == "mcm":
                        self.assertTrue((config / "config.json").is_file())
                        self.assertTrue((config / "lib.swf").is_file())
                    else:
                        self.assertFalse((config / "config.json").exists())
                        self.assertFalse((config / "lib.swf").exists())

        self.assertEqual(before, self.source_hashes())
        self.assertFalse((ROOT / "Data").exists())

        module_config = ET.parse(
            self.package.PACKAGE / "fomod" / "ModuleConfig.xml"
        )
        group = module_config.find(".//group")
        self.assertIsNotNone(group)
        self.assertEqual("SelectExactlyOne", group.attrib["type"])
        self.assertEqual(
            ["Mod Configuration Menu", "Dear Modding UI"],
            [
                plugin.attrib["name"]
                for plugin in module_config.findall(".//plugin")
            ],
        )

    def test_alternate_output_refuses_stale_or_source_layouts(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            root = pathlib.Path(temporary)
            dll = root / "HouseRules.dll"
            pdb = root / "HouseRules.pdb"
            dll.write_bytes(b"dll")
            pdb.write_bytes(b"pdb")

            stale = root / "stale"
            stale.mkdir()
            (stale / "old-file.txt").write_text("old", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "stale files"):
                self.package.prepare_package(dll, pdb, stale)
            self.assertEqual(
                "old",
                (stale / "old-file.txt").read_text(encoding="utf-8"),
            )

            with self.assertRaisesRegex(ValueError, "under build"):
                self.package.prepare_package(dll, pdb, ROOT / "src" / "archive")
            with self.assertRaisesRegex(ValueError, "overlap"):
                self.package.prepare_package(
                    dll,
                    pdb,
                    self.package.PACKAGE / "nested",
                )

    def test_optional_esp_is_preserved_in_both_choices(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            root = pathlib.Path(temporary)
            source = root / "source"
            for relative in self.package.STATIC_FILES:
                self.package.copy_file(
                    self.package.PACKAGE / relative, source / relative
                )
            esp = source / "core" / "HouseRules.esp"
            esp.write_bytes(b"optional house rules esp")
            dll = root / "HouseRules.dll"
            pdb = root / "HouseRules.pdb"
            dll.write_bytes(b"dll")
            pdb.write_bytes(b"pdb")
            archive = root / "archive"
            with mock.patch.object(self.package, "PACKAGE", source):
                self.package.prepare_package(dll, pdb, archive)
            self.assertEqual(file_hash(esp), file_hash(archive / "core" / esp.name))
            for plugin_name, frontend in self.package.FRONTEND_CHOICES:
                installed = root / frontend
                self.package.materialize_installer_choice(
                    archive, plugin_name, installed
                )
                self.assertEqual(file_hash(esp), file_hash(installed / esp.name))
            (archive / "core" / "unrelated.esp").write_bytes(b"not an asset")
            with self.assertRaisesRegex(ValueError, "unexpected"):
                self.package.validate_package(archive)

    def test_metadata_version_tracks_xmake(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            root = pathlib.Path(temporary)
            archive = root / "archive"
            for relative in self.package.STATIC_FILES:
                self.package.copy_file(
                    self.package.PACKAGE / relative, archive / relative
                )
            info_path = archive / "fomod" / "info.xml"
            info = ET.parse(info_path)
            info.find("Version").text = "1.3.0"
            info.write(info_path, encoding="utf-8", xml_declaration=True)
            xmake = root / "xmake.lua"
            xmake.write_text('set_version("1.3.0")\n', encoding="utf-8")
            with mock.patch.object(self.package, "ROOT", root):
                self.package.validate_metadata(archive)
                xmake.write_text('set_version("1.4.0")\n', encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "does not match xmake"):
                    self.package.validate_metadata(archive)
                xmake.write_text("-- missing version\n", encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "exactly one"):
                    self.package.validate_metadata(archive)

    def test_release_zip_installs_both_frontends(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            root = pathlib.Path(temporary)
            dll, pdb = root / "HouseRules.dll", root / "HouseRules.pdb"
            dll.write_bytes(b"release dll")
            pdb.write_bytes(b"matching pdb")
            package = self.package.prepare_package(dll, pdb, root / "package")
            destination = root / "HouseRules.zip"
            self.package.create_archive(package, destination)
            extracted = root / "extracted"
            with zipfile.ZipFile(destination) as archive:
                self.assertIsNone(archive.testzip())
                self.assertEqual(
                    {path.relative_to(package).as_posix() for path in package.rglob("*") if path.is_file()},
                    set(archive.namelist()),
                )
                self.assertEqual(
                    {"core", "fomod", "frontends"},
                    {name.split("/")[0] for name in archive.namelist()},
                )
                archive.extractall(extracted)
            self.package.validate_package(extracted)
            with self.assertRaisesRegex(ValueError, "inside the package"):
                self.package.create_archive(package, package / "recursive.zip")
            with self.assertRaisesRegex(ValueError, "under build"):
                self.package.create_archive(package, ROOT / "release.zip")

    def test_installer_paths_and_binary_sources_cannot_escape(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            root = pathlib.Path(temporary)
            with self.assertRaises(ValueError):
                self.package.safe_child(root, r"..\outside")
            with self.assertRaises(ValueError):
                self.package.safe_child(root, r"C:\outside")

            installer = root / "installer"
            installer.mkdir()
            for relative in self.package.STATIC_FILES:
                self.package.copy_file(
                    self.package.PACKAGE / relative,
                    installer / relative,
                )
            module_config = installer / "fomod" / "ModuleConfig.xml"
            original = module_config.read_text(encoding="utf-8")

            module_config.write_text(
                original.replace(
                    'source="core"',
                    'source="..\\outside"',
                    1,
                ),
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                self.package.materialize_installer_choice(
                    installer,
                    "Mod Configuration Menu",
                    root / "escaped-source",
                )

            module_config.write_text(
                original.replace(
                    'destination=""',
                    'destination="..\\outside"',
                    1,
                ),
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                self.package.materialize_installer_choice(
                    installer,
                    "Mod Configuration Menu",
                    root / "escaped-destination",
                )

        with tempfile.TemporaryDirectory() as external:
            external_root = pathlib.Path(external)
            dll = external_root / "HouseRules.dll"
            pdb = external_root / "HouseRules.pdb"
            dll.write_bytes(b"dll")
            pdb.write_bytes(b"pdb")
            with self.assertRaisesRegex(ValueError, "inside the repository"):
                self.package.prepare_package(
                    dll,
                    pdb,
                    ROOT / "build" / "external-source-test",
                )


if __name__ == "__main__":
    unittest.main()
