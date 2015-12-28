/// This is where the magic happens; program your plug-in's core behavior here.

#include "game_hooks.h"
#include <graphics/graphics.h>
#include <SCBW/api.h>
#include <SCBW/scbwdata.h>
#include <SCBW/ExtendSightLimit.h>
#include "psi_field.h"
#include <cstdio>

#include <SCBW/UnitFinder.h>

bool chargeTargetInRange_gameHooks(const CUnit *zealot) {
  if (!zealot->orderTarget.unit)
    return false;
  CUnit *chargeTarget = zealot->orderTarget.unit;
  int maxChargeRange = 3 << 5;
  int minChargeRange = 16;
  int chargeRange = zealot->getDistanceToTarget(zealot->orderTarget.unit);
  if (zealot->mainOrderId != OrderId::AttackUnit)
    return false;
  if (minChargeRange > chargeRange
    || chargeRange > maxChargeRange)
    return false;
  return true;
}

static u32 warpOverlay(u32 thisUnitId){
  switch(thisUnitId) {
    case UnitId::ProtossZealot: //Zealot
      return ImageId::ZealotWarpFlash;
    case UnitId::Hero_Raszagal: //Adept
      return ImageId::DarkTemplar_Hero;
    case UnitId::Hero_FenixZealot: //Sentry
      return ImageId::DarkTemplar_Hero;
    case UnitId::ProtossDragoon: //Dragoon
      return ImageId::DragoonWarpFlash;
    case UnitId::Hero_FenixDragoon: //Stalker
      return ImageId::ValkyrieEngines2_Unused;
    case UnitId::ProtossHighTemplar: //High Templar
      return ImageId::HighTemplarWarpFlash;
    case UnitId::ProtossDarkTemplar: //Dark Templar
      return ImageId::DarkTemplar_Unit;
    default: return ImageId::DarkTemplar_Hero;
  }
}

/* KYSXD Unit Cooldowns:
  Zealot    = 28
  Adept     = 28
  Sentry    = 32
  Dragoon   = 32
  Stalker   = 32
  HTemplat  = 45
  DTemplar  = 45
*/
static u16 warpCooldown(u32 thisUnitId){
  switch(thisUnitId) {
    case UnitId::ProtossZealot:
      return 1; break;
    case UnitId::Hero_Raszagal: //Adept
      return 1; break;
    case UnitId::Hero_FenixZealot: //Sentry
      return 2; break;
    case UnitId::ProtossDragoon:
      return 2; break;
    case UnitId::Hero_FenixDragoon: //Stalker
      return 2; break;
    case UnitId::ProtossHighTemplar:
      return 3; break;
    case UnitId::ProtossDarkTemplar:
      return 3; break;
    default: return 4; break;
  }
}

bool isInHarvestState(const CUnit *worker) {
  if (worker->unusedTimer == 0) {
    return false;
  }
  else return true;
}

bool thisIsMineral(const CUnit *resource) {
  if (UnitId::ResourceMineralField <= resource->id && resource->id <= UnitId::ResourceMineralFieldType3) {
    return true;
  }
  else return false;
}

bool hasHarvestOrders(const CUnit *worker) {
  if (OrderId::Harvest1 <= worker->mainOrderId &&
      worker->mainOrderId <= OrderId::Harvest5){
    return true;
  }
  else return false;
}

//KYSXD custom helper class
class HarvestTargetFinder: public scbw::UnitFinderCallbackMatchInterface {
  const CUnit *mainHarvester;
  public:
    void setmainHarvester(const CUnit *mainHarvester) { this->mainHarvester = mainHarvester; }
    bool match(const CUnit *unit) {
      if (!unit)
        return false;

      if (mainHarvester->getDistanceToTarget(unit) > (16 << 5)) //Harvest distance
        return false;

      if (!(thisIsMineral(unit)))
        return false;

      return true;
    }
};
HarvestTargetFinder harvestTargetFinder;

class ContainerTargetFinder: public scbw::UnitFinderCallbackMatchInterface {
  const CUnit *mineral;
  public:
    void setMineral(const CUnit *mineral) { this->mineral = mineral; }
    bool match(const CUnit *unit) {
      if (!unit)
        return false;

      if (!(units_dat::BaseProperty[unit->id] & UnitProperty::ResourceDepot))
        return false;

      if (!(unit->status & UnitStatus::Completed))
        return false;

      if (scbw::getActiveTileAt(mineral->getX(), mineral->getY()).groundHeight
        != scbw::getActiveTileAt(unit->getX(), unit->getY()).groundHeight)
        return false;

      if (!(unit->hasPathToUnit(mineral)))
        return false;

      return true;
    }
};
ContainerTargetFinder containerTargetFinder;

namespace hooks {

/// This hook is called every frame; most of your plugin's logic goes here.
bool nextFrame() {

  if (!scbw::isGamePaused()) { //If the game is not paused

    scbw::setInGameLoopState(true); //Needed for scbw::random() to work
    graphics::resetAllGraphics();
    hooks::updatePsiFieldProviders();

    //KYSXD idle worker amount
    u32 idleWorkerCount = 0;

    //Number of displayed lines:
    u32 titleLayer = 1;

    //KYSXD for use with Warpgate
    u32 warpgateAvailable = 0;
    u32 warpgateAmount = 0;
    u32 warpgateTime = 0;
    CUnit *lastWG[8];
    bool warpgateUpdate[8];
    u32 warpgateCall[8];
    for (int i = 0; i < 8; i++) {
      lastWG[i] = NULL;
      warpgateUpdate[i] = false;
      warpgateCall[i] = 0;
    }
    
    //This block is executed once every game.
    if (*elapsedTimeFrames == 0) {
      scbw::printText(PLUGIN_NAME ": Test");

      //For non-custom games - start
      if (!(*GAME_TYPE == 10)) {

        //KYSXD - Increase initial amount of Workers - From GagMania
        u16 initialworkeramount = 12;
        for (CUnit* base = *firstVisibleUnit; base; base = base->link.next) {
          if (base->mainOrderId != OrderId::Die) {
            u16 workerUnitId = UnitId::None;
            switch (base->id){
              case UnitId::command_center:
                workerUnitId = UnitId::scv; break;
              case UnitId::hatchery:
                workerUnitId = UnitId::drone; break;
              case UnitId::nexus:
                workerUnitId = UnitId::probe; break;
              default: break;
            }
            for (int i = 0; i < (initialworkeramount-4); i++) {
              scbw::createUnitAtPos(workerUnitId, base->playerId, base->getX(), base->getY());
            }
          }
        }

        //KYSXD Send all units to harvest on first run
        for (CUnit *harvesterUnit = *firstVisibleUnit; harvesterUnit; harvesterUnit = harvesterUnit->link.next) {
          if (units_dat::BaseProperty[harvesterUnit->id] & UnitProperty::Worker) {
            harvestTargetFinder.setmainHarvester(harvesterUnit);
            const CUnit *harvestTarget = scbw::UnitFinder::getNearestTarget(
              harvesterUnit->getX() - 512, harvesterUnit->getY() - 512,
              harvesterUnit->getX() + 512, harvesterUnit->getY() + 512,
              harvesterUnit,
              harvestTargetFinder);
            if (harvestTarget) {
              harvesterUnit->orderTo(OrderId::Harvest1, harvestTarget);
            }
          }
        }
      }
      //For non-custom games - end
    }

    //Loop through all visible units in the game.
    for (CUnit *unit = *firstVisibleUnit; unit; unit = unit->link.next) {

      //KYSXD worker no collision if harvesting - start
      if (units_dat::BaseProperty[unit->id] & UnitProperty::Worker) {
        if (hasHarvestOrders(unit)) {
            unit->unusedTimer = 2;
            unit->status |= UnitStatus::NoCollide;
        }
        else {
          unit->status &= ~(UnitStatus::NoCollide);
        }
      } //KYSXD worker no collision if harvesting - end

  //KYSXD - Protoss plugins:

      //KYSXD stalker's blink start
      if (unit->id == UnitId::Hero_FenixDragoon) {
        if (unit->mainOrderId == OrderId::CarrierIgnore1) {
          u16 thisX = unit->orderTarget.pt.x;
          u16 thisY = unit->orderTarget.pt.y;
          scbw::moveUnit(unit, thisX, thisY);
          unit->mainOrderId = OrderId::Nothing2;
        }
      } //KYSXD stalker's blink end

      //KYSXD zealot's charge start
      //Check max_energy.cpp and unit_speed.cpp for more info
      if (unit->id == UnitId::ProtossZealot
        && unit->status & UnitStatus::SpeedUpgrade) {
        //Unit isn't in charge state
        if (!unit->stimTimer) {
          if (chargeTargetInRange_gameHooks(unit)) {
            //Must be: orderTo(CastStimPack)
            if (unit->energy == unit->getMaxEnergy()) {
              unit->stimTimer = 5;
              unit->energy = 0;
            }
          }
        }
      } //KYSXD zealot's charge end

      //KYSXD Chrono boost cast start
      if (unit->id == UnitId::ProtossNexus
        && unit->status & UnitStatus::Completed) {
        if (unit->mainOrderId == OrderId::PlaceScanner) {
          if (unit->orderTarget.unit
            && scbw::isAlliedTo(unit->playerId, unit->orderTarget.unit->playerId)) {
            unit->orderTarget.unit->stimTimer = 60; //~30 seconds
            unit->mainOrderId = OrderId::Nothing2;
            if (!scbw::isCheatEnabled(CheatFlags::TheGathering)) {
              unit->energy -= 50 << 8;
            }
          }
          else unit->mainOrderId = OrderId::Nothing2;
        }
      } //KYSXD Chrono boost cast end

      //KYSXD Chrono boost behavior start
      //Should be moved to update_unit_state
      if (unit->status & UnitStatus::GroundedBuilding
        && unit->status & UnitStatus::Completed
        && unit->stimTimer
        && !(unit->isFrozen())) {
        //If the building is working
        switch (unit->mainOrderId) {
          case OrderId::Upgrade:
            if (unit->building.upgradeResearchTime >= 4)//Still need to set the right value
              unit->building.upgradeResearchTime -= 4;
            else unit->building.upgradeResearchTime = 0;
            break;
          case OrderId::ResearchTech:
            if (unit->building.upgradeResearchTime >= 4)//Still need to set the right value
              unit->building.upgradeResearchTime -= 4;
            else unit->building.upgradeResearchTime = 0;
            break;
          case OrderId::Train:
            if (unit->currentBuildUnit) {
              if (unit->currentBuildUnit->remainingBuildTime >= 4)//Still need to set the right value
                unit->currentBuildUnit->remainingBuildTime -= 4;
              else unit->currentBuildUnit->remainingBuildTime = 0;
              break;
            }
          default: break;
        }
        //Case: Warpgate
        if (unit->id == UnitId::ProtossGateway
          && unit->previousUnitType != 0) {
          u32 energyHolder = unit->energy + 4; //Still need to set the right value
          if (energyHolder >= unit->getMaxEnergy())
            unit->energy = unit->getMaxEnergy();
          else unit->energy = energyHolder;
        }
        //Case: Larva spawn
        if (unit->building.larvaTimer
          && !unit->orderQueueTimer) { //Larva spawn it's an order
          unit->building.larvaTimer--; //NOTE: -=1 it's too fast
        }
      } //KYSXD Chrono boost behavior end

      //KYSXD Warpgate start
      //Check max_energy.cpp
      if (unit->id == UnitId::ProtossGateway
        && unit->status & UnitStatus::Completed
        && unit->playerId == *LOCAL_NATION_ID
        && !(unit->isFrozen())) {
        if (unit->playerId == *LOCAL_NATION_ID) {
          ++warpgateAmount;
        }
        unit->currentButtonSet = UnitId::None;
        if (unit->getMaxEnergy() == unit->energy) {
          unit->previousUnitType = 0;
          if (unit->playerId == *LOCAL_NATION_ID) {
            ++warpgateAvailable;
          }
          if (lastWG[unit->playerId] == NULL) {
            lastWG[unit->playerId] = unit;
          }
        }
        else {
          u32 val = unit->getMaxEnergy() - unit->energy;
          if (warpgateTime == 0
            || val < warpgateTime) {
            warpgateTime = val;
          }
        }
        if (unit->buildQueue[unit->buildQueueSlot] != UnitId::None) {
          u32 thisUnitId = unit->buildQueue[unit->buildQueueSlot];
          warpgateCall[unit->playerId] = thisUnitId;
          u32 yPos = unit->orderTarget.pt.y;
          u32 xPos = unit->orderTarget.pt.x;
          CUnit *warpUnit = scbw::createUnitAtPos(thisUnitId, unit->playerId, xPos, yPos);
          if (warpUnit){
//            warpUnit->sprite->createOverlay(warpOverlay(thisUnitId));
          }
          unit->mainOrderId = OrderId::Nothing2;
          unit->buildQueue[unit->buildQueueSlot] = UnitId::None;
          if (!scbw::isCheatEnabled(CheatFlags::OperationCwal)
            && unit->playerId == *LOCAL_NATION_ID) {
            --warpgateAvailable;
          }
          warpgateUpdate[unit->playerId] = true;
        }
      } //KYSXD Warpgate end

      //KYSXD Idle worker count
      if ((unit->playerId == *LOCAL_NATION_ID || scbw::isInReplay())
          && units_dat::BaseProperty[unit->id] & UnitProperty::Worker
          && unit->mainOrderId == OrderId::PlayerGuard) {
        ++idleWorkerCount;
      }
    }

    //KYSXD update last warpgate
    for (int i = 0; i < 8; i++) {
      if (!scbw::isCheatEnabled(CheatFlags::OperationCwal)
        && warpgateUpdate[i] == true
        && lastWG[i] != NULL) {
        lastWG[i]->previousUnitType = warpCooldown(warpgateCall[i]);
      }      
    }

    for (int i = 0; i < *clientSelectionCount; ++i) {
      CUnit *selUnit = clientSelectionGroup->unit[i];

      //KYSXD update ButtonSet for WG
      if (selUnit->id == UnitId::ProtossGateway
        && selUnit->status & UnitStatus::Completed) {
        selUnit->currentButtonSet =
          (warpgateAvailable != 0 || scbw::isCheatEnabled(CheatFlags::OperationCwal) ? UnitId::ProtossGateway : UnitId::None);
      }

      /*/KYSXD unit worker count start  
      if (selUnit->playerId == *LOCAL_NATION_ID
        && selUnit->status & UnitStatus::Completed
        && units_dat::BaseProperty[selUnit->id] & UnitProperty::ResourceDepot) {
        int nearmineral = 0;
        int nearworkers = 0;
        CUnit *nearestContainer = NULL;
        //KYSXD count the neares minerals
        static scbw::UnitFinder mineralfinder;
        mineralfinder.search(selUnit->getX()-256, selUnit->getY()-256, selUnit->getX()+256, selUnit->getY()+256);
        for(int k = 0; k < mineralfinder.getUnitCount(); ++k) {
          CUnit *curMin = mineralfinder.getUnit(k);
          if (thisIsMineral(curMin)) {
            containerTargetFinder.setMineral(curMin);
            nearestContainer = scbw::UnitFinder::getNearestTarget(
            curMin->getX() - 300, curMin->getY() - 300,
            curMin->getX() + 300, curMin->getY() + 300,
            curMin,
            containerTargetFinder);
            if (nearestContainer != NULL
              && nearestContainer == selUnit) {
              nearmineral++;
            }
          }
          for(CUnit *thisunit = *firstVisibleUnit; thisunit; thisunit = thisunit->link.next){
            if (thisunit->playerId == *LOCAL_NATION_ID
              && (units_dat::BaseProperty[thisunit->id] & UnitProperty::Worker)
              && isInHarvestState(thisunit)
              && thisunit->worker.targetResource.unit != NULL
              && thisunit->worker.targetResource.unit == curMin) {
                nearworkers++;
            }
          }
        }
        //KYSXD count the nearest Workers
        if (nearmineral > 0) {
          char unitcount[64];
          sprintf_s(unitcount, "Workers: %d / %d", nearworkers, nearmineral);
          graphics::drawText(selUnit->getX() - 32, selUnit->getY() - 60, unitcount, graphics::FONT_MEDIUM, graphics::ON_MAP);
        }
      }*/
      //KYSXD unit worker count end

      //KYSXD Twilight Archon
      if (selUnit->id == UnitId::ProtossDarkTemplar || selUnit->id == UnitId::ProtossHighTemplar) {
        if (selUnit->mainOrderId == OrderId::ReaverStop) {
          for (int j = i+1; j < *clientSelectionCount && selUnit->mainOrderId == OrderId::ReaverStop; ++j) {
            CUnit *nextUnit = clientSelectionGroup->unit[j];
            if (nextUnit->id == (UnitId::ProtossDarkTemplar + UnitId::ProtossHighTemplar) - selUnit->id
              && nextUnit->mainOrderId == OrderId::ReaverStop) {
              selUnit->orderTo(OrderId::WarpingDarkArchon, nextUnit);
              nextUnit->orderTo(OrderId::WarpingDarkArchon, selUnit);
            }
          }
        }
      }

    //KYSXD - For selected units - From SC Transition - start
      //Draw attack radius circles for Siege Mode Tanks in current selection
      if (selUnit->id == UnitId::TerranSiegeTankSiegeMode) {
        graphics::drawCircle(selUnit->getX(), selUnit->getY(),
        selUnit->getMaxWeaponRange(units_dat::GroundWeapon[selUnit->subunit->id]) + 30,
        graphics::TEAL, graphics::ON_MAP);
      }

      //Display rally points for factories selected
      if (selUnit->status & UnitStatus::GroundedBuilding
        && units_dat::GroupFlags[selUnit->id].isFactory
        && (selUnit->playerId == *LOCAL_NATION_ID || scbw::isInReplay())) //Show only if unit is your own or the game is in replay mode
      {
        const CUnit *rallyUnit = selUnit->rally.unit;
        //Rallied to self; disable rally altogether
        if (rallyUnit != selUnit) {
          //Is usable rally unit
          if (rallyUnit && rallyUnit->playerId == selUnit->playerId) {
            graphics::drawLine(selUnit->getX(), selUnit->getY(),
              rallyUnit->getX(), rallyUnit->getY(),
              graphics::GREEN, graphics::ON_MAP);
            graphics::drawCircle(rallyUnit->getX(), rallyUnit->getY(), 10,
              graphics::GREEN, graphics::ON_MAP);
          }
          //Use map position instead
          else if (selUnit->rally.pt.x != 0) {
            graphics::drawLine(selUnit->getX(), selUnit->getY(),
              selUnit->rally.pt.x, selUnit->rally.pt.y,
              graphics::YELLOW, graphics::ON_MAP);
            graphics::drawCircle(selUnit->rally.pt.x, selUnit->rally.pt.y, 10,
              graphics::YELLOW, graphics::ON_MAP);
          }
        }

        //Show worker rally point
        const CUnit *workerRallyUnit = selUnit->moveTarget.unit;
        if (workerRallyUnit) {
          graphics::drawLine(selUnit->getX(), selUnit->getY(),
            workerRallyUnit->getX(), workerRallyUnit->getY(),
            graphics::ORANGE, graphics::ON_MAP);
          graphics::drawCircle(
            workerRallyUnit->getX(), workerRallyUnit->getY(), 10,
            graphics::ORANGE, graphics::ON_MAP);
        }
      }
      //KYSXD - For selected units - From SC Transition - end
    }

    //KYSXD Print info on screen
    if (idleWorkerCount != 0) {
      char idleworkers[64];
      sprintf_s(idleworkers, "Idle Workers: %d", idleWorkerCount);
      graphics::drawText(10, 10*titleLayer, idleworkers, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
      ++titleLayer;
    }

    if (warpgateAmount != 0) {
      char WGates[64];
      sprintf_s(WGates, "Online Warpgates: %d", warpgateAvailable);
      graphics::drawText(10, 10*titleLayer, WGates, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
      ++titleLayer;
      if (warpgateAvailable < warpgateAmount) {
        char nextWGTime[64];
        sprintf_s(nextWGTime, "(Next in: %d sec)", (warpgateTime+127)/128);
        graphics::drawText(20, 10*titleLayer, nextWGTime, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;
      }
    }

//For research:
    if (*clientSelectionCount == 1) {
      CUnit *thisUnit = clientSelectionGroup->unit[0];

      char mainOrderTimer[64];
      sprintf_s(mainOrderTimer, "mainOrderTimer: %d", thisUnit->mainOrderTimer);
      graphics::drawText(10, 10*titleLayer, mainOrderTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
      ++titleLayer;

      char orderQueueTimer[64];
      sprintf_s(orderQueueTimer, "orderQueueTimer: %d", thisUnit->orderQueueTimer);
      graphics::drawText(10, 10*titleLayer, orderQueueTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
      ++titleLayer;

      char lastEventTimer[64];
      sprintf_s(lastEventTimer, "lastEventTimer: %d", thisUnit->lastEventTimer);
      graphics::drawText(10, 10*titleLayer, lastEventTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
      ++titleLayer;
    }

    scbw::setInGameLoopState(false);
  }
  return true;
}

bool gameOn() {
  return true;
}

bool gameEnd() {
  return true;
}

} //hooks