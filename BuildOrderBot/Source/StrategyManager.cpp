#include "Common.h"
#include "StrategyManager.h"
#include <boost/lexical_cast.hpp>
#include <base/StarcraftBuildOrderSearchManager.h>
#include "StrategyPlanner.h"

const std::string LOG_FILE = "log.txt";

// constructor
StrategyManager::StrategyManager() 
	: firstAttackSent(false)
	, selfRace(BWAPI::Broodwar->self()->getRace())
	, enemyRace(BWAPI::Broodwar->enemy()->getRace())
{
	
}

// get an instance of this
StrategyManager & StrategyManager::Instance() 
{
	static StrategyManager instance;
	return instance;
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
		unsigned int numStrategies = StrategyPlanner::Instance().getUsableStrategies().size();
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
	unsigned int numStrategies = StrategyPlanner::Instance().getUsableStrategies().size();
	while (strategy < numStrategies) 
	{
		f_out << results[strategy].first   << "\n";
		f_out << results[strategy++].second  << "\n";
	}

	f_out.close();
}

void StrategyManager::onEnd(const bool isWinner)
{
	// write the win/loss data to file if we're using IO
	if (Options::Modules::USING_STRATEGY_IO)
	{
		// if the game ended before the tournament time limit
		int currentStrategy = StrategyPlanner::Instance().getCurrentStrategy();
		if (BWAPI::Broodwar->getFrameCount() < Options::Tournament::GAME_END_FRAME)
		{
			if (isWinner)
			{
				results[currentStrategy].first = results[currentStrategy].first + 1;
			}
			else
			{
				results[currentStrategy].second = results[currentStrategy].second + 1;
			}
		}
		// otherwise game timed out so use in-game score
		else
		{
			if (getScore(BWAPI::Broodwar->self()) > getScore(BWAPI::Broodwar->enemy()))
			{
				results[currentStrategy].first = results[currentStrategy].first + 1;
			}
			else
			{
				results[currentStrategy].second = results[currentStrategy].second + 1;
			}
		}
		
		writeResults();
	}
}

const double StrategyManager::getUCBValue(const size_t & strategy) const
{
	std::vector<int> usableStrategies = StrategyPlanner::Instance().getUsableStrategies();
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
	//int ourForceSize = (int)freeUnits.size();
	StrategyManager::Instance().log("doAttack() started");
	int frame =	BWAPI::Broodwar->getFrameCount();
	StrategyManager::Instance().log("frameCount is " + frame);
	int desiredAttackTiming = StrategyPlanner::Instance().getDesiredAttackTiming();
	StrategyManager::Instance().log("desiredAttackTiming is " + desiredAttackTiming);
	const MetaMap desiredTroops = StrategyPlanner::Instance().getArmyComposition();
	StrategyManager::Instance().log("desiredTroops is something");
	
	bool timingOK = frame > desiredAttackTiming;
	bool armyOK = sufficientTroops(desiredTroops, freeUnits);
	bool doAttack  = timingOK && armyOK;

	if (doAttack)
	{
		firstAttackSent = true;
	}

	StrategyManager::Instance().log("doAttack() ended");
	return doAttack || firstAttackSent;
}


const bool StrategyManager::sufficientTroops(const MetaMap desiredArmy, const std::set<BWAPI::Unit *> & units) const
{
	MetaMap::const_iterator typeIt;
	std::set<BWAPI::Unit *>::const_iterator unitIt;

	for (typeIt = desiredArmy.begin(); typeIt != desiredArmy.end(); ++typeIt)
	{
		int soldiers = 0;
		for (unitIt = units.begin(); unitIt != units.end(); ++unitIt)
		{
			if (typeIt->first == (*unitIt)->getType()) 
			{
				soldiers++;
			}
		}

		if (typeIt->second > soldiers) { return false; }
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
	return StrategyPlanner::Instance().getBuildOrderGoal();
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