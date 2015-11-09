/// This is where the magic happens; program your plug-in's core behavior here.

#include "game_hooks.h"
#include <graphics/graphics.h>
#include <SCBW/api.h>
#include <SCBW/scbwdata.h>
#include <SCBW/ExtendSightLimit.h>
#include "psi_field.h"
#include <cstdio>

#include <SCBW/UnitFinder.h>

u32 warpOverlay(u32 thisUnitId){
  switch(thisUnitId) {
    case UnitId::ProtossZealot:
      return ImageId::ZealotWarpFlash;
    case UnitId::ProtossDragoon:
      return ImageId::DragoonWarpFlash;
    case UnitId::ProtossHighTemplar:
      return ImageId::HighTemplarWarpFlash;
    case UnitId::Hero_Raszagal:
      return ImageId::DarkTemplar_Hero;
    default: return ImageId::DarkTemplar_Hero;
  }
}

u16 warpCooldown(u32 thisUnitId){
  switch(thisUnitId) {
    case UnitId::ProtossZealot:
      return 1;
      break;
    case UnitId::ProtossDragoon:
      return 2;
      break;
    case UnitId::ProtossHighTemplar:
      return 3;
      break;
    case UnitId::Hero_Raszagal:
      return 4;
      break;
    default: return 5; break;
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

      if (mainHarvester->getDistanceToTarget(unit) > 512) //Harvest distance
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

      if(!(unit->status & UnitStatus::Completed))
        return false;

     if(scbw::getActiveTileAt(mineral->getX(), mineral->getY()).groundHeight
       != scbw::getActiveTileAt(unit->getX(), unit->getY()).groundHeight)
       return false;

      if(!(unit->hasPathToUnit(mineral)))
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

    //KYSXD for use with WarpGate
    u32 warpGateAvailable = 0;
    u32 warpGateAmount = 0;
    CUnit *lastWG[8];
    bool warpGateUpdate[8];
    u32 warpGateCall[8];
    for (int i = 0; i < 8; i++) {
      lastWG[i] = NULL;
      warpGateUpdate[i] = false;
      warpGateCall[i] = 0;
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
            for (int i=0; i<(initialworkeramount-4); i++) {
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
      } //For non-custom games - end
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

      //KYSXD WarpGate start
      if (unit->id == UnitId::ProtossGateway
        && unit->status & UnitStatus::Completed
        && unit->playerId == *LOCAL_HUMAN_ID
        && !(unit->isFrozen())) {
        if (unit->playerId == *LOCAL_NATION_ID) {
          ++warpGateAmount;
        }
        unit->currentButtonSet = UnitId::None;
        if (unit->getMaxEnergy() == unit->energy) {
          unit->previousUnitType = 0;
          if (unit->playerId == *LOCAL_NATION_ID) {
            ++warpGateAvailable;
          }
          if (lastWG[unit->playerId] == NULL) {
            lastWG[unit->playerId] = unit;
          }
        }
        if (unit->buildQueue[unit->buildQueueSlot] != UnitId::None) {
          u32 thisUnitId = unit->buildQueue[unit->buildQueueSlot];
          warpGateCall[unit->playerId] = thisUnitId;
          u32 yPos = unit->orderTarget.pt.y;
          u32 xPos = unit->orderTarget.pt.x;
          CUnit *warpUnit = scbw::createUnitAtPos(thisUnitId, unit->playerId, xPos, yPos);
          if(warpUnit){
            warpUnit->sprite->createOverlay(warpOverlay(thisUnitId));
          }
          unit->mainOrderId = OrderId::Nothing2;
          unit->buildQueue[unit->buildQueueSlot] = UnitId::None;
          if (unit->playerId == *LOCAL_NATION_ID) {
            --warpGateAvailable;
          }
          warpGateUpdate[unit->playerId] = true;
        }
      } //KYSXD WarpGate end


      //KYSXD Idle worker count
      if ((unit->playerId == *LOCAL_NATION_ID || scbw::isInReplay())
          && units_dat::BaseProperty[unit->id] & UnitProperty::Worker
          && unit->mainOrderId == OrderId::PlayerGuard) {
        ++idleWorkerCount;
      }

    }

    //KYSXD update last warpgate
    for (int i = 0; i < 8; i++) {
      if(warpGateUpdate[i] == true && lastWG[i] != NULL) {
        lastWG[i]->previousUnitType = warpCooldown(warpGateCall[i]);
      }      
    }

    //KYSXD - For selected units - From SC Transition - start
    for (int i = 0; i < *clientSelectionCount; ++i) {
      CUnit *selUnit = clientSelectionGroup->unit[i];

      //KYSXD update ButtonSet for WG
      if(selUnit->id == UnitId::ProtossGateway) {
        selUnit->currentButtonSet = (warpGateAvailable != 0 ? UnitId::ProtossGateway : UnitId::None);
      }

      /*/KYSXD unit worker count start  
      if(selUnit->playerId == *LOCAL_NATION_ID
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
            if(thisunit->playerId == *LOCAL_NATION_ID
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
      if(selUnit->id == UnitId::ProtossDarkTemplar || selUnit->id == UnitId::ProtossHighTemplar) {
        if(selUnit->mainOrderId == OrderId::ReaverStop) {
          for (int j = i+1; j < *clientSelectionCount && selUnit->mainOrderId == OrderId::ReaverStop; ++j) {
            CUnit *nextUnit = clientSelectionGroup->unit[j];
            if(nextUnit->id == (UnitId::ProtossDarkTemplar + UnitId::ProtossHighTemplar) - selUnit->id
              && nextUnit->mainOrderId == OrderId::ReaverStop) {
              selUnit->orderTo(OrderId::WarpingDarkArchon, nextUnit);
              nextUnit->orderTo(OrderId::WarpingDarkArchon, selUnit);
            }
          }
        }
      }

      //Draw attack radius circles for Siege Mode Tanks in current selection
      if (selUnit->id == UnitId::siege_tank_s) {
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
      graphics::drawText(30, 10, idleworkers, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
    }

    if (warpGateAmount != 0) {
      char WGates[64];
      sprintf_s(WGates, "Available Warpgates: %d / %d", warpGateAvailable, warpGateAmount);
      graphics::drawText(30, 20, WGates, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
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