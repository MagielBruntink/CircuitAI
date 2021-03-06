/*
 * TravelAction.h
 *
 *  Created on: Feb 16, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_TRAVELACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_TRAVELACTION_H_

#include "unit/action/UnitAction.h"
#include "util/Defines.h"

#include <memory>

namespace circuit {

class ITravelAction: public IUnitAction {
public:
	ITravelAction(CCircuitUnit* owner, Type type, int squareSize, float speed = MAX_UNIT_SPEED);
	ITravelAction(CCircuitUnit* owner, Type type, const std::shared_ptr<F3Vec>& pPath, int squareSize, float speed = MAX_UNIT_SPEED);
	virtual ~ITravelAction();

	void SetPath(const std::shared_ptr<F3Vec>& pPath, float speed = MAX_UNIT_SPEED);

protected:
	int CalcSpeedStep(int frame, float& stepSpeed);

	std::shared_ptr<F3Vec> pPath;
	float speed;
	int pathIterator;
	int increment;
	int minSqDist;
	bool isForce;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_TRAVELACTION_H_
