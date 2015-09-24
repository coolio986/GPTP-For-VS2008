/// This is where the magic happens; program your plug-in's core behavior here.

#include "game_hooks.h"
#include <graphics/graphics.h>
#include <SCBW/api.h>
#include <SCBW/scbwdata.h>
#include <SCBW/ExtendSightLimit.h>
#include "psi_field.h"
#include <cstdio>

#include <SCBW/UnitFinder.h>

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

      if ((unit->id != UnitId::mineral_field_1) &&
          (unit->id != UnitId::mineral_field_2) &&
          (unit->id != UnitId::mineral_field_3))
        return false;

      return true;
    }
};
HarvestTargetFinder harvestTargetFinder;

class ContainerTargetFinder: public scbw::UnitFinderCallbackMatchInterface {
  const CUnit *mineral;
  public:
    void setContainer(const CUnit *mineral) { this->mineral = mineral; }
    bool match(const CUnit *unit) {
      if (!unit)
        return false;

      if (mineral->getDistanceToTarget(unit) > 288) //Building distance
        return false;

     if(scbw::getActiveTileAt(mineral->getX(), mineral->getY()).groundHeight
       != scbw::getActiveTileAt(unit->getX(), unit->getY()).groundHeight)
       return false;

      if(!(unit->hasPathToUnit(mineral)))
        return false;

      if ((unit->id != UnitId::command_center) &&
          (unit->id != UnitId::nexus) &&
          (unit->id != UnitId::hatchery) &&
          (unit->id != UnitId::lair) &&
          (unit->id != UnitId::hive))
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

      //KYSXD draws lines from the mineral field to nearest resource container
      if (unit->id == UnitId::mineral_field_1
        ||unit->id == UnitId::mineral_field_2
        ||unit->id == UnitId::mineral_field_3) {        
        containerTargetFinder.setContainer(unit);
        unit->building.addon = scbw::UnitFinder::getNearestTarget(
          unit->getX() - 288, unit->getY() - 288,
          unit->getX() + 288, unit->getY() + 288,
          unit,
          containerTargetFinder);
      }

      if(unit->id == UnitId::command_center
        ||unit->id == UnitId::nexus
        ||unit->id == UnitId::hatchery
        ||unit->id == UnitId::lair
        ||unit->id == UnitId::hive) {
        u32 nearmineral = 0;
        static scbw::UnitFinder mineralfinder;
        mineralfinder.search(unit->getX()-300, unit->getY()-300, unit->getX()+300, unit->getY()+300);
        for(int k = 0; k < mineralfinder.getUnitCount(); ++k) {
          if ((mineralfinder.getUnit(k)->id == UnitId::mineral_field_1
            || mineralfinder.getUnit(k)->id == UnitId::mineral_field_2
            || mineralfinder.getUnit(k)->id == UnitId::mineral_field_3)
            && mineralfinder.getUnit(k)->building.addon == unit) {
              nearmineral++;
          }
        }
        u32 nearworkers = 0;
        for(CUnit *thisunit = *firstVisibleUnit; thisunit; thisunit = thisunit->link.next){
          if((units_dat::BaseProperty[thisunit->id] & UnitProperty::Worker)
            && thisunit->worker.targetResource.unit != NULL
            && thisunit->worker.targetResource.unit->building.addon != NULL
            && thisunit->worker.targetResource.unit->building.addon == unit
            &&(thisunit->mainOrderId == OrderId::Harvest1
              || thisunit->mainOrderId == OrderId::Harvest2
              || thisunit->mainOrderId == OrderId::Harvest3
              || thisunit->mainOrderId == OrderId::Harvest4
              || thisunit->mainOrderId == OrderId::Harvest5
              || thisunit->mainOrderId == OrderId::HarvestGas1
              || thisunit->mainOrderId == OrderId::HarvestGas2
              || thisunit->mainOrderId == OrderId::HarvestGas3
              || thisunit->mainOrderId == OrderId::ReturnGas
              || thisunit->mainOrderId == OrderId::ReturnMinerals
              || thisunit->mainOrderId == OrderId::HarvestMinerals2
              || thisunit->mainOrderId == OrderId::MoveToMinerals
              || thisunit->mainOrderId == OrderId::MiningMinerals)) {
              nearworkers++;
          }
        }
        char unitcount[64];
        sprintf_s(unitcount, "Workers: %d / %d", nearworkers, nearmineral);
        graphics::drawText(unit->getX()-32, unit->getY()-30, unitcount, graphics::FONT_MEDIUM, graphics::ON_MAP);
      }

      //KYSXD worker no collision if harvesting - start
      if (units_dat::BaseProperty[unit->id] & UnitProperty::Worker) {
        if (unit->mainOrderId == OrderId::Harvest1
          || unit->mainOrderId == OrderId::Harvest2
          || unit->mainOrderId == OrderId::Harvest3
          || unit->mainOrderId == OrderId::Harvest4
          || unit->mainOrderId == OrderId::Harvest5
          || unit->mainOrderId == OrderId::HarvestGas1
          || unit->mainOrderId == OrderId::HarvestGas2
          || unit->mainOrderId == OrderId::HarvestGas3
          || unit->mainOrderId == OrderId::ReturnGas
          || unit->mainOrderId == OrderId::ReturnMinerals
          || unit->mainOrderId == OrderId::HarvestMinerals2
          || unit->mainOrderId == OrderId::MoveToMinerals
          || unit->mainOrderId == OrderId::MiningMinerals) {
            unit->status |= UnitStatus::NoCollide;
        }
        else {
          unit->status &= ~(UnitStatus::NoCollide);
        }
      }
      //KYSXD worker no collision if harvesting - end

      //KYSXD Idle worker count
      if (unit->playerId == *LOCAL_HUMAN_ID
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