#include "Common.h"
#include "StrategyManager.h"
#include <boost/lexical_cast.hpp>
#include <base/StarcraftBuildOrderSearchManager.h>

const std::string LOG_FILE = "log.txt";
const std::string OPENINGS_FOLDER = BOB_DATA_FILEPATH + "openings/";
const std::string OPENINGS_SUFFIX = "_strats.txt";

const std::string ATTACK_TIMINGS_FOLDER = BOB_DATA_FILEPATH + "attack/";
const std::string ATTACK_TIMINGS_SUFFIX = "_timings.txt";

// constructor
StrategyManager::StrategyManager() 
	: firstAttackSent(false)
	, currentStrategy(0)
	, selfRace(BWAPI::Broodwar->self()->getRace())
	, enemyRace(BWAPI::Broodwar->enemy()->getRace())
{
	addStrategies();
	setStrategy();

	loadPlannedAttacksFromFile();
}

// get an instance of this
StrategyManager & StrategyManager::Instance() 
{
	static StrategyManager instance;
	return instance;
}

void StrategyManager::addStrategies() 
{
	readDir = OPENINGS_FOLDER;
	loadStrategiesFromFile(selfRace);

	if (Options::Modules::USING_STRATEGY_IO)
	{
		readResults();
	}
}

void StrategyManager::readResults()
{
	// read in the name of the read and write directories from settings file
	struct stat buf;

	// if the file doesn't exist something is wrong so just set them to default settings
	if (stat(Options::FileIO::FILE_SETTINGS, &buf) == -1)
	{
		readDir = "bwapi-data/testio/read/";
		writeDir = "bwapi-data/testio/write/";
	}
	else
	{
		std::ifstream f_in(Options::FileIO::FILE_SETTINGS);
		getline(f_in, readDir);
		getline(f_in, writeDir);
		f_in.close();
	}

	// the file corresponding to the enemy's previous results
	std::string readFile = readDir + BWAPI::Broodwar->enemy()->getName() + ".txt";

	// if the file doesn't exist, set the results to zeros
	if (stat(readFile.c_str(), &buf) == -1)
	{
		std::fill(results.begin(), results.end(), IntPair(0,0));
	}
	// otherwise read in the results
	else
	{
		std::ifstream f_in(readFile.c_str());
		std::string line;

		unsigned int strategy = 0;
		unsigned int numStrategies = usableStrategies.size();
		while (strategy < numStrategies) 
		{
			getline(f_in, line);
			results[strategy++].second = atoi(line.c_str());
		}

		f_in.close();
	}
	/*BWAPI::Broodwar->printf("Results (%s): (%d %d) (%d %d) (%d %d)", BWAPI::Broodwar->enemy()->getName().c_str(), 
		results[0].first, results[0].second, results[1].first, results[1].second, results[2].first, results[2].second);*/
}

void StrategyManager::writeResults()
{
	std::string writeFile = writeDir + BWAPI::Broodwar->enemy()->getName() + ".txt";
	std::ofstream f_out(writeFile.c_str());

	unsigned int strategy = 0;
	unsigned int numStrategies = usableStrategies.size();
	while (strategy < numStrategies) 
	{
		f_out << results[strategy].first   << "\n";
		f_out << results[strategy++].second  << "\n";
	}

	f_out.close();
}

void StrategyManager::setStrategy()
{
	// if we are using file io to determine strategy, do so
	if (Options::Modules::USING_STRATEGY_IO)
	{
		double bestUCB = -1;
		int bestStrategyIndex = 0;

		// UCB requires us to try everything once before using the formula
		for (size_t strategyIndex(0); strategyIndex<usableStrategies.size(); ++strategyIndex)
		{
			int sum = results[usableStrategies[strategyIndex]].first + results[usableStrategies[strategyIndex]].second;

			if (sum == 0)
			{
				currentStrategy = usableStrategies[strategyIndex];
				return;
			}
		}

		// if we have tried everything once, set the maximizing ucb value
		for (size_t strategyIndex(0); strategyIndex<usableStrategies.size(); ++strategyIndex)
		{
			double ucb = getUCBValue(usableStrategies[strategyIndex]);

			if (ucb > bestUCB)
			{
				bestUCB = ucb;
				bestStrategyIndex = strategyIndex;
			}
		}
		
		currentStrategy = usableStrategies[bestStrategyIndex];
	}
	else
	{
		// otherwise return a random strategy
		currentStrategy = rand() % usableStrategies.size();
		StrategyManager::Instance().log(boost::lexical_cast<std::string>(currentStrategy));
		StrategyManager::Instance().log(openingBook[currentStrategy]);
		//currentStrategy = usableStrategies[rand() % usableStrategies.size()];
	}
}

void StrategyManager::onEnd(const bool isWinner)
{
	// write the win/loss data to file if we're using IO
	if (Options::Modules::USING_STRATEGY_IO)
	{
		// if the game ended before the tournament time limit
		if (BWAPI::Broodwar->getFrameCount() < Options::Tournament::GAME_END_FRAME)
		{
			if (isWinner)
			{
				results[getCurrentStrategy()].first = results[getCurrentStrategy()].first + 1;
			}
			else
			{
				results[getCurrentStrategy()].second = results[getCurrentStrategy()].second + 1;
			}
		}
		// otherwise game timed out so use in-game score
		else
		{
			if (getScore(BWAPI::Broodwar->self()) > getScore(BWAPI::Broodwar->enemy()))
			{
				results[getCurrentStrategy()].first = results[getCurrentStrategy()].first + 1;
			}
			else
			{
				results[getCurrentStrategy()].second = results[getCurrentStrategy()].second + 1;
			}
		}
		
		writeResults();
	}
}

const double StrategyManager::getUCBValue(const size_t & strategy) const
{
	double totalTrials(0);
	for (size_t s(0); s<usableStrategies.size(); ++s)
	{
		totalTrials += results[usableStrategies[s]].first + results[usableStrategies[s]].second;
	}

	double C		= 0.7;
	double wins		= results[strategy].first;
	double trials	= results[strategy].first + results[strategy].second;

	double ucb = (wins / trials) + C * sqrt(std::log(totalTrials) / trials);
	return ucb;
}

const int StrategyManager::getScore(BWAPI::Player * player) const
{
	return player->getBuildingScore() + player->getKillScore() + player->getRazingScore() + player->getUnitScore();
}

const std::string StrategyManager::getOpening() const
{
	return openingBook[currentStrategy];
}

// when do we want to defend with our workers?
// this function can only be called if we have no fighters to defend with
const int StrategyManager::defendWithWorkers()
{
	if (!Options::Micro::WORKER_DEFENSE)
	{
		return false;
	}

	// our home nexus position
	BWAPI::Position homePosition = BWTA::getStartLocation(BWAPI::Broodwar->self())->getPosition();;

	// enemy units near our workers
	int enemyUnitsNearWorkers = 0;

	// defense radius of nexus
	int defenseRadius = 300;

	// fill the set with the types of units we're concerned about
	BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->enemy()->getUnits())
	{
		// if it's a zergling or a worker we want to defend
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
		{
			if (unit->getDistance(homePosition) < defenseRadius)
			{
				enemyUnitsNearWorkers++;
			}
		}
	}

	// if there are enemy units near our workers, we want to defend
	return enemyUnitsNearWorkers;
}

// called by combat commander to determine whether or not to send an attack force
// freeUnits are the units available to do this attack
const bool StrategyManager::doAttack(const std::set<BWAPI::Unit *> & freeUnits)
{
	int ourForceSize = (int)freeUnits.size();
	int frame =	BWAPI::Broodwar->getFrameCount();

	int desiredAttackTiming = getDesiredAttackTiming();
	std::map<BWAPI::UnitType, int> desiredTroops = StrategyManager::extractArmyComposition(armyCompositions.front());
	
	bool timingOK = frame > desiredAttackTiming;
	bool armyOK = sufficientArmy(desiredTroops, freeUnits);
	bool doAttack  = timingOK && armyOK;

	if (doAttack)
	{
		firstAttackSent = true;
	}

	return doAttack || firstAttackSent;
}


const bool StrategyManager::sufficientArmy(std::map<BWAPI::UnitType, int> desiredArmy, const std::set<BWAPI::Unit *> & units) const
{
	std::map<BWAPI::UnitType, int>::iterator typeIt;
	std::set<BWAPI::Unit *>::const_iterator unitIt;

	for (typeIt = desiredArmy.begin(); typeIt != desiredArmy.end(); ++typeIt)
	{
		for (unitIt = units.begin(); unitIt != units.end(); ++unitIt)
		{
			if (typeIt->first == (*unitIt)->getType()) 
			{
				typeIt->second -= 1;
			}
		}

		if (typeIt->second > 0) { return false; }
	}

	return true;
}

const bool StrategyManager::expandProtossZealotRush() const
{
	// if there is no place to expand to, we can't expand
	if (MapTools::Instance().getNextExpansion() == BWAPI::TilePositions::None)
	{
		return false;
	}

	int numNexus =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	int numZealots =			BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Zealot);
	int frame =					BWAPI::Broodwar->getFrameCount();

	// if there are more than 10 idle workers, expand
	if (WorkerManager::Instance().getNumIdleWorkers() > 10)
	{
		return true;
	}

	// 2nd Nexus Conditions:
	//		We have 12 or more zealots
	//		It is past frame 7000
	if ((numNexus < 2) && (numZealots > 12 || frame > 9000))
	{
		return true;
	}

	// 3nd Nexus Conditions:
	//		We have 24 or more zealots
	//		It is past frame 12000
	if ((numNexus < 3) && (numZealots > 24 || frame > 15000))
	{
		return true;
	}

	if ((numNexus < 4) && (numZealots > 24 || frame > 21000))
	{
		return true;
	}

	if ((numNexus < 5) && (numZealots > 24 || frame > 26000))
	{
		return true;
	}

	if ((numNexus < 6) && (numZealots > 24 || frame > 30000))
	{
		return true;
	}

	return false;
}

const bool StrategyManager::expandProtossObserver() const
{
	// if there is no place to expand to, we can't expand
	if (MapTools::Instance().getNextExpansion() == BWAPI::TilePositions::None)
	{
		return false;
	}
	int numNexus =				BWAPI::Broodwar->self()->allUnitCount(BWAPI::UnitTypes::Protoss_Nexus);
	
	//we need 3 nexus and only 3 nexus. 
	if(numNexus < 3)
	{
		return true;
	}
	else 
	{
		return false;
	}
}

const MetaPairVector StrategyManager::getBuildOrderGoal()
{
	MetaPairVector goal;

	std::map<BWAPI::UnitType, int> desiredArmy = StrategyManager::extractArmyComposition(armyCompositions.front());
	std::map<BWAPI::UnitType, int>::iterator typeIt;
	std::set<BWAPI::Unit *>::const_iterator unitIt;

	for (typeIt = desiredArmy.begin(); typeIt != desiredArmy.end(); ++typeIt)
	{
		int neededUnits = typeIt->second - BWAPI::Broodwar->self()->allUnitCount(typeIt->first);
		goal.push_back(MetaPair(typeIt->first, neededUnits));
	}

	return goal;
}


const int StrategyManager::getCurrentStrategy()
{
	return currentStrategy;
}


void StrategyManager::loadStrategiesFromFile(BWAPI::Race race)
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
		results = std::vector<IntPair>(i);
	}else
	{
		BWAPI::Broodwar->printf(
			"Unable to open file, some things may not be working or the entire program may crash glhf :).");
	}
}

void StrategyManager::loadPlannedAttacksFromFile()
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


	}else
	{
		BWAPI::Broodwar->printf(
			"Unable to open file, some things may not be working or the entire program may crash glhf :).");
	}
}

/* Extracts information from two strings. 
 * StringPair.first - the unit codes
 * StringPair.second - the amount of troops of each unit type
 * Eg) "0 24 17", "10 5 1"   means   10 units of unit type 0, 5 of type 24 and 1 of type 17
 */
const std::map<BWAPI::UnitType, int> StrategyManager::extractArmyComposition(StringPair pair)
{
	std::map<BWAPI::UnitType, int> unitMap;
	std::vector<MetaType> units = StarcraftBuildOrderSearchManager::Instance().getMetaVector(pair.first);
	
	std::vector<MetaType>::iterator it = units.begin();
	std::stringstream ss;
	ss << pair.second;

	int num(0);
	while (ss >> num && it != units.end())
	{
		BWAPI::UnitType unit = it->unitType;
		unitMap.insert(std::make_pair(unit, num));
	}

	return unitMap;
}

const int StrategyManager::getDesiredAttackTiming()
{
	return attackTimings.front();
}

void StrategyManager::log(std::string filename, std::string output)
{
	std::ofstream file;
	file.open(filename.c_str(), std::ios::app);
	file << output << "\n";
	file.close();
}

void StrategyManager::log(std::string output)
{
	log(LOG_FILE, output);
}