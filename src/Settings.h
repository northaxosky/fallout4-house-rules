#pragma once

namespace HouseRules
{
	class Settings
	{
	public:
		class General
		{
		public:
			inline static REX::INI::Bool<> bEnabled{ "General", "bEnabled", true };
		};

		// All toggles: OFF = vanilla Survival behavior (default), ON = tweak applied.
		class Unlocks
		{
		public:
			inline static REX::INI::Bool<> bConsole{ "Unlocks", "bConsole", false };
			inline static REX::INI::Bool<> bFastTravel{ "Unlocks", "bFastTravel", false };
			inline static REX::INI::Bool<> bGodMode{ "Unlocks", "bGodMode", false };
			inline static REX::INI::Bool<> bSaveAuto{ "Unlocks", "bSaveAuto", false };
			inline static REX::INI::Bool<> bSaveSelf{ "Unlocks", "bSaveSelf", false };
			inline static REX::INI::Bool<> bCompassEnemies{ "Unlocks", "bCompassEnemies", false };
			inline static REX::INI::Bool<> bCompassLocations{ "Unlocks", "bCompassLocations", false };
			inline static REX::INI::Bool<> bNoAlchWeight{ "Unlocks", "bNoAlchWeight", false };
			inline static REX::INI::Bool<> bNoAmmoWeight{ "Unlocks", "bNoAmmoWeight", false };
			inline static REX::INI::Bool<> bNoSurvivalCarryWeight{ "Unlocks", "bNoSurvivalCarryWeight", false };
			inline static REX::INI::Bool<> bReenableSurvival{ "Unlocks", "bReenableSurvival", false };
			inline static REX::INI::Bool<> bKeepExitSave{ "Unlocks", "bKeepExitSave", false };
		};

		// Runtime magnitude settings. 1.0 = vanilla for implemented paths; some
		// reserved entries remain inert until backed by runtime evidence.
		class Magnitudes
		{
		public:
			inline static REX::INI::F32<> fStimpakHeal      { "Magnitudes", "fStimpakHeal",       1.0f };
			inline static REX::INI::F32<> fStimpakLimbRepair{ "Magnitudes", "fStimpakLimbRepair", 1.0f };
			inline static REX::INI::F32<> fRadAwayStrength  { "Magnitudes", "fRadAwayStrength",   1.0f };
			inline static REX::INI::F32<> fRadXPotency      { "Magnitudes", "fRadXPotency",       1.0f };
			inline static REX::INI::F32<> fFoodHeal         { "Magnitudes", "fFoodHeal",          1.0f };
			inline static REX::INI::F32<> fHungerPenalty    { "Magnitudes", "fHungerPenalty",     1.0f };
			inline static REX::INI::F32<> fThirstPenalty    { "Magnitudes", "fThirstPenalty",     1.0f };
			inline static REX::INI::F32<> fSleepPenalty     { "Magnitudes", "fSleepPenalty",      1.0f };
		};

		// Per-difficulty multipliers for vanilla GMSTs. 1.0 = vanilla. Survival
		// here means the current TSV difficulty; legacy SV GMSTs are skipped.
		class Difficulty
		{
		public:
			// Incoming damage (player damage taken)
			inline static REX::INI::F32<> fIncomingDamageVeryEasy{ "Difficulty", "fIncomingDamageVeryEasy", 1.0f };
			inline static REX::INI::F32<> fIncomingDamageEasy    { "Difficulty", "fIncomingDamageEasy",     1.0f };
			inline static REX::INI::F32<> fIncomingDamageNormal  { "Difficulty", "fIncomingDamageNormal",   1.0f };
			inline static REX::INI::F32<> fIncomingDamageHard    { "Difficulty", "fIncomingDamageHard",     1.0f };
			inline static REX::INI::F32<> fIncomingDamageVeryHard{ "Difficulty", "fIncomingDamageVeryHard", 1.0f };
			inline static REX::INI::F32<> fIncomingDamageSurvival{ "Difficulty", "fIncomingDamageSurvival", 1.0f };

			// Outgoing damage (player damage dealt)
			inline static REX::INI::F32<> fOutgoingDamageVeryEasy{ "Difficulty", "fOutgoingDamageVeryEasy", 1.0f };
			inline static REX::INI::F32<> fOutgoingDamageEasy    { "Difficulty", "fOutgoingDamageEasy",     1.0f };
			inline static REX::INI::F32<> fOutgoingDamageNormal  { "Difficulty", "fOutgoingDamageNormal",   1.0f };
			inline static REX::INI::F32<> fOutgoingDamageHard    { "Difficulty", "fOutgoingDamageHard",     1.0f };
			inline static REX::INI::F32<> fOutgoingDamageVeryHard{ "Difficulty", "fOutgoingDamageVeryHard", 1.0f };
			inline static REX::INI::F32<> fOutgoingDamageSurvival{ "Difficulty", "fOutgoingDamageSurvival", 1.0f };

			// XP progression
			inline static REX::INI::F32<> fXPBase{ "Difficulty", "fXPBase", 1.0f };
			inline static REX::INI::F32<> fXPMult{ "Difficulty", "fXPMult", 1.0f };

			// Legendary chance
			inline static REX::INI::F32<> fLegendaryChanceVeryEasy{ "Difficulty", "fLegendaryChanceVeryEasy", 1.0f };
			inline static REX::INI::F32<> fLegendaryChanceEasy    { "Difficulty", "fLegendaryChanceEasy",     1.0f };
			inline static REX::INI::F32<> fLegendaryChanceNormal  { "Difficulty", "fLegendaryChanceNormal",   1.0f };
			inline static REX::INI::F32<> fLegendaryChanceHard    { "Difficulty", "fLegendaryChanceHard",     1.0f };
			inline static REX::INI::F32<> fLegendaryChanceVeryHard{ "Difficulty", "fLegendaryChanceVeryHard", 1.0f };
			inline static REX::INI::F32<> fLegendaryChanceSurvival{ "Difficulty", "fLegendaryChanceSurvival", 1.0f };

			// Legendary rarity
			inline static REX::INI::F32<> fLegendaryRarityVeryEasy{ "Difficulty", "fLegendaryRarityVeryEasy", 1.0f };
			inline static REX::INI::F32<> fLegendaryRarityEasy    { "Difficulty", "fLegendaryRarityEasy",     1.0f };
			inline static REX::INI::F32<> fLegendaryRarityNormal  { "Difficulty", "fLegendaryRarityNormal",   1.0f };
			inline static REX::INI::F32<> fLegendaryRarityHard    { "Difficulty", "fLegendaryRarityHard",     1.0f };
			inline static REX::INI::F32<> fLegendaryRarityVeryHard{ "Difficulty", "fLegendaryRarityVeryHard", 1.0f };
			inline static REX::INI::F32<> fLegendaryRaritySurvival{ "Difficulty", "fLegendaryRaritySurvival", 1.0f };

			// Effect duration (Difficulty II). Multiplies the per-difficulty
			// effect-duration GMST. Survival's TSV baseline is much longer
			// than Normal, so 1.00 here means "vanilla Survival baseline".
			inline static REX::INI::F32<> fEffectDurationVeryEasy{ "Difficulty", "fEffectDurationVeryEasy", 1.0f };
			inline static REX::INI::F32<> fEffectDurationEasy    { "Difficulty", "fEffectDurationEasy",     1.0f };
			inline static REX::INI::F32<> fEffectDurationNormal  { "Difficulty", "fEffectDurationNormal",   1.0f };
			inline static REX::INI::F32<> fEffectDurationHard    { "Difficulty", "fEffectDurationHard",     1.0f };
			inline static REX::INI::F32<> fEffectDurationVeryHard{ "Difficulty", "fEffectDurationVeryHard", 1.0f };
			inline static REX::INI::F32<> fEffectDurationSurvival{ "Difficulty", "fEffectDurationSurvival", 1.0f };

			// Effect magnitude (Difficulty II). Multiplies the per-difficulty
			// effect-magnitude GMST. Survival's TSV baseline is much smaller
			// than Normal, so 1.00 here means "vanilla Survival baseline".
			inline static REX::INI::F32<> fEffectMagnitudeVeryEasy{ "Difficulty", "fEffectMagnitudeVeryEasy", 1.0f };
			inline static REX::INI::F32<> fEffectMagnitudeEasy    { "Difficulty", "fEffectMagnitudeEasy",     1.0f };
			inline static REX::INI::F32<> fEffectMagnitudeNormal  { "Difficulty", "fEffectMagnitudeNormal",   1.0f };
			inline static REX::INI::F32<> fEffectMagnitudeHard    { "Difficulty", "fEffectMagnitudeHard",     1.0f };
			inline static REX::INI::F32<> fEffectMagnitudeVeryHard{ "Difficulty", "fEffectMagnitudeVeryHard", 1.0f };
			inline static REX::INI::F32<> fEffectMagnitudeSurvival{ "Difficulty", "fEffectMagnitudeSurvival", 1.0f };
		};

		// Character: AP pool/regen + sprint-drain sliders. Defaults
		// reflect vanilla; with preserveBaselineAtNeutral, Direct entries
		// preserve any baseline edits from other mods until the user moves
		// the slider off the vanilla default.
		class Character
		{
		public:
			// AP pool / regen delays
			inline static REX::INI::F32<> fAPBaseMult             { "Character", "fAPBaseMult",              1.0f };
			inline static REX::INI::F32<> fAPPerAgilityMult       { "Character", "fAPPerAgilityMult",        1.0f };
			inline static REX::INI::F32<> fAPRegenRate            { "Character", "fAPRegenRate",             6.0f };
			inline static REX::INI::F32<> fCombatAPRegenMult      { "Character", "fCombatAPRegenMult",       0.75f };
			inline static REX::INI::F32<> fAPRegenDelay           { "Character", "fAPRegenDelay",            1.0f };
			inline static REX::INI::F32<> fOutOfBreathAPRegenDelay{ "Character", "fOutOfBreathAPRegenDelay", 2.0f };
			inline static REX::INI::F32<> fAPRegenDelayMax        { "Character", "fAPRegenDelayMax",         5.0f };

			// Sprint formula
			inline static REX::INI::F32<> fSprintBaseDrain        { "Character", "fSprintBaseDrain",         1.05f };
			inline static REX::INI::F32<> fSprintEnduranceDrain   { "Character", "fSprintEnduranceDrain",   -0.05f };
			inline static REX::INI::F32<> fSprintDrainMult        { "Character", "fSprintDrainMult",         1.0f };

			// Carry weight (Multiplier mode; 1.0 = vanilla)
			inline static REX::INI::F32<> fCarryWeightBaseMult       { "Character", "fCarryWeightBaseMult",       1.0f };
			inline static REX::INI::F32<> fCarryWeightPerStrengthMult{ "Character", "fCarryWeightPerStrengthMult",1.0f };

			// Health scaling (Multiplier mode; 1.0 = vanilla)
			inline static REX::INI::F32<> fHealthPerEnduranceMult    { "Character", "fHealthPerEnduranceMult",    1.0f };
			inline static REX::INI::F32<> fHealthPerLevelMult        { "Character", "fHealthPerLevelMult",        1.0f };
			inline static REX::INI::F32<> fPassiveHealthRegen        { "Character", "fPassiveHealthRegen",        0.0f };
			inline static REX::INI::F32<> fCombatHealthRegenMult     { "Character", "fCombatHealthRegenMult",     0.0f };

		};

		// Damage Formulas: raw damage factors and armor reduction
		// exponents for radiation, physical, and energy damage. All Direct
		// mode with vanilla defaults; preserveBaselineAtNeutral keeps any
		// baseline edits from other mods until the user moves a slider off
		// the vanilla default.
		class DamageFormulas
		{
		public:
			// Radiation
			inline static REX::INI::F32<> fRadiationDamageFactor       { "DamageFormulas", "fRadiationDamageFactor",        0.15f };
			inline static REX::INI::F32<> fRadiationArmorReductionExponent{ "DamageFormulas", "fRadiationArmorReductionExponent", 0.365f };

			// Physical
			inline static REX::INI::F32<> fPhysicalDamageFactor        { "DamageFormulas", "fPhysicalDamageFactor",         0.15f };
			inline static REX::INI::F32<> fPhysicalArmorReductionExponent{ "DamageFormulas", "fPhysicalArmorReductionExponent", 0.365f };

			// Energy
			inline static REX::INI::F32<> fEnergyDamageFactor          { "DamageFormulas", "fEnergyDamageFactor",           0.15f };
			inline static REX::INI::F32<> fEnergyArmorReductionExponent{ "DamageFormulas", "fEnergyArmorReductionExponent", 0.365f };
		};

		// Power Armor: Jetpack, fusion-core drain, and PA
		// durability GMSTs. Multiplier defaults at 1.0; Direct defaults
		// match each vanilla GMST baseline so preserveBaselineAtNeutral
		// keeps other mods' edits until the user moves the slider off.
		class PowerArmor
		{
		public:
			// Jetpack (Multiplier mode; 1.0 = vanilla)
			inline static REX::INI::F32<> fJetpackInitialDrainMult   { "PowerArmor", "fJetpackInitialDrainMult",    1.0f };
			inline static REX::INI::F32<> fJetpackSustainedDrainMult { "PowerArmor", "fJetpackSustainedDrainMult",  1.0f };
			inline static REX::INI::F32<> fJetpackInitialThrustMult  { "PowerArmor", "fJetpackInitialThrustMult",   1.0f };
			inline static REX::INI::F32<> fJetpackSustainedThrustMult{ "PowerArmor", "fJetpackSustainedThrustMult", 1.0f };

			// Jetpack (Direct mode; vanilla defaults as neutral)
			inline static REX::INI::F32<> fJetpackMinFuelRequired    { "PowerArmor", "fJetpackMinFuelRequired",     3.0f };
			inline static REX::INI::F32<> fJetpackTimeToSustained    { "PowerArmor", "fJetpackTimeToSustained",     0.15f };

			// Fusion core drain (Multiplier mode; 1.0 = vanilla)
			inline static REX::INI::F32<> fPAPowerDrainPerAPMult     { "PowerArmor", "fPAPowerDrainPerAPMult",      1.0f };
			inline static REX::INI::F32<> fPAPowerDrainRunningMult   { "PowerArmor", "fPAPowerDrainRunningMult",    1.0f };

			// Fusion core drain (Direct mode; vanilla 0)
			inline static REX::INI::F32<> fPAPowerDrainPerJump       { "PowerArmor", "fPAPowerDrainPerJump",        0.0f };
			inline static REX::INI::F32<> fPAPowerDrainPerMeleeAttack{ "PowerArmor", "fPAPowerDrainPerMeleeAttack", 0.0f };
			inline static REX::INI::F32<> fPAPowerDrainPerImpactLand { "PowerArmor", "fPAPowerDrainPerImpactLand",  0.0f };

			// Durability (Direct mode; vanilla defaults as neutral)
			inline static REX::INI::F32<> fPAPlayerArmorDamageMultiplier{ "PowerArmor", "fPAPlayerArmorDamageMultiplier", 1.0f };
			inline static REX::INI::F32<> fPANPCArmorDamageMultiplier   { "PowerArmor", "fPANPCArmorDamageMultiplier",    3.0f };
		};

		// Economy: Buy/sell pricing GMSTs. All Direct mode with
		// vanilla defaults as neutral; preserveBaselineAtNeutral keeps
		// any other mod's edits until the user moves a slider off the
		// vanilla default.
		class Economy
		{
		public:
			// Barter pricing (vendor buy/sell multipliers)
			inline static REX::INI::F32<> fBarterMin    { "Economy", "fBarterMin",     2.0f };
			inline static REX::INI::F32<> fBarterMax    { "Economy", "fBarterMax",     3.5f };
			inline static REX::INI::F32<> fBarterBuyMax { "Economy", "fBarterBuyMax",  1.2f };
			inline static REX::INI::F32<> fBarterSellMax{ "Economy", "fBarterSellMax", 0.8f };

			// Persuasion (Charisma speech-check) outcome bounds
			inline static REX::INI::F32<> fSpeechcraftFavorMin{ "Economy", "fSpeechcraftFavorMin", 0.5f };
			inline static REX::INI::F32<> fSpeechcraftFavorMax{ "Economy", "fSpeechcraftFavorMax", 1.0f };
		};

		// Progression: Non-difficulty XP source GMSTs (cooking,
		// workbench, workshop, lockpick rewards, mine disarm. All Direct
		// mode with vanilla defaults as neutral.
		class Progression
		{
		public:
			// Cooking
			inline static REX::INI::F32<> fCookingExpBase{ "Progression", "fCookingExpBase",  1.0f  };
			inline static REX::INI::F32<> fCookingExpMax { "Progression", "fCookingExpMax",  10.0f  };
			inline static REX::INI::F32<> fCookingExpMult{ "Progression", "fCookingExpMult",  0.15f };

			// Weapon/Armor Workbench
			inline static REX::INI::F32<> fWorkbenchExpBase{ "Progression", "fWorkbenchExpBase",  2.0f  };
			inline static REX::INI::F32<> fWorkbenchExpMax { "Progression", "fWorkbenchExpMax",  50.0f  };
			inline static REX::INI::F32<> fWorkbenchExpMult{ "Progression", "fWorkbenchExpMult",  0.03f };

			// Settlement Workshop
			inline static REX::INI::F32<> fWorkshopExpBase{ "Progression", "fWorkshopExpBase",  2.0f  };
			inline static REX::INI::F32<> fWorkshopExpMax { "Progression", "fWorkshopExpMax",  25.0f  };
			inline static REX::INI::F32<> fWorkshopExpMult{ "Progression", "fWorkshopExpMult",  0.10f };

			// Lockpick rewards (XP per successful pick at each tier)
			inline static REX::INI::F32<> fLockpickXPApprentice{ "Progression", "fLockpickXPApprentice",  5.0f };
			inline static REX::INI::F32<> fLockpickXPAdept     { "Progression", "fLockpickXPAdept",      10.0f };
			inline static REX::INI::F32<> fLockpickXPExpert    { "Progression", "fLockpickXPExpert",     15.0f };
			inline static REX::INI::F32<> fLockpickXPMaster    { "Progression", "fLockpickXPMaster",     20.0f };

			// Mine disarm (int)
			inline static REX::INI::I32<> iMineDisarmXP{ "Progression", "iMineDisarmXP", 5 };

			// Kill / discovery XP rewards
			inline static REX::INI::I32<> iXPRewardKillOpponent     { "Progression", "iXPRewardKillOpponent",      20 };
			inline static REX::INI::I32<> iXPRewardDiscoverMapMarker{ "Progression", "iXPRewardDiscoverMapMarker", 20 };
			inline static REX::INI::I32<> iXPRewardDiscoverSecretArea{ "Progression", "iXPRewardDiscoverSecretArea", 20 };

			// XP formula
			inline static REX::INI::I32<> iXPBase     { "Progression", "iXPBase",      200 };
			inline static REX::INI::I32<> iXPBumpBase { "Progression", "iXPBumpBase",   75 };
			inline static REX::INI::F32<> fXPModBase  { "Progression", "fXPModBase",     1.0f  };
			inline static REX::INI::F32<> fXPModMult  { "Progression", "fXPModMult",     0.03f };
			inline static REX::INI::F32<> fXPDeathRewardHealthThreshold{
				"Progression", "fXPDeathRewardHealthThreshold", 0.25f };
		};

		// VATS: targeting distance, target-select time scale, and
		// player damage multiplier. All Direct mode with vanilla defaults
		// as neutral.
		class VATS
		{
		public:
			inline static REX::INI::F32<> fVATSMaxEngageDistance   { "VATS", "fVATSMaxEngageDistance",    5000.0f };
			inline static REX::INI::F32<> fVATSTimeMultTargetSelect{ "VATS", "fVATSTimeMultTargetSelect", 0.04f   };
			inline static REX::INI::F32<> fVATSPlayerDamageMult    { "VATS", "fVATSPlayerDamageMult",     0.10f   };
		};

		// Skills: pickpocket, hacking, and lockpicking GMSTs.
		// All Direct mode with vanilla defaults as neutral.
		class Skills
		{
		public:
			// Pickpocket
			inline static REX::INI::F32<> fPickPocketMinChance      { "Skills", "fPickPocketMinChance",       0.0f  };
			inline static REX::INI::F32<> fPickPocketMaxChance      { "Skills", "fPickPocketMaxChance",       90.0f };
			inline static REX::INI::F32<> fReversePickpocketTimer   { "Skills", "fReversePickpocketTimer",    2.0f  };

			// Hacking
			inline static REX::INI::I32<> iHackingMinWords          { "Skills", "iHackingMinWords",           6 };
			inline static REX::INI::I32<> iHackingMaxWords          { "Skills", "iHackingMaxWords",           20 };

			// Lockpicking
			inline static REX::INI::F32<> fLockPickBreakBase        { "Skills", "fLockPickBreakBase",         0.05f };
			inline static REX::INI::F32<> fLockpickSkillSweetSpotBase{ "Skills", "fLockpickSkillSweetSpotBase", 0.20f };
			inline static REX::INI::F32<> fLockpickBreakApprentice  { "Skills", "fLockpickBreakApprentice",   2.0f  };
			inline static REX::INI::F32<> fLockpickBreakAdept       { "Skills", "fLockpickBreakAdept",        2.0f  };
			inline static REX::INI::F32<> fLockpickBreakExpert      { "Skills", "fLockpickBreakExpert",       2.0f  };
			inline static REX::INI::F32<> fLockpickBreakMaster      { "Skills", "fLockpickBreakMaster",       2.0f  };
			inline static REX::INI::F32<> fSweetspotApprentice      { "Skills", "fSweetspotApprentice",       80.0f };
			inline static REX::INI::F32<> fSweetspotAdept           { "Skills", "fSweetspotAdept",            60.0f };
			inline static REX::INI::F32<> fSweetspotExpert          { "Skills", "fSweetspotExpert",           40.0f };
			inline static REX::INI::F32<> fSweetspotMaster          { "Skills", "fSweetspotMaster",           20.0f };
		};

		// Companions Affinity: nine vanilla TESGlobals for per-reaction affinity deltas and event cooldowns.
		class CompanionsAffinity
		{
		public:
			// Per-reaction affinity change (added to companion's affinity score
			// when an event of that reaction type fires)
			inline static REX::INI::F32<> fLove       { "CompanionsAffinity", "fLove",         35.0f };
			inline static REX::INI::F32<> fLike       { "CompanionsAffinity", "fLike",         15.0f };
			inline static REX::INI::F32<> fIndifferent{ "CompanionsAffinity", "fIndifferent",   0.0f };
			inline static REX::INI::F32<> fDislike    { "CompanionsAffinity", "fDislike",     -15.0f };
			inline static REX::INI::F32<> fHates      { "CompanionsAffinity", "fHates",       -35.0f };

			// Reaction cooldowns in game days (how often a companion will
			// re-react to the same kind of player action)
			inline static REX::INI::F32<> fCooldownImmediate{ "CompanionsAffinity", "fCooldownImmediate", 0.0f  };
			inline static REX::INI::F32<> fCooldownShort    { "CompanionsAffinity", "fCooldownShort",     0.05f };
			inline static REX::INI::F32<> fCooldownMedium   { "CompanionsAffinity", "fCooldownMedium",    2.0f  };
			inline static REX::INI::F32<> fCooldownLong     { "CompanionsAffinity", "fCooldownLong",      5.0f  };
		};

		// Sneak: sneak attack and core detection GMSTs.
		// All Direct mode with vanilla defaults as neutral.
		class Sneak
		{
		public:
			// Sneak attack damage
			inline static REX::INI::F32<> fSneakAttackGunMult      { "Sneak", "fSneakAttackGunMult",       2.0f    };
			inline static REX::INI::F32<> fSneakAttackUnarmedMult  { "Sneak", "fSneakAttackUnarmedMult",   3.0f    };
			inline static REX::INI::F32<> fSneakAttackOneHandMult  { "Sneak", "fSneakAttackOneHandMult",   3.0f    };
			inline static REX::INI::F32<> fSneakAttackTwoHandMult  { "Sneak", "fSneakAttackTwoHandMult",   3.0f    };

			// Detection
			inline static REX::INI::F32<> fSneakExteriorDistanceMult{ "Sneak", "fSneakExteriorDistanceMult", 1.25f   };
			inline static REX::INI::F32<> fSneakLightMult           { "Sneak", "fSneakLightMult",            0.33f   };
			inline static REX::INI::F32<> fSneakMaxDistance         { "Sneak", "fSneakMaxDistance",          4096.0f };
		};

		// Combat Perks: trigger chances and Light/Heavy Armor perk-tier multipliers.
		class CombatPerks
		{
		public:
			// Disarm / stagger / knockdown / paralyze chances triggered by combat perks
			inline static REX::INI::I32<> iAttackDisarmChance         { "CombatPerks", "iAttackDisarmChance",          50 };
			inline static REX::INI::I32<> iBlockDisarmChance          { "CombatPerks", "iBlockDisarmChance",           50 };
			inline static REX::INI::I32<> iBlockStaggerChance         { "CombatPerks", "iBlockStaggerChance",           5 };
			inline static REX::INI::I32<> iHandToHandBlockRecoilChance{ "CombatPerks", "iHandToHandBlockRecoilChance", 25 };
			inline static REX::INI::I32<> iMarksmanKnockdownChance    { "CombatPerks", "iMarksmanKnockdownChance",      5 };
			inline static REX::INI::I32<> iMarksmanParalyzeChance     { "CombatPerks", "iMarksmanParalyzeChance",       5 };

			// Light Armor perk tier multipliers
			inline static REX::INI::F32<> fLightArmorNoviceDamageMult    { "CombatPerks", "fLightArmorNoviceDamageMult",     1.5f };
			inline static REX::INI::F32<> fLightArmorJourneymanDamageMult{ "CombatPerks", "fLightArmorJourneymanDamageMult", 0.5f };
			inline static REX::INI::F32<> fLightArmorExpertSpeedMult     { "CombatPerks", "fLightArmorExpertSpeedMult",      0.0f };
			inline static REX::INI::F32<> fLightArmorMasterRatingMult    { "CombatPerks", "fLightArmorMasterRatingMult",     1.5f };
			inline static REX::INI::I32<> iLightArmorMasterMinSum        { "CombatPerks", "iLightArmorMasterMinSum",          5  };

			// Heavy Armor perk tier multipliers
			inline static REX::INI::F32<> fHeavyArmorNoviceDamageMult    { "CombatPerks", "fHeavyArmorNoviceDamageMult",     1.5f  };
			inline static REX::INI::F32<> fHeavyArmorJourneymanDamageMult{ "CombatPerks", "fHeavyArmorJourneymanDamageMult", 0.5f  };
			inline static REX::INI::F32<> fHeavyArmorExpertSpeedMult     { "CombatPerks", "fHeavyArmorExpertSpeedMult",      0.5f  };
			inline static REX::INI::F32<> fHeavyArmorMasterSpeedMult     { "CombatPerks", "fHeavyArmorMasterSpeedMult",      0.0f  };
			inline static REX::INI::F32<> fHeavyArmorSinkGravityMult     { "CombatPerks", "fHeavyArmorSinkGravityMult",     15.0f  };
			inline static REX::INI::I32<> iHeavyArmorJumpSum             { "CombatPerks", "iHeavyArmorJumpSum",              35   };
			inline static REX::INI::I32<> iHeavyArmorSinkSum             { "CombatPerks", "iHeavyArmorSinkSum",              35   };
		};

		// Settlements: vanilla workshop GMSTs (build/repair/wire/settler-cap/placement).
		class Settlements
		{
		public:
			// Settler / build timers / repair / wire
			inline static REX::INI::I32<> iSettlerPopulationMax  { "Settlements", "iSettlerPopulationMax",  10  };
			inline static REX::INI::F32<> fBuildBaseTimer        { "Settlements", "fBuildBaseTimer",         0.1f  };
			inline static REX::INI::F32<> fBuildResourceTimer    { "Settlements", "fBuildResourceTimer",     0.4f  };
			inline static REX::INI::F32<> fRepairComponentMult   { "Settlements", "fRepairComponentMult",    0.1f  };
			inline static REX::INI::F32<> fWireMaxLength         { "Settlements", "fWireMaxLength",       1100.0f  };

			// Placement radius constraints
			inline static REX::INI::F32<> fRestrictedBuildSpacing       { "Settlements", "fRestrictedBuildSpacing",        32.0f };
			inline static REX::INI::F32<> fRestrictedBuildMaxFloraRadius{ "Settlements", "fRestrictedBuildMaxFloraRadius",  50.0f };
			inline static REX::INI::F32<> fRestrictedBuildMaxKeywordRadius{ "Settlements", "fRestrictedBuildMaxKeywordRadius", 50.0f };
			inline static REX::INI::F32<> fRestrictedBuildMaxTurretRadius { "Settlements", "fRestrictedBuildMaxTurretRadius",  70.0f };
		};

		// Survival: kill-switch toggles for the vanilla hardcore rule globals + tuning sliders that write the manager script's bound-object properties.
		class Survival
		{
		public:
			// Kill switches (TESGlobal writes via Globals::WriteByFormID)
			inline static REX::INI::Bool<> bDisableSustenance      { "Survival", "bDisableSustenance",       false };
			inline static REX::INI::Bool<> bDisableSleepDeprivation{ "Survival", "bDisableSleepDeprivation", false };
			inline static REX::INI::Bool<> bDisableDiseases        { "Survival", "bDisableDiseases",         false };
			inline static REX::INI::Bool<> bDisableAdrenaline      { "Survival", "bDisableAdrenaline",       false };

			// Sustenance tuning (HC_ManagerScript properties)
			inline static REX::INI::I32<>  iFoodCostPerTick                  { "Survival", "iFoodCostPerTick",                  4    };
			inline static REX::INI::I32<>  iDrinkCostPerTick                 { "Survival", "iDrinkCostPerTick",                 4    };
			inline static REX::INI::F32<>  fGamesHoursPerTick                { "Survival", "fGamesHoursPerTick",                1.0f };
			inline static REX::INI::F32<>  fBonusDigestionHours              { "Survival", "fBonusDigestionHours",              1.0f };
			inline static REX::INI::F32<>  fSustenanceTickWhileSleepingMult  { "Survival", "fSustenanceTickWhileSleepingMult",  0.25f };
			inline static REX::INI::F32<>  fTickHoursCostPerCombat           { "Survival", "fTickHoursCostPerCombat",           0.25f };

			// Food stage thresholds (negative pool values that trigger each stage)
			inline static REX::INI::I32<>  iFoodPoolPeckishAmount   { "Survival", "iFoodPoolPeckishAmount",   -24  };
			inline static REX::INI::I32<>  iFoodPoolHungryAmount    { "Survival", "iFoodPoolHungryAmount",    -48  };
			inline static REX::INI::I32<>  iFoodPoolFamishedAmount  { "Survival", "iFoodPoolFamishedAmount",  -96  };
			inline static REX::INI::I32<>  iFoodPoolRavenousAmount  { "Survival", "iFoodPoolRavenousAmount", -144 };
			inline static REX::INI::I32<>  iFoodPoolStarvingAmount  { "Survival", "iFoodPoolStarvingAmount", -256 };

			// Drink stage thresholds
			inline static REX::INI::I32<>  iDrinkPoolParchedAmount             { "Survival", "iDrinkPoolParchedAmount",             -16  };
			inline static REX::INI::I32<>  iDrinkPoolThirstyAmount             { "Survival", "iDrinkPoolThirstyAmount",             -36  };
			inline static REX::INI::I32<>  iDrinkPoolMildlyDehydratedAmount    { "Survival", "iDrinkPoolMildlyDehydratedAmount",    -72  };
			inline static REX::INI::I32<>  iDrinkPoolDehydratedAmount          { "Survival", "iDrinkPoolDehydratedAmount",         -120  };
			inline static REX::INI::I32<>  iDrinkPoolSeverelyDehydratedAmount  { "Survival", "iDrinkPoolSeverelyDehydratedAmount", -180  };

			// Sleep tuning
			inline static REX::INI::F32<>  fGameTimerIntervalSleepDeprivation { "Survival", "fGameTimerIntervalSleepDeprivation", 14.0f };
			inline static REX::INI::F32<>  fInsomniaSleepMult                 { "Survival", "fInsomniaSleepMult",                  0.5f };
			inline static REX::INI::I32<>  iMinHoursForCuringSleepEffects     { "Survival", "iMinHoursForCuringSleepEffects",      2    };
			inline static REX::INI::F32<>  fCaffeineInducedSleepDelay         { "Survival", "fCaffeineInducedSleepDelay",          2.333f };
			inline static REX::INI::F32<>  fExtraCaffeineInducedSleepDelay    { "Survival", "fExtraCaffeineInducedSleepDelay",     7.0f };
			inline static REX::INI::F32<>  fDiseasedSleepinessTimerMult       { "Survival", "fDiseasedSleepinessTimerMult",         0.5f };

			// Disease tuning
			inline static REX::INI::F32<>  fDiseaseGracePeriod              { "Survival", "fDiseaseGracePeriod",               1.0f  };
			inline static REX::INI::F32<>  fDiseaseRiskRollThreshold        { "Survival", "fDiseaseRiskRollThreshold",         0.25f };
			inline static REX::INI::F32<>  fDiseaseRiskDrainPerCycle        { "Survival", "fDiseaseRiskDrainPerCycle",        -0.01f };
			inline static REX::INI::F32<>  fCurrentDiseasePoolValueMult     { "Survival", "fCurrentDiseasePoolValueMult",      1.75f };
			inline static REX::INI::F32<>  fDiseaseNeedMoreFoodMult         { "Survival", "fDiseaseNeedMoreFoodMult",          0.5f  };
			inline static REX::INI::F32<>  fImmunodeficiencyDiseaseMult     { "Survival", "fImmunodeficiencyDiseaseMult",      1.2f  };
			inline static REX::INI::F32<>  fDiseaseRiskCannibalAmount       { "Survival", "fDiseaseRiskCannibalAmount",        0.05f };
			inline static REX::INI::F32<>  fDiseaseRiskChemsAmount          { "Survival", "fDiseaseRiskChemsAmount",           0.07f };
			inline static REX::INI::F32<>  fDiseaseRiskCombatantAmount      { "Survival", "fDiseaseRiskCombatantAmount",       0.05f };
			inline static REX::INI::F32<>  fDiseaseRiskFoodHighAmount       { "Survival", "fDiseaseRiskFoodHighAmount",        0.12f };
			inline static REX::INI::F32<>  fDiseaseRiskFoodStandardAmount   { "Survival", "fDiseaseRiskFoodStandardAmount",    0.07f };
			inline static REX::INI::F32<>  fDiseaseRiskRainAmount           { "Survival", "fDiseaseRiskRainAmount",            0.03f };
			inline static REX::INI::F32<>  fDiseaseRiskSwimmingAmount       { "Survival", "fDiseaseRiskSwimmingAmount",        0.03f };

			// Adrenaline tuning (vanilla MaxAdrenaline is computed as KillsForPerkLevel * MaxPerkLevel)
			inline static REX::INI::I32<>  iKillsForAdrenalinePerkLevel { "Survival", "iKillsForAdrenalinePerkLevel",  5 };
			inline static REX::INI::I32<>  iMaxAdrenalinePerkLevel      { "Survival", "iMaxAdrenalinePerkLevel",      10 };

			inline static REX::INI::F32<>  fGameTimerIntervalEncumbrance { "Survival", "fGameTimerIntervalEncumbrance", 24.0f };
		};

		// Diagnostics. Some values are exposed in MCM; developer-only toggles
		// remain INI-only.
		class Diagnostic
		{
		public:
			inline static REX::INI::Bool<> bSurvivalObserver{
				"Diagnostic", "bSurvivalObserver", false
			};
			inline static REX::INI::Bool<> bSurvivalObserverActorValues{
				"Diagnostic", "bSurvivalObserverActorValues", true
			};
			inline static REX::INI::Bool<> bSurvivalObserverActiveEffects{
				"Diagnostic", "bSurvivalObserverActiveEffects", true
			};
			inline static REX::INI::Bool<> bSurvivalObserverLifecycle{
				"Diagnostic", "bSurvivalObserverLifecycle", true
			};
			inline static REX::INI::Str<> sSurvivalObserverFilter{
				"Diagnostic", "sSurvivalObserverFilter",
				std::string{ "Hardcore,Survival,Hunger,Thirst,Sleep,Fatigue,Peckish,Weary,Parched,Dehydr,Starv" }
			};
			inline static REX::INI::Bool<> bSleepWaitObserver{
				"Diagnostic", "bSleepWaitObserver", false
			};
			inline static REX::INI::Bool<> bSleepWaitObserverLifecycle{
				"Diagnostic", "bSleepWaitObserverLifecycle", true
			};
			inline static REX::INI::Bool<> bSleepWaitObserverSnapshots{
				"Diagnostic", "bSleepWaitObserverSnapshots", true
			};
			inline static REX::INI::Bool<> bDumpOnLoad{
				"Diagnostic", "bDumpOnLoad", false
			};
			inline static REX::INI::Bool<> bActorValueProbe{
				"Diagnostic", "bActorValueProbe", false
			};
			inline static REX::INI::Bool<> bActorValueProbePhaseA{
				"Diagnostic", "bActorValueProbePhaseA", true
			};
			inline static REX::INI::Bool<> bActorValueProbePhaseB{
				"Diagnostic", "bActorValueProbePhaseB", true
			};
			inline static REX::INI::Bool<> bActorValueProbeDFOBArray{
				"Diagnostic", "bActorValueProbeDFOBArray", false
			};
			inline static REX::INI::Bool<> bActorValueProbePlayerSnapshot{
				"Diagnostic", "bActorValueProbePlayerSnapshot", true
			};
			inline static REX::INI::Bool<> bActorValueProbeRerunOnPause{
				"Diagnostic", "bActorValueProbeRerunOnPause", false
			};
			inline static REX::INI::Str<> sActorValueProbeFilter{
				"Diagnostic", "sActorValueProbeFilter",
				std::string{ "apRateMult,restoreAPRate,actionPoints,healRateMult,restoreHealthRate,combatHealthRegenMult,health,fatigueAPMax,fatigue" }
			};
			inline static REX::INI::Str<> sDefaultObjectProbeFilter{
				"Diagnostic", "sDefaultObjectProbeFilter",
				std::string{ "ActionPoint,APRate,HealRate,Regen,Hardcore,Combat" }
			};
			inline static REX::INI::Bool<> bHCManagerProbe{
				"Diagnostic", "bHCManagerProbe", false
			};
			inline static REX::INI::Bool<> bHCManagerProbeRerunOnPause{
				"Diagnostic", "bHCManagerProbeRerunOnPause", false
			};
			inline static REX::INI::Str<> sHCManagerProbeFilter{
				"Diagnostic", "sHCManagerProbeFilter",
				std::string{ "bHardcoreIsRunning,FoodCostPerTick,DrinkCostPerTick,GameTimerInterval_SleepDeprivation,GameTimerInterval_Sustenance,GamesHoursPerTick,BonusDigestionHours,SustenanceTickWhileSleepingMult,TickHoursCostPerCombat,FoodPool,FoodReqs,DrinkPool,DrinkReqs,NextSustenanceTickDay,LastSleepUpdateDay,NextSleepUpdateDay,ProcessingSleep,bFirstSleep,HC_Rule_SustenanceEffects,HC_Rule_SleepEffects,HC_HungerEffect,HC_ThirstEffect,HC_SleepEffect" }
			};
			inline static REX::INI::Bool<> bMagnitudesTrace{
				"Diagnostic", "bMagnitudesTrace", false
			};
			inline static REX::INI::Bool<> bMagnitudesTraceDeltaOnly{
				"Diagnostic", "bMagnitudesTraceDeltaOnly", true
			};
			inline static REX::INI::Bool<> bMagnitudesAlwaysSummarize{
				"Diagnostic", "bMagnitudesAlwaysSummarize", false
			};
			inline static REX::INI::Bool<> bDifficultyTrace{
				"Diagnostic", "bDifficultyTrace", false
			};
			inline static REX::INI::Bool<> bGameSettingsTrace{
				"Diagnostic", "bGameSettingsTrace", false
			};
			// Central log verbosity for MCM. 0=Quiet, 1=Normal, 2=Verbose,
			// 3=Trace. Invalid values fall back to sLogLevel.
			inline static REX::INI::I32<> iLogLevel{
				"Diagnostic", "iLogLevel", 1
			};
			// Legacy/manual fallback. Accepted: Quiet, Normal, Verbose, Trace
			// (case-insensitive). Used only when iLogLevel is out of range.
			inline static REX::INI::Str<> sLogLevel{
				"Diagnostic", "sLogLevel", std::string{ "Normal" }
			};
			// Validation audit emits structured HRVERIFY[_SUMMARY] lines from
			// Tweaks::GameSettings so post-run scripts can confirm GMST writes
			// landed. Off by default; orthogonal to bGameSettingsTrace.
			inline static REX::INI::Bool<> bValidationAudit{
				"Diagnostic", "bValidationAudit", false
			};
			// Summary = HRVERIFY_SUMMARY only; Full = per-target HRVERIFY too.
			inline static REX::INI::Str<> sValidationAuditMode{
				"Diagnostic", "sValidationAuditMode", std::string{ "Summary" }
			};
			inline static REX::INI::Str<> sDumpFilter{
				"Diagnostic", "sDumpFilter",
				std::string{ "Hardcore,Survival,Hunger,Thirst,Sleep,Fatigue,Stimpak,RadAway,RadX,Peckish,Weary,Parched,Dehydr,Starv" }
			};
		};

		static void Update();
	};
}

// Source compatibility for downstream gameplay modules while ownership lives
// in the UI-independent HouseRules namespace.
namespace MCM
{
	using Settings = HouseRules::Settings;
}
