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
typedef std::map<BWAPI::UnitType, int> MetaMap;
typedef std::set<BWAPI::Unit *> UnitSet;

class StrategyPlanner
{
	StrategyPlanner();
	~StrategyPlanner() {}

	/****************** Variables *************/
	std::vector<std::string>	openingBook;
	std::vector<int>			usableStrategies;
	int							currentStrategy;
	int							numStrategies;

	BWAPI::Race					selfRace;
	BWAPI::Race					enemyRace;

	bool						newAttackGoal;
	std::queue<int>				attackTimings;
	std::queue<StringPair>		armyCompositions;

	MetaMap						currentWantedArmyComposition;

	UnitSet						unitsAllowedToAttack;

	/****************** Methods *************/
	
	void	setStrategy();
	
	
	// log stuff
	void	log(std::string filename, std::string output);
	void	log(std::string output);

	// load stuff from files
	void	loadStrategiesFromFile(BWAPI::Race race);
	void	loadPlannedAttacksFromFile();

public:

	static	StrategyPlanner &	Instance();

	const	void				moveToNextAttackGoal();
	const	int					getCurrentStrategy();
	const	int					getDesiredAttackTiming();

	const	UnitSet				getAttackSquad(const UnitSet freeUnits);
	const	UnitSet				getAttackSquad(const MetaMap wantedSquad, const UnitSet freeUnits);
	const	MetaMap				getArmyComposition();
	const	MetaMap				getArmyComposition(StringPair armyComposition);

	const	MetaPairVector		getBuildOrderGoal();
	const	std::string			getOpening() const;
	const	std::vector<int>	getUsableStrategies();
};

