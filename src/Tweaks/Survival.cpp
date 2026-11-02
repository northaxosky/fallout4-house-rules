#include "PCH.h"

#include "Tweaks/Survival.h"

#include "Globals/Globals.h"
#include "Settings.h"
#include "Survival/HCManagerScript.h"

#include <array>
#include <cstdint>
#include <optional>

namespace Tweaks::Survival
{
	namespace
	{
		// FormIDs sourced from the HCManagerProbe dump (Hardcore:HC_ManagerScript bound-object surface). All four globals default to 1; writing 0 disables the corresponding rule subsystem.
		constexpr std::uint32_t kHCRuleSustenanceEffects = 0x00000854u;
		constexpr std::uint32_t kHCRuleSleepEffects      = 0x00000812u;
		constexpr std::uint32_t kHCRuleDiseaseEffects    = 0x0000088Au;
		constexpr std::uint32_t kHCRuleAdrenalineOn      = 0x00000810u;

		void ApplyKillSwitches()
		{
			using S = MCM::Settings::Survival;
			Globals::WriteByFormID(kHCRuleSustenanceEffects, S::bDisableSustenance.GetValue()       ? 0.0f : 1.0f);
			Globals::WriteByFormID(kHCRuleSleepEffects,      S::bDisableSleepDeprivation.GetValue() ? 0.0f : 1.0f);
			Globals::WriteByFormID(kHCRuleDiseaseEffects,    S::bDisableDiseases.GetValue()         ? 0.0f : 1.0f);
			Globals::WriteByFormID(kHCRuleAdrenalineOn,      S::bDisableAdrenaline.GetValue()       ? 0.0f : 1.0f);
		}

		// The consume path ignores HC_Rule_SustenanceEffects; zero its costs too.
		struct ConsumeCost
		{
			const char*          property;
			std::optional<float> vanilla;
		};

		std::array g_consumeCosts{
			ConsumeCost{ "IncreasesHungerCostMult", std::nullopt },
			ConsumeCost{ "IncreasesThirstCostMult", std::nullopt },
			ConsumeCost{ "NukaFoodCostMult", std::nullopt }
		};

		void ApplyConsumeCosts(::Survival::HCManagerScript::Batch& a_hc)
		{
			const bool disabled = MCM::Settings::Survival::bDisableSustenance.GetValue();
			for (auto& cost : g_consumeCosts) {
				// Const properties reload from plugin data, so one snapshot per process holds.
				if (!cost.vanilla) {
					cost.vanilla = a_hc.GetFloat(cost.property);
				}
				if (cost.vanilla) {
					a_hc.SetFloat(cost.property, disabled ? 0.0f : *cost.vanilla);
				}
			}
		}

		void ApplyScriptTuning()
		{
			using S = MCM::Settings::Survival;
			::Survival::HCManagerScript::Batch hc;
			if (!hc) {
				return;
			}

			ApplyConsumeCosts(hc);

			// Sustenance tuning
			hc.SetInt  ("FoodCostPerTick",                 S::iFoodCostPerTick.GetValue());
			hc.SetInt  ("DrinkCostPerTick",                S::iDrinkCostPerTick.GetValue());
			hc.SetFloat("GamesHoursPerTick",               S::fGamesHoursPerTick.GetValue());
			hc.SetFloat("BonusDigestionHours",             S::fBonusDigestionHours.GetValue());
			hc.SetFloat("SustenanceTickWhileSleepingMult", S::fSustenanceTickWhileSleepingMult.GetValue());
			hc.SetFloat("TickHoursCostPerCombat",          S::fTickHoursCostPerCombat.GetValue());

			// Food stage thresholds
			hc.SetInt("iFoodPoolPeckishAmount",  S::iFoodPoolPeckishAmount.GetValue());
			hc.SetInt("iFoodPoolHungryAmount",   S::iFoodPoolHungryAmount.GetValue());
			hc.SetInt("iFoodPoolFamishedAmount", S::iFoodPoolFamishedAmount.GetValue());
			hc.SetInt("iFoodPoolRavenousAmount", S::iFoodPoolRavenousAmount.GetValue());
			hc.SetInt("iFoodPoolStarvingAmount", S::iFoodPoolStarvingAmount.GetValue());

			// Drink stage thresholds
			hc.SetInt("iDrinkPoolParchedAmount",            S::iDrinkPoolParchedAmount.GetValue());
			hc.SetInt("iDrinkPoolThirstyAmount",            S::iDrinkPoolThirstyAmount.GetValue());
			hc.SetInt("iDrinkPoolMildlyDehydratedAmount",   S::iDrinkPoolMildlyDehydratedAmount.GetValue());
			hc.SetInt("iDrinkPoolDehydratedAmount",         S::iDrinkPoolDehydratedAmount.GetValue());
			hc.SetInt("iDrinkPoolSeverelyDehydratedAmount", S::iDrinkPoolSeverelyDehydratedAmount.GetValue());

			// Sleep tuning (vanilla typo "Disase..." preserved on the script property name)
			hc.SetFloat("GameTimerInterval_SleepDeprivation",         S::fGameTimerIntervalSleepDeprivation.GetValue());
			hc.SetFloat("InsomniaSleepMult",                          S::fInsomniaSleepMult.GetValue());
			hc.SetInt  ("MinHoursForCuringSleepEffects",              S::iMinHoursForCuringSleepEffects.GetValue());
			hc.SetFloat("CaffeineInducedSleepDelay",                  S::fCaffeineInducedSleepDelay.GetValue());
			hc.SetFloat("ExtraCaffeineInducedSleepDelay",             S::fExtraCaffeineInducedSleepDelay.GetValue());
			hc.SetFloat("DisaseSleepinessSleepDeprivationTimerMult",  S::fDiseasedSleepinessTimerMult.GetValue());

			// Disease tuning
			hc.SetFloat("DiseaseGracePeriod",                 S::fDiseaseGracePeriod.GetValue());
			hc.SetFloat("DiseaseRiskRollThreshold",           S::fDiseaseRiskRollThreshold.GetValue());
			hc.SetFloat("DiseaseRiskDrainPerCycle",           S::fDiseaseRiskDrainPerCycle.GetValue());
			hc.SetFloat("CurrentDiseasePoolValueMult",        S::fCurrentDiseasePoolValueMult.GetValue());
			hc.SetFloat("DiseaseNeedMoreFoodMult",            S::fDiseaseNeedMoreFoodMult.GetValue());
			hc.SetFloat("ImmunodeficiencyDiseaseChanceMult",  S::fImmunodeficiencyDiseaseMult.GetValue());
			hc.SetFloat("DiseaseRiskCannibalAmount",          S::fDiseaseRiskCannibalAmount.GetValue());
			hc.SetFloat("DiseaseRiskChemsAmount",             S::fDiseaseRiskChemsAmount.GetValue());
			hc.SetFloat("DiseaseRiskCombatantAmount",         S::fDiseaseRiskCombatantAmount.GetValue());
			hc.SetFloat("DiseaseRiskFoodHighAmount",          S::fDiseaseRiskFoodHighAmount.GetValue());
			hc.SetFloat("DiseaseRiskFoodStandardAmount",      S::fDiseaseRiskFoodStandardAmount.GetValue());
			hc.SetFloat("DiseaseRiskRainAmount",              S::fDiseaseRiskRainAmount.GetValue());
			hc.SetFloat("DiseaseRiskSwimmingAmount",          S::fDiseaseRiskSwimmingAmount.GetValue());

			// Adrenaline tuning (MaxAdrenaline itself is a script-local, derived from these two)
			hc.SetInt("KillsForAdrenalinePerkLevel", S::iKillsForAdrenalinePerkLevel.GetValue());
			hc.SetInt("MaxAdrenalinePerkLevel",      S::iMaxAdrenalinePerkLevel.GetValue());

			// Encumbrance
			hc.SetFloat("GameTimerInterval_Encumbrance", S::fGameTimerIntervalEncumbrance.GetValue());
		}
	}

	void Apply()
	{
		if (!MCM::Settings::General::bEnabled.GetValue()) {
			return;
		}
		ApplyKillSwitches();
		ApplyScriptTuning();
	}
}
