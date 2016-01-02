/*
 * CircuitDef.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitDef.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/utils.h"

#include "WeaponMount.h"
#include "WeaponDef.h"
#include "Damage.h"
#include "Shield.h"
#include "MoveData.h"

#include <regex>

namespace circuit {

using namespace springai;

CCircuitDef::CCircuitDef(CCircuitAI* circuit, UnitDef* def, std::unordered_set<Id>& buildOpts, Resource* res)
		: def(def)
		, role(RoleType::NONE)
		, buildOptions(buildOpts)
		, count(0)
		, buildCounts(0)
		, hasDGunAA(false)
//		, dgunReload(-1)
		, dgunRange(.0f)
		, dgunMount(nullptr)
		, shieldMount(nullptr)
		, dps(.0f)
		, power(.0f)
		, maxRange({.0f})
		, maxShield(.0f)
		, targetCategory(0)
		, immobileTypeId(-1)
		, mobileTypeId(-1)
		, hasAntiAir(false)
		, hasAntiLand(false)
		, hasAntiWater(false)
		, isAmphibious(false)
		, retreat(.0f)
{
	id = def->GetUnitDefId();

	buildDistance = def->GetBuildDistance();
	buildSpeed    = def->GetBuildSpeed();
	maxThisUnit   = def->GetMaxThisUnit();

	maxRange[static_cast<unsigned>(RangeType::MAX)] = def->GetMaxWeaponRange();
	isManualFire    = def->CanManualFire();
	noChaseCategory = def->GetNoChaseCategory();

	MoveData* md = def->GetMoveData();
	isSubmarine = (md == nullptr) ? false : md->IsSubMarine();
	delete md;
	isAbleToFly    = def->IsAbleToFly();
	isPlane        = !def->IsHoverAttack() && isAbleToFly;
	isFloater      = def->IsFloater() && !isSubmarine;
	isSonarStealth = def->IsSonarStealth();

	speed     = def->GetSpeed() / FRAMES_PER_SEC;  // NOTE: SetMaxWantedSpeed expects value/FRAMES_PER_SEC
	losRadius = def->GetLosRadius() * circuit->GetLosConv();
	cost      = def->GetCost(res);

	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto it = customParams.find("is_drone");
	if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
		category = ~circuit->GetGoodCategory();
	} else {
		category = def->GetCategory();
	}

	it = customParams.find("midposoffset");
	if (it != customParams.end()) {
		const std::string& str = it->second;
		std::string::const_iterator start = str.begin();
		std::string::const_iterator end = str.end();
		std::regex pattern("(-?\\d+)");
		std::smatch section;
		int index = 0;
		while (std::regex_search(start, end, section, pattern)) {
			midPosOffset[index++] = utils::string_to_float(section[1]);
			start = section[0].second;
		}
	} else {
		midPosOffset = ZeroVector;
	}

	WeaponDef* sd = def->GetShieldDef();
	bool isShield = (sd != nullptr);
	if (isShield) {
		Shield* shield = sd->GetShield();
		maxShield = shield->GetPower();
		delete shield;
	}
	delete sd;

	if (!def->IsAbleToAttack()) {
		// FIXME: Decouple ScoutTask into RaidTask and ScoutTask
		if (std::string("corawac") == def->GetName()) {
			dps = 10.0f;
		}

		if (isShield) {
			auto mounts = std::move(def->GetWeaponMounts());
			for (WeaponMount* mount : mounts) {
				WeaponDef* wd = mount->GetWeaponDef();
				if ((shieldMount == nullptr) && wd->IsShield()) {
					shieldMount = mount;  // NOTE: Unit may have more than 1 shield
				} else {
					delete mount;
				}
				delete wd;
			}
		}
		// NOTE: Aspis (mobile shield) has 10 damage for some reason, break
		return;
	}

	/*
	 * DPS and Weapon calculations
	 */
	float bestReload = std::numeric_limits<float>::max();
	float bestRange = .0f;
	WeaponMount* bestMount = nullptr;
	bool canTargetAir = false;
	bool canTargetLand = false;
	bool canTargetWater = false;
	auto mounts = std::move(def->GetWeaponMounts());
	for (WeaponMount* mount : mounts) {
		WeaponDef* wd = mount->GetWeaponDef();
		const std::map<std::string, std::string>& customParams = wd->GetCustomParams();

		float scale = wd->IsParalyzer() ? 0.5f : 1.0f;

		float extraDmg = .0f;
		auto it = customParams.find("extra_damage");
		if (it != customParams.end()) {
			extraDmg += utils::string_to_float(it->second);
		}

		it = customParams.find("disarmdamageonly");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 0.5f;
		}

		it = customParams.find("timeslow_onlyslow");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 0.5f;
		}

		it = customParams.find("is_capture");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 2.0f;
		}

		it = customParams.find("area_damage_dps");
		if (it != customParams.end()) {
			extraDmg += utils::string_to_float(it->second);
			it = customParams.find("area_damage_is_impulse");
			if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
				scale = 0.02f;
			}
		}

		float reloadTime = wd->GetReload();
		if (extraDmg > 0.1f) {
			dps += extraDmg * wd->GetSalvoSize() / reloadTime * scale;
		}
		Damage* damage = wd->GetDamage();
		const std::vector<float>& damages = damage->GetTypes();
		delete damage;
		float ldps = .0f;
		for (float dmg : damages) {
			ldps += dmg;
		}
		dps += ldps * wd->GetSalvoSize() / damages.size() / reloadTime * scale;
		int weaponCat = mount->GetOnlyTargetCategory();
		targetCategory |= weaponCat;

		std::string wt(wd->GetType());
		bool isAirWeapon = false;
		float range = wd->GetRange();
		if (range > 300.0f) {
			isAirWeapon = ((wt == "Cannon") || (wt == "DGun") || (wt == "EmgCannon") || (wt == "Flame") ||
					(wt == "LaserCannon") || (wt == "AircraftBomb")) && (wd->GetProjectileSpeed() * FRAMES_PER_SEC >= 400.0f);  // Cannons with fast projectiles
			isAirWeapon |= (wt == "BeamLaser") || (wt == "LightningCannon") || (wt == "Rifle") ||  // Instant-hit
					(((wt == "MissileLauncher") || (wt == "StarburstLauncher") || ((wt == "TorpedoLauncher") && wd->IsSubMissile())) && wd->IsTracks());  // Missiles
			canTargetAir |= isAirWeapon;
		}
		bool isLandWeapon = ((wt != "TorpedoLauncher") || wd->IsSubMissile());
		canTargetLand |= isLandWeapon;
		bool isWaterWeapon = wd->IsWaterWeapon();
		canTargetWater |= isWaterWeapon;

		if ((weaponCat & circuit->GetAirCategory()) && isAirWeapon) {
			float& mr = maxRange[static_cast<unsigned>(RangeType::AIR)];
			mr = std::max(mr, range);
		}
		if ((weaponCat & circuit->GetLandCategory()) && isLandWeapon) {
			float& mr = maxRange[static_cast<unsigned>(RangeType::LAND)];
			mr = std::max(mr, range);
		}
		if ((weaponCat & circuit->GetWaterCategory()) && isWaterWeapon) {
			float& mr = maxRange[static_cast<unsigned>(RangeType::WATER)];
			mr = std::max(mr, range);
		}

		if (wd->IsManualFire() && (reloadTime < bestReload)) {
			bestReload = reloadTime;
			bestRange = range;
			delete bestMount;
			bestMount = mount;
			hasDGunAA |= (weaponCat & circuit->GetAirCategory()) && isAirWeapon;
		} else if ((shieldMount == nullptr) && wd->IsShield()) {
			shieldMount = mount;  // NOTE: Unit may have more than 1 shield
		} else {
			delete mount;
		}
		delete wd;
	}

	if (bestReload < std::numeric_limits<float>::max()) {
//		dgunReload = math::ceil(bestReload * FRAMES_PER_SEC)/* + FRAMES_PER_SEC*/;
		dgunRange = bestRange;
		dgunMount = bestMount;
	}

	if (IsMobile() && !IsAttacker()) {  // mobile bomb?
		WeaponDef* wd = def->GetDeathExplosion();
		if (wd->GetAreaOfEffect() > 80.0f) {
			Damage* damage = wd->GetDamage();
			const std::vector<float>& damages = damage->GetTypes();
			delete damage;
			float ldps = .0f;
			for (float dmg : damages) {
				ldps += dmg;
			}
			dps = ldps * wd->GetSalvoSize() / damages.size();
			targetCategory = wd->GetOnlyTargetCategory();
			if (~targetCategory == 0) {
				targetCategory = circuit->GetGoodCategory();
			}
		}
		delete wd;
	}

	// NOTE: isTracks filters units with slow weapon (hermit, recluse, rocko)
	hasAntiAir   = (targetCategory & circuit->GetAirCategory()) && canTargetAir;
	hasAntiLand  = (targetCategory & circuit->GetLandCategory()) && canTargetLand;
	hasAntiWater = (targetCategory & circuit->GetWaterCategory()) && canTargetWater;

	power = dps * sqrtf(def->GetHealth() / 100.0f);
}

CCircuitDef::~CCircuitDef()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete def;
	delete dgunMount;
	delete shieldMount;
}

void CCircuitDef::Init(CCircuitAI* circuit)
{
	CTerrainData& terrainData = circuit->GetGameAttribute()->GetTerrainData();
	assert(terrainData.IsInitialized());

	if (IsAbleToFly()) {

	} else if (!IsMobile()) {  // for immobile units

		immobileTypeId = terrainData.udImmobileType[GetId()];
		// If a unit can build mobile units then it will inherit mobileType from it's options
		std::map<STerrainMapMobileType::Id, float> mtUsability;
		for (CCircuitDef::Id buildId : GetBuildOptions()) {
			CCircuitDef* bdef = circuit->GetCircuitDef(buildId);
			if ((bdef == nullptr) || !bdef->IsMobile() || !bdef->IsAttacker()) {
				continue;
			}
			STerrainMapMobileType::Id mtId = terrainData.udMobileType[bdef->GetId()];
			if ((mtId < 0) || (mtUsability.find(mtId) != mtUsability.end())) {
				continue;
			}
			STerrainMapMobileType& mt = terrainData.areaData0.mobileType[mtId];
			mtUsability[mtId] = mt.area.empty() ? 00.0 : mt.areaLargest->percentOfMap;
		}
		float useMost = .0f;
		STerrainMapMobileType::Id mtId = mobileTypeId;  // -1
		for (auto& mtkv : mtUsability) {
			if (mtkv.second > useMost) {
				mtId = mtkv.first;
				useMost = mtkv.second;
			}
		}
		mobileTypeId = mtId;

	} else {  // for mobile units

		mobileTypeId = terrainData.udMobileType[GetId()];
	}

	if (IsMobile()) {
		if (mobileTypeId >= 0) {
			STerrainMapMobileType& mt = terrainData.areaData0.mobileType[mobileTypeId];
			isAmphibious = ((mt.minElevation < -SQUARE_SIZE * 5) || (mt.maxElevation < SQUARE_SIZE * 5)) && !IsFloater();
		}
	} else {
		STerrainMapImmobileType& it = terrainData.areaData0.immobileType[immobileTypeId];
		isAmphibious = ((it.minElevation < -SQUARE_SIZE * 5) || (it.maxElevation < SQUARE_SIZE * 5)) && !IsFloater();
	}

}

CCircuitDef& CCircuitDef::operator++()
{
	++count;
	return *this;
}

// FIXME: ~CCircuitDef should fail with delete
//CCircuitDef CCircuitDef::operator++(int)
//{
//	CCircuitDef temp = *this;
//	count++;
//	return temp;
//}

CCircuitDef& CCircuitDef::operator--()
{
	--count;
	return *this;
}

//CCircuitDef CCircuitDef::operator--(int)
//{
//	CCircuitDef temp = *this;
//	count--;
//	return temp;
//}

} // namespace circuit
