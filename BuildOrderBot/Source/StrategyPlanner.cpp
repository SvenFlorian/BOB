#include "Common.h"
#include "StrategyPlanner.h"
#include <boost/lexical_cast.hpp>
#include <base/StarcraftBuildOrderSearchManager.h>

const std::string LOG_FILE = "log.txt";
const std::string OPENINGS_FOLDER = BOB_DATA_FILEPATH + "openings/";
const std::string OPENINGS_SUFFIX = "_strats.txt";

const std::string ATTACK_TIMINGS_FOLDER = BOB_DATA_FILEPATH + "attack/";
const std::string ATTACK_TIMINGS_SUFFIX = "_timings.txt";

StrategyPlanner::StrategyPlanner(void)
	: currentStrategy(0)
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
	currentStrategy = rand() % usableStrategies.size();
	StrategyPlanner::Instance().log(boost::lexical_cast<std::string>(currentStrategy));
	StrategyPlanner::Instance().log(openingBook[currentStrategy]);
	//currentStrategy = usableStrategies[rand() % usableStrategies.size()];
}


const void StrategyPlanner::moveToNextAttackGoal()
{
	if (attackTimings.size() > 1)
	{
		attackTimings.pop();
		armyCompositions.pop();
		newAttackGoal = true;
	}
}

const UnitSet StrategyPlanner::getAttackSquad(const UnitSet freeUnits)
{
	return getAttackSquad(getArmyComposition(), freeUnits);
}

const UnitSet StrategyPlanner::getAttackSquad(const MetaMap wantedSquad, const UnitSet freeUnits)
{
	UnitSet attackSquad;

	MetaMap::const_iterator typeIt;
	UnitSet::const_iterator unitIt;

	//to improve efficiency: added soldiers should be removed from the set/iterator set
	for (typeIt = wantedSquad.begin(); typeIt != wantedSquad.end(); ++typeIt)
	{
		int soldiers = 0;
		for (unitIt = freeUnits.begin(); unitIt != freeUnits.end() && soldiers < typeIt->second; ++unitIt)
		{
			if (typeIt->first == (*unitIt)->getType()) 
			{
				attackSquad.insert((*unitIt));
				soldiers++;
			}
		}
	}

	return unitsAllowedToAttack = attackSquad;
}

const MetaMap StrategyPlanner::getArmyComposition()
{
	return getArmyComposition(armyCompositions.front());
}

/* Extracts information from two strings. 
 * StringPair.first - the unit codes
 * StringPair.second - the amount of troops of each unit type
 * Eg) "0 24 17", "10 5 1"   means   10 units of unit type 0, 5 of type 24 and 1 of type 17
 */
const MetaMap StrategyPlanner::getArmyComposition(StringPair armyComposition)
{
	StrategyPlanner::Instance().log("getArmyComposition(StringPair) started");
	if (!newAttackGoal) { return currentWantedArmyComposition; }
	StrategyPlanner::Instance().log("No new attack goal.");

	MetaMap unitMap;
	std::vector<MetaType> units = StarcraftBuildOrderSearchManager::Instance().getMetaVector(armyComposition.first);
	StrategyPlanner::Instance().log("We got the MetaVector.");

	std::vector<MetaType>::iterator it = units.begin();
	std::stringstream ss;
	ss << armyComposition.second;
	StrategyPlanner::Instance().log("We did some stuff, now let's start with looping.");

	int num(0);
	while (ss >> num && it != units.end())
	{
		BWAPI::UnitType unit = it->unitType;
		unitMap.insert(std::make_pair(unit, num));
		StrategyPlanner::Instance().log("We just inserted something into a map.");
	}

	StrategyPlanner::Instance().log("getArmyComposition(StringPair) ended");
	return currentWantedArmyComposition = unitMap;
}


/***************** SIMPLE GETTERS ********************/

const int StrategyPlanner::getCurrentStrategy()
{
	return currentStrategy;
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
	return openingBook[currentStrategy];
}

const MetaPairVector StrategyPlanner::getBuildOrderGoal()
{
	MetaPairVector goal;

	MetaMap desiredArmy = StrategyPlanner::getArmyComposition();
	MetaMap::iterator typeIt;
	std::set<BWAPI::Unit *>::const_iterator unitIt;

	for (typeIt = desiredArmy.begin(); typeIt != desiredArmy.end(); ++typeIt)
	{
		int neededUnits = typeIt->second - BWAPI::Broodwar->self()->allUnitCount(typeIt->first);
		goal.push_back(MetaPair(typeIt->first, neededUnits));
	}

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