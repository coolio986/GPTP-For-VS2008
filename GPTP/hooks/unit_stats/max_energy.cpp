#include "max_energy.h"
#include <SCBW/scbwdata.h>
#include <SCBW/enumerations.h>
#include <SCBW/api.h>

namespace hooks {

/// Replaces the CUnit::getMaxEnergy() function.
/// Return the amount of maximum energy that a unit can have.
/// Note: 1 energy displayed in-game equals 256 energy.
u16 getUnitMaxEnergyHook(const CUnit* const unit) {
  //Default StarCraft behavior

  if (units_dat::BaseProperty[unit->id] & UnitProperty::Hero)
    return 64000; //250

  switch (unit->id) {
    case UnitId::science_vessel:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::TitanReactor))
        return 64000; //250
      break;
    case UnitId::ghost:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::MoebiusReactor))
        return 64000; //250
      break;
    case UnitId::wraith:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::ApolloReactor))
        return 64000; //250
      break;
    case UnitId::battlecruiser:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::ColossusReactor))
        return 64000; //250
      break;
    case UnitId::queen:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::GameteMeiosis))
        return 64000; //250
      break;
    case UnitId::defiler:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::MetasynapticNode))
        return 64000; //250
      break;
    case UnitId::high_templar:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::KhaydarinAmulet))
        return 64000; //250
      break;
    case UnitId::arbiter:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::KhaydarinCore))
        return 64000; //250
      break;
    case UnitId::corsair:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::ArgusJewel))
        return 64000; //250
      break;
    case UnitId::medic:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::CaduceusReactor))
        return 64000; //250
      break;
    case UnitId::dark_archon:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::ArgusTalisman))
        return 64000; //250
      break;
    //KYSXD for use with WarpGate
      /* KYSXD Unit Cooldowns:
        1 -> 28
        2 -> 32
        3 -> 45
      */
    case UnitId::gateway:
      switch(unit->previousUnitType) {
        case 0: return 0; break;
        case 1: return 28 << 7; break;
        case 2: return 32 << 7; break;
        case 3: return 45 << 7; break;
        default: return 1 << 8; break;
      }
      break;
    //KYSXD for use with the zealot's charge plugin
    case UnitId::zealot:
      if (scbw::getUpgradeLevel(unit->playerId, UpgradeId::LegEnhancements))
        return 10 << 7; //10
      else return 0;
      break;
  }

  return 51200; //200
}

} //hooks
