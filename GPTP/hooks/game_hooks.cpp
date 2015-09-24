/// This is where the magic happens; program your plug-in's core behavior here.

#include "game_hooks.h"
#include <graphics/graphics.h>
#include <SCBW/api.h>
#include <SCBW/scbwdata.h>
#include <SCBW/ExtendSightLimit.h>
#include "psi_field.h"
#include <cstdio>

#include <SCBW/UnitFinder.h>

bool isInHarvestState(const CUnit *worker) {
  if (worker->unusedTimer == 0) {
    return false;
  }
  else return true;
}

bool isMineral(const CUnit *resource) {
  if (176 <= resource->id && resource->id <= 178) {
    return true;
  }
  else return false;
}

bool hasHarvestOrders(const CUnit *worker) {
  if (worker->mainOrderId == OrderId::Harvest1
      || worker->mainOrderId == OrderId::Harvest2
      || worker->mainOrderId == OrderId::Harvest3
      || worker->mainOrderId == OrderId::Harvest4
      || worker->mainOrderId == OrderId::Harvest5
      || worker->mainOrderId == OrderId::HarvestGas1
      || worker->mainOrderId == OrderId::HarvestGas2
      || worker->mainOrderId == OrderId::HarvestGas3
      || worker->mainOrderId == OrderId::ReturnGas
      || worker->mainOrderId == OrderId::ReturnMinerals
      || worker->mainOrderId == OrderId::HarvestMinerals2
      || worker->mainOrderId == OrderId::MoveToMinerals
      || worker->mainOrderId == OrderId::MiningMinerals){
    return true;
  }
  else return false;
}

//KYSXD custom helper class
class HarvestTargetFinder: public scbw::UnitFinderCallbackMatchInterface {
  const CUnit *worker;
  public:
    void setWorker(const CUnit *worker) { this->worker = worker; }
    bool match(const CUnit *unit) {
      if (!unit)
        return false;

      if (worker->getDistanceToTarget(unit) > 512) //Harvest distance
        return false;

      if (!(isMineral(unit)))
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
    
    //This block is executed once every game.
    if (*elapsedTimeFrames == 0) {
      scbw::printText(PLUGIN_NAME ": Test");

      if (!(*GAME_TYPE == 10)) {
      //For non-custom games - start

        //KYSXD - Increase initial amount of workers - From GagMania
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
        for (CUnit *worker = *firstVisibleUnit; worker; worker = worker->link.next) {
          if (units_dat::BaseProperty[worker->id] & UnitProperty::Worker) {
            harvestTargetFinder.setWorker(worker);
            const CUnit *harvestTarget = scbw::UnitFinder::getNearestTarget(
              worker->getX() - 512, worker->getY() - 512,
              worker->getX() + 512, worker->getY() + 512,
              worker,
              harvestTargetFinder);
            if (harvestTarget) {
              worker->orderTo(OrderId::Harvest1, harvestTarget);
            }
          }
        }
      //For non-custom games - end
      }
    }

    //Loop through all visible units in the game.
    for (CUnit *unit = *firstVisibleUnit; unit; unit = unit->link.next) {

    //KYSXD unit worker count start  
      //KYSXD if is resource depot
      if(unit->playerId == *LOCAL_NATION_ID
        && unit->status & UnitStatus::Completed
        && units_dat::BaseProperty[unit->id] & UnitProperty::ResourceDepot) {
        int nearmineral = 0;
        //KYSXD count the neares minerals
        static scbw::UnitFinder mineralfinder;
        mineralfinder.search(unit->getX()-256, unit->getY()-256, unit->getX()+256, unit->getY()+256);
        for(int k = 0; k < mineralfinder.getUnitCount(); ++k) {
          CUnit *curMin = mineralfinder.getUnit(k);
          if (isMineral(curMin)) {
            if (curMin->building.addon ==  NULL
              || (curMin->building.addon !=  NULL
              && curMin->getDistanceToTarget(curMin->building.addon) < 288)) {
              containerTargetFinder.setMineral(curMin);
              curMin->building.addon = scbw::UnitFinder::getNearestTarget(
              curMin->getX() - 300, curMin->getY() - 300,
              curMin->getX() + 300, curMin->getY() + 300,
              curMin,
              containerTargetFinder);
            }
            if (curMin->building.addon != NULL
              &&curMin->building.addon == unit) {
              nearmineral++;
            }
          }
        }
        int nearworkers = 0;
        //KYSXD count the nearest workers
        if (nearmineral > 0) {
          for(CUnit *thisunit = *firstVisibleUnit; thisunit; thisunit = thisunit->link.next){
            if(thisunit->playerId == *LOCAL_NATION_ID
              && (units_dat::BaseProperty[thisunit->id] & UnitProperty::Worker)
              && isInHarvestState(thisunit)
              && thisunit->worker.targetResource.unit != NULL
              && thisunit->worker.targetResource.unit->building.addon != NULL
              && thisunit->worker.targetResource.unit->building.addon == unit) {
                nearworkers++;
            }
          }
          char unitcount[64];
          sprintf_s(unitcount, "Workers: %d / %d", nearworkers, nearmineral);
          graphics::drawText(unit->getX() - 32, unit->getY() - 60, unitcount, graphics::FONT_MEDIUM, graphics::ON_MAP);
        }
      }
      //KYSXD unit worker count end

      //KYSXD worker no collision if harvesting - start
      if (units_dat::BaseProperty[unit->id] & UnitProperty::Worker) {
        if (hasHarvestOrders(unit)) {
            unit->unusedTimer = 2;
            unit->status |= UnitStatus::NoCollide;
        }
        else {
          unit->status &= ~(UnitStatus::NoCollide);
        }
      }
      //KYSXD worker no collision if harvesting - end

      //KYSXD Idle worker count
      if ((unit->playerId == *LOCAL_NATION_ID || scbw::isInReplay())
          && units_dat::BaseProperty[unit->id] & UnitProperty::Worker
          && unit->mainOrderId == OrderId::PlayerGuard) {
        ++idleWorkerCount;
      }

    }

    //KYSXD - For selected units - From SC Transition - start
    for (int i = 0; i < *clientSelectionCount; ++i) {
      CUnit *selUnit = clientSelectionGroup->unit[i];

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

      if (idleWorkerCount != 0) {
        char idleworkers[64];
        sprintf_s(idleworkers, "Idle Worker: %d", idleWorkerCount);
        graphics::drawText(560, 30, idleworkers, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
      }

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