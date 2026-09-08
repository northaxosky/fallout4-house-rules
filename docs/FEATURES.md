# House Rules - Feature Reference

A complete list of every setting House Rules exposes through either the MCM or native Dear Modding UI frontend. Both are generated from the same catalog and write the same `HouseRules.ini` user overrides. For installation and project info see the [README](../README.md).

## How sliders work

- **Default** is the value Fallout 4 normally uses. If you leave a slider on its default, House Rules does not change anything. Any changes from other mods stay in effect.
- **Slider types:**
  - **Direct sliders** set the value to exactly what you choose.
  - **Multiplier sliders** scale the original game value. `1.00` means no change. `2.00` means double. `0.50` means half.
  - **Toggles** turn a setting on or off (used in Survival Unlocks).
- **When changes apply:** MCM sliders take effect when you close the pause menu. Native edits remain pending until you select **Apply**. Native persistence is immediate, but gameplay mutation is queued to the game thread and deferred until the next loading-screen close if a save is not ready. Some changes also update when you eat or drink a consumable.

## Front Page

| Setting | Type | Default | Effect |
|---|---|---|---|
| Enable Plugin | Toggle | On | Master switch. With this off, none of the gameplay changes apply. Diagnostics can still run. |
| Log Level | Stepper | Normal | How much detail House Rules writes to its log file: `Quiet`, `Normal`, `Verbose`, or `Trace`. |

## Survival Unlocks

All toggles default OFF (vanilla Survival). Turning one ON applies that change while keeping real Survival difficulty active. Damage scaling, food, water, sleep, and the rest stay in effect.

| Toggle | What it does |
|---|---|
| Allow Console | Lets you open the developer console. |
| Allow Manual Saves | Lets you use F5 quicksave and the pause-menu Save / Load options. |
| Allow Auto-Saves | Lets the game auto-save (menu close, door entry, and so on). |
| Allow Fast Travel | Lets you fast travel from the Pip-Boy map. House Rules protects you from rad damage during the fast-travel loading screen, but only during fast travel. Other dangers are not affected. |
| Show Enemies on Compass | Brings back red enemy markers on the compass. |
| Show Locations on Compass | Brings back unexplored-location markers on the compass. |
| Remove Chem/Aid Weight | Removes the extra weight Survival adds to chems, food, drinks, and stimpaks. |
| Remove Ammo Weight | Removes the extra weight Survival adds to ammunition. |
| Remove Survival Carry-Weight Penalty | Zeroes the Survival carry-weight reduction (the `HC_ReduceCarryWeightAbility` spell's effect magnitudes) on you and your companions. Stacks with the carry-weight sliders on the Character page. **Requires save reload after toggling** for the change to take effect on already-applied characters. |
| Allow God Mode | Lets the `tgm` and `tim` console commands work in Survival. |
| Allow Re-entering Survival | Lets you switch Survival on and off freely. Vanilla only allows leaving Survival, never coming back. Updates right away. |
| Keep Exit Save | Stops the game deleting the Survival exit save on load. Toggle OFF to delete an exit save from the load menu. |

## Magnitudes

Sliders for how strong consumables and Survival penalties are. These edit base item records, so changes apply the next time you use the item. The Pip-Boy tooltip will keep showing the old value until then.

### Consumables

| Slider | Type | Default | Effect |
|---|---|---|---|
| Stimpak Heal | Multiplier | 1.0 | How much health a Stimpak restores. |
| Stimpak Limb Repair | Multiplier | 1.0 | How much limb damage a Stimpak repairs. |
| RadAway Strength | Multiplier | 1.0 | How much radiation RadAway removes. |
| Rad-X Potency | Multiplier | 1.0 | How much radiation resistance Rad-X grants. |
| Food Heal | Multiplier | 1.0 | How much health food items restore. |

### Survival Penalties

| Slider | Type | Default | Effect |
|---|---|---|---|
| Hunger Penalty | Multiplier | 1.0 | How harsh the hunger stage penalties are (Peckish, Hungry, Famished, Starving). |
| Thirst Penalty | Multiplier | 1.0 | How harsh the thirst stage penalties are (Parched, Thirsty, Dehydrated). |
| Sleep Penalty | Multiplier | 1.0 | How harsh the sleep stage penalties are (Tired, Weary, Exhausted, Incapacitated). |

`0.0` on any penalty slider removes that penalty completely.

## Difficulty I (Combat & Rewards)

Multipliers for combat damage and rewards on each difficulty level. `1.00` means no change. House Rules saves the original game value the first time it applies a setting, so changing the slider does not stack across save loads.

| Slider | Game Setting | What it changes |
|---|---|---|
| Incoming Damage | `fDiffMultHPByPCVS<difficulty>` | Damage you take, per difficulty level. |
| Outgoing Damage | `fDiffMultHPToPCVS<difficulty>` | Damage you deal, per difficulty level. |
| Base XP Rate | `fXPDifficultyMult` | How much XP you earn overall. |
| Intelligence XP Bonus | `fXPPerIntelligenceMult` | How much extra XP each point of Intelligence gives. |
| Legendary Chance | `fLegendaryDropChance<tier>` | The chance an enemy drops a legendary item. |
| Legendary Rarity | `fLegendaryDropRarityMult<tier>` | How rare the legendary effect is when one drops. |

## Difficulty II (Effect Duration & Strength)

Multipliers for how long status effects last and how strong they are. Each difficulty level has its own pair of sliders.

A note about Survival: vanilla Survival uses unusual values here. Status effects last much longer in Survival, but their magnitude is much weaker. The slider multiplies that Survival value, so `1.00` on Survival is still Survival. It does not become Normal.

| Slider Group | Game Setting Family | What it changes |
|---|---|---|
| Effect Duration (per difficulty) | `fDiffMultEffectDuration_*` | How long status effects last. Difficulty levels: VeryEasy, Easy, Normal, Hard, VeryHard, Survival. |
| Effect Magnitude (per difficulty) | `fDiffMultEffectMagnitude_*` | How strong status effects are. Same difficulty levels. |

## Character (Action Points, Sprint, Carry, Health)

Sliders for AP, sprint, carry weight, and health.

### Action Points (AP)

| Slider | Default | What it changes |
|---|---|---|
| AP Pool Base | Direct | Your starting AP, before SPECIAL bonuses. |
| AP per Agility | Direct | How much AP you get per Agility point. |
| AP Regen Rate | Direct | How fast AP regenerates outside combat. |
| Combat AP Regen Multiplier | Direct (vanilla 0.75) | How fast AP regenerates during combat. |
| AP Regen Delay | Direct | The pause after spending AP before it starts regenerating. |
| Out-of-Breath Delay | Direct | The maximum length of the regen pause. House Rules logs a warning if you set this lower than `AP Regen Delay`. |
| Sprint-Stop Regen Delay | Direct | The minimum delay before AP regen starts after you stop sprinting. |

### Sprint

| Slider | Default | What it changes |
|---|---|---|
| Sprint Drain Base | Direct | The basic AP cost per second of sprinting. |
| Sprint Endurance Term | Direct (vanilla -0.05) | How much Endurance reduces sprint cost. |
| Sprint Drain Multiplier | Multiplier | An overall multiplier on sprint AP cost. |

### Carry Weight

| Slider | Default | What it changes |
|---|---|---|
| Carry Weight Base | Direct | Your base carry weight, before SPECIAL bonuses. |
| Carry Weight per Strength | Direct | How much extra carry weight each Strength point gives. |

The `Remove Survival Carry-Weight Penalty` toggle (on Survival Unlocks) stacks with these sliders. The slider sets the base formula. The toggle removes the Survival-only weight penalty on top of that.

### Health

| Slider | Default | What it changes |
|---|---|---|
| Health per Endurance | Direct | How much max health each Endurance point gives. |
| Health per Level | Direct | How much max health you gain per character level. |
| Passive Health Regen | Direct | How fast you heal outside combat. |
| Combat Health Regen Multiplier | Direct | How fast you heal during combat. |

A few of these sliders change the formula but not your current value right away. Your AP pool, carry weight, and max health refresh when the game recalculates them: at level-up, when you change equipment, after fast travel, or after you load a save.

The three regen sliders (AP Regen, Passive Health Regen, Combat Health Regen) are different from the others. They write a value directly to your character. This means they always have a known reset point so saves stay clean.

## Damage Formulas

Sliders for the basic damage and armor calculations the game uses. These apply to both you and NPCs.

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Radiation Damage Factor | `fRadsDamageFactor` | 0.15 | How much damage radiation does. Higher = more damage. |
| Radiation Armor Exponent | `fRadsArmorDmgReductionExp` | 0.365 | How much rad resistance reduces rad damage. |
| Physical Damage Factor | `fPhysicalDamageFactor` | 0.15 | How much damage physical attacks do. Stacks with the Difficulty I sliders. |
| Physical Armor Exponent | `fPhysicalArmorDmgReductionExp` | 0.365 | How much physical armor reduces damage. |
| Energy Damage Factor | `fEnergyDamageFactor` | 0.15 | How much damage energy attacks do. Stacks with the Difficulty I sliders. |
| Energy Armor Exponent | `fEnergyArmorDmgReductionExp` | 0.365 | How much energy resistance reduces energy damage. |

All Direct sliders. Leaving one on its default keeps any changes another mod has made.

## Power Armor & Jetpack

Sliders for jetpack feel, fusion core drain, and how fast Power Armor breaks.

### Jetpack

| Slider | Default | Type | What it changes |
|---|---|---|---|
| Initial Drain | 1.0 | Multiplier | Fuel cost when you first activate the jetpack. |
| Sustained Drain | 1.0 | Multiplier | Fuel cost while flying. |
| Initial Thrust | 1.0 | Multiplier | How much lift you get when you first activate the jetpack. |
| Sustained Thrust | 1.0 | Multiplier | How much lift you get while flying. |
| Min Fuel to Ignite | 3 | Direct | The smallest amount of fuel needed to start the jetpack. |
| Sustained-Mode Time | 0.15 sec | Direct | How long after activation before sustained mode starts. |

### Fusion Core Drain

| Slider | Default | Type | What it changes |
|---|---|---|---|
| Drain per AP Spent | 1.0 | Multiplier | Fuel cost per AP you spend in Power Armor. |
| Drain per Second Running | 1.0 | Multiplier | Fuel cost per second you run in Power Armor. |
| Drain per Jump | 0 | Direct | Extra fuel cost when you jump. |
| Drain per Melee Attack | 0 | Direct | Extra fuel cost on melee attacks. |
| Drain per Hard Landing | 0 | Direct | Extra fuel cost on hard landings. |

### Durability

| Slider | Default | What it changes |
|---|---|---|
| Player PA Damage Multiplier | 1.0 | How fast your Power Armor takes damage. `0` makes it unbreakable. |
| NPC PA Damage Multiplier | 3 | How fast NPC Power Armor takes damage. |

## Economy

### Vendor Pricing

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Buy Price Floor | `fBarterMin` | 2.0 | The lowest price multiplier you can pay when buying. **Higher = vendors charge more.** |
| Sell Price Floor | `fBarterMax` | 3.5 | The divisor used when selling. **Higher = vendors pay less.** |
| Max Buy Multiplier | `fBarterBuyMax` | 1.2 | The cap the Barter perk and Charisma can reach when you buy. |
| Max Sell Multiplier | `fBarterSellMax` | 0.8 | The cap the Barter perk and Charisma can reach when you sell. |

### Persuasion

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Persuasion Floor | `fSpeechcraftFavorMin` | 0.5 | Minimum success chance the Charisma speech-check curve can fall to. |
| Persuasion Ceiling | `fSpeechcraftFavorMax` | 1.0 | Maximum success chance the Charisma speech-check curve can reach. |

All Direct sliders. Leaving one on its default keeps any changes another mod has made.

## Progression (XP)

Sliders for XP from sources other than combat (Difficulty I covers combat XP).

### Crafting XP

| Slider Group | Game Settings | Defaults |
|---|---|---|
| Cooking | `fCookingExp{Base,Max,Mult}` | 1 / 10 / 0.15 |
| Weapon/Armor Workbench | `fWorkbenchExperience{Base,Max,Mult}` | 2 / 50 / 0.03 |
| Settlement Workshop | `fWorkshopExperience{Base,Max,Mult}` | 2 / 25 / 0.10 |

The game keeps crafting XP between Base and Max. If you set Base higher than Max, House Rules logs a warning and does not change anything.

### Lockpick XP

| Lock Tier | Game Setting | Default |
|---|---|---|
| Apprentice | `fLockpickXPRewardEasy` | 5 |
| Adept | `fLockpickXPRewardAverage` | 10 |
| Expert | `fLockpickXPRewardHard` | 15 |
| Master | `fLockpickXPRewardVeryHard` | 20 |

### Other

| Slider | Game Setting | Default |
|---|---|---|
| Mine Disarm XP | `iMineDisarmExperience` | 5 |

### Kill & Discovery XP

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Kill XP | `iXPRewardKillOpponent` | 20 | XP per killed opponent. |
| Map Marker XP | `iXPRewardDiscoverMapMarker` | 20 | XP per newly discovered map marker. |
| Secret Area XP | `iXPRewardDiscoverSecretArea` | 20 | XP per discovered secret area. |

### XP Formula

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| XP Base | `iXPBase` | 200 | Base XP scalar used by the level-up curve. |
| Level-Up Bump | `iXPBumpBase` | 75 | Per-level XP bump value. |
| Mod XP Base | `fXPModBase` | 1.0 | Base XP for installing or removing a weapon/armor mod at a workbench. |
| Mod XP Multiplier | `fXPModMult` | 0.03 | Multiplier on Mod XP. |
| Death Reward Health Threshold | `fXPDeathRewardHealthThreshold` | 0.25 | Engine penalty threshold for kills made just before the player's death. |

All Direct sliders.

## VATS

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Max Engage Distance | `fVATSMaxEngageDistance` | 5000 | How far you can be from a target to use VATS. `0` disables VATS. |
| Target-Select Time | `fVATSTimeMultTargetSelect` | 0.04 | How much time slows while you choose targets. Lower = slower. |
| Player Damage Multiplier | `fVATSPlayerDamageMult` | 0.10 | Damage you take while VATS is active. `1.0` removes the vanilla damage reduction. |

## Skills (Pickpocket, Hacking, Lockpicking)

### Pickpocket

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Min Chance | `fPickPocketMinChance` | 0 | The smallest pickpocket success chance. |
| Max Chance | `fPickPocketMaxChance` | 90 | The largest pickpocket success chance. |
| Reverse-Pickpocket Grenade Timer | `fProjectileInventoryGrenadeTimer` | 2 sec | How long the fuse is on grenades you place on NPCs. |

### Hacking

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Min Words | `iHackingMinWords` | 6 | The smallest number of words on a terminal screen. |
| Max Words | `iHackingMaxWords` | 20 | The largest number of words on a terminal screen. |

### Lockpicking

Sliders by lock tier for break threshold, sweet-spot base, durability, and sweet-spot width. Lockpick XP is on the Progression page.

## Sneak

### Sneak Attack Damage

| Slider | Game Setting |
|---|---|
| Gun | `fSneakAttackMult` |
| Unarmed / Fist | `fSneakAttackMultUnarmed` |
| One-Handed Melee | `fSneakAttackMult1H` |
| Two-Handed Melee | `fSneakAttackMult2H` |

### Detection

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Outdoor Detection Distance | `fDetectionExteriorDistanceMult` | 1.0 | How far enemies can spot you outdoors. |
| Light Sensitivity | `fDetectionLightMod` | 1.0 | How much ambient light affects detection. |
| Max Detection Distance | `fDetectionMaxDistance` | varies | The maximum range an enemy can detect you at. |

NPC search timers and stealth-point internals are not exposed. They may come later.

## Companions Affinity

Sliders for the nine vanilla game values that control companion affinity events.

### Affinity Per Reaction

How much affinity is gained or lost when a companion reacts to your action. Positive values raise affinity; negative values lower it.

| Slider | Default | Range | What it changes |
|---|---|---|---|
| Loves Reaction | +35 | -250 to 250 | Affinity gained when a companion 'Loves' your action. |
| Likes Reaction | +15 | -250 to 250 | Affinity gained when a companion 'Likes' your action. |
| Indifferent Reaction | 0 | -250 to 250 | Affinity change for a neutral reaction. Vanilla 0 (no change). |
| Dislikes Reaction | -15 | -250 to 250 | Affinity lost when a companion 'Dislikes' your action. |
| Hates Reaction | -35 | -250 to 250 | Affinity lost when a companion 'Hates' your action. |

### Reaction Cooldowns

How often the same kind of reaction can fire, measured in game days.

| Slider | Default | Range | What it changes |
|---|---|---|---|
| Immediate Cooldown | 0 | 0 to 5 | Cooldown for back-to-back reactions. |
| Short Cooldown | 0.05 | 0 to 5 | Cooldown for short-tier reactions. |
| Medium Cooldown | 2 | 0 to 5 | Cooldown for medium-tier reactions. |
| Long Cooldown | 5 | 0 to 5 | Cooldown for long-tier reactions. |

All Direct sliders. Leaving one on its default keeps the vanilla value.

## Combat Perks

Sliders for trigger chances and tier multipliers tied to combat perks.

### Combat Chances

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Attack Disarm Chance | `iPerkAttackDisarmChance` | 50 | Disarm chance from the Big Leagues / Iron Fist disarm rank. |
| Block Disarm Chance | `iPerkBlockDisarmChance` | 50 | Disarm chance when blocking with a melee weapon. |
| Block Stagger Chance | `iPerkBlockStaggerChance` | 5 | Stagger chance when blocking. |
| Hand-to-Hand Block Recoil Chance | `iPerkHandToHandBlockRecoilChance` | 25 | Recoil chance when blocking unarmed attacks. |
| Marksman Knockdown Chance | `iPerkMarksmanKnockdownChance` | 5 | Knockdown chance from the Rifleman / Gunslinger knockdown ranks. |
| Marksman Paralyze Chance | `iPerkMarksmanParalyzeChance` | 5 | Paralyze chance from the Rifleman / Gunslinger paralyze ranks. |

### Light Armor Perk Tiers

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Novice Damage Multiplier | `fPerkLightArmorNoviceDamageMult` | 1.5 | Damage taken at Light Armor rank 1. |
| Journeyman Damage Multiplier | `fPerkLightArmorJourneymanDamageMult` | 0.5 | Damage taken at Light Armor rank 2. |
| Expert Speed Multiplier | `fPerkLightArmorExpertSpeedMult` | 0.0 | Speed bonus at Light Armor rank 3. |
| Master Rating Multiplier | `fPerkLightArmorMasterRatingMult` | 1.5 | Armor rating multiplier at Light Armor rank 4. |
| Master Min Sum | `iPerkLightArmorMasterMinSum` | 5 | Minimum light armor pieces required for the Master bonus. |

### Heavy Armor Perk Tiers

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Novice Damage Multiplier | `fPerkHeavyArmorNoviceDamageMult` | 1.5 | Damage taken at Heavy Armor rank 1. |
| Journeyman Damage Multiplier | `fPerkHeavyArmorJourneymanDamageMult` | 0.5 | Damage taken at Heavy Armor rank 2. |
| Expert Speed Multiplier | `fPerkHeavyArmorExpertSpeedMult` | 0.5 | Speed multiplier at Heavy Armor rank 3. |
| Master Speed Multiplier | `fPerkHeavyArmorMasterSpeedMult` | 0.0 | Speed multiplier at Heavy Armor rank 4. |
| Sink Gravity Multiplier | `fPerkHeavyArmorSinkGravityMult` | 15 | Underwater gravity multiplier at the heavy armor sink threshold. |
| Jump Sum Threshold | `iPerkHeavyArmorJumpSum` | 35 | Total armor weight where the heavy armor jump penalty triggers. |
| Sink Sum Threshold | `iPerkHeavyArmorSinkSum` | 35 | Total armor weight where the heavy armor sink penalty triggers. |

All Direct sliders. Leaving one on its default keeps any changes another mod has made.

## Settlements

Sliders for vanilla workshop game settings: build timers, repair cost, wire length, settler cap, and placement constraints. Settlement-surplus production caps are baked into script code (not game settings) and aren't covered here.

### Settler & Building

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Settler Population Cap | `iWorkshopSettlerPopulationMax` | 10 | Hard cap on settlers per settlement (before Charisma bonuses). |
| Build Base Timer | `fWorkshopBuildBaseTimer` | 0.1 | Seconds the workshop UI takes to register a build. |
| Build Resource Timer | `fWorkshopBuildResourceTimer` | 0.4 | Seconds the workshop UI takes to deduct resources. |
| Repair Component Multiplier | `fWorkshopRepairComponentMult` | 0.1 | Component cost multiplier when repairing damaged objects. |
| Power Wire Max Length | `fWorkshopWireMaxLength` | 1100 | Maximum reach of a single power wire. |

### Placement Constraints

| Slider | Game Setting | Default | What it changes |
|---|---|---|---|
| Restricted Build Spacing | `fWorkshopRestrictedBuildSpacing` | 32 | Minimum spacing between restricted build objects. |
| Max Flora Build Radius | `fWorkshopRestrictedBuildMaxFloraRadius` | 50 | Radius around flora where restricted objects cannot be placed. |
| Max Keyword Build Radius | `fWorkshopRestrictedBuildMaxKeywordRadius` | 50 | Radius around keyword-tagged objects where restricted builds are blocked. |
| Max Turret Build Radius | `fWorkshopRestrictedBuildMaxTurretRadius` | 70 | Minimum spacing between automated turrets. |

All Direct sliders. Leaving one on its default keeps any changes another mod has made.

## Survival

Toggles for the vanilla Survival hardcore-rule systems. Use these to keep Survival difficulty active while disabling its food/water, sleep, disease, or adrenaline subsystems independently. All four default OFF (vanilla behavior).

| Toggle | What it does |
|---|---|
| Disable Hunger and Thirst | Turns off the food and water need system entirely. Stages no longer accumulate or apply. |
| Disable Sleep Deprivation | Turns off the sleep deprivation system. Sleep timer no longer accumulates and stages no longer apply. |
| Disable Diseases | Turns off the Survival disease system. Risk events no longer roll and disease effects no longer apply. |
| Disable Adrenaline | Turns off the Survival Adrenaline perk system. Kill streaks no longer build the Adrenaline rank, and the perk's damage bonus no longer applies. |

Each toggle writes the matching vanilla global variable (`HC_Rule_SustenanceEffects`, `HC_Rule_SleepEffects`, `HC_Rule_DiseaseEffects`, `HC_Rule_AdrenalineOn`) that the hardcore manager script reads each tick. **Save and reload after toggling for immediate effect**; otherwise the change takes hold on the next in-game tick (a few in-game minutes).

### Sustenance Tuning

Slows or speeds the food/water accumulation independently of the kill-switch.

| Slider | Default | What it changes |
|---|---|---|
| Food Cost Per Tick | 4 | How much the food pool drops each tick. Lower = needs accumulate slower. |
| Drink Cost Per Tick | 4 | How much the drink pool drops each tick. |
| Game Hours Per Tick | 1.0 | In-game hours between sustenance ticks. Higher = slower accumulation. |
| Bonus Digestion Hours | 1.0 | Protected window after eating during which no further need accumulates. |
| Tick Multiplier While Sleeping | 0.25 | Multiplier on sustenance accrual while asleep. |
| Extra Hours Per Combat | 0.25 | Extra in-game hours added to the tick during combat. Higher = combat makes you hungrier/thirstier faster. |

### Food Stage Thresholds

Negative pool values at which each food stage triggers. Lower (more negative) defers the stage onset.

| Slider | Default |
|---|---|
| Peckish At | -24 |
| Hungry At | -48 |
| Famished At | -96 |
| Ravenous At | -144 |
| Starving At | -256 |

### Drink Stage Thresholds

| Slider | Default |
|---|---|
| Parched At | -16 |
| Thirsty At | -36 |
| Mildly Dehydrated At | -72 |
| Dehydrated At | -120 |
| Severely Dehydrated At | -180 |

### Sleep Tuning

| Slider | Default | What it changes |
|---|---|---|
| Sleep Deprivation Interval | 14 | In-game hours between sleep-deprivation checks. Higher = sleep stages progress slower. |
| Insomnia Sleep Multiplier | 0.5 | Multiplier on sleep recovery while the Insomnia disease is active. |
| Minimum Hours to Cure Sleep | 2 | Minimum in-game hours of sleep needed to cure any sleep stage effect. |
| Caffeine Sleep Delay | 2.333 | Hours of sleep delay imposed after consuming a caffeinated item. |
| Extra Caffeine Sleep Delay | 7 | Additional sleep-delay hours stacked for repeated caffeine use. |
| Sleepiness Disease Timer Mult | 0.5 | Multiplier on the sleep-deprivation timer while Sleepiness is active. Lower = sleep stages progress faster when diseased. |

### Disease Tuning

| Slider | Default | What it changes |
|---|---|---|
| Disease Risk Threshold | 0.25 | Risk-pool value above which a disease roll is made. Higher = diseases trigger less often. |
| Disease Grace Period | 1.0 | In-game days after the last disease before another can trigger. |
| Disease Risk Drain Per Cycle | -0.01 | Passive disease-risk drain per cycle when no risk events fire. |
| Disease Pool Value Mult | 1.75 | Multiplier on the disease pool's current value. |
| Need More Food Multiplier | 0.5 | Sustenance-cost multiplier while the Need More Food disease is active. |
| Immunodeficiency Mult | 1.2 | Disease-chance multiplier while Immunodeficiency is active. |
| Risk: Cannibalism | 0.05 | Risk pool gain per cannibalism event. |
| Risk: Chems | 0.07 | Risk pool gain per chem use. |
| Risk: Combat | 0.05 | Risk pool gain from combat exposure to disease-carrying enemies. |
| Risk: Risky Food | 0.12 | Risk pool gain from high-risk food (raw, irradiated, etc.). |
| Risk: Standard Food | 0.07 | Risk pool gain from standard food. |
| Risk: Rain Exposure | 0.03 | Risk pool gain from being in radioactive rain. |
| Risk: Swimming | 0.03 | Risk pool gain from swimming. |

### Adrenaline

| Slider | Default | What it changes |
|---|---|---|
| Kills Per Perk Rank | 5 | Kills needed to advance one Adrenaline rank. |
| Max Adrenaline Rank | 10 | Maximum Adrenaline perk rank. |

### Encumbrance

| Slider | Default | What it changes |
|---|---|---|
| Encumbrance Check Interval | 24 | In-game hours between over-encumbrance damage ticks. |

All tuning sliders write the `Hardcore:HC_ManagerScript` Papyrus object's properties directly. The script reads its own properties each tick, so changes propagate within a few in-game minutes; save+reload for immediate effect.

## Coming Soon

Finer-grained Survival tuning sliders (tick rates, stage thresholds, combat-tick multipliers) are planned for a follow-up release once the Papyrus VM writer infrastructure lands.

## Validation

House Rules has a built-in self-test. Set `bValidationAudit=1` under `[Diagnostic]` in `Data/MCM/Config/HouseRules/settings.ini` (optionally `sValidationAuditMode=Full` for per-target detail), then open and close the pause menu in-game. The plugin writes per-module PASS / FAIL / SKIP records to `HouseRules.log` on the next apply pass. Every active GMST writer should show PASS. Set the audit back to `0` afterward so it stops writing on every menu close.

To check the log automatically:

```powershell
python tools\validate_house_rules_log.py `
    --require-module DifficultyEffects --require-module Character `
    --require-module ActorValues --require-module DamageFormulas `
    --require-module PowerArmor --require-module Economy `
    --require-module Progression --require-module VATS `
    --require-module Skills --require-module Sneak `
    --require-module CombatPerks --require-module Settlements
```

Companions Affinity, Survival kill-switches + tuning, and SurvivalCarryWeight don't run through this GMST audit; they verify via their own log line prefixes (`Globals: wrote FormID ...`, `HCManagerScript: 'PropName' = value`, `SurvivalCarryWeight: gate=ON wrote N effect magnitude(s)`).

The validation list will grow as new tracks ship.
