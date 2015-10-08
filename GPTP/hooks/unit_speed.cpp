//Contains hooks that control unit movement speed, acceleration, and turn speed.

#include "unit_speed.h"
#include "../SCBW/enumerations.h"
#include "../SCBW/scbwdata.h"
#include <SCBW/api.h>

//V241 for VS2008

namespace hooks {

/// Calculates the unit's modified movement speed, factoring in upgrades and status effects.
///
/// @return		The modified speed value.
u32 getModifiedUnitSpeedHook(const CUnit* unit, u32 baseSpeed) {
	//KYSXD new behavior
	u32 speed = baseSpeed;
	u32 upgrademodifier = 10;
	u32 creepmodifier = 10;
	ActiveTile actTile = scbw::getActiveTileAt(unit->getX(), unit->getY());
	if (unit->status & UnitStatus::SpeedUpgrade) {
		switch(unit->id) {
			case UnitId::zergling:
				upgrademodifier = 25; //Was upgrademodifier = 16
			case UnitId::overlord:
				if (speed < 853) {
					speed = 853;
				}
				upgrademodifier = 10; //Was upgrademodifier = 32
			case UnitId::zealot:
				upgrademodifier = 13;
			case UnitId::hydralisk:
				upgrademodifier = (actTile.hasCreep ? 10 : 16);
			case UnitId::vulture:
				upgrademodifier = 15;
			case UnitId::hunter_killer:
				upgrademodifier = 15;
            default: 13;
        }
	}
	//KYSXD zerg speed start
	if (unit->getRace() == RaceId::Zerg
		&& !(unit->status & UnitStatus::InAir)
		&& actTile.hasCreep) {
		if (unit->id != UnitId::drone) {
			if (unit->id == UnitId::hydralisk) {
				creepmodifier = 16;
			}
			else  {
				creepmodifier = 13;
			}
		}
	} //KYSXD zerg speed end
	speed = (speed * (unit->stimTimer ? (15) : 10) *
					(unit->ensnareTimer ? (5) : 10) *
					upgrademodifier *
					creepmodifier)/10000;
	return speed;
}

/// Calculates the unit's acceleration, factoring in upgrades and status effects.
///
/// @return		The modified acceleration value.
u32 getModifiedUnitAccelerationHook(const CUnit* unit) {
	//Default StarCraft behavior
	u32 acceleration = flingy_dat::Acceleration[units_dat::Graphic[unit->id]];
	int modifier = (unit->stimTimer ? 1 : 0) - (unit->ensnareTimer ? 1 : 0)
                 + (unit->status & UnitStatus::SpeedUpgrade ? 1 : 0);
	if (modifier > 0)
		acceleration <<= 1;
	else if (modifier < 0)
		acceleration -= acceleration >> 2;
	return acceleration;
}

/// Calculates the unit's turn speed, factoring in upgrades and status effects.
///
/// @return		The modified turning speed value.
u32 getModifiedUnitTurnSpeedHook(const CUnit* unit) {
	//Default StarCraft behavior
	u32 turnSpeed = flingy_dat::TurnSpeed[units_dat::Graphic[unit->id]];
	int modifier = (unit->stimTimer ? 1 : 0) - (unit->ensnareTimer ? 1 : 0)
                 + (unit->status & UnitStatus::SpeedUpgrade ? 1 : 0);
	if (modifier > 0)
		turnSpeed <<= 1;
	else if (modifier < 0)
		turnSpeed -= turnSpeed >> 2;
	return turnSpeed;
}

} //hooks