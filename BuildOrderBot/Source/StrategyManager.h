#pragma once

#include "Common.h"
#include "BWTA.h"
#include "base/BuildOrderQueue.h"
#include "InformationManager.h"
#include "base/WorkerManager.h"
#include "base/StarcraftBuildOrderSearchManager.h"
#include <sys/stat.h>
#include <cstdlib>

#include "..\..\StarcraftBuildOrderSearch\Source\starcraftsearch\StarcraftData.hpp"

typedef std::pair<int, int> IntPair;
typedef std::pair<std::string, std::string> StringPair;
typedef std::pair<MetaType, UnitCountType> MetaPair;
typedef std::vector<MetaPair> MetaPairVector;

class StrategyManager 
{
	StrategyManager();
	~StrategyManager() {}

	std::vector<std::string>	openingBook;

	std::string					readDir;
	std::string					writeDir;
	std::vector<IntPair>		results;
	std::string					readFile;
	std::vector<int>			usableStrategies;
	int							currentStrategy;
	int							numStrategies;

	BWAPI::Race					selfRace;
	BWAPI::Race					enemyRace;

	bool						firstAttackSent;
	bool						enemyIsRandom;

	std::queue<int>				attackTimings;
	std::queue<StringPair>		armyCompositions;

	const	bool				sufficientArmy( std::map<BWAPI::UnitType, int> desiredArmy, const std::set<BWAPI::Unit *> & units) const;
	

	void	addStrategies();
	void	setStrategy();
	void	readResults();
	void	writeResults();
	void	loadStrategiesFromFile(BWAPI::Race race);
	void	loadPlannedAttacksFromFile();
	void	log(std::string filename, std::string output);
	void	log(std::string output);
	const	std::map<BWAPI::UnitType, int>	extractArmyComposition(StringPair pair);

	const	int					getScore(BWAPI::Player * player) const;
	const	double				getUCBValue(const size_t & strategy) const;
	
	const	MetaPairVector		getOpeningBook() const;

	// protoss strategy
	const	bool				expandProtossZealotRush() const;
	const	bool				expandProtossObserver() const;

	const void loadPlannedAttacks();

public:

	static	StrategyManager &	Instance();

			void				onEnd(const bool isWinner);
	
	const	bool				regroup(int numInRadius);
	const	bool				doAttack(const std::set<BWAPI::Unit *> & freeUnits);
	const	int				    defendWithWorkers();
	const	bool				rushDetected();

	const	int					getCurrentStrategy();
	const	int					getDesiredAttackTiming();

	const	MetaPairVector		getBuildOrderGoal();
	const	std::string			getOpening() const;
};
