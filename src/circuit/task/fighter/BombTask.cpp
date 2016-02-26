/*
 * BombTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/BombTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "unit/EnemyUnit.h"
#include "unit/action/MoveAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBombTask::CBombTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::BOMB)
{
}

CBombTask::~CBombTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBombTask::CanAssignTo(CCircuitUnit* unit) const
{
	return units.empty() && unit->GetCircuitDef()->IsRoleBomber();
}

void CBombTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CMoveAction* moveAction = new CMoveAction(unit, squareSize);
	unit->PushBack(moveAction);
	moveAction->SetActive(false);
}

void CBombTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CBombTask::Execute(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CBombTask::Update()
{
	bool isExecute = (++updCount % 4 == 0);
	for (CCircuitUnit* unit : units) {
		if (unit->IsForceExecute() || isExecute) {
			Execute(unit, true);
		} else {
			IFighterTask::Update();
		}
	}
}

void CBombTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	IUnitAction* act = static_cast<IUnitAction*>(unit->End());
	if (!act->IsEqual(IUnitAction::Mask::MOVE)) {
		return;
	}
	CMoveAction* moveAction = static_cast<CMoveAction*>(act);

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	if (!unit->IsWeaponReady(frame)) {  // is unit armed?
		// force rearm/repair | CMD_FIND_PAD
		unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		return;
	}

	const AIFloat3& pos = unit->GetPos(frame);
	std::shared_ptr<F3Vec> pPath = std::make_shared<F3Vec>();
	CEnemyUnit* bestTarget = FindTarget(unit, pos, *pPath);

	if (bestTarget != nullptr) {
		position = bestTarget->GetPos();
		unit->GetUnit()->Attack(bestTarget->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		moveAction->SetActive(false);
		return;
	} else if (!pPath->empty()) {
		position = pPath->back();
		moveAction->SetPath(pPath);
		moveAction->SetActive(true);
		unit->Update(circuit);
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& threatPos = moveAction->IsActive() ? position : pos;
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < threatMap->GetUnitThreat(unit));
	if (!proceed) {
		position = circuit->GetMilitaryManager()->GetScoutPosition(unit);
	}

	if ((position != -RgtVector) && terrainManager->CanMoveToPos(unit->GetArea(), position)) {
		AIFloat3 startPos = pos;
		AIFloat3 endPos = position;

		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(unit, threatMap, frame);
		pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());

		if (!pPath->empty()) {
//			position = path.back();
			moveAction->SetPath(pPath);
			moveAction->SetActive(true);
			unit->Update(circuit);
			return;
		}
	}

	if (proceed) {
		return;
	}
	float x = rand() % (terrainManager->GetTerrainWidth() + 1);
	float z = rand() % (terrainManager->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	moveAction->SetActive(false);
}

void CBombTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);

	if (units.find(unit) != units.end()) {
		RemoveAssignee(unit);
	}
}

CEnemyUnit* CBombTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos, F3Vec& path)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CThreatMap* threatMap = circuit->GetThreatMap();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float power = threatMap->GetUnitThreat(unit) * 8.0f;
	const float speed = cdef->GetSpeed();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(unit->GetUnit()->GetMaxRange() + threatMap->GetSquareSize() * 2,
						   cdef->GetLosRadius());
	const float sqRange = SQUARE(range);
	float maxThreat = .0f;

	CEnemyUnit* bestTarget = nullptr;
	CEnemyUnit* mediumTarget = nullptr;
	CEnemyUnit* worstTarget = nullptr;
	F3Vec enemyPositions;
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (threatMap->GetThreatAt(enemy->GetPos()) >= power)) {
			continue;
		}
		if (!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)) {
			continue;
		}
		int targetCat;
		if (enemy->GetCircuitDef() != nullptr) {
			if (enemy->GetCircuitDef()->GetSpeed() * 1.8f > speed) {
				continue;
			}
			targetCat = enemy->GetCircuitDef()->GetCategory();
			if ((targetCat & canTargetCat) == 0) {
				continue;
			}
		} else {
			targetCat = UNKNOWN_CATEGORY;
		}

		float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (enemy->IsInRadarOrLOS() && (sqDist < sqRange)) {
			if ((enemy->GetThreat() > maxThreat) && !enemy->GetUnit()->IsBeingBuilt()) {
				bestTarget = enemy;
				maxThreat = enemy->GetThreat();
			} else if (bestTarget == nullptr) {
				if ((targetCat & noChaseCat) == 0) {
					mediumTarget = enemy;
				} else if (mediumTarget == nullptr) {
					worstTarget = enemy;
				}
			}
			continue;
		}
		if (sqDist < SQUARE(2000.f)) {  // maxSqDist
			enemyPositions.push_back(enemy->GetPos());
		}
	}
	if (bestTarget == nullptr) {
		bestTarget = (mediumTarget != nullptr) ? mediumTarget : worstTarget;
	}

	path.clear();
	if ((bestTarget != nullptr) || enemyPositions.empty()) {
		return bestTarget;
	}

	AIFloat3 startPos = pos;
	circuit->GetPathfinder()->SetMapData(unit, threatMap, circuit->GetLastFrame());
	circuit->GetPathfinder()->FindBestPath(path, startPos, range * 0.5f, enemyPositions);

	return nullptr;
}

} // namespace circuit
