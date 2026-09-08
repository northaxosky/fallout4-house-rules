{
  House Rules -- generates HouseRules.esp from scratch.

  Phase 2 scope: one global (HR_NoCarryWeight), one condition-override of
  HC_ReduceCarryWeightAbility's effects. When HR_NoCarryWeight == 0 (default),
  vanilla behavior; when == 1, the conditions fail and the 50/125/125 carry
  weight reduction effects don't apply.

  This follows SurvivalOptions' pattern (verified via HR_InspectSurvivalOptions)
  WITHOUT forking HC_ManagerScript -- we only add a condition block to the
  existing vanilla effects. Any mod that edits the SPEL itself will conflict,
  but mods that edit HC_ManagerScript or other survival machinery won't.

  Invoke:
    tools\esp\run-fo4edit.ps1 -Script HR_BuildHouseRulesESP.pas -OutMod HouseRules.esp

  xEdit output: the installed game Data\HouseRules.esp. Copy the result into
  package\core\HouseRules.esp for a future shipped package.
}
unit UserScript;

const
  HOUSE_RULES_ESP = 'HouseRules.esp';
  // Vanilla carry-weight spell
  CARRY_SPEL_FORMID = $00249F6A;

var
  housEsp: IInterface;
  progressLog: TStringList;

procedure LogProgress(msg: string);
begin
  AddMessage('[build] ' + msg);
  if Assigned(progressLog) then begin
    progressLog.Add(msg);
    progressLog.SaveToFile(ProgramPath + 'HR_BuildProgress.log');
  end;
end;

// Find or create HouseRules.esp. -Mod: makes xEdit auto-create + preselect it.
function GetOrCreateEsp: IInterface;
var
  i: integer;
  f: IInterface;
begin
  Result := nil;
  for i := 0 to FileCount - 1 do begin
    f := FileByIndex(i);
    if SameText(GetFileName(f), HOUSE_RULES_ESP) then begin
      Result := f;
      AddMessage('[build] found existing ' + HOUSE_RULES_ESP);
      Exit;
    end;
  end;
  Result := AddNewFileName(HOUSE_RULES_ESP, False);
  if Assigned(Result) then begin
    AddMessage('[build] created ' + HOUSE_RULES_ESP);
    // Flag as ESL so it doesn't eat a load-order slot
    SetElementNativeValues(ElementByPath(Result, 'File Header\Record Header\Record Flags'), 'ESL', 1);
  end;
end;

// Find a GLOB with given EDID in target file, create if missing.
function EnsureGlobal(f: IInterface; edid: string; defaultVal: single): IInterface;
var
  i: integer;
  grp, rec: IInterface;
begin
  LogProgress('  EnsureGlobal: GroupBySignature GLOB');
  grp := GroupBySignature(f, 'GLOB');
  if Assigned(grp) then begin
    for i := 0 to ElementCount(grp) - 1 do begin
      rec := ElementByIndex(grp, i);
      if SameText(GetElementEditValues(rec, 'EDID'), edid) then begin
        LogProgress('  EnsureGlobal: ' + edid + ' already present');
        Result := rec;
        Exit;
      end;
    end;
  end;
  if not Assigned(grp) then begin
    LogProgress('  EnsureGlobal: Add(f, GLOB)');
    grp := Add(f, 'GLOB', True);
  end;
  LogProgress('  EnsureGlobal: Add(grp, GLOB)');
  rec := Add(grp, 'GLOB', True);
  LogProgress('  EnsureGlobal: set EDID');
  SetElementEditValues(rec, 'EDID', edid);
  LogProgress('  EnsureGlobal: set FLTV');
  SetElementNativeValues(rec, 'FLTV', defaultVal);
  LogProgress('  EnsureGlobal: done');
  Result := rec;
end;

// Add a GetGlobalValue condition to an effect's CTDA block.
// compValue: value the global must match for the effect to apply (usually 0 = default/off).
// op: 10000000 = Equal, 20000000 = NotEqual, etc. 0x00 = OR, 0x01 = combine following CTDAs with AND into one logical unit.
procedure AddGlobalCondition(effect: IInterface; glob: IInterface; compValue: single);
var
  conds, ctda: IInterface;
begin
  conds := ElementByPath(effect, 'Conditions');
  if not Assigned(conds) then
    conds := Add(effect, 'Conditions', True);

  ctda := ElementAssign(conds, HighInteger, nil, False);

  // Type defaults to 0 (Equal to + AND) on Assign -- leave it.
  // Function enum: GetGlobalValue.
  SetElementEditValues(ctda, 'CTDA\Function', 'GetGlobalValue');
  // Comparison Value (union; Float branch).
  SetElementNativeValues(ctda, 'CTDA\Comparison Value - Float', compValue);
  // Parameter #1 for GetGlobalValue is the GLOB FormID.
  SetElementNativeValues(ctda, 'CTDA\Global', GetLoadOrderFormID(glob));
  SetElementEditValues(ctda, 'CTDA\Run On', 'Subject');
end;

// Override a vanilla SPEL into our ESP (deep copy as override), then add our
// global condition to each effect.
function OverrideSpellWithGate(vanillaFormID: cardinal; glob: IInterface; gateValue: single): IInterface;
var
  i: integer;
  vanillaFile, rec, override, effects, effect: IInterface;
begin
  Result := nil;
  LogProgress('  Override: FileByIndex(0)');
  vanillaFile := FileByIndex(0);
  LogProgress('  Override: master=' + GetFileName(vanillaFile));
  LogProgress('  Override: AddMasterIfMissing Fallout4.esm');
  AddMasterIfMissing(housEsp, GetFileName(vanillaFile));
  LogProgress('  Override: RecordByFormID ' + IntToHex(vanillaFormID, 8));
  rec := RecordByFormID(vanillaFile, vanillaFormID, True);
  if not Assigned(rec) then begin
    LogProgress('  Override: ERROR SPEL not found');
    Exit;
  end;
  LogProgress('  Override: found ' + Name(rec));
  LogProgress('  Override: wbCopyElementToFile');
  override := wbCopyElementToFile(rec, housEsp, False, True);
  if not Assigned(override) then begin
    LogProgress('  Override: ERROR copy failed');
    Exit;
  end;
  LogProgress('  Override: copied, iterating effects');

  effects := ElementByPath(override, 'Effects');
  if not Assigned(effects) then begin
    LogProgress('  Override: no Effects block');
    Result := override;
    Exit;
  end;

  for i := 0 to ElementCount(effects) - 1 do begin
    LogProgress('  Override: effect #' + IntToStr(i) + ' AddGlobalCondition');
    effect := ElementByIndex(effects, i);
    AddGlobalCondition(effect, glob, gateValue);
  end;

  Result := override;
end;

function Initialize: integer;
var
  noCarryGlob: IInterface;
  sl: TStringList;
begin
  progressLog := TStringList.Create;
  LogProgress('=== HR_BuildHouseRulesESP start ===');

  LogProgress('step 1: GetOrCreateEsp');
  housEsp := GetOrCreateEsp;
  if not Assigned(housEsp) then begin
    LogProgress('FATAL: could not create or locate ' + HOUSE_RULES_ESP);
    Result := 1;
    Exit;
  end;
  LogProgress('step 1 done, esp=' + GetFileName(housEsp));

  LogProgress('step 2: EnsureGlobal HR_NoCarryWeight');
  noCarryGlob := EnsureGlobal(housEsp, 'HR_NoCarryWeight', 0.0);
  LogProgress('step 2 done, glob=' + Name(noCarryGlob));

  LogProgress('step 3: OverrideSpellWithGate');
  OverrideSpellWithGate(CARRY_SPEL_FORMID, noCarryGlob, 0.0);
  LogProgress('step 3 done');

  LogProgress('step 4: write sentinel (file gets saved on FO4Edit exit)');
  sl := TStringList.Create;
  try
    sl.Add('done');
    sl.SaveToFile(ProgramPath + 'HR_LastScriptDone.txt');
  finally
    sl.Free;
  end;
  LogProgress('step 4 done -- sentinel written');

  // Return 1 so FO4Edit SKIPS Process loop on every record (would take minutes on 500K+).
  Result := 1;
end;

function Process(e: IInterface): integer; begin Result := 0; end;
function Finalize: integer; begin Result := 0; end;

end.
