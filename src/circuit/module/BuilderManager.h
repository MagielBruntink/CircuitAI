/*
 * BuilderManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_BUILDERMANAGER_H_
#define SRC_CIRCUIT_MODULE_BUILDERMANAGER_H_

#include "module/UnitModule.h"
#include "task/builder/BuilderTask.h"

#include "AIFloat3.h"

#include <map>
#include <set>
#include <vector>
#include <unordered_set>

namespace circuit {

class CCircuitDef;
struct STerrainMapMobileType;
struct STerrainMapArea;

class CBuilderManager: public IUnitModule {
public:
	CBuilderManager(CCircuitAI* circuit);
	virtual ~CBuilderManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	void AddBuildList(CCircuitUnit* unit);
	void RemoveBuildList(CCircuitUnit* unit);

	float GetBuilderPower();
	bool CanEnqueueTask();
	const std::set<IBuilderTask*>& GetTasks(IBuilderTask::BuildType type);
	IBuilderTask* EnqueueTask(IBuilderTask::Priority priority,
							  springai::UnitDef* buildDef,
							  const springai::AIFloat3& position,
							  IBuilderTask::BuildType type,
							  float cost,
							  int timeout = 0);
	IBuilderTask* EnqueueTask(IBuilderTask::Priority priority,
							  springai::UnitDef* buildDef,
							  const springai::AIFloat3& position,
							  IBuilderTask::BuildType type,
							  int timeout = 0);
	IBuilderTask* EnqueueTask(IBuilderTask::Priority priority,
							  const springai::AIFloat3& position,
							  IBuilderTask::BuildType type,
							  int timeout = 0);
private:
	IBuilderTask* AddTask(IBuilderTask::Priority priority,
						  springai::UnitDef* buildDef,
						  const springai::AIFloat3& position,
						  IBuilderTask::BuildType type,
						  float cost,
						  int timeout);
	void DequeueTask(IBuilderTask* task, bool done = false);

public:
	// TODO: Move into CTerrainManager??
	// position must be valid
	bool CanBeBuiltAt(CCircuitDef* cdef, const springai::AIFloat3& position, const float& range = .0);  // NOTE: returns false if the area was too small to be recorded
	bool CanBuildAt(CCircuitUnit* unit, const springai::AIFloat3& destination);
	bool IsBuilderInArea(springai::UnitDef* buildDef, const springai::AIFloat3& position);  // Check if build-area has proper builder

	virtual void AssignTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void SpecialCleanUp(CCircuitUnit* unit);
	virtual void SpecialProcess(CCircuitUnit* unit);
	virtual void FallbackTask(CCircuitUnit* unit);

private:
	void Init();
	void Watchdog();
	void UpdateIdle();
	void UpdateRetreat();
	void UpdateBuild();

	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 damagedHandler;
	Handlers2 destroyedHandler;

	std::map<CCircuitUnit*, IBuilderTask*> unfinishedUnits;
	std::vector<std::set<IBuilderTask*>> builderTasks;  // owner
	int builderTasksCount;
	float builderPower;
	std::set<IBuilderTask*> updateTasks;  // temporary tasks holder to keep updating every task
	std::set<IBuilderTask*> deleteTasks;

	std::set<CCircuitUnit*> workers;
	std::set<CCircuitUnit*> assistants;  // workers with temporary task

	std::unordered_set<const STerrainMapMobileType*> workerMobileTypes;
	std::map<const STerrainMapArea*, std::map<CCircuitDef*, int>> buildAreas;  // area <=> worker types
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_BUILDERMANAGER_H_
