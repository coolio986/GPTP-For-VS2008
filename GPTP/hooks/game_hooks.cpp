/// This is where the magic happens; program your plug-in's core behavior here.

#include "game_hooks.h"
#include <graphics/graphics.h>
#include <SCBW/api.h>
#include <SCBW/scbwdata.h>
#include <SCBW/ExtendSightLimit.h>
#include "psi_field.h"
#include <cstdio>

#include <SCBW/UnitFinder.h>

//KYSXD helpers
CUnit *nearestTemplarMergePartner(CUnit *unit) {

  CUnit *nearest_unit = NULL; //was ebp-04
  u32 best_distance = MAXINT32;

  for (u32 i = 0; i < *clientSelectionCount; ++i) {
    CUnit *nextUnit = clientSelectionGroup->unit[i];

    if(nextUnit != NULL
      && nextUnit != unit
      && unit->id == (UnitId::ProtossDarkTemplar + UnitId::ProtossHighTemplar) - nextUnit->id
      && nextUnit->mainOrderId == OrderId::ReaverStop) {

      u32 x_distance, y_distance;
      x_distance = 
        unit->getX() -
        nextUnit->getX();

      x_distance *= x_distance;

      y_distance = 
        unit->getY() -
        nextUnit->getY();

      y_distance *= y_distance;

      if( (x_distance + y_distance) < best_distance ) {

        CUnit *new_nearest_unit = nextUnit;
        nextUnit = nearest_unit;
        nearest_unit = new_nearest_unit;
        best_distance = x_distance + y_distance;

      }
    }
  }

  return nearest_unit;
}

//Same as chargeTarget_ in unit_speed.cpp
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

//Returns the warp effect from images_dat
const u32 warpOverlay(u32 thisUnitId) {
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
      worker->mainOrderId <= OrderId::Harvest5) {
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

//KYSXD - display info on screen - Credits to GagMania
static const int viewingStatus = 4;
static int viewingCheck[8];
  //0 = Normal info. Eventually to be removed
  //1 = Basic info

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
    u16 warpgateCall[8];
    for (int i = 0; i < 8; i++) {
      lastWG[i] = NULL;
      warpgateUpdate[i] = false;
      warpgateCall[i] = UnitId::None;
    }
    
    bool isOperationCwalEnabled = scbw::isCheatEnabled(CheatFlags::OperationCwal);

    //This block is executed once every game.
    if (*elapsedTimeFrames == 0) {
      scbw::printText(PLUGIN_NAME ": Test");

      //start viewingCheck
      for (int i = 0; i < 8; i++) {
        viewingCheck[i] = 0;
      }

      //All nexi in 50
      for (CUnit *unit = *firstVisibleUnit; unit; unit = unit->link.next) {
        switch (unit->id) {
          case UnitId::ProtossNexus:
            unit->energy = 50 << 8;
            break;
          case UnitId::ProtossGateway:
            unit->previousUnitType = UnitId::None;
            break;
          default: break;
        }
      }

      //KYSXD - For non-custom games - start
      if (!(*GAME_TYPE == 10)) {

        //KYSXD - Increase initial amount of Workers - From GagMania
        u16 initialworkeramount = 12;
        for (CUnit *base = *firstVisibleUnit; base; base = base->link.next) {
          if (base->mainOrderId != OrderId::Die) {
            u16 workerUnitId = UnitId::None;
            switch (base->id) {
              case UnitId::TerranCommandCenter:
                workerUnitId = UnitId::TerranSCV; break;
              case UnitId::ZergHatchery:
                workerUnitId = UnitId::ZergDrone; break;
              case UnitId::ProtossNexus:
                workerUnitId = UnitId::ProtossProbe; break;
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

    //Loop through all visible units in the game - start
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

      //KYSXD Idle worker count
      if ((unit->playerId == *LOCAL_NATION_ID || scbw::isInReplay())
          && units_dat::BaseProperty[unit->id] & UnitProperty::Worker
          && unit->mainOrderId == OrderId::PlayerGuard) {
        ++idleWorkerCount;
      }

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
        //If the building is working - start
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
          default: break;
        }
        switch (unit->secondaryOrderId) {
          case OrderId::Train:
            if (unit->currentBuildUnit) {
              CUnit *unitInQueue = unit->currentBuildUnit;
              if (isOperationCwalEnabled)
                unitInQueue->remainingBuildTime -= std::min<u16>(unitInQueue->remainingBuildTime, 16);
              else
                unitInQueue->remainingBuildTime--;
            }
          default: break;
        }
        //Case: Warpgate
        if (unit->id == UnitId::ProtossGateway
          && unit->previousUnitType != UnitId::None) {
          u32 energyHolder = unit->energy + 17; //Should be on update_unit_state.cpp?
          if (energyHolder >= unit->getMaxEnergy())
            unit->energy = unit->getMaxEnergy();
          else unit->energy = energyHolder;
        }
        //Case: Larva spawn
          //Check larva_creep_spawn.cpp
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
          unit->previousUnitType = UnitId::None;
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
          u16 thisUnitId = unit->buildQueue[unit->buildQueueSlot];
          warpgateCall[unit->playerId] = thisUnitId;
          u32 yPos = unit->orderTarget.pt.y;
          u32 xPos = unit->orderTarget.pt.x;
          CUnit *warpUnit = scbw::createUnitAtPos(thisUnitId, unit->playerId, xPos, yPos);
          if (warpUnit) {
//            warpUnit->sprite->createOverlay(warpOverlay(thisUnitId));
          }
          unit->mainOrderId = OrderId::Nothing2;
          unit->buildQueue[unit->buildQueueSlot] = UnitId::None;
          if (!isOperationCwalEnabled
            && unit->playerId == *LOCAL_NATION_ID) {
            --warpgateAvailable;
          }
          warpgateUpdate[unit->playerId] = true;
        }
      } //KYSXD Warpgate end

  //KYSXD - Terran plugins
      //KYSXD - Reactor add-on plugin start

      //Check unit state
      if ((unit->id == UnitId::TerranFactory
        || unit->id == UnitId::TerranStarport)
        && unit->status & UnitStatus::Completed
        && unit->mainOrderId != OrderId::Die) {
        //Check add-on
        if (unit->building.addon != NULL) {
          CUnit *reactor = unit->building.addon;
          reactor->rally = unit->rally; //Update rally

          //If the unit is trainning, do reactor behavior
          if (unit->secondaryOrderId == OrderId::Train) {
            u16 changeQueue = (unit->buildQueueSlot + 1)%5;
            u16 tempId = unit->buildQueue[changeQueue];
            //If the addon isn't trainning
            if (tempId != UnitId::None
              && reactor->buildQueue[reactor->buildQueueSlot] == UnitId::None) {
              reactor->buildQueue[reactor->buildQueueSlot] = tempId;
              reactor->setSecondaryOrder(OrderId::Train);

              //Update remaining queue
              u16 queueId[5];
              for (int i = 0; i < 5; i++) {
                if (i != 4) {
                  queueId[i] = unit->buildQueue[(unit->buildQueueSlot+i+1)%5];
                }
                else queueId[i] = UnitId::None;
              }
              for (int i = 1; i < 5; i++) {
                unit->buildQueue[(unit->buildQueueSlot+i)%5] = queueId[i];
              }
            }
          }
          /*
          //If isn't trainning, disable reactor
          else if (reactor->secondaryOrderId == OrderId::Train) {
            unit->buildQueue[unit->buildQueueSlot] = reactor->buildQueue[reactor->buildQueueSlot];
            unit->setSecondaryOrder(OrderId::Train);
            if(unit->currentBuildUnit) {
              unit->currentBuildUnit->remainingBuildTime =
                reactor->currentBuildUnit->remainingBuildTime;              
            }
            reactor->setSecondaryOrder(OrderId::Nothing2);
            //Update remaining queue
            u16 queueId[5];
            for (int i = 0; i < 5; i++) {
              if (i != 4) {
                queueId[i] = reactor->buildQueue[(reactor->buildQueueSlot+i+1)%5];
              }
              else queueId[i] = UnitId::None;
            }
            for (int i = 0; i < 5; i++) {
              reactor->buildQueue[(reactor->buildQueueSlot+i)%5] = queueId[i];
            }
          }
          */
        }
      } //KYSXD - Reactor add-on plugin end

    } //Loop through all visible units in the game - end

    //KYSXD update last warpgate
    for (int i = 0; i < 8; i++) {
      if (!isOperationCwalEnabled
        && warpgateUpdate[i] == true
        && lastWG[i] != NULL) {
        lastWG[i]->previousUnitType = warpgateCall[i];
      }
    }

  //KYSXD - Selection plugins

    for (int i = 0; i < *clientSelectionCount; ++i) {
      CUnit *selUnit = clientSelectionGroup->unit[i];

      //KYSXD update ButtonSet for WG
      if (selUnit->id == UnitId::ProtossGateway
        && selUnit->status & UnitStatus::Completed) {
        selUnit->currentButtonSet =
          (warpgateAvailable != 0 || isOperationCwalEnabled ? UnitId::ProtossGateway : UnitId::None);
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
          for(CUnit *thisunit = *firstVisibleUnit; thisunit; thisunit = thisunit->link.next) {
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
          CUnit *templarPartner = nearestTemplarMergePartner(selUnit);
          if (templarPartner != NULL) {
              selUnit->orderTo(OrderId::WarpingDarkArchon, templarPartner);
              templarPartner->orderTo(OrderId::WarpingDarkArchon, selUnit);
          }
          else selUnit->mainOrderId = OrderId::Stop;
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
    if (viewingCheck[*LOCAL_HUMAN_ID] == 0) {
      if (idleWorkerCount != 0) {
        char idleworkers[64];
        sprintf_s(idleworkers, "Idle Workers: %d", idleWorkerCount);
        graphics::drawText(10, 10*titleLayer, idleworkers, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;
      }

      if (warpgateAmount != 0) {
        char WGates[64];
        if (isOperationCwalEnabled) {
          warpgateAvailable = warpgateAmount;
        }
        sprintf_s(WGates, "Online Warpgates: %d", warpgateAvailable);
        graphics::drawText(10, 10*titleLayer, WGates, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;
        if (warpgateAvailable < warpgateAmount) {
          char nextWGTime[64];
          sprintf_s(nextWGTime, "(Next in: %d sec)", warpgateTime >> 8);
          graphics::drawText(20, 10*titleLayer, nextWGTime, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
          ++titleLayer;
        }
      }
    }

  //For research:
    if (viewingCheck[*LOCAL_HUMAN_ID] == 1) {
      if (*clientSelectionCount == 1) {
        CUnit *thisUnit = clientSelectionGroup->unit[0];

        char hitPoints[64];
        sprintf_s(hitPoints, "hitPoints: %d", thisUnit->hitPoints);
        graphics::drawText(10, 10*titleLayer, hitPoints, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char getCurrentHpInGame[64];
        sprintf_s(getCurrentHpInGame, "getCurrentHpInGame(): %d", thisUnit->getCurrentHpInGame());
        graphics::drawText(10, 10*titleLayer, getCurrentHpInGame, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char shields[64];
        sprintf_s(shields, "shields: %d", thisUnit->shields);
        graphics::drawText(10, 10*titleLayer, shields, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char getCurrentShieldsInGame[64];
        sprintf_s(getCurrentShieldsInGame, "getCurrentShieldsInGame(): %d", thisUnit->getCurrentShieldsInGame());
        graphics::drawText(10, 10*titleLayer, getCurrentShieldsInGame, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char getCurrentLifeInGame[64];
        sprintf_s(getCurrentLifeInGame, "getCurrentLifeInGame(): %d", thisUnit->getCurrentLifeInGame());
        graphics::drawText(10, 10*titleLayer, getCurrentLifeInGame, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char energy[64];
        sprintf_s(energy, "energy: %d", thisUnit->energy);
        graphics::drawText(10, 10*titleLayer, energy, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char getMaxEnergy[64];
        sprintf_s(getMaxEnergy, "getMaxEnergy: %d", thisUnit->getMaxEnergy());
        graphics::drawText(10, 10*titleLayer, getMaxEnergy, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char getArmor[64];
        sprintf_s(getArmor, "getArmor: %d", thisUnit->getArmor());
        graphics::drawText(10, 10*titleLayer, getArmor, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char getArmorBonus[64];
        sprintf_s(getArmorBonus, "getArmorBonus: %d", thisUnit->getArmorBonus());
        graphics::drawText(10, 10*titleLayer, getArmorBonus, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char MineralCost[64];
        sprintf_s(MineralCost, "MineralCost: %d", units_dat::MineralCost[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, MineralCost, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char GasCost[64];
        sprintf_s(GasCost, "GasCost: %d", units_dat::GasCost[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, GasCost, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char TimeCost[64];
        sprintf_s(TimeCost, "TimeCost: %d", units_dat::TimeCost[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, TimeCost, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char DisplayedTimeCost[64];
        sprintf_s(DisplayedTimeCost, "DisplayedTimeCost: %d", units_dat::TimeCost[thisUnit->id]/15);
        graphics::drawText(10, 10*titleLayer, DisplayedTimeCost, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char SupplyRequired[64];
        sprintf_s(SupplyRequired, "SupplyRequired: %d", units_dat::SupplyRequired[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, SupplyRequired, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char SupplyProvided[64];
        sprintf_s(SupplyProvided, "SupplyProvided: %d", units_dat::SupplyProvided[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, SupplyProvided, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char SpaceRequired[64];
        sprintf_s(SpaceRequired, "SpaceRequired: %d", units_dat::SpaceRequired[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, SpaceRequired, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char SpaceProvided[64];
        sprintf_s(SpaceProvided, "SpaceProvided: %d", units_dat::SpaceProvided[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, SpaceProvided, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char SizeType[64];
        sprintf_s(SizeType, "SizeType: %d", units_dat::SizeType[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, SizeType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char SeekRange[64];
        sprintf_s(SeekRange, "SeekRange: %d", units_dat::SeekRange[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, SeekRange, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char SightRange[64];
        sprintf_s(SightRange, "SightRange: %d", units_dat::SightRange[thisUnit->id]);
        graphics::drawText(10, 10*titleLayer, SightRange, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

      }
    }

  //For research:
    if (viewingCheck[*LOCAL_HUMAN_ID] == 2) {
      if (*clientSelectionCount == 1) {
        CUnit *thisUnit = clientSelectionGroup->unit[0];

        char _unknown_0x066[64];
        sprintf_s(_unknown_0x066, "_unknown_0x066: %i", thisUnit->_unknown_0x066);
        graphics::drawText(10, 10*titleLayer, _unknown_0x066, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char previousUnitType[64];
        sprintf_s(previousUnitType, "previousUnitType: %i", thisUnit->previousUnitType);
        graphics::drawText(10, 10*titleLayer, previousUnitType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char status[64];
        sprintf_s(status, "status: %i", thisUnit->status);
        graphics::drawText(10, 10*titleLayer, status, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char orderSignal[64];
        sprintf_s(orderSignal, "orderSignal: %i", thisUnit->orderSignal);
        graphics::drawText(10, 10*titleLayer, orderSignal, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char orderUnitType[64];
        sprintf_s(orderUnitType, "orderUnitType: %i", thisUnit->orderUnitType);
        graphics::drawText(10, 10*titleLayer, orderUnitType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char wireframeRandomizer[64];
        sprintf_s(wireframeRandomizer, "wireframeRandomizer: %i", thisUnit->wireframeRandomizer);
        graphics::drawText(10, 10*titleLayer, wireframeRandomizer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char mainOrderId[64];
        sprintf_s(mainOrderId, "mainOrderId: 0x%02x", thisUnit->mainOrderId);
        graphics::drawText(10, 10*titleLayer, mainOrderId, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char mainOrderTimer[64];
        sprintf_s(mainOrderTimer, "mainOrderTimer: %d", thisUnit->mainOrderTimer);
        graphics::drawText(10, 10*titleLayer, mainOrderTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char mainOrderState[64];
        sprintf_s(mainOrderState, "mainOrderState: %d", thisUnit->mainOrderState);
        graphics::drawText(10, 10*titleLayer, mainOrderState, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char secondaryOrderId[64];
        sprintf_s(secondaryOrderId, "secondaryOrderId: 0x%02x", thisUnit->secondaryOrderId);
        graphics::drawText(10, 10*titleLayer, secondaryOrderId, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char secondaryOrderTimer[64];
        sprintf_s(secondaryOrderTimer, "secondaryOrderTimer: %d", thisUnit->secondaryOrderTimer);
        graphics::drawText(10, 10*titleLayer, secondaryOrderTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char secondaryOrderState[64];
        sprintf_s(secondaryOrderState, "secondaryOrderState: %d", thisUnit->secondaryOrderState);
        graphics::drawText(10, 10*titleLayer, secondaryOrderState, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char orderQueueTimer[64];
        sprintf_s(orderQueueTimer, "orderQueueTimer: %d", thisUnit->orderQueueTimer);
        graphics::drawText(10, 10*titleLayer, orderQueueTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char remainingBuildTime[64];
        sprintf_s(remainingBuildTime, "remainingBuildTime: %d", thisUnit->remainingBuildTime);
        graphics::drawText(10, 10*titleLayer, remainingBuildTime, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char lastEventTimer[64];
        sprintf_s(lastEventTimer, "lastEventTimer: %d", thisUnit->lastEventTimer);
        graphics::drawText(10, 10*titleLayer, lastEventTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char recentOrderTimer[64];
        sprintf_s(recentOrderTimer, "recentOrderTimer: %d", thisUnit->recentOrderTimer);
        graphics::drawText(10, 10*titleLayer, recentOrderTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char cycleCounter[64];
        sprintf_s(cycleCounter, "cycleCounter: %d", thisUnit->cycleCounter);
        graphics::drawText(10, 10*titleLayer, cycleCounter, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char airStrength[64];
        sprintf_s(airStrength, "airStrength: %d", thisUnit->airStrength);
        graphics::drawText(10, 10*titleLayer, airStrength, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char groundStrength[64];
        sprintf_s(groundStrength, "groundStrength: %d", thisUnit->groundStrength);
        graphics::drawText(10, 10*titleLayer, groundStrength, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char buildQueueSlot[64];
        sprintf_s(buildQueueSlot, "buildQueueSlot: %d", thisUnit->buildQueueSlot);
        graphics::drawText(10, 10*titleLayer, buildQueueSlot, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        for (int i = 0; i < 5; i++) {
          char buildQueue[64];
          sprintf_s(buildQueue, "buildQueue[%d]: %d",
            i,
            thisUnit->buildQueue[i]);
          graphics::drawText(10, 10*titleLayer, buildQueue, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
          ++titleLayer;          
        }

      }
    }

    if (viewingCheck[*LOCAL_HUMAN_ID] == 3) {
      if (*clientSelectionCount == 1) {
        CUnit *thisUnit = clientSelectionGroup->unit[0];

        u8 hasAddon = (thisUnit->building.addon != NULL ? 1 : 0);

        char AddOn[64];
        sprintf_s(AddOn, "AddOn: %d", hasAddon);
        graphics::drawText(10, 10*titleLayer, AddOn, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char addonBuildType[64];
        sprintf_s(addonBuildType, "addonBuildType: %d", thisUnit->building.addonBuildType);
        graphics::drawText(10, 10*titleLayer, addonBuildType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char upgradeResearchTime[64];
        sprintf_s(upgradeResearchTime, "upgradeResearchTime: %d", thisUnit->building.upgradeResearchTime);
        graphics::drawText(10, 10*titleLayer, upgradeResearchTime, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char techType[64];
        sprintf_s(techType, "techType: 0x%x", thisUnit->building.techType);
        graphics::drawText(10, 10*titleLayer, techType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char upgradeType[64];
        sprintf_s(upgradeType, "upgradeType: 0x%x", thisUnit->building.upgradeType);
        graphics::drawText(10, 10*titleLayer, upgradeType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char larvaTimer[64];
        sprintf_s(larvaTimer, "larvaTimer: %d", thisUnit->building.larvaTimer);
        graphics::drawText(10, 10*titleLayer, larvaTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char landingTimer[64];
        sprintf_s(landingTimer, "landingTimer: %d", thisUnit->building.landingTimer);
        graphics::drawText(10, 10*titleLayer, landingTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char creepTimer[64];
        sprintf_s(creepTimer, "creepTimer: %d", thisUnit->building.creepTimer);
        graphics::drawText(10, 10*titleLayer, creepTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char upgradeLevel[64];
        sprintf_s(upgradeLevel, "upgradeLevel: %d", thisUnit->building.upgradeLevel);
        graphics::drawText(10, 10*titleLayer, upgradeLevel, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

        char _padding_0E[64];
        sprintf_s(_padding_0E, "_padding_0E: %d", thisUnit->building._padding_0E);
        graphics::drawText(10, 10*titleLayer, _padding_0E, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleLayer;

/*
CUnit*  addon;
*/  
      }
    }

    int localPlayer = *LOCAL_HUMAN_ID;
    if (GetAsyncKeyState(VK_F5) & 0x0001) {
      viewingCheck[localPlayer] = (viewingCheck[localPlayer]+1)%viewingStatus;
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