#include "Common.h"
#include "StrategyPlanner.h"
#include <boost/lexical_cast.hpp>
#include <base/StarcraftBuildOrderSearchManager.h>

const std::string LOG_FILE = "bwapi-data/BOB/data/StrategyPlannerLog.txt";
const std::string OPENINGS_FOLDER = BOB_DATA_FILEPATH + "openings/";
const std::string OPENINGS_SUFFIX = "_strats.txt";

const std::string ATTACK_TIMINGS_FOLDER = BOB_DATA_FILEPATH + "attack/";
const std::string ATTACK_TIMINGS_SUFFIX = "_timings.txt";

StrategyPlanner::StrategyPlanner(void)
	: currentStrategyIndex(0)
	, attackOrderIndex(0)
	, newAttackOrder(true)
	, selfRace(BWAPI::Broodwar->self()->getRace())
	, enemyRace(BWAPI::Broodwar->enemy()->getRace())
{
	loadStrategiesFromFile(selfRace);
	loadPlannedAttacksFromFile();
	//TODO try with reverse order?

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
	if (attackOrderIndex < (attackTimings.size() - 1))
	{
		attackOrderIndex++;
		newAttackOrder = true;
	}
}

const UnitSet StrategyPlanner::getAttackSquad(const UnitSet freeUnits)
{
	return getAttackSquad(getArmyComposition(), freeUnits);
}

const UnitSet StrategyPlanner::getAttackSquad(const MetaMap wantedSquad, const UnitSet freeUnits)
{
	// if the time is not right, then don't add more attacking units
	if (BWAPI::Broodwar->getFrameCount() < attackTimings[attackOrderIndex])
	{
		return unitsAllowedToAttack; 
	}

	UnitSet attackSquad;

	MetaMap::const_iterator typeIt;
	UnitSet::const_iterator unitIt;

	//to improve efficiency: added soldiers should be removed from the set/iterator set
	for (typeIt = wantedSquad.begin(); typeIt != wantedSquad.end(); ++typeIt)
	{
		int soldiers = 0;
		for (unitIt = freeUnits.begin(); unitIt != freeUnits.end() && soldiers < typeIt->second; ++unitIt)
		{
			//TODO check for ID?
			if (typeIt->first == (*unitIt)->getType()) 
			{
				attackSquad.insert((*unitIt));
				soldiers++;
			}
		}

		// if not enough troops, only return currently attacking squad
		if (soldiers < typeIt->second) { return unitsAllowedToAttack; }
	}

	
	for (unitIt = unitsAllowedToAttack.begin(); unitIt != unitsAllowedToAttack.end(); ++unitIt)
	{
		if ( (*unitIt)->getHitPoints() < 1 )
		{
			unitsAllowedToAttack.erase((*unitIt));
		}
	}

	StrategyPlanner::Instance().log("UnitsAllowedToAttack: ");
	StrategyPlanner::Instance().log(unitsAllowedToAttack.size());
	unitsAllowedToAttack.insert(attackSquad.begin(), attackSquad.end());
	StrategyPlanner::Instance().log(unitsAllowedToAttack.size());
	StrategyPlanner::moveToNextAttackGoal();
	return unitsAllowedToAttack;
}

const MetaMap StrategyPlanner::getArmyComposition()
{
	if (!newAttackOrder) { return currentWantedArmyComposition; }
	newAttackOrder = false;
	return currentWantedArmyComposition = getArmyComposition(armyCompositions[attackOrderIndex]);
}

/* Extracts information from two strings. 
 * StringPair.first - the unit codes
 * StringPair.second - the amount of troops of each unit type
 * Eg) "0 24 17", "10 5 1"   means   10 units of unit type 0, 5 of type 24 and 1 of type 17
 */
const MetaMap StrategyPlanner::getArmyComposition(StringPair armyComposition)
{
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
	}

	return unitMap;
}


/***************** SIMPLE GETTERS ********************/

const int StrategyPlanner::getCurrentStrategyIndex()
{
	return currentStrategyIndex;
}

const int StrategyPlanner::getDesiredAttackTiming()
{
	return attackTimings[attackOrderIndex];
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
	StrategyPlanner::Instance().log("StrategyPlanner::getBuildOrderGoal()");

	MetaPairVector goal;
	MetaMap desiredArmy;
	int index = attackOrderIndex;

	//loop over attack orders until we get one where we need more troops to do it
	while (goal.empty() && (index < armyCompositions.size()))
	{
		StrategyPlanner::Instance().log("loop");
		MetaMap nextOrderArmyComposition = StrategyPlanner::getArmyComposition(armyCompositions[index]);
		desiredArmy = StrategyPlanner::mergeMetaMaps(desiredArmy, nextOrderArmyComposition);
		MetaMap::iterator typeIt;
		UnitSet::const_iterator unitIt;

		for (typeIt = desiredArmy.begin(); typeIt != desiredArmy.end(); ++typeIt)
		{
			StrategyPlanner::Instance().log("iterate"); //IT NEVER GOES HERE!!???? :(
			int neededUnits = typeIt->second - BWAPI::Broodwar->self()->allUnitCount(typeIt->first);
			if (neededUnits > 0)
			{
				goal.push_back(MetaPair(typeIt->first, neededUnits));
				StrategyPlanner::Instance().log(neededUnits);
			}
		}

		index++; 
	}
	StrategyPlanner::Instance().log("over");

	//if enough troops for all attack orders
	if (goal.empty())
	{
		//improvise?
	}

	return goal;
}

/* Merge two MetaMap's. */
const MetaMap StrategyPlanner::mergeMetaMaps(MetaMap map1, MetaMap map2)
{
	MetaMap map;
	MetaMap::iterator it1;
	MetaMap::iterator it2;
	for (it1 = map1.begin(); it1 != map1.end(); ++it1)
	{
		map[it1->first] = it1->second;
	}

	for (it2 = map2.begin(); it2 != map2.end(); ++it2)
	{
		map[it2->first] += it2->second;
	}
			
	//____________________

	/*for (it1 = map1.begin(); it1 != map1.end(); ++it1)
	{
		int troops = it1->second;
		map[it1->first] = it1->second;
		for (it2 = map2.begin(); it2 != map2.end(); ++it2)
		{
			if (it1->first == it2->first)
			{
				troops += it2->second;
			}
		}

		if (troops > 0)
		{
			map.insert(std::make_pair(it1->first, troops));
			map.
		}
	}

	for (*/

	return map;
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
			attackTimings.push_back(atoi(timing_line.c_str()));
			armyCompositions.push_back(StringPair(unit_type_line, num_unit_line));
			++i;
		}
		myfile.close();
	}
	else
	{
		BWAPI::Broodwar->printf(
			"Unable to open file, some things may not be working or the entire program may crash glhf :).");
	}
}