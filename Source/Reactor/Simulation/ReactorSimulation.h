// Reactor — Copyright (c) 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GridSimulation.h"
#include "Simulation/SubstanceTable.h"

/**
 * Drives one settlement simulation.
 *
 * Split from FGridSimulation so the kernel (which holds the determinism and
 * double-buffering contracts) stays free of any loading or lifetime concern, and
 * so a test can own the table and the grid independently.
 *
 * Intentionally not a UObject: nothing here needs reflection yet, and keeping
 * the kernel non-reflected is what allows the whole simulation to be exercised
 * from an automated test with no world and no GC.
 */
class REACTOR_API FReactorSimulation
{
public:
	FReactorSimulation() = default;

	/** Path of the default table, relative to the project directory. */
	static const TCHAR* GetDefaultTablePath()
	{
		return TEXT("Content/Data/Reactor/DefaultSubstances.json");
	}

	/**
	 * Load a table and create a grid from it.
	 *
	 * Returns false and leaves the object empty when the table is missing,
	 * malformed, or fails validation; OutError carries the reason. A caller that
	 * ignores the return value gets an empty simulation, never a half-built one.
	 */
	bool InitializeFromTableFile(const FString& InPath, FString& OutError);

	/** Initialize from a table already in memory. Used by tests and by the JSON-string path. */
	void InitializeFromTable(const FSubstanceTable& InTable, const FSimulationParams& InParams);

	/**
	 * Initialize both table and grid from a JSON document held in memory.
	 * The path automated tests take — no Content/ dependency, no file I/O.
	 */
	static bool CreateFromJsonString(const FString& InJson, FReactorSimulation& OutSimulation, FString& OutError);

	/**
	 * Load the table once and return it. Lets a caller that builds several grids
	 * from one table avoid re-reading the file.
	 */
	static bool LoadTable(const FString& InPath, FSubstanceTable& OutTable, FString& OutError);

	const FSubstanceTable& GetTable() const { return Table; }
	FGridSimulation& GetGrid() { return Grid; }
	const FGridSimulation& GetGrid() const { return Grid; }

	/** One settled frame plus its tables, for a reader that should not hold the simulation. */
	FSimulationState GetState() const;

	bool IsInitialized() const { return bInitialized; }

private:
	FSubstanceTable Table;
	FGridSimulation Grid;
	bool bInitialized = false;
};
