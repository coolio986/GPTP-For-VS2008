/// This is where the magic happens; program your plug-in's core behavior here.

#include "game_hooks.h"
#include <graphics/graphics.h>
#include <SCBW/api.h>
#include <SCBW/scbwdata.h>
#include <SCBW/ExtendSightLimit.h>

bool firstBoot =  false;

#include "psi_field.h"
#include <cstdio>

#include <SCBW/UnitFinder.h>

namespace {

//KYSXD - Replace unit function, copied from siege_transform.cpp and merge_units.cpp
//Useful for... i don't know, i was playing with it xD
const u32 Func_ReplaceUnitWithType = 0x0049FED0;
void replaceUnitWithType(CUnit *unit, u16 newUnitId) {
  u32 newUnitId_ = newUnitId;

  __asm {
    PUSHAD
    PUSH newUnitId_
    MOV EAX, unit
    CALL Func_ReplaceUnitWithType
    POPAD
  }

}

//copied from unit_morph_inject.cpp
const u32 Func_ReplaceSpriteImages = 0x00499BB0;
void replaceSpriteImages(CSprite *sprite, u16 imageId, u8 imageDirection) {
  u32 imageId_ = imageId, imageDirection_ = imageDirection;
  __asm {
    PUSHAD
    PUSH imageDirection_
    PUSH imageId_
    MOV EAX, sprite
    CALL Func_ReplaceSpriteImages
    POPAD
  }
}

//copied from bunker_hooks.cpp
const u32 Func_SetImageDirection = 0x004D5F80;
void setImageDirection(CImage *image, s8 direction) {
  u32 direction_ = direction;

  __asm {
    PUSHAD
    PUSH direction_
    MOV ESI, image
    CALL Func_SetImageDirection
    POPAD
  }
}


//KYSXD helpers
CUnit *nearestTemplarMergePartner(CUnit *unit) {

  CUnit *nearest_unit = NULL;
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
bool chargeTargetInRange(const CUnit *zealot) {
  if(!zealot->orderTarget.unit)
    return false;
  CUnit *chargeTarget = zealot->orderTarget.unit;
  int maxChargeRange = 3 << 5;
  int minChargeRange = 16;
  int chargeRange = zealot->getDistanceToTarget(zealot->orderTarget.unit);
  if(zealot->mainOrderId != OrderId::AttackUnit)
    return false;
  if(minChargeRange > chargeRange
    || chargeRange > maxChargeRange)
    return false;
  return true;
}

bool isInHarvestState(const CUnit *worker) {
  if(worker->unusedTimer == 0) {
    return false;
  }
  else return true;
}

bool thisIsMineral(const CUnit *resource) {
  if(UnitId::ResourceMineralField <= resource->id && resource->id <= UnitId::ResourceMineralFieldType3) {
    return true;
  }
  else return false;
}

bool hasHarvestOrders(const CUnit *worker) {
  if(OrderId::Harvest1 <= worker->mainOrderId &&
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
      if(!unit)
        return false;

      if(mainHarvester->getDistanceToTarget(unit) > (16 << 5)) //Harvest distance
        return false;

      if(!(thisIsMineral(unit)))
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
      if(!unit)
        return false;

      if(!(units_dat::BaseProperty[unit->id] & UnitProperty::ResourceDepot))
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

void runFirstFrameBehaviour() {
  scbw::printText("You're now playing:\n     "
    PLUGIN_NAME "\n     By " AUTHOR_NAME);
  //All nexi in 50
  for(CUnit *unit = *firstVisibleUnit; unit; unit = unit->link.next) {
    switch(unit->id) {
      case UnitId::ProtossNexus:
        unit->energy = 50 << 8;
        break;
      case UnitId::ProtossGateway:
      case UnitId::Special_WarpGate:
        unit->previousUnitType = UnitId::None;
        break;
      default: break;
    }
  }
  //KYSXD - For non-custom games - start
  if(!(*GAME_TYPE == 10)) {
    CUnit *firstMineral[8];
    u16 initialworkeramount = 12;
    for(CUnit *base = *firstVisibleUnit; base; base = base->link.next) {
      //KYSXD - Increase initial amount of Workers - From GagMania
      if(units_dat::BaseProperty[base->id] & UnitProperty::ResourceDepot) {
        u16 workerUnitId = UnitId::None;
        switch(base->getRace()) {
          case RaceId::Terran:
            workerUnitId = UnitId::TerranSCV; break;
          case RaceId::Zerg:
            workerUnitId = UnitId::ZergDrone; break;
          case RaceId::Protoss:
            workerUnitId = UnitId::ProtossProbe; break;
          default: break;
        }
        if(workerUnitId != UnitId::None) {
          for(u16 i = 0; i < (initialworkeramount-4); i++) {
            scbw::createUnitAtPos(workerUnitId, base->playerId, base->getX(), base->getY());
          }
        }
      //KYSXD - Find nearest mineral patch
        harvestTargetFinder.setmainHarvester(base);
        firstMineral[base->playerId] = scbw::UnitFinder::getNearestTarget(
          base->getX() - 512, base->getY() - 512,
          base->getX() + 512, base->getY() + 512,
          base,
          harvestTargetFinder);
      }
    }
    for(CUnit *worker = *firstVisibleUnit; worker; worker = worker->link.next) {
      //KYSXD Send all units to harvest on first run
      if(units_dat::BaseProperty[worker->id] & UnitProperty::Worker) {
        if(firstMineral[worker->playerId]) {
          worker->orderTo(OrderId::Harvest1, firstMineral[worker->playerId]);
        }
      }
    }
  }
  //For non-custom games - end  
}

//KYSXD worker no collision if harvesting - start
void manageWorkerCollision(CUnit *unit) {
  if(units_dat::BaseProperty[unit->id] & UnitProperty::Worker) {
    if(hasHarvestOrders(unit)) {
        unit->unusedTimer = 2;
        unit->status |= UnitStatus::NoCollide;
    }
    else {
      unit->status &= ~(UnitStatus::NoCollide);
    }
  }
}

//KYSXD PsionicTransfer cast
//Original from Eisetley, KYSXD modification
void runAdeptPsionicTransfer_cast(CUnit *adept) {
  if(adept->id == UnitId::Hero_Raszagal
    && adept->mainOrderId == OrderId::PlaceScanner) {

    adept->mainOrderId = OrderId::Nothing2;
    CUnit*shade = scbw::createUnitAtPos(UnitId::aldaris, adept->playerId, adept->getX(), adept->getY());
    if(shade) {
      shade->connectedUnit = adept;
      shade->unusedTimer = 2 * 7;

      shade->status |= UnitStatus::NoCollide;
      shade->status |= UnitStatus::IsGathering;
      if(adept->orderTarget.unit) {
       shade->orderTo(OrderId::Follow, adept->orderTarget.unit);
      }
      else
        shade->orderTo(OrderId::Move, adept->orderTarget.pt.x, adept->orderTarget.pt.y);

      if(!scbw::isCheatEnabled(CheatFlags::TheGathering)) {
        adept->energy = 0;
      }      
    }
  }
}

//KYSXD PsionicTransfer behavior
//Original from Eisetley, KYSXD modification
void runAdeptPsionicTransfer_behavior(CUnit *shade) {
  if(shade->id == UnitId::aldaris) {
    CUnit *adept = shade->connectedUnit;

    if(!adept) {
      shade->orderTo(OrderId::Die);
    }
    else {
      if(adept->getCurrentHpInGame() <= 0) {
        shade->orderTo(OrderId::Die);
      }

      if(!shade->unusedTimer) {
//      createThingy(375, adept->getX(), adept->getY(), adept->playerId);
        CImage *adeptSprite = adept->sprite->images.head;
        setImageDirection(adeptSprite, shade->currentDirection1);

        u16 thisX = shade->getX();
        u16 thisY = shade->getY();
        scbw::moveUnit(adept, thisX, thisY);
/*
        shade->userActionFlags |= 0x4;
        shade->remove();
        adept->sprite->createOverlay(557);
        scbw::playSound(1140, adept);
*/
      }      
    }
  }
}

//KYSXD Chrono boost cast
void runChronoBoost_cast(CUnit *unit) {
  if(unit->id == UnitId::ProtossNexus
    && unit->status & UnitStatus::Completed) {
    if(unit->mainOrderId == OrderId::PlaceScanner) {
      if(unit->orderTarget.unit
        && scbw::isAlliedTo(unit->playerId, unit->orderTarget.unit->playerId)) {
        unit->orderTarget.unit->stimTimer = 60; //~30 seconds
        unit->mainOrderId = OrderId::Nothing2;
        if(!scbw::isCheatEnabled(CheatFlags::TheGathering)) {
          unit->energy -= 50 << 8;
        }
      }
      else unit->mainOrderId = OrderId::Nothing2;
    }
  }
}

//KYSXD Chrono boost behavior
//Should be moved to update_unit_state
void runChronoBoost_behavior(CUnit *unit) {
  if(unit->status & UnitStatus::GroundedBuilding
    && unit->status & UnitStatus::Completed
    && unit->stimTimer
    && !(unit->isFrozen())) {
    //If the building is working - start
    switch (unit->mainOrderId) {
      case OrderId::Upgrade:
      case OrderId::ResearchTech:
        if(unit->building.upgradeResearchTime) {
          unit->building.upgradeResearchTime -= std::min<u16>(unit->building.upgradeResearchTime, 4);
        }
        break;
      default: break;
    }
    switch(unit->secondaryOrderId) {
      case OrderId::BuildAddon:
      case OrderId::Train:
        if(unit->currentBuildUnit) {
          CUnit *unitInQueue = unit->currentBuildUnit;
          if(unitInQueue->remainingBuildTime) {
            static s32 hpHolder;
            static s32 maxHp;
            maxHp = units_dat::MaxHitPoints[unitInQueue->id];
            if(scbw::isCheatEnabled(CheatFlags::OperationCwal)) {
              unitInQueue->remainingBuildTime -= std::min<u16>(unitInQueue->remainingBuildTime, 16);

              hpHolder = unitInQueue->hitPoints + (unitInQueue->buildRepairHpGain << 4);
              unitInQueue->hitPoints = std::min<s32>(maxHp, hpHolder);              
            }
            else {
              unitInQueue->remainingBuildTime--;
              
              hpHolder = unitInQueue->hitPoints + unitInQueue->buildRepairHpGain;
              unitInQueue->hitPoints = std::min<s32>(maxHp, hpHolder);              
            }
          }
        }
        break;
      default: break;
    }
    //Case: Warpgate
    if(unit->id == UnitId::Special_WarpGate
      && unit->previousUnitType != UnitId::None) {
      u32 energyHolder = unit->energy + 17; //Should be on update_unit_state.cpp?
      unit->energy = std::min<u32>(unit->getMaxEnergy(), energyHolder);
    }
    //Case: Larva spawn - Check larva_creep_spawn.cpp
  }
}

//KYSXD stalker's blink
void runStalkerBlink(CUnit *unit) {
  if(unit->id == UnitId::Hero_FenixDragoon) {
    if(unit->mainOrderId == OrderId::Ensnare) {
      u16 thisX = unit->orderTarget.pt.x;
      u16 thisY = unit->orderTarget.pt.y;
      scbw::moveUnit(unit, thisX, thisY);
      unit->mainOrderId = OrderId::Nothing2;
      if(!scbw::isCheatEnabled(CheatFlags::TheGathering)) {
        unit->energy = 0;
      }
    }
  }
}

//KYSXD zealot's charge
void runZealotCharge(CUnit *unit) {
  //Check max_energy.cpp and unit_speed.cpp for more info
  if(unit->id == UnitId::ProtossZealot
    && unit->status & UnitStatus::SpeedUpgrade) {
    //Unit isn't in charge state
    if(!unit->stimTimer) {
      if(chargeTargetInRange(unit)) {
        //Must be: orderTo(CastStimPack)
        if(unit->energy == unit->getMaxEnergy()) {
          unit->stimTimer = 5;
          unit->energy = 0;
        }
      }
    }
  }
}

//KYSXD Warpgate morph
void runWarpgateMorph(CUnit *unit) {
  if((unit->id == UnitId::ProtossGateway
    || unit->id == UnitId::Special_WarpGate)
    && unit->mainOrderId == OrderId::ReaverStop) {
    unit->mainOrderId = OrderId::Nothing2;
    if(unit->status & UnitStatus::Completed
      && !(unit->isFrozen())) {
      if(unit->previousUnitType == UnitId::None) {
        u16 morphId = UnitId::ProtossGateway + UnitId::Special_WarpGate - unit->id;
        replaceUnitWithType(unit, morphId);
        unit->mainOrderId = OrderId::Nothing2;
        unit->previousUnitType = UnitId::None;
        unit->shields = units_dat::MaxShieldPoints[morphId] << 8;
        unit->energy = 0;          
      }
    }
  }
}

//KYSXD - display info on screen - Credits to GagMania
static const int viewingStatus = 5;
static int viewingCheck[8];
  //0 = Normal info. Eventually to be removed
  //1 = Basic info
  //2 = Order related
  //3 = Addon related
}

namespace hooks {

/// This hook is called every frame; most of your plugin's logic goes here.
bool nextFrame() {

  if(!scbw::isGamePaused()) { //If the game is not paused

    scbw::setInGameLoopState(true); //Needed for scbw::random() to work
    graphics::resetAllGraphics();
    hooks::updatePsiFieldProviders();

    //KYSXD idle worker amount
    u32 idleWorkerCount = 0;

    //Number of displayed lines:
    u32 titleRow = 1;
    u32 titleCol = 1;

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
    if(*elapsedTimeFrames == 0) {
      runFirstFrameBehaviour();

      //start viewingCheck
      for (int i = 0; i < 8; i++) {
        viewingCheck[i] = 0;
      }

    }

    //Loop through all visible units in the game - start
    for (CUnit *unit = *firstVisibleUnit; unit; unit = unit->link.next) {
      manageWorkerCollision(unit);

      //KYSXD Idle worker count
      if((unit->playerId == *LOCAL_NATION_ID || scbw::isInReplay())
          && units_dat::BaseProperty[unit->id] & UnitProperty::Worker
          && unit->mainOrderId == OrderId::PlayerGuard) {
        ++idleWorkerCount;
      }

      //KYSXD - reset warpgates when build
      if((unit->id == UnitId::Special_WarpGate
        || unit->id == UnitId::ProtossGateway)
        && unit->mainOrderId == OrderId::BuildSelf2) {
        unit->previousUnitType = UnitId::None;
      }

  //KYSXD - Protoss plugins:
      runAdeptPsionicTransfer_cast(unit);
      runAdeptPsionicTransfer_behavior(unit);

      runChronoBoost_cast(unit);
      runChronoBoost_behavior(unit);

      runStalkerBlink(unit);
      runZealotCharge(unit);

      runWarpgateMorph(unit);

      //KYSXD Warpgate start
      //Check max_energy.cpp
      if(unit->id == UnitId::Special_WarpGate
        && unit->status & UnitStatus::Completed
        && unit->playerId == *LOCAL_NATION_ID
        && !(unit->isFrozen())) {
        if(unit->playerId == *LOCAL_NATION_ID) {
          ++warpgateAmount;
        }
        unit->currentButtonSet = UnitId::None;
        if(unit->getMaxEnergy() == unit->energy) {
          unit->previousUnitType = UnitId::None;
          if(unit->playerId == *LOCAL_NATION_ID) {
            ++warpgateAvailable;
          }
          if(lastWG[unit->playerId] == NULL) {
            lastWG[unit->playerId] = unit;
          }
        }
        else {
          u32 val = unit->getMaxEnergy() - unit->energy;
          if(warpgateTime == 0
            || val < warpgateTime) {
            warpgateTime = val;
          }
        }
        if(unit->buildQueue[unit->buildQueueSlot] != UnitId::None) {
          u16 thisUnitId = unit->buildQueue[unit->buildQueueSlot];
          warpgateCall[unit->playerId] = thisUnitId;
          u32 yPos = unit->orderTarget.pt.y;
          u32 xPos = unit->orderTarget.pt.x;
          CUnit *warpUnit = scbw::createUnitAtPos(thisUnitId, unit->playerId, xPos, yPos);
          if(warpUnit) {
            //Clean building
            unit->buildQueue[unit->buildQueueSlot] = UnitId::None;
            unit->mainOrderId = OrderId::Nothing2;
            unit->secondaryOrderId = OrderId::Nothing2;


            u16 warpSeconds = 5;
            u16 warpTime = warpSeconds * 15;

            s32 maxShield = (s32)(units_dat::MaxShieldPoints[thisUnitId]) << 8;
            s32 maxHp = units_dat::MaxHitPoints[thisUnitId];

            s32 hpRegen = maxHp / warpTime;
            s32 shieldRegen = maxShield / warpTime;

            //Set unit
            warpUnit->buildRepairHpGain = hpRegen;
            warpUnit->shieldGain = shieldRegen;
            warpUnit->remainingBuildTime = warpTime;

            warpUnit->hitPoints = 1;
            warpUnit->shields = 1;

            replaceSpriteImages(warpUnit->sprite,
              ImageId::WarpAnchor, warpUnit->currentDirection1);
            warpUnit->status &= ~UnitStatus::Completed;
            warpUnit->orderTo(OrderId::BuildSelf2);
            warpUnit->currentButtonSet = UnitId::Buildings;
          }
          if(!isOperationCwalEnabled
            && unit->playerId == *LOCAL_NATION_ID) {
            --warpgateAvailable;
          }
          warpgateUpdate[unit->playerId] = true;
        }
      } //KYSXD Warpgate end

  //KYSXD - Terran plugins
      //KYSXD - Reactor add-on plugin start

      //Check unit state
      if((unit->id == UnitId::TerranFactory
        || unit->id == UnitId::TerranStarport)
        && unit->status & UnitStatus::Completed
        && unit->mainOrderId != OrderId::Die) {
        //Check add-on
        if(unit->building.addon != NULL) {
          CUnit *reactor = unit->building.addon;
          reactor->rally = unit->rally; //Update rally

          //If the unit is trainning, do reactor behavior
          if(unit->secondaryOrderId == OrderId::Train) {
            u16 changeQueue = (unit->buildQueueSlot + 1)%5;
            u16 tempId = unit->buildQueue[changeQueue];
            //If the addon isn't trainning
            if(tempId != UnitId::None
              && reactor->buildQueue[reactor->buildQueueSlot] == UnitId::None) {
              reactor->buildQueue[reactor->buildQueueSlot] = tempId;
              reactor->setSecondaryOrder(OrderId::Train);

              //Update remaining queue
              u16 queueId[5];
              for (int i = 0; i < 5; i++) {
                if(i != 4) {
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
          else if(reactor->secondaryOrderId == OrderId::Train) {
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
              if(i != 4) {
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

  //KYSXD - Zerg plugins
      //KYSXD - Burrow movement start
      if(unit->id == UnitId::ZergZergling
        && unit->playerId == *LOCAL_HUMAN_ID) {
//          KYSXD_ReplaceUnitWithType(unit, UnitId::TerranGoliath);
      } //KYSXD - Burrow movement end
    } //Loop through all visible units in the game - end

    //KYSXD update last warpgate
    for (int i = 0; i < 8; i++) {
      if(!isOperationCwalEnabled
        && warpgateUpdate[i] == true
        && lastWG[i] != NULL) {
        lastWG[i]->previousUnitType = warpgateCall[i];
      }
    }

  //KYSXD - Selection plugins

    for (int i = 0; i < *clientSelectionCount; ++i) {
      CUnit *selUnit = clientSelectionGroup->unit[i];

      //KYSXD update ButtonSet for WG
      if(selUnit->id == UnitId::Special_WarpGate
        && selUnit->status & UnitStatus::Completed) {
        selUnit->currentButtonSet =
          (warpgateAvailable != 0 || isOperationCwalEnabled ? UnitId::Special_WarpGate : UnitId::None);
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
          if(thisIsMineral(curMin)) {
            containerTargetFinder.setMineral(curMin);
            nearestContainer = scbw::UnitFinder::getNearestTarget(
            curMin->getX() - 300, curMin->getY() - 300,
            curMin->getX() + 300, curMin->getY() + 300,
            curMin,
            containerTargetFinder);
            if(nearestContainer != NULL
              && nearestContainer == selUnit) {
              nearmineral++;
            }
          }
          for(CUnit *thisUnit = *firstVisibleUnit; thisUnit; thisUnit = thisUnit->link.next) {
            if(thisUnit->playerId == *LOCAL_NATION_ID
              && (units_dat::BaseProperty[thisUnit->id] & UnitProperty::Worker)
              && isInHarvestState(thisUnit)
              && thisUnit->worker.targetResource.unit != NULL
              && thisUnit->worker.targetResource.unit == curMin) {
                nearworkers++;
            }
          }
        }
        //KYSXD count the nearest Workers
        if(nearmineral > 0) {
          static char unitcount[64];
          sprintf_s(unitcount, "Workers: %d / %d", nearworkers, nearmineral);
          graphics::drawText(selUnit->getX() - 32, selUnit->getY() - 60, unitcount, graphics::FONT_MEDIUM, graphics::ON_MAP);
        }
      }*/
      //KYSXD unit worker count end

      //KYSXD Twilight Archon
      if(selUnit->id == UnitId::ProtossDarkTemplar || selUnit->id == UnitId::ProtossHighTemplar) {
        if(selUnit->mainOrderId == OrderId::ReaverStop) {
          CUnit *templarPartner = nearestTemplarMergePartner(selUnit);
          if(templarPartner != NULL) {
              selUnit->orderTo(OrderId::WarpingDarkArchon, templarPartner);
              templarPartner->orderTo(OrderId::WarpingDarkArchon, selUnit);
          }
          else selUnit->mainOrderId = OrderId::Stop;
        }
      }

    //KYSXD - For selected units - From SC Transition - start
      //Draw attack radius circles for Siege Mode Tanks in current selection
      if(selUnit->id == UnitId::TerranSiegeTankSiegeMode) {
        graphics::drawCircle(selUnit->getX(), selUnit->getY(),
        selUnit->getMaxWeaponRange(units_dat::GroundWeapon[selUnit->subunit->id]) + 30,
        graphics::TEAL, graphics::ON_MAP);
      }

      //Display rally points for factories selected
      if(selUnit->status & UnitStatus::GroundedBuilding
        && units_dat::GroupFlags[selUnit->id].isFactory
        && (selUnit->playerId == *LOCAL_NATION_ID || scbw::isInReplay())) //Show only if unit is your own or the game is in replay mode
      {
        const CUnit *rallyUnit = selUnit->rally.unit;
        //Rallied to self; disable rally altogether
        if(rallyUnit != selUnit) {
          //Is usable rally unit
          if(rallyUnit && rallyUnit->playerId == selUnit->playerId) {
            graphics::drawLine(selUnit->getX(), selUnit->getY(),
              rallyUnit->getX(), rallyUnit->getY(),
              graphics::GREEN, graphics::ON_MAP);
            graphics::drawCircle(rallyUnit->getX(), rallyUnit->getY(), 10,
              graphics::GREEN, graphics::ON_MAP);
          }
          //Use map position instead
          else if(selUnit->rally.pt.x != 0) {
            graphics::drawLine(selUnit->getX(), selUnit->getY(),
              selUnit->rally.pt.x, selUnit->rally.pt.y,
              graphics::YELLOW, graphics::ON_MAP);
            graphics::drawCircle(selUnit->rally.pt.x, selUnit->rally.pt.y, 10,
              graphics::YELLOW, graphics::ON_MAP);
          }
        }

        //Show worker rally point
        const CUnit *workerRallyUnit = selUnit->moveTarget.unit;
        if(workerRallyUnit) {
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
    if(viewingCheck[*LOCAL_HUMAN_ID] == 0) {
      if(idleWorkerCount != 0) {
        static char idleworkers[64];
        sprintf_s(idleworkers, "Idle Workers: %d", idleWorkerCount);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, idleworkers, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }
      }

      if(warpgateAmount != 0) {
        static char WGates[64];
        if(isOperationCwalEnabled) {
          warpgateAvailable = warpgateAmount;
        }
        sprintf_s(WGates, "Online Warpgates: %d", warpgateAvailable);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, WGates, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }
        if(warpgateAvailable < warpgateAmount) {
          static char nextWGTime[64];
          sprintf_s(nextWGTime, "(Next in: %d sec)", warpgateTime >> 8);
          graphics::drawText(20, 10*titleRow, nextWGTime, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
          ++titleRow;
          if(10*titleRow > 250) {
            titleRow = 1;
            titleCol++;
          }
        }
      }
    }

  //For research:
    if(viewingCheck[*LOCAL_HUMAN_ID] == 1) {
      if(*clientSelectionCount == 1) {
        CUnit *thisUnit = clientSelectionGroup->unit[0];

        static char hitPoints[64];
        sprintf_s(hitPoints, "hitPoints: %d", thisUnit->hitPoints);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, hitPoints, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char getCurrentHpInGame[64];
        sprintf_s(getCurrentHpInGame, "getCurrentHpInGame(): %d", thisUnit->getCurrentHpInGame());
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, getCurrentHpInGame, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char shields[64];
        sprintf_s(shields, "shields: %d", thisUnit->shields);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, shields, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char getCurrentShieldsInGame[64];
        sprintf_s(getCurrentShieldsInGame, "getCurrentShieldsInGame(): %d", thisUnit->getCurrentShieldsInGame());
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, getCurrentShieldsInGame, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char getCurrentLifeInGame[64];
        sprintf_s(getCurrentLifeInGame, "getCurrentLifeInGame(): %d", thisUnit->getCurrentLifeInGame());
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, getCurrentLifeInGame, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char energy[64];
        sprintf_s(energy, "energy: %d", thisUnit->energy);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, energy, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char getMaxEnergy[64];
        sprintf_s(getMaxEnergy, "getMaxEnergy: %d", thisUnit->getMaxEnergy());
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, getMaxEnergy, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char getArmor[64];
        sprintf_s(getArmor, "getArmor: %d", thisUnit->getArmor());
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, getArmor, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char getArmorBonus[64];
        sprintf_s(getArmorBonus, "getArmorBonus: %d", thisUnit->getArmorBonus());
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, getArmorBonus, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char MineralCost[64];
        sprintf_s(MineralCost, "MineralCost: %d", units_dat::MineralCost[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, MineralCost, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char GasCost[64];
        sprintf_s(GasCost, "GasCost: %d", units_dat::GasCost[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, GasCost, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char TimeCost[64];
        sprintf_s(TimeCost, "TimeCost: %d", units_dat::TimeCost[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, TimeCost, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char DisplayedTimeCost[64];
        sprintf_s(DisplayedTimeCost, "DisplayedTimeCost: %d", units_dat::TimeCost[thisUnit->id]/15);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, DisplayedTimeCost, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char SupplyRequired[64];
        sprintf_s(SupplyRequired, "SupplyRequired: %d", units_dat::SupplyRequired[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, SupplyRequired, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char SupplyProvided[64];
        sprintf_s(SupplyProvided, "SupplyProvided: %d", units_dat::SupplyProvided[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, SupplyProvided, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char SpaceRequired[64];
        sprintf_s(SpaceRequired, "SpaceRequired: %d", units_dat::SpaceRequired[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, SpaceRequired, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char SpaceProvided[64];
        sprintf_s(SpaceProvided, "SpaceProvided: %d", units_dat::SpaceProvided[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, SpaceProvided, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char SizeType[64];
        sprintf_s(SizeType, "SizeType: %d", units_dat::SizeType[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, SizeType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char SeekRange[64];
        sprintf_s(SeekRange, "SeekRange: %d", units_dat::SeekRange[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, SeekRange, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char SightRange[64];
        sprintf_s(SightRange, "SightRange: %d", units_dat::SightRange[thisUnit->id]);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, SightRange, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

      }
    }

  //For research:
    if(viewingCheck[*LOCAL_HUMAN_ID] == 2) {
      if(*clientSelectionCount == 1) {
        CUnit *thisUnit = clientSelectionGroup->unit[0];

        static char id[64];
        sprintf_s(id, "id: %i", thisUnit->id);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, id, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char currentButtonSet[64];
        sprintf_s(currentButtonSet, "currentButtonSet: %i", thisUnit->currentButtonSet);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, currentButtonSet, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char index[64];
        sprintf_s(index, "index: %i", thisUnit->getIndex());
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, index, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char currentBuildUnit_Index[64];
        if(thisUnit->currentBuildUnit != NULL)
          sprintf_s(currentBuildUnit_Index, "currentBuildUnit_Index: %i", thisUnit->currentBuildUnit->getIndex());          
        else
          sprintf_s(currentBuildUnit_Index, "currentBuildUnit_Index: NULL");
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, currentBuildUnit_Index, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char currentBuildUnit_Hp[64];
        if(thisUnit->currentBuildUnit != NULL)
          sprintf_s(currentBuildUnit_Hp, "currentBuildUnit_Hp: %d", thisUnit->currentBuildUnit->hitPoints);
        else
          sprintf_s(currentBuildUnit_Hp, "currentBuildUnit_Hp: NULL");
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, currentBuildUnit_Hp, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char currentBuildUnit_getHp[64];
        if(thisUnit->currentBuildUnit != NULL)
          sprintf_s(currentBuildUnit_getHp, "currentBuildUnit_getHp: %d", thisUnit->currentBuildUnit->getCurrentHpInGame());
        else
          sprintf_s(currentBuildUnit_getHp, "currentBuildUnit_getHp: NULL");
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, currentBuildUnit_getHp, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char currentBuildUnit_hpGain[64];
        if(thisUnit->currentBuildUnit != NULL)
          sprintf_s(currentBuildUnit_hpGain, "currentBuildUnit_hpGain: %d", thisUnit->currentBuildUnit->buildRepairHpGain);
        else
          sprintf_s(currentBuildUnit_hpGain, "currentBuildUnit_hpGain: NULL");
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, currentBuildUnit_hpGain, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char animation[64];
        sprintf_s(animation, "animation: 0x%02x", thisUnit->sprite->mainGraphic->animation);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, animation, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char _unknown_0x066[64];
        sprintf_s(_unknown_0x066, "_unknown_0x066: %i", thisUnit->_unknown_0x066);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, _unknown_0x066, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char previousUnitType[64];
        sprintf_s(previousUnitType, "previousUnitType: %i", thisUnit->previousUnitType);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, previousUnitType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char orderSignal[64];
        sprintf_s(orderSignal, "orderSignal: %i", thisUnit->orderSignal);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, orderSignal, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char orderUnitType[64];
        sprintf_s(orderUnitType, "orderUnitType: %i", thisUnit->orderUnitType);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, orderUnitType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char wireframeRandomizer[64];
        sprintf_s(wireframeRandomizer, "wireframeRandomizer: %i", thisUnit->wireframeRandomizer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, wireframeRandomizer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char mainOrderId[64];
        sprintf_s(mainOrderId, "mainOrderId: 0x%02x", thisUnit->mainOrderId);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, mainOrderId, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char mainOrderTimer[64];
        sprintf_s(mainOrderTimer, "mainOrderTimer: %d", thisUnit->mainOrderTimer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, mainOrderTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char mainOrderState[64];
        sprintf_s(mainOrderState, "mainOrderState: %d", thisUnit->mainOrderState);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, mainOrderState, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char secondaryOrderId[64];
        sprintf_s(secondaryOrderId, "secondaryOrderId: 0x%02x", thisUnit->secondaryOrderId);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, secondaryOrderId, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char secondaryOrderTimer[64];
        sprintf_s(secondaryOrderTimer, "secondaryOrderTimer: %d", thisUnit->secondaryOrderTimer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, secondaryOrderTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char secondaryOrderState[64];
        sprintf_s(secondaryOrderState, "secondaryOrderState: %d", thisUnit->secondaryOrderState);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, secondaryOrderState, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char orderQueueTimer[64];
        sprintf_s(orderQueueTimer, "orderQueueTimer: %d", thisUnit->orderQueueTimer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, orderQueueTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char remainingBuildTime[64];
        sprintf_s(remainingBuildTime, "remainingBuildTime: %d", thisUnit->remainingBuildTime);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, remainingBuildTime, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char lastEventTimer[64];
        sprintf_s(lastEventTimer, "lastEventTimer: %d", thisUnit->lastEventTimer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, lastEventTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char recentOrderTimer[64];
        sprintf_s(recentOrderTimer, "recentOrderTimer: %d", thisUnit->recentOrderTimer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, recentOrderTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char cycleCounter[64];
        sprintf_s(cycleCounter, "cycleCounter: %d", thisUnit->cycleCounter);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, cycleCounter, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char airStrength[64];
        sprintf_s(airStrength, "airStrength: %d", thisUnit->airStrength);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, airStrength, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char groundStrength[64];
        sprintf_s(groundStrength, "groundStrength: %d", thisUnit->groundStrength);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, groundStrength, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char buildQueueSlot[64];
        sprintf_s(buildQueueSlot, "buildQueueSlot: %d", thisUnit->buildQueueSlot);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, buildQueueSlot, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        for (int i = 0; i < 5; i++) {
          static char buildQueue[64];
          sprintf_s(buildQueue, "buildQueue[%d]: %d",
            i,
            thisUnit->buildQueue[i]);
          graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, buildQueue, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
          ++titleRow;
          if(10*titleRow > 250) {
            titleRow = 1;
            titleCol++;
          }          
        }

      }
    }

    if(viewingCheck[*LOCAL_HUMAN_ID] == 3) {
      if(*clientSelectionCount == 1) {
        CUnit *thisUnit = clientSelectionGroup->unit[0];

        u8 hasAddon = (thisUnit->building.addon != NULL ? 1 : 0);

        static char AddOn[64];
        sprintf_s(AddOn, "AddOn: %d", hasAddon);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, AddOn, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char addonBuildType[64];
        sprintf_s(addonBuildType, "addonBuildType: %d", thisUnit->building.addonBuildType);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, addonBuildType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char upgradeResearchTime[64];
        sprintf_s(upgradeResearchTime, "upgradeResearchTime: %d", thisUnit->building.upgradeResearchTime);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, upgradeResearchTime, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char techType[64];
        sprintf_s(techType, "techType: 0x%x", thisUnit->building.techType);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, techType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char upgradeType[64];
        sprintf_s(upgradeType, "upgradeType: 0x%x", thisUnit->building.upgradeType);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, upgradeType, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char larvaTimer[64];
        sprintf_s(larvaTimer, "larvaTimer: %d", thisUnit->building.larvaTimer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, larvaTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char landingTimer[64];
        sprintf_s(landingTimer, "landingTimer: %d", thisUnit->building.landingTimer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, landingTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char creepTimer[64];
        sprintf_s(creepTimer, "creepTimer: %d", thisUnit->building.creepTimer);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, creepTimer, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char upgradeLevel[64];
        sprintf_s(upgradeLevel, "upgradeLevel: %d", thisUnit->building.upgradeLevel);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, upgradeLevel, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        static char _padding_0E[64];
        sprintf_s(_padding_0E, "_padding_0E: %d", thisUnit->building._padding_0E);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, _padding_0E, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

      }
    }

    if(viewingCheck[*LOCAL_HUMAN_ID] == 4) {
      if(*clientSelectionCount == 1) {
        CUnit *thisUnit = clientSelectionGroup->unit[0];

        static char status[64];
        sprintf_s(status, "status: %08x", thisUnit->status);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, status, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_Completed = (thisUnit->status & UnitStatus::Completed ? 1: 0);
        static char Completed[64];
        sprintf_s(Completed, "Completed: %d", status_Completed);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, Completed, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_GroundedBuilding = (thisUnit->status & UnitStatus::GroundedBuilding ? 1: 0);
        static char GroundedBuilding[64];
        sprintf_s(GroundedBuilding, "GroundedBuilding: %d", status_GroundedBuilding);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, GroundedBuilding, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_InAir = (thisUnit->status & UnitStatus::InAir ? 1: 0);
        static char InAir[64];
        sprintf_s(InAir, "InAir: %d", status_InAir);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, InAir, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_Disabled = (thisUnit->status & UnitStatus::Disabled ? 1: 0);
        static char Disabled[64];
        sprintf_s(Disabled, "Disabled: %d", status_Disabled);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, Disabled, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_Burrowed = (thisUnit->status & UnitStatus::Burrowed ? 1: 0);
        static char Burrowed[64];
        sprintf_s(Burrowed, "Burrowed: %d", status_Burrowed);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, Burrowed, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_InBuilding = (thisUnit->status & UnitStatus::InBuilding ? 1: 0);
        static char InBuilding[64];
        sprintf_s(InBuilding, "InBuilding: %d", status_InBuilding);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, InBuilding, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_InTransport = (thisUnit->status & UnitStatus::InTransport ? 1: 0);
        static char InTransport[64];
        sprintf_s(InTransport, "InTransport: %d", status_InTransport);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, InTransport, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_CanBeChased = (thisUnit->status & UnitStatus::CanBeChased ? 1: 0);
        static char CanBeChased[64];
        sprintf_s(CanBeChased, "CanBeChased: %d", status_CanBeChased);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, CanBeChased, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_Cloaked = (thisUnit->status & UnitStatus::Cloaked ? 1: 0);
        static char Cloaked[64];
        sprintf_s(Cloaked, "Cloaked: %d", status_Cloaked);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, Cloaked, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_DoodadStatesThing = (thisUnit->status & UnitStatus::DoodadStatesThing ? 1: 0);
        static char DoodadStatesThing[64];
        sprintf_s(DoodadStatesThing, "DoodadStatesThing: %d", status_DoodadStatesThing);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, DoodadStatesThing, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_CloakingForFree = (thisUnit->status & UnitStatus::CloakingForFree ? 1: 0);
        static char CloakingForFree[64];
        sprintf_s(CloakingForFree, "CloakingForFree: %d", status_CloakingForFree);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, CloakingForFree, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_CanNotReceiveOrders = (thisUnit->status & UnitStatus::CanNotReceiveOrders ? 1: 0);
        static char CanNotReceiveOrders[64];
        sprintf_s(CanNotReceiveOrders, "CanNotReceiveOrders: %d", status_CanNotReceiveOrders);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, CanNotReceiveOrders, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_NoBrkCodeStart = (thisUnit->status & UnitStatus::NoBrkCodeStart ? 1: 0);
        static char NoBrkCodeStart[64];
        sprintf_s(NoBrkCodeStart, "NoBrkCodeStart: %d", status_NoBrkCodeStart);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, NoBrkCodeStart, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_UNKNOWN2 = (thisUnit->status & UnitStatus::UNKNOWN2 ? 1: 0);
        static char UNKNOWN2[64];
        sprintf_s(UNKNOWN2, "UNKNOWN2: %d", status_UNKNOWN2);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, UNKNOWN2, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_CanNotAttack = (thisUnit->status & UnitStatus::CanNotAttack ? 1: 0);
        static char CanNotAttack[64];
        sprintf_s(CanNotAttack, "CanNotAttack: %d", status_CanNotAttack);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, CanNotAttack, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_CanTurnAroundToAttack = (thisUnit->status & UnitStatus::CanTurnAroundToAttack ? 1: 0);
        static char CanTurnAroundToAttack[64];
        sprintf_s(CanTurnAroundToAttack, "CanTurnAroundToAttack: %d", status_CanTurnAroundToAttack);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, CanTurnAroundToAttack, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_IsBuilding = (thisUnit->status & UnitStatus::IsBuilding ? 1: 0);
        static char IsBuilding[64];
        sprintf_s(IsBuilding, "IsBuilding: %d", status_IsBuilding);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, IsBuilding, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_IgnoreTileCollision = (thisUnit->status & UnitStatus::IgnoreTileCollision ? 1: 0);
        static char IgnoreTileCollision[64];
        sprintf_s(IgnoreTileCollision, "IgnoreTileCollision: %d", status_IgnoreTileCollision);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, IgnoreTileCollision, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_Unmovable = (thisUnit->status & UnitStatus::Unmovable ? 1: 0);
        static char Unmovable[64];
        sprintf_s(Unmovable, "Unmovable: %d", status_Unmovable);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, Unmovable, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_IsNormal = (thisUnit->status & UnitStatus::IsNormal ? 1: 0);
        static char IsNormal[64];
        sprintf_s(IsNormal, "IsNormal: %d", status_IsNormal);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, IsNormal, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_NoCollide = (thisUnit->status & UnitStatus::NoCollide ? 1: 0);
        static char NoCollide[64];
        sprintf_s(NoCollide, "NoCollide: %d", status_NoCollide);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, NoCollide, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_UNKNOWN5 = (thisUnit->status & UnitStatus::UNKNOWN5 ? 1: 0);
        static char UNKNOWN5[64];
        sprintf_s(UNKNOWN5, "UNKNOWN5: %d", status_UNKNOWN5);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, UNKNOWN5, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_IsGathering = (thisUnit->status & UnitStatus::IsGathering ? 1: 0);
        static char IsGathering[64];
        sprintf_s(IsGathering, "IsGathering: %d", status_IsGathering);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, IsGathering, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_UNKNOWN6 = (thisUnit->status & UnitStatus::UNKNOWN6 ? 1: 0);
        static char UNKNOWN6[64];
        sprintf_s(UNKNOWN6, "UNKNOWN6: %d", status_UNKNOWN6);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, UNKNOWN6, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_UNKNOWN7 = (thisUnit->status & UnitStatus::UNKNOWN7 ? 1: 0);
        static char UNKNOWN7[64];
        sprintf_s(UNKNOWN7, "UNKNOWN7: %d", status_UNKNOWN7);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, UNKNOWN7, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_Invincible = (thisUnit->status & UnitStatus::Invincible ? 1: 0);
        static char Invincible[64];
        sprintf_s(Invincible, "Invincible: %d", status_Invincible);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, Invincible, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_HoldingPosition = (thisUnit->status & UnitStatus::HoldingPosition ? 1: 0);
        static char HoldingPosition[64];
        sprintf_s(HoldingPosition, "HoldingPosition: %d", status_HoldingPosition);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, HoldingPosition, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_SpeedUpgrade = (thisUnit->status & UnitStatus::SpeedUpgrade ? 1: 0);
        static char SpeedUpgrade[64];
        sprintf_s(SpeedUpgrade, "SpeedUpgrade: %d", status_SpeedUpgrade);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, SpeedUpgrade, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_CooldownUpgrade = (thisUnit->status & UnitStatus::CooldownUpgrade ? 1: 0);
        static char CooldownUpgrade[64];
        sprintf_s(CooldownUpgrade, "CooldownUpgrade: %d", status_CooldownUpgrade);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, CooldownUpgrade, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_IsHallucination = (thisUnit->status & UnitStatus::IsHallucination ? 1: 0);
        static char IsHallucination[64];
        sprintf_s(IsHallucination, "IsHallucination: %d", status_IsHallucination);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, IsHallucination, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

        u8 status_IsSelfDestructing = (thisUnit->status & UnitStatus::IsSelfDestructing ? 1: 0);
        static char IsSelfDestructing[64];
        sprintf_s(IsSelfDestructing, "IsSelfDestructing: %d", status_IsSelfDestructing);
        graphics::drawText(10 + (titleCol - 1)*150, 10*titleRow, IsSelfDestructing, graphics::FONT_MEDIUM, graphics::ON_SCREEN);
        ++titleRow;
        if(10*titleRow > 250) {
          titleRow = 1;
          titleCol++;
        }

      }
    }

    int localPlayer = *LOCAL_HUMAN_ID;
    if(GetAsyncKeyState(VK_F5) & 0x0001) {
      viewingCheck[localPlayer] = (viewingCheck[localPlayer]+1)%viewingStatus;
    }

    scbw::setInGameLoopState(false);
  }
  return true;
}

bool gameOn() {
  if(firstBoot == false) {
    setMaxSightRange<20>();
    firstBoot = true;
  }
  return true;
}

bool gameEnd() {
  return true;
}

} //hooks