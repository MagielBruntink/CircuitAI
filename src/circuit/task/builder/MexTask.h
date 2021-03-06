/*
 * MexTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBMexTask: public IBuilderTask {
public:
	CBMexTask(ITaskManager* mgr, Priority priority,
			  CCircuitDef* buildDef, const springai::AIFloat3& position,
			  float cost, int timeout);
	virtual ~CBMexTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;

	virtual void Execute(CCircuitUnit* unit);
protected:
	virtual void Cancel();

public:
	virtual void OnUnitIdle(CCircuitUnit* unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_MEXTASK_H_
