#include <AI/ai_common.h>

namespace AI {

bool AI_spellcasterHook(CUnit* unit, bool isUnitBeingAttacked);

}  // namespace AI

namespace hooks {

void injectSpellcasterAI();

}  // namespace hooks