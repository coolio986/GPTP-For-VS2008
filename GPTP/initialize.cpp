#include "definitions.h"
#include "Plugin.h"
#include "hook_tools.h"

//Hook header files
#include "hooks/game_hooks.h"
#include "graphics/draw_hook.h"

#include "hooks/apply_upgrade_flags.h"
#include "hooks/attack_priority.h"
#include "hooks/bunker_hooks.h"
#include "hooks/cloak_nearby_units.h"
#include "hooks/cloak_tech.h"
#include "hooks/consume.h"
#include "hooks/detector.h"
#include "hooks/harvest.h"
#include "hooks/rally_point.h"
#include "hooks/recharge_shields.h"
#include "hooks/spider_mine.h"
#include "hooks/stim_packs.h"
#include "hooks/tech_target_check.h"
#include "hooks/transfer_tech_upgrades.h"
#include "hooks/unit_speed.h"
#include "hooks/update_status_effects.h"
#include "hooks/update_unit_state.h"
#include "hooks/weapon_cooldown.h"
#include "hooks/weapon_damage.h"
#include "hooks/weapon_fire.h"

#include "hooks/unit_destructor_special.h"
#include "hooks/psi_field.h"

#include "hooks/unit_morph.h"
#include "hooks/building_morph.h"

#include "hooks/unit_stats/armor_bonus.h"
#include "hooks/unit_stats/sight_range.h"
#include "hooks/unit_stats/max_energy.h"
#include "hooks/unit_stats/weapon_range.h"
#include "hooks/interface/weapon_armor_tooltip.h"

#include "hooks/orders/infestation.h"
#include "hooks/orders/siege_transform.h"
#include "hooks/interface/buttonsets.h"
//#include "hooks/interface/selection.h"
#include "hooks/interface/select_one.h"

#include "hooks/orders/merge_units.h"
#include "hooks/orders/larva_creep_spawn.h"
#include "hooks/orders/liftland.h"
#include "hooks/orders/base_orders/attack_orders.h"
#include "hooks/orders/base_orders/stopholdpos_orders.h"
#include "hooks/orders/spells/recall_spell.h"
#include "hooks/orders/enter_nydus.h"

#include "AI/spellcasting.h"

#include <cstdio>
#include <ctime>

namespace {
  int compareDates(int d1_day, int d1_month, int d1_year, int d2_day, int d2_month, int d2_year) {
    int res = 0;
    res += (d2_year - d1_year)*10000;
    res += (d2_month - d1_month)*100;
    res += (d2_day - d1_day);
    return res;
  }

  void runDemoValidation() {
    struct tm aTime;
    memset(&aTime,0,sizeof(struct tm));
    time_t currentTime = time(NULL);
    localtime_s(&aTime, &currentTime);

    char *Months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};

    //Current date
    const int year = aTime.tm_year + 1900;
    const int month = aTime.tm_mon + 1;
    const int day = aTime.tm_mday;

    //Initial date for the demo
    const int i_YY = 2016;
    const int i_MM = 4;
    const int i_DD = 22;

    //Last date for the demo
    const int f_YY = 2016;
    const int f_MM = 4;
    const int f_DD = 24;

    if(compareDates(i_DD, i_MM, i_YY, day, month, year) < 0) {
      char demoText[200];
      sprintf_s(demoText, "Sorry!\nThe demo period begins %02d/%s/%04d", i_DD, Months[i_MM-1], i_YY);
      sprintf_s(demoText, "%s\nFor more info contact us at: %s", demoText, CONTACT_EMAIL);
      MessageBox(NULL, demoText, PLUGIN_NAME, MB_OK);
      exit(0);
    }
    else if(compareDates(day, month, year, f_DD, f_MM, f_YY) < 0) {
      char demoText[200];
      sprintf_s(demoText, "Sorry!\nThe demo period concluded %02d/%s/%04d", f_DD, Months[f_MM-1], f_YY);
      sprintf_s(demoText, "%s\nFor more info contact us at: %s", demoText, CONTACT_EMAIL);
      MessageBox(NULL, demoText, PLUGIN_NAME, MB_OK);
      exit(0);
    }
    return;
  }
}

/// This function is called when the plugin is loaded into StarCraft.
/// You can enable/disable each group of hooks by commenting them.
/// You can also add custom modifications to StarCraft.exe by using:
///    memoryPatch(address_to_patch, value_to_patch_with);

BOOL WINAPI Plugin::InitializePlugin(IMPQDraftServer *lpMPQDraftServer) {
  //KYSXD - Demo's Date validation
//  runDemoValidation();

  //KYSXD W-Mode start
  char exeChar[81];
  sprintf_s(exeChar, "Initializing %s", PLUGIN_NAME);
  sprintf_s(exeChar, "%s\nInclude W-Mode?", exeChar);
  int msgboxID = MessageBox(NULL, exeChar, PLUGIN_NAME , MB_YESNOCANCEL);
  switch (msgboxID) {
    case IDYES:
      HINSTANCE hDll;
      hDll =LoadLibrary("./WMODE.dll");
      hDll =LoadLibrary("./WMODE_FIX.dll");
      break;
    case IDCANCEL:
      exit(0);
      break;
    default: break;
  }//KYSXD W-Mode end

  //StarCraft.exe version check
  char exePath[300];
  const DWORD pathLen = GetModuleFileName(NULL, exePath, sizeof(exePath));
  if (pathLen == sizeof(exePath)) {
    MessageBox(NULL, "Error: Cannot check version of StarCraft.exe. The file path is too long.", NULL, MB_OK);
    return FALSE;
  }
  if (!checkStarCraftExeVersion(exePath))
    return FALSE;

  hooks::injectGameHooks();
  hooks::injectDrawHook();
  
  hooks::injectInfestationHooks();
  hooks::injectSiegeTransformHooks();
  hooks::injectButtonSetHooks();
//  hooks::injectSelectMod();
  hooks::injectSelectOneHooks();
  
  hooks::injectMergeUnitsHooks();
  hooks::injectLarvaCreepSpawnHooks();
  hooks::injectLiftLandHooks();
  hooks::injectAttackOrdersHooks();
  hooks::injectStopHoldPosOrdersHooks();
  hooks::injectRecallSpellHooks();
  hooks::injectEnterNydusHooks();

  hooks::injectApplyUpgradeFlags();
  hooks::injectAttackPriorityHooks();
  hooks::injectBunkerHooks();
  hooks::injectCloakNearbyUnits();
  hooks::injectCloakingTechHooks();
  hooks::injectConsumeHooks();
  hooks::injectDetectorHooks();
  hooks::injectHarvestResource();
  hooks::injectRallyHooks();
  hooks::injectRechargeShieldsHooks();
  hooks::injectSpiderMineHooks();
  hooks::injectStimPacksHooks();
  hooks::injectTechTargetCheckHooks();
  hooks::injectTransferTechAndUpgradesHooks();
  hooks::injectUnitSpeedHooks();
  hooks::injectUpdateStatusEffects();
  hooks::injectUpdateUnitState();
  hooks::injectWeaponCooldownHook();
  hooks::injectWeaponDamageHook();
  hooks::injectWeaponFireHooks();
  
  hooks::injectUnitDestructorSpecial();
  hooks::injectPsiFieldHooks();

  hooks::injectUnitMorphHooks();
  hooks::injectBuildingMorphHooks();
  
  hooks::injectArmorBonusHook();
  hooks::injectSightRangeHook();
  hooks::injectUnitMaxEnergyHook();
  hooks::injectWeaponRangeHooks();
  
  hooks::injectUnitTooltipHook();

  hooks::injectSpellcasterAI();

  return TRUE;
}
