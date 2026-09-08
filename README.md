# House Rules

An F4SE plugin that exposes high-impact vanilla Fallout 4 settings through either Mod Configuration Menu (MCM) or native Dear Modding UI pages. Both frontends use the same `HouseRules.dll`, settings files, validation, persistence, and gameplay application code. The goal is to give players the tools they need to tune vanilla mechanics so they fit a modded playthrough - not to add new gameplay.

## Status

**v1.2.0** - adds explicit support for Fallout 4 1.11.240 (AE), fixes the Re-enable Survival unlock on NG/AE, and adds a selectable native Dear Modding UI frontend without changing the gameplay settings format.

## Compatibility

The DLL declares support for these Fallout 4 runtimes:

- OG 1.10.163 (pre-NG)
- NG 1.10.984
- AE 1.11.137 - 1.11.240

The plugin declares an explicit runtime list rather than an open-ended one, so F4SE refuses to load it on an unlisted runtime instead of trusting unverified addresses. Every hook call site is audited against the available game binaries with `tools/audit_hook_offsets.py`. This frontend change was built and statically validated, but it does not claim a fresh in-game `HRVERIFY` pass on every listed runtime.

Requires **F4SE** and **Address Library for F4SE**. Choose one optional settings UI when installing:

- **MCM (default/backward-compatible):** requires [Mod Configuration Menu](https://www.nexusmods.com/fallout4/mods/21497).
- **Native Dear Modding UI:** requires a matching standalone Dear Modding UI host. If the host is absent or incompatible, the DLL logs the reason and continues headless with saved gameplay settings; it does not pretend an MCM page is available.

The native option must pair the House Rules build with the Dear Modding UI host/API revision linked by this repository's CommonLibF4 submodule. An older or newer incompatible host is reported in `HouseRules.log` as a native connection failure.

## Features

The full per-slider reference (defaults, units, behavior notes) lives in [docs/FEATURES.md](docs/FEATURES.md).

- **Survival Unlocks** - opt-in toggles for console, manual / auto saves, fast travel, compass enemies / locations, chem / ammo weight, carry weight, god mode, re-entering Survival, and keeping the exit save on load.
- **Survival** - kill-switches for the four Hardcore subsystems (Hunger/Thirst, Sleep, Diseases, Adrenaline) plus ~38 tuning sliders that write `Hardcore:HC_ManagerScript` Papyrus properties.
- **Magnitudes** - stimpak / limb repair / RadAway / Rad-X / food healing and hunger / thirst / sleep penalty severity.
- **Difficulty I** - per-tier incoming / outgoing damage, XP base + Intelligence bonus, legendary chance / rarity.
- **Difficulty II** - per-tier effect duration and effect magnitude.
- **Character** - AP pool, sprint cost, carry capacity, max-health scaling, AP / passive / combat health regen.
- **Damage Formulas** - radiation, physical, and energy damage factor + armor exponent.
- **Power Armor & Jetpack** - jetpack drain / thrust, fusion-core drain, player / NPC PA durability.
- **Economy** - barter min / max floors, buy / sell multiplier caps.
- **Progression** - cooking / workbench / workshop XP, lockpick rewards, mine disarm XP.
- **VATS** - max engage distance, target-select time scale, player damage mult.
- **Skills** - pickpocket, hacking, lockpicking.
- **Sneak** - sneak attack multipliers, exterior / light / max detection.
- **Combat Perks** - disarm / stagger / knockdown / paralyze chances and Light / Heavy Armor perk-tier multipliers.
- **Settlements** - workshop build / repair / wire timers, settler population cap, placement-radius constraints.
- **Companions Affinity** - the nine vanilla TESGlobals that drive per-reaction affinity deltas and event cooldowns.

Two caveats to know about:

- **Carry-weight unlock** and the **Survival kill-switches** require a save reload after toggling for immediate effect; otherwise they apply on the script's next tick (a few in-game minutes).
- MCM edits take effect when the pause menu closes. Native edits remain pending until **Apply**, are saved to the same user override INI, and are dispatched to the game thread. If a save is not ready, gameplay mutation is deferred until `LoadingMenu` closes.
- Some Magnitude changes wait for the next consumable use.

## Design Principles

- **Vanilla-first.** No new mechanics; only exposes existing ones.
- **Yield to conflicts.** Other mods editing the same records win; House Rules is a floor, not a ceiling.
- **One authoritative settings layer.** The native and MCM adapters are generated from `settings/catalog.json` and share the installed defaults and user overrides at `Data/MCM/Config/HouseRules/settings.ini` and `Data/MCM/Settings/HouseRules.ini`.
- **Right tool per feature.** Some settings are runtime hooks in the DLL (byte patches, composable sliders), others are direct GMST / ActorValue / TESGlobal writes. The DLL is used when a setting must live-toggle, compose, or reach a target only accessible from native code.
- **Optional UI dependency.** F4SE and Address Library are required; install either MCM or Dear Modding UI for an in-game settings page.

## Validation

A built-in self-test verifies every GMST target. Set `bValidationAudit=1` (and optionally `sValidationAuditMode=Full`) under `[Diagnostic]` in `Data/MCM/Config/HouseRules/settings.ini`, open and close the pause menu in-game, then run:

```sh
python tools/validate_house_rules_log.py
```

`HRVERIFY` reports the active GMST writer set for the runtime and configuration being tested. Do not treat an older target count as proof of universal coverage on another executable. Companions Affinity uses vanilla TESGlobal writes, so it does not appear in `HRVERIFY`; grep the plugin log for `Globals: wrote FormID ...` to verify those.

## Installation

1. Download the latest zip from the [Releases page](https://github.com/northaxosky/fallout4-house-rules/releases).
2. Install it through a FOMOD-capable mod manager and choose exactly one frontend. **Mod Configuration Menu** installs `config.json` and `lib.swf`; **Dear Modding UI** installs the native selector and omits those bridge-discoverable MCM page assets.
3. Restart Fallout 4 after changing frontend choice. Frontend registration is startup-only.

Both choices install the same DLL/PDB and shared packaged defaults. Neither package includes or overwrites `Data/MCM/Settings/HouseRules.ini`, so existing user overrides remain compatible.

The startup selector is `Data/F4SE/Plugins/HouseRules.frontend.ini`, using `[Interface]` and `Frontend=mcm` or `Frontend=dmui`. Edit or replace it only while Fallout 4 is stopped, then restart the game.

When switching an existing installation, use your mod manager's **replace/remove old files** behavior rather than merging. In particular, remove stale `Data/MCM/Config/HouseRules/config.json` and `lib.swf` before selecting native Dear Modding UI, or the separate DMUI-MCM bridge can import a duplicate MCM page. The DLL never deletes user files at runtime.

Manual in-game checks:

1. Confirm `HouseRules.log` names the selected `mcm` or `dmui` frontend.
2. For native UI, verify all 16 pages appear, edit a value, confirm pending feedback, select Apply, and reload the game to confirm persistence.
3. Apply once at the main menu or during loading and confirm the UI says gameplay changes are deferred until a save is ready.
4. Temporarily remove/disable the Dear Modding UI host with the native payload and confirm House Rules logs a clear headless-mode diagnostic while gameplay settings still load.
5. For MCM, change a value and close the pause menu to confirm the legacy reload path still applies it.

## Build

Requires [xmake](https://xmake.io) 3.0.0+ on Windows with MSVC. CommonLibF4 is a git submodule.

```sh
git clone --recursive https://github.com/northaxosky/fallout4-house-rules.git
cd fallout4-house-rules
xmake config -m releasedbg
xmake build
xmake run HouseRulesRuntimeTests
python tools/generate_settings.py --check
python -m unittest discover -s tests -v
```

Set `COMMONLIBF4_PATH` to reuse an existing CommonLibF4 checkout instead of initialising the submodule.

The repository's authoritative, ready-to-zip Nexus archive root is
`package/`. A `releasedbg` build copies `HouseRules.dll` and the required PDB
into `package/core/F4SE/Plugins/`, then validates both choices by reading the
tracked `package/fomod/ModuleConfig.xml`. Zip the **contents** of `package/`,
not the `package` directory itself. The tracked generated defaults live under
`package/core/`, while MCM-only `config.json`/`lib.swf` assets and both tracked
frontend selectors live under `package/frontends/`.

Only `releasedbg` builds populate upload binaries; debug builds are for local
development and are never copied into the release package. Run
`python tools/package_release.py --dll build/windows/x64/releasedbg/HouseRules.dll --pdb build/windows/x64/releasedbg/HouseRules.pdb`
to prepare and validate the archive root explicitly.

Development deployment defaults to the MCM frontend. Use
`xmake f -m releasedbg --deploy_frontend=dmui` for native deployment. A native
deploy refuses to proceed if stale `config.json`/`lib.swf` files are already in
the destination; clean the dedicated dev mod folder manually instead of asking
the build to delete external files. Deployment still uses installed `Data/`
paths, and both frontend choices continue to share
`Data/MCM/Settings/HouseRules.ini` without packaging or overwriting that user
override.

## License

MIT - see [LICENSE](LICENSE).
