#include "Common.h"
#include "StrategyPlanner.h"
#include <boost/lexical_cast.hpp>
#include <base/StarcraftBuildOrderSearchManager.h>
#include <algorithm>
#include <base/ProductionManager.h>

const std::string LOG_FILE = "bwapi-data/BOB/data/planner.txt";
const std::string OPENINGS_FOLDER = BOB_DATA_FILEPATH + "openings/";
const std::string OPENINGS_SUFFIX = "_strats.txt";

const std::string ATTACK_TIMINGS_FOLDER = BOB_DATA_FILEPATH + "attack/";
const std::string ATTACK_TIMINGS_SUFFIX = "_timings.txt";

StrategyPlanner::StrategyPlanner(void)
	: currentStrategyIndex(0)
	, newAttackGoal(true)
	, selfRace(BWAPI::Broodwar->self()->getRace())
	, enemyRace(BWAPI::Broodwar->enemy()->getRace())
{
	loadStrategiesFromFile(selfRace);
	loadPlannedAttacksFromFile();

	setStrategy();
}

// get an instance of this
StrategyPlanner & StrategyPlanner::Instance() 
{
	static StrategyPlanner instance;
	return instance;
}

void StrategyPlanner::setStrategy()
{
	// return a random strategy
	currentStrategyIndex = rand() % usableStrategies.size();
	StrategyPlanner::Instance().log(boost::lexical_cast<std::string>(currentStrategyIndex));
	StrategyPlanner::Instance().log(openingBook[currentStrategyIndex]);
	//currentStrategy = usableStrategies[rand() % usableStrategies.size()];
}


const void StrategyPlanner::moveToNextAttackGoal()
{
	StrategyPlanner::Instance().log("moveToNextAttackGoal(): ");
	StrategyPlanner::Instance().log(attackTimings.size());
	if (attackTimings.size() > 1)
	{
		attackTimings.pop();
		armyCompositions.pop();
		StrategyPlanner::Instance().log(attackTimings.size());
	}
	newAttackGoal = true;
}

const UnitSet StrategyPlanner::getAttackSquad(const UnitSet freeUnits)
{
	return getAttackSquad(getArmyComposition(), freeUnits);
}

const UnitSet StrategyPlanner::getAttackSquad(const MetaMap wantedSquad, const UnitSet freeUnits)
{
	// if the time is not right, then don't add more attacking units
	if (BWAPI::Broodwar->getFrameCount() < attackTimings.front())
	{
		return unitsAllowedToAttack;
	}

	UnitSet attackSquad;

	MetaMap::const_iterator typeIt;
	UnitSet::const_iterator unitIt;

	//to improve efficiency: added soldiers should be removed from the set/iterator set
	for (typeIt = wantedSquad.begin(); typeIt != wantedSquad.end(); ++typeIt)
	{
		if (!isAttackUnit(typeIt->first)) { continue; }
		int soldiers = 0;
		for (unitIt = freeUnits.begin(); unitIt != freeUnits.end() && soldiers < typeIt->second; ++unitIt)
		{
			if (typeIt->first == (*unitIt)->getType()) 
			{
				attackSquad.insert((*unitIt));
				soldiers++;
			}
		}

		// if not enough troops, only return currently attacking squad
		if (soldiers < typeIt->second) { return unitsAllowedToAttack; }
	}

	UnitSet::iterator setIt;
	for (setIt = unitsAllowedToAttack.begin(); setIt != unitsAllowedToAttack.end(); )
	{
		if ((*setIt)->getHitPoints() < 1)
		{
			unitsAllowedToAttack.erase(setIt);
		} 
		else
		{
			setIt++;
		}
	}
	

	unitsAllowedToAttack.insert(attackSquad.begin(), attackSquad.end());
	StrategyPlanner::moveToNextAttackGoal();
	return unitsAllowedToAttack;
}

const MetaMap StrategyPlanner::getArmyComposition()
{
	return currentWantedArmyComposition = getArmyComposition(armyCompositions.front());
}

/* Extracts information from two strings. 
 * StringPair.first - the unit codes
 * StringPair.second - the amount of troops of each unit type
 * Eg) "0 24 17", "10 5 1"   means   10 units of unit type 0, 5 of type 24 and 1 of type 17
 */
const MetaMap StrategyPlanner::getArmyComposition(StringPair armyComposition)
{
	if (!newAttackGoal) { return currentWantedArmyComposition; }

	MetaMap unitMap;
	std::vector<MetaType> units = StarcraftBuildOrderSearchManager::Instance().getMetaVector(armyComposition.first);

	std::vector<MetaType>::iterator it = units.begin();
	std::stringstream ss;
	ss << armyComposition.second;

	int num(0);
	while (ss >> num && it != units.end())
	{
		BWAPI::UnitType unit = it->unitType;
		unitMap.insert(std::make_pair(unit, num));
		it++;
	}

	return unitMap;
}

const bool StrategyPlanner::isAttackUnit(BWAPI::UnitType type)
{
	return !(type.isBuilding() || type.isAddon() || type.isSpell() || type.isWorker());
}


/***************** SIMPLE GETTERS ********************/

const int StrategyPlanner::getCurrentStrategyIndex()
{
	return currentStrategyIndex;
}

const int StrategyPlanner::getDesiredAttackTiming()
{
	return attackTimings.front();
}

const std::vector<int> StrategyPlanner::getUsableStrategies()
{
	return usableStrategies;
}

const std::string StrategyPlanner::getOpening() const
{
	return openingBook[currentStrategyIndex];
}

const MetaPairVector StrategyPlanner::getBuildOrderGoal()
{
	MetaPairVector goal;

	MetaMap desiredArmy = StrategyPlanner::getArmyComposition();
	MetaMap::iterator typeIt;
	UnitSet::const_iterator unitIt;

	int supplyNeeded = 0;
	int supplySpace = BWAPI::Broodwar->self()->supplyTotal() - BWAPI::Broodwar->self()->supplyUsed();
	int supplyBuildingsNeeded = 0;
	BWAPI::UnitType supplyBuilding = BWAPI::Broodwar->self()->getRace().getSupplyProvider();
	for (typeIt = desiredArmy.begin(); typeIt != desiredArmy.end(); ++typeIt)
	{
		goal = addRequiredUnits(goal, (*typeIt));
		int neededUnits = typeIt->second;

		//StrategyPlanner::Instance().log(typeIt->first.getName());
		//StrategyPlanner::Instance().log(neededUnits);
					
		supplyNeeded += typeIt->first.supplyRequired() * (neededUnits - BWAPI::Broodwar->self()->allUnitCount(typeIt->first));
		supplySpace -= supplyNeeded;
		
		while (supplySpace < 0)
		{
			supplyBuildingsNeeded++;
			supplySpace += supplyBuilding.supplyProvided();
		}
	}

	if (supplyBuildingsNeeded > 0) { goal.push_back(MetaPair(supplyBuilding, supplyBuildingsNeeded)); }

	//std::ofstream file;
	//std::string filename = "bwapi-data/BOB/data/planner.txt";
	//file.open(filename.c_str(), std::ios::app);
	//file << "\nsetBuildOrder: \n";
	//for (int i = 0; i < goal.size(); i++)
	//{
	//	
	//	file << "planner";
	//	file << goal[i].second << "\n";	
	//}
	//file.close();

	if (goal.empty()) 
	{ 
		moveToNextAttackGoal();
		return getBuildOrderGoal();
	}

	return goal;
}

//const get

const MetaPairVector StrategyPlanner::addRequiredUnits(MetaPairVector goal, MetaPair pair)
{
	if (BWAPI::Broodwar->self()->allUnitCount(pair.first.unitType) >= pair.second) { return goal; }
	
	//don't add the same unit/building twice
	MetaPairVector::iterator iterator;
	for (iterator = goal.begin(); iterator != goal.end(); ++iterator) { if (iterator->first.getName() == pair.first.getName()) { return goal; } }
	
	BWAPI::UnitType type = pair.first.unitType;
	MetaMap requirements = type.requiredUnits();

	// add any required units first
	if (!requirements.empty())
	{
		MetaMap::iterator it;
		for (it = requirements.begin(); it != requirements.end(); ++it)
		{
			goal = addRequiredUnits(goal, (*it));
		}
	}

	// and lastly add the unit itself
	int availableUnits = BWAPI::Broodwar->self()->allUnitCount(pair.first.unitType);
	UnitSet::iterator it;
	for (it = unitsAllowedToAttack.begin(); it != unitsAllowedToAttack.end(); ++it)
	{
		if ((*it)->getType() == pair.first.unitType)
		{
			availableUnits--;
		}
	}
	std::vector<MetaType> buildQueue = ProductionManager::Instance().getQueueAsVector();
	std::vector<MetaType>::const_iterator queueIt;
	for (queueIt = buildQueue.begin(); queueIt != buildQueue.end(); ++queueIt)
	{
		if (queueIt->unitType == pair.first.unitType)
		{
			availableUnits++;
		}
	}

	if (pair.second > availableUnits) { goal.push_back(pair); }
	return goal;
}


/***************** LOG STUFF ********************/

void StrategyPlanner::log(std::string filename, std::string output)
{
	std::ofstream file;
	file.open(filename.c_str(), std::ios::app);
	file << output << "\n";
	file.close();
}

void StrategyPlanner::log(std::string output)
{
	log(LOG_FILE, output);
}

void StrategyPlanner::log(std::string filename, int output)
{
	std::ofstream file;
	file.open(filename.c_str(), std::ios::app);
	file << output << "\n";
	file.close();
}

void StrategyPlanner::log(int output)
{
	log(LOG_FILE, output);
}


/***************** LOAD STUFF FROM FILES ********************/

void StrategyPlanner::loadStrategiesFromFile(BWAPI::Race race)
{
	std::string filename = OPENINGS_FOLDER + race.getName().c_str() + OPENINGS_SUFFIX;
	std::ifstream myfile (filename.c_str());
	std::string line;
	int i = 0;
	if (myfile.is_open())
	{
		while (getline(myfile,line)) 
		{	
			openingBook.push_back(line);
			usableStrategies.push_back(i);
			i++;
		}
		myfile.close();
	}
	else
	{
		BWAPI::Broodwar->printf(
			"Unable to open file, some things may not be working or the entire program may crash glhf :).");
	}
}

void StrategyPlanner::loadPlannedAttacksFromFile()
{
	std::string filename = ATTACK_TIMINGS_FOLDER + selfRace.getName().c_str() + ATTACK_TIMINGS_SUFFIX;

	std::ifstream myfile (filename.c_str());
	std::string timing_line, unit_type_line, num_unit_line;
	int i = 0;
	if (myfile.is_open())
	{
		while (getline(myfile,timing_line) && getline(myfile,unit_type_line) && getline(myfile,num_unit_line)) 
		{	
			attackTimings.push(atoi(timing_line.c_str()));
			armyCompositions.push(StringPair(unit_type_line, num_unit_line));
			i++;
		}
		myfile.close();
	}
	else
	{
		BWAPI::Broodwar->printf(
			"Unable to open file, some things may not be working or the entire program may crash glhf :).");
	}
}