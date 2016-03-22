#include "stopholdpos_orders.h"
#include <SCBW/api.h>

//helper functions def

namespace {

CUnit* MedicHeal_TargetAcquire(CUnit* medic);															//0x004422A0
CUnit* findBestAttackTarget(CUnit* unit);																//0x00443080
u32 doMedicHeal(CUnit* unit, CUnit* target);															//0x00463C40
void removeOrderFromUnitQueue(CUnit* unit);																//0x004742D0
void performAnotherOrder(CUnit* unit, u8 orderId, Point16* pos, CUnit* target, u16 targetUnitId);		//0x004745F0
void orderImmediate(CUnit* unit, u8 order);																//0x00474B40
void function_00476FC0(CUnit* unit, CUnit* target, u32 unk1, u32 unk2);									//0x00476FC0
Bool32 function_004770E0(CUnit* unit);																	//0x004770E0
u32 RandBetween(u32 min, u32 max, u32 someIndex);														//0x004DC550
void setNextWaypoint_Sub4EB290(CUnit* unit);															//0x004EB290

} //unnamed namespace

namespace hooks {

void orders_MedicHoldPosition(CUnit* unit) {

	u32 switchValue;

	if(unit->mainOrderState == 0) {

		setNextWaypoint_Sub4EB290(unit);

		if(
			unit->moveTarget.pt.x != unit->nextTargetWaypoint.x ||
			unit->moveTarget.pt.y != unit->nextTargetWaypoint.y
		)
		{
			unit->nextTargetWaypoint.x = unit->moveTarget.pt.x;
			unit->nextTargetWaypoint.y = unit->moveTarget.pt.y;
		}

		//6407F:
		unit->status |= UnitStatus::HoldingPosition;

		if(unit->subunit != NULL)
			unit->subunit->status |= UnitStatus::HoldingPosition;

		unit->mainOrderState = 1;
		unit->sprite->playIscriptAnim(IscriptAnimation::WalkingToIdle,true);

	}

	//640AF

	//try healing current target, return a value depending
	//on what happened
	switchValue = doMedicHeal(unit,unit->orderTarget.unit);

	if(switchValue == 3) //640C6
		unit->orderTarget.unit = NULL;

	if(switchValue == 0 || switchValue == 2 || switchValue == 3) //640CD
		unit->sprite->playIscriptAnim(IscriptAnimation::WalkingToIdle,true);

	if(switchValue != 1) { //640D9

		//if timer is 0, seek a target in range
		if(unit->mainOrderTimer == 0) {

			unit->mainOrderTimer = 15;
			unit->orderTarget.unit = MedicHeal_TargetAcquire(unit);

			if(unit->orderTarget.unit != NULL) {
				unit->orderTarget.pt.x = (unit->orderTarget.unit)->getX();
				unit->orderTarget.pt.y = (unit->orderTarget.unit)->getY();
			}
				
		}

	}
	else { //64109 (switchValue == 1)

		if(!(unit->orderSignal & 1)) {

			CImage* current_image = unit->sprite->images.head;

			while(current_image != NULL) {
				current_image->playIscriptAnim(IscriptAnimation::SpecialState1);
				current_image = current_image->link.next;
			}

			if(
				unit->orderQueueHead == NULL ||
				unit->orderQueueHead->orderId != OrderId::MedicHeal2
			) 	//6413D
				orderImmediate(unit,OrderId::MedicHeal2); //return to idle after heal

		}

		//64146
		if(
			(unit->orderTarget.unit)->getX() != unit->nextTargetWaypoint.x ||
			(unit->orderTarget.unit)->getY() != unit->nextTargetWaypoint.y
		)
		{
			unit->nextTargetWaypoint.x = (unit->orderTarget.unit)->getX();
			unit->nextTargetWaypoint.y = (unit->orderTarget.unit)->getY();
		}

	}



} //void orders_MedicHoldPosition(CUnit* unit)

;

void orders_ReaverStop(CUnit* unit) {

	CUnit* outHangarChild = unit->carrier.outHangarChild;
	Point16 pos = {0,0}; //[EBP-0x08]

	setNextWaypoint_Sub4EB290(unit);

	if(
		unit->moveTarget.pt.x != unit->nextTargetWaypoint.x ||
		unit->moveTarget.pt.y != unit->nextTargetWaypoint.y
	) 
	{
		unit->nextTargetWaypoint.x = unit->moveTarget.pt.x;
		unit->nextTargetWaypoint.y = unit->moveTarget.pt.y;
	}

	//65500
	while(outHangarChild != NULL) {

		outHangarChild->userActionFlags |= 1;

		if(outHangarChild->mainOrderId != OrderId::Die) {

			bool bNotRemovableOrder = false;

			while(outHangarChild->orderQueueTail != NULL && !bNotRemovableOrder) {

				if(!orders_dat::CanBeInterrupted[outHangarChild->orderQueueTail->orderId]) {
					if(outHangarChild->orderQueueTail->orderId != OrderId::SelfDestrucing)
						bNotRemovableOrder = true;
					else
						removeOrderFromUnitQueue(outHangarChild);
				}
				else
					removeOrderFromUnitQueue(outHangarChild);

			}

			//65539
			performAnotherOrder(outHangarChild,OrderId::SelfDestrucing,&pos,NULL,UnitId::None);

		}

		//65554
		prepareForNextOrder(outHangarChild);
		outHangarChild = outHangarChild->interceptor.hangar_link.prev;

	} //while(outHangarChild != NULL)

	//65565

	unit->userActionFlags |= 1;

	if(unit->orderTarget.unit != NULL) {
		pos.x = (unit->orderTarget.unit)->getX();
		pos.y = (unit->orderTarget.unit)->getY();
	}
	else {
		pos.x = 0;
		pos.y = 0;
	}

	//65598

	if(unit->mainOrderId != OrderId::Die) {

		bool bNotRemovableOrder = false;

		while(unit->orderQueueTail != NULL && !bNotRemovableOrder) {

			if(!orders_dat::CanBeInterrupted[unit->orderQueueTail->orderId]) {
				if(unit->orderQueueTail->orderId != OrderId::Reaver)
					bNotRemovableOrder = true;
				else
					removeOrderFromUnitQueue(unit);
			}
			else
				removeOrderFromUnitQueue(unit);

		}

		//655C3
		performAnotherOrder(unit,OrderId::Reaver,&pos,unit->orderTarget.unit,UnitId::None);

	}

	//655DB
	prepareForNextOrder(unit);

} //void orders_ReaverStop(CUnit* unit)

;

void orders_Guard(CUnit* unit) {

	unit->mainOrderTimer = RandBetween(0,15,29);

	if(playerTable[unit->playerId].type == PlayerType::Computer) {

		unit->mainOrderId = OrderId::ComputerAI;

		if(unit->pAI == NULL) {

			if(units_dat::BaseProperty[unit->id] & UnitProperty::Hero)
				unit->mainOrderId = OrderId::PlayerGuard;

		}
		else
		if(((u8*)unit->pAI)[8] == 1)
			unit->mainOrderId = OrderId::GuardPost;

	}
	else
		unit->mainOrderId = OrderId::PlayerGuard;

} //void orders_Guard(CUnit* unit)

;

;

//NOT HOOKED (since most of the action happen in another function)
void orders_TurretGuard(CUnit* unit) {

	if(
		unit->subunit->nextTargetWaypoint.x != unit->nextTargetWaypoint.x ||
		unit->subunit->nextTargetWaypoint.y != unit->nextTargetWaypoint.y
	) 
	{
		unit->nextTargetWaypoint.x = unit->subunit->nextTargetWaypoint.x;
		unit->nextTargetWaypoint.y = unit->subunit->nextTargetWaypoint.y;
	}

	function_004774A0(unit);

}

;

//Guard related function, may be a poorly identified official guard order
void function_004774A0(CUnit* unit) {

	//if unit->autoTargetUnit != NULL, attack this target
	//then disable the unit...I think
	//That's why if it return true, nothing else is done
	//since has already attacked
	if(!function_004770E0(unit)) {

		if(unit->mainOrderTimer == 0) {

			unit->mainOrderTimer = 15;

			if(units_dat::BaseProperty[unit->id] & UnitProperty::Subunit) {
				if(
					unit->subunit->nextTargetWaypoint.x != unit->nextTargetWaypoint.x ||
					unit->subunit->nextTargetWaypoint.y != unit->nextTargetWaypoint.y
				) 
				{
					unit->nextTargetWaypoint.x = unit->subunit->nextTargetWaypoint.x;
					unit->nextTargetWaypoint.y = unit->subunit->nextTargetWaypoint.y;
				}
			}

			if(unit->getSeekRange() != 0) {

				CUnit* target = findBestAttackTarget(unit);

				if(target != NULL)
					//attack target I think, also used by function_004770E0
					function_00476FC0(unit, target, 0,1); 

			}


		}

	}

}

;

} //namespace hooks

;

//-------- Helper function definitions. Do NOT modify! --------//

namespace {

const u32 Func_MedicHeal_TargetAcquire = 0x004422A0;
CUnit* MedicHeal_TargetAcquire(CUnit* medic) {

	static CUnit* target;

	__asm {
		PUSHAD
		PUSH medic
		CALL Func_MedicHeal_TargetAcquire
		MOV target, EAX
		POPAD
	}

	return target;
	
}

;

const u32 Func_Sub443080 = 0x00443080;
//named according to attack_priority_inject.cpp
CUnit* findBestAttackTarget(CUnit* unit) {

	static CUnit* result;

	__asm {
		PUSHAD
		MOV EAX, unit
		CALL Func_Sub443080
		MOV EAX, result
		POPAD
	}

	return result;

}

;

const u32 Func_doMedicHeal = 0x00463C40;
u32 doMedicHeal(CUnit* unit, CUnit* target) {

	static u32 return_value;

	__asm {
		PUSHAD
		MOV EAX, unit
		PUSH target
		CALL Func_doMedicHeal
		MOV return_value, EAX
		POPAD
	}

	return return_value;

}

;

const u32 Func_removeOrderFromUnitQueue = 0x004742D0;
void removeOrderFromUnitQueue(CUnit* unit) {

	static COrder* orderQueueHead = unit->orderQueueHead;

  __asm {
    PUSHAD
	MOV ECX, unit
	MOV EAX, orderQueueHead
	CALL Func_removeOrderFromUnitQueue
    POPAD
  }

}

;

const u32 Func_PerformAnotherOrder = 0x004745F0;
void performAnotherOrder(CUnit* unit, u8 orderId, Point16* pos, CUnit* target, u16 targetUnitId) {

	__asm {
		PUSHAD
		PUSH target
		PUSH [pos]
		MOV BL, orderId
		MOVZX EDX, targetUnitId
		MOV ESI, unit
		XOR EDI, EDI
		CALL Func_PerformAnotherOrder
		POPAD
	}

}

;

const u32 Func_OrderImmediate = 0x00474B40;
void orderImmediate(CUnit* unit, u8 order) {

	__asm {
		PUSHAD
		MOV ECX, unit
		MOV AL, order
		CALL Func_OrderImmediate
		POPAD
	}

}

;

const u32 Func_Sub476FC0 = 0x00476FC0;
void function_00476FC0(CUnit* unit, CUnit* target, u32 unk1, u32 unk2) {

	__asm {
		PUSHAD
		PUSH unk1
		PUSH unk2
		MOV EAX, target
		MOV EDI, unit
		CALL Func_Sub476FC0
		POPAD
	}

}

;

const u32 Func_Sub4770E0 = 0x004770E0;
Bool32 function_004770E0(CUnit* unit) {

	static Bool32 return_value;

	__asm {
		PUSHAD
		MOV EAX, unit
		CALL Func_Sub4770E0
		MOV return_value, EAX
		POPAD
	}

	return return_value;

}

;

const u32 Func_RandBetween = 0x004DC550;
u32 RandBetween(u32 min, u32 max, u32 someIndex) {

	static u32 return_value;

	__asm {
		PUSHAD
		PUSH max
		MOV EAX, someIndex
		MOV EDX, min
		CALL Func_RandBetween
		MOV return_value, EAX
		POPAD
	}

	return return_value;

}

;

//Related to path/movement decision
const u32 Func_sub_4EB290 = 0x004EB290;
void setNextWaypoint_Sub4EB290(CUnit* unit) {

	__asm {
		PUSHAD
		MOV EAX, unit
		CALL Func_sub_4EB290
		POPAD
	}
}



;

} //Unnamed namespace

//End of helper functions
