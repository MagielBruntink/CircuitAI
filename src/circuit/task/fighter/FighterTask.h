/*
 * FighterTask.h
 *
 *  Created on: Aug 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_FIGHTERTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_FIGHTERTASK_H_

#include "task/UnitTask.h"
#include "util/Defines.h"

namespace circuit {

class CEnemyUnit;

class IFighterTask: public IUnitTask {
public:
	enum class FightType: char {RALLY = 0, GUARD, DEFEND, SCOUT, RAID, ATTACK, BOMB, MELEE, ARTY, AA, AH, SUPPORT, SUPER, _SIZE_};
	using FT = std::underlying_type<FightType>::type;

protected:
	IFighterTask(ITaskManager* mgr, FightType type, float powerMod, int timeout = ASSIGN_TIMEOUT);
public:
	virtual ~IFighterTask();

	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);

	FightType GetFightType() const { return fightType; }
	const springai::AIFloat3& GetPosition() const { return position; }

	float GetAttackPower() const { return attackPower; }
	CEnemyUnit* GetTarget() const { return target; }
	void ClearTarget() { target = nullptr; }  // Only for ~CEnemyUnit

	const std::set<CCircuitUnit*>& GetShields() const { return shields; }

protected:
	void SetTarget(CEnemyUnit* enemy);

	FightType fightType;
	springai::AIFloat3 position;  // attack/scout position

	float attackPower;
	float powerMod;
	CEnemyUnit* target;

	std::set<CCircuitUnit*> cowards;
	std::set<CCircuitUnit*> shields;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_FIGHTERTASK_H_
