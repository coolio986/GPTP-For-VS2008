#include "train_cmd_receive.h"
#include <SCBW/api.h>

//Helper functions declaration

namespace {

bool HasMoneyCanMake(CUnit* builder, u32 unitToBuild);	//67250		//HasMoneyCanMake

u32 getFighterId(CUnit* trainer);
bool isFightersTrainer(CUnit *trainer);

CUnit *getBestTrainer();
CUnit *getBestBuilder(u16 wUnitType);
CUnit *getLowestQueueUnit(CUnit **units_array, int array_length);

} //unnamed namespace

namespace hooks {

void CMDRECV_TrainFighter()
{
	CUnit *trainer = getBestTrainer();
	if(trainer != NULL)
	{
		if(HasMoneyCanMake(trainer, getFighterId(trainer))
			&& trainer->secondaryOrderState != 2)
		{
			trainer->secondaryOrderId = OrderId::TrainFighter;
			trainer->secondaryOrderPos.y = 0;
			trainer->secondaryOrderPos.x = 0;
			trainer->currentBuildUnit = NULL;
			trainer->secondaryOrderState = 0;
		}
		scbw::refreshConsole();
	}
	return;
};

//Note: the default function isn't compatible with multiple selection
void CMDRECV_Train(u16 wUnitType) {

	CUnit* builder = getBestBuilder(wUnitType);

	if(builder != NULL)
	{
		/*
		if(builder->canMakeUnit(wUnitType,*ACTIVE_NATION_ID) &&
			wUnitType <= UnitId::Spell_DisruptionWeb //id before buildings id
		)
		*/
		if(HasMoneyCanMake(builder,wUnitType))
		{
			builder->setSecondaryOrder(OrderId::Train);
		}
		scbw::refreshConsole();
	}
	return;
};

} //namespace hooks

//-------- Helper function definitions. Do NOT modify! --------//

namespace {

const u32 Func_HasMoneyCanMake = 0x00467250;
bool HasMoneyCanMake(CUnit* builder, u32 unitToBuild) {

	static Bool32 bPreResult;

	__asm {
		PUSHAD
		PUSH unitToBuild
		MOV EDI, builder
		CALL Func_HasMoneyCanMake
		MOV bPreResult, EAX
		POPAD
	}

	return (bPreResult != 0);

}

u32 getFighterId(CUnit* trainer)
{
	u32 fighterId;
	switch(trainer->id)
	{
		case UnitId::ProtossCarrier:
		case UnitId::Hero_Gantrithor:
			fighterId = UnitId::ProtossInterceptor;
			break;
		case UnitId::ProtossReaver:
		case UnitId::Hero_Warbringer:
			fighterId = UnitId::ProtossScarab;
			break;
		default:
			fighterId = UnitId::None;
			break;
	}
	return fighterId;
}

bool isFightersTrainer(CUnit *trainer)
{
	switch(trainer->id)
	{
		case UnitId::ProtossCarrier:
		case UnitId::Hero_Gantrithor:
		case UnitId::ProtossReaver:
		case UnitId::Hero_Warbringer:
			return true;
			break;
		default:
			return false;
			break;
	}
}

CUnit *getBestTrainer()
{
	int i = 0;
	CUnit *trainers[SELECTION_ARRAY_LENGTH] = {NULL};

	*selectionIndexStart = 0;
	CUnit *current_unit = getActivePlayerNextSelection();
	while(current_unit != NULL)
	{
		if(isFightersTrainer(current_unit)
			&& current_unit->canMakeUnit(getFighterId(current_unit),
				*ACTIVE_NATION_ID))
		{
			trainers[i++] = current_unit;
		}
		current_unit = getActivePlayerNextSelection();
	}
	return getLowestQueueUnit(trainers, i);
}


CUnit *getBestBuilder(u16 wUnitType)
{
	int i = 0;
	CUnit *builder[SELECTION_ARRAY_LENGTH] = {NULL};

	*selectionIndexStart = 0;
	CUnit *current_unit = getActivePlayerNextSelection();
	while(current_unit != NULL)
	{
		if(current_unit->canMakeUnit(wUnitType, *ACTIVE_NATION_ID))
		{
			builder[i++] = current_unit;
		}
		current_unit = getActivePlayerNextSelection();
	}
	return getLowestQueueUnit(builder, i);
}

CUnit *getLowestQueueUnit(CUnit **units_array, int array_length)
{
	CUnit *bestUnit = units_array[0];
	int bestLength = 5;

	for(int i = 0; i < array_length; i++)
	{
		if(units_array[i] != NULL)
		{
			int currentLength = 0;
			while(units_array[i]->buildQueue[ (units_array[i]->buildQueueSlot + currentLength)%5 ] != UnitId::None)
			{
				currentLength++;
			}
			if(currentLength < bestLength)
			{
				bestLength = currentLength;
				bestUnit = units_array[i];
			}
		}
	}
	return bestUnit;
}

} //unnamed namespace

//End of helper functions