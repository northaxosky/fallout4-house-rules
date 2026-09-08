# HouseRules.esp regeneration (dormant)

**Status: not used in current builds.** As of v0.5.0 House Rules ships as a
DLL only; the prior carry-weight gate is now a runtime SPEL-magnitude
mutation (`Tweaks::SurvivalCarryWeight`) instead of a condition-overridden
record. The pipeline below is kept as a template in case a future feature
needs condition-gated record overrides that can't be done at runtime.

The original ESP contained:

- `HR_NoCarryWeight` GLOB (type: Float, default 0.0)
- Override of `HC_ReduceCarryWeightAbility` [SPEL:00249F6A] with one extra
  condition on each of its 3 effects:
  `GetGlobalValue HR_NoCarryWeight == 0`

The Pascal script `HR_BuildHouseRulesESP.pas` builds an ESP deterministically.
The save step is manual because xEdit script mode is not compatible with
`-quickedit`, `-P:` still leaves the Module Selection flow exposed, and
automating xEdit's modal save dialogs is flaky.

## Fast path

From the repo root:

```powershell
.\tools\esp\build-esp-manual.ps1
```

The helper copies the Pascal script into FO4Edit, launches FO4Edit, prints the
manual clicks to perform, then waits. After you save `HouseRules.esp` and press
Enter in the PowerShell window, it copies the ESP into `package\core\` and runs
`xmake build` so the optional core asset deploys with the mod.

## Manual procedure

1. Copy the scripts into FO4Edit's Edit Scripts folder:

   ```powershell
   Copy-Item tools/esp/HR_*.pas "C:/Games/Modding/FO4Tools/FO4Edit 4.1.5f/Edit Scripts/"
   ```

2. Launch FO4Edit via the normal MO2 / desktop shortcut.
3. In the Module Selection dialog, select only `Fallout4.esm`. Click OK.
4. When loading finishes, right-click `Fallout4.esm` in the tree > Apply
   Script > `HR_BuildHouseRulesESP`.
5. The script's progress log at
   `C:/Games/Modding/FO4Tools/FO4Edit 4.1.5f/HR_BuildProgress.log`
   should end with `step 4 done -- sentinel written`.
6. Close the FO4Script result window. File > Close. When prompted to save,
   check `HouseRules.esp` and click OK / Yes. FO4Edit will write the installed
   game file at `Data/HouseRules.esp`.
7. Flag the ESP as ESL to avoid consuming a load-order slot (optional but
   recommended):
   - Open `HouseRules.esp` in FO4Edit > File Header > Record Header > Record
     Flags > check `ESL` > save.
8. Copy the saved ESP into the repo:

   ```powershell
   Copy-Item "C:/games/steam/SteamApps/common/Fallout 4/Data/HouseRules.esp" `
             "C:/Users/Kuz/Documents/Projects/Fallout/HouseRules/package/core/HouseRules.esp"
   ```

9. Run `xmake build` to deploy the ESP with the mod.
10. Commit the updated `package/core/HouseRules.esp`.

## Verification

After regenerating, re-open the ESP in FO4Edit and spot-check:

- `GLOB > HR_NoCarryWeight` exists, FLTV = 0.0.
- `SPEL > HC_ReduceCarryWeightAbility` override has 3 effects, each with at
  least one CTDA referencing `GetGlobalValue HR_NoCarryWeight == 0`.

## Files in this directory

- `HR_BuildHouseRulesESP.pas` — the builder. Creates or updates
  `HouseRules.esp` with the GLOB + condition-gated SPEL override.
- `HR_InspectSurvivalOptions.pas` — diagnostic dumper for SurvivalOptions'
  own condition-gated pattern. Reference only; run manually from FO4Edit if
  needed.
- `build-esp-manual.ps1` — recommended helper for this build flow. It launches
  FO4Edit for the manual script/save step, then copies and deploys the ESP.
