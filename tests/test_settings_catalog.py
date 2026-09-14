from __future__ import annotations

import importlib.util
import json
import pathlib
import copy
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


def load_generator():
    path = ROOT / "tools" / "generate_settings.py"
    spec = importlib.util.spec_from_file_location("generate_settings", path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    spec.loader.exec_module(module)
    return module


class SettingsCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.generator = load_generator()
        cls.catalog = json.loads(
            (ROOT / "settings" / "catalog.json").read_text(encoding="utf-8")
        )

    def test_catalog_is_valid_and_complete(self):
        self.generator.validate(self.catalog)
        exposed = [entry for entry in self.catalog["settings"] if entry.get("ui")]
        self.assertEqual(255, len(self.catalog["settings"]))
        self.assertEqual(226, len(exposed))
        self.assertEqual(16, len(self.catalog["pages"]))

    def test_ui_control_counts_match_legacy_menu(self):
        controls = [
            entry["ui"]["control"]
            for entry in self.catalog["settings"]
            if entry.get("ui")
        ]
        self.assertEqual(17, controls.count("switcher"))
        self.assertEqual(208, controls.count("slider"))
        self.assertEqual(1, controls.count("stepper"))

    def test_mcm_bindings_match_catalog_types(self):
        document = self.generator.mcm_document(self.catalog)
        controls = []
        for item in document["content"]:
            if "id" in item:
                controls.append(item)
        for page in document["pages"]:
            controls.extend(item for item in page["content"] if "id" in item)
        by_id = {entry["id"]: entry for entry in self.catalog["settings"]}
        self.assertEqual(set(by_id_entry["id"] for by_id_entry in self.catalog["settings"] if by_id_entry.get("ui")), {control["id"] for control in controls})
        source_types = {
            "bool": "ModSettingBool",
            "float": "ModSettingFloat",
            "int": "ModSettingInt",
        }
        for control in controls:
            entry = by_id[control["id"]]
            self.assertEqual(
                source_types[entry["type"]],
                control["valueOptions"]["sourceType"],
            )

    def test_generated_outputs_have_no_drift(self):
        for path, expected in self.generator.render_outputs(self.catalog).items():
            self.assertTrue(path.is_file(), path)
            self.assertEqual(
                expected,
                path.read_text(encoding="utf-8-sig"),
                path.relative_to(ROOT),
            )

    def test_header_defaults_match_catalog(self):
        parsed = self.generator.parse_settings_header()
        keys = ("id", "section", "key", "cpp", "type", "default")
        self.assertEqual(
            [{key: entry[key] for key in keys} for entry in parsed],
            [
                {key: entry[key] for key in keys}
                for entry in self.catalog["settings"]
            ],
        )

    def test_native_categories_cover_existing_pages(self):
        self.assertEqual(
            {
                "Overview": ["general"],
                "Survival & Healing": ["survival-unlocks", "survival", "magnitudes"],
                "Character & Progression": ["character", "progression", "skills", "sneak"],
                "Combat": ["difficulty", "damage-formulas", "power-armor", "vats", "combat-perks"],
                "World": ["economy", "companions", "settlements"],
            },
            {
                category["name"]: [
                    page["id"] for page in self.catalog["pages"]
                    if page["category"] == category["id"]
                ]
                for category in self.catalog["categories"]
            },
        )
        before = self.generator.mcm_document(self.catalog)
        regrouped = copy.deepcopy(self.catalog)
        regrouped["categories"].reverse()
        for page in regrouped["pages"]:
            page["category"] = regrouped["categories"][0]["id"]
        self.assertEqual(before, self.generator.mcm_document(regrouped))

    def test_category_errors_fail_closed(self):
        for mutation in ("duplicate", "unknown", "empty"):
            with self.subTest(mutation=mutation):
                catalog = copy.deepcopy(self.catalog)
                if mutation == "duplicate":
                    catalog["categories"].append(catalog["categories"][0])
                elif mutation == "unknown":
                    catalog["pages"][0]["category"] = "missing"
                else:
                    catalog["categories"].append({"id": "unused", "name": "Unused"})
                with self.assertRaises(ValueError):
                    self.generator.validate(catalog)

    def test_reimport_preserves_native_categories(self):
        imported = self.generator.import_existing()
        self.generator.validate(imported)
        self.assertEqual(self.catalog["categories"], imported["categories"])
        self.assertEqual(
            [(page["id"], page["category"]) for page in self.catalog["pages"]],
            [(page["id"], page["category"]) for page in imported["pages"]],
        )

    def test_unrecognized_and_invalid_entries_fail_closed(self):
        unknown = copy.deepcopy(self.catalog)
        unknown["settings"][0]["ui"]["surprise"] = True
        with self.assertRaises(ValueError):
            self.generator.validate(unknown)

        invalid_range = copy.deepcopy(self.catalog)
        slider = next(
            entry
            for entry in invalid_range["settings"]
            if entry.get("ui", {}).get("control") == "slider"
        )
        slider["ui"]["min"], slider["ui"]["max"] = 10, 1
        with self.assertRaises(ValueError):
            self.generator.validate(invalid_range)


if __name__ == "__main__":
    unittest.main()
