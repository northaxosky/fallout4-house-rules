# House Rules

An F4SE plugin that exposes high-impact vanilla Fallout 4 settings through a single MCM menu. The goal is to give players the tools they need to tune vanilla mechanics so they fit a modded playthrough - not to add new gameplay.

## Status

**v1.1.0** - adds the Keep Exit Save unlock + an AV-cache refresh so formula sliders take effect immediately. Self-test (`HRVERIFY`) reports 381 GMST targets PASS, 0 failed / 0 skipped across 12 modules.

## Compatibility

Validated on all three main Fallout 4 runtimes:

- OG 1.10.163 (pre-NG)
- NG 1.10.984
- AE 1.11.x (1.11.191 tested)

Requires **F4SE** and **Address Library for F4SE**. Mod Configuration Menu (MCM) is required for the settings UI.

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
- Sliders take effect when the pause menu closes; some Magnitude changes wait for the next consumable use.

## Design Principles

- **Vanilla-first.** No new mechanics; only exposes existing ones.
- **Yield to conflicts.** Other mods editing the same records win; House Rules is a floor, not a ceiling.
- **Right tool per feature.** Some settings are runtime hooks in the DLL (byte patches, composable MCM sliders), others are direct GMST / ActorValue / TESGlobal writes. The DLL is used when a setting must live-toggle, compose, or reach a target only accessible from native code.
- **Minimum dependencies.** F4SE + Address Library + MCM. No script extenders beyond F4SE core; no hard-required companion mods.

## Validation

A built-in self-test verifies every GMST target. Set `bValidationAudit=1` (and optionally `sValidationAuditMode=Full`) under `[Diagnostic]` in `Data/MCM/Config/HouseRules/settings.ini`, open and close the pause menu in-game, then run:

```sh
python tools/validate_house_rules_log.py
```

v1.1.0 ships with 0 failed / 0 skipped across 12 GMST modules (381 targets) on OG / NG / AE. Companions Affinity uses vanilla TESGlobal writes, so it doesn't show up in HRVERIFY; grep the plugin log for `Globals: wrote FormID ...` to verify those.

## Installation

1. Download the latest zip from the [Releases page](https://github.com/northaxosky/fallout4-house-rules/releases).
2. Install through your mod manager (MO2, Vortex) or unzip into `<Fallout 4>/Data/`.
3. Configure in-game under Settings > Mod Configuration Menu > House Rules.

## Build

Requires [xmake](https://xmake.io) 2.9.4+ on Windows with MSVC. CommonLibF4 is a git submodule.

```sh
git clone --recursive https://github.com/northaxosky/fallout4-house-rules.git
cd fallout4-house-rules
xmake config -m releasedbg
xmake build
```

Set `COMMONLIBF4_PATH` to reuse an existing CommonLibF4 checkout instead of initialising the submodule.

## License

MIT - see [LICENSE](LICENSE).
