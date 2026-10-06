// Reactor — Copyright (c) 2026. All Rights Reserved.

#include "Simulation/ReactorSimulation.h"

DEFINE_LOG_CATEGORY_STATIC(LogReactorSimulation, Log, All);

bool FReactorSimulation::LoadTable(const FString& InPath, FSubstanceTable& OutTable, FString& OutError)
{
	if (!OutTable.LoadFromJsonFile(InPath, OutError))
	{
		return false;
	}

	TArray<FString> Problems;
	OutTable.Validate(Problems);
	if (Problems.Num() > 0)
	{
		// Every problem is reported, not just the first: a hand-edited table
		// usually has several, and fixing them one reload at a time is how a data
		// format becomes unpleasant to iterate on.
		OutError = FString::Printf(TEXT("Substance table failed validation (%d problem(s)): %s"),
			Problems.Num(), *FString::Join(Problems, TEXT(" | ")));
		return false;
	}

	return true;
}

bool FReactorSimulation::InitializeFromTableFile(const FString& InPath, FString& OutError)
{
	FSubstanceTable LoadedTable;
	if (!LoadTable(InPath, LoadedTable, OutError))
	{
		bInitialized = false;
		return false;
	}

	InitializeFromTable(LoadedTable, LoadedTable.GetParams());
	return true;
}

void FReactorSimulation::InitializeFromTable(const FSubstanceTable& InTable, const FSimulationParams& InParams)
{
	Table = InTable;
	Grid.Initialize(Table, InParams);
	bInitialized = true;
}

bool FReactorSimulation::CreateFromJsonString(const FString& InJson, FReactorSimulation& OutSimulation, FString& OutError)
{
	FSubstanceTable ParsedTable;
	if (!FSubstanceTable::FromJsonString(InJson, ParsedTable, OutError))
	{
		OutError = FString::Printf(TEXT("Substance table failed validation: %s"), *OutError);
		return false;
	}

	TArray<FString> Problems;
	ParsedTable.Validate(Problems);
	if (Problems.Num() > 0)
	{
		OutError = FString::Printf(TEXT("Substance table failed validation (%d problem(s)): %s"),
			Problems.Num(), *FString::Join(Problems, TEXT(" | ")));
		return false;
	}

	OutSimulation.InitializeFromTable(ParsedTable, ParsedTable.GetParams());
	return true;
}

FSimulationState FReactorSimulation::GetState() const
{
	FSimulationState State;
	State.Width = Grid.GetWidth();
	State.Height = Grid.GetHeight();
	State.Cells = TArray<FSubstanceId>();
	State.Cells.Reserve(Grid.GetCellCount());

	// Read through the public accessor rather than reaching into the kernel, so a
	// future change to how cells are stored does not also change this snapshot.
	for (int32 Y = 0; Y < Grid.GetHeight(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetWidth(); ++X)
		{
			State.Cells.Add(Grid.GetCell(X, Y));
		}
	}

	State.Changes = Grid.GetChanges();
	State.Occupancy = Grid.GetOccupancy();
	State.ChangedCellCount = Grid.GetChangedCellCount();
	State.ReactionCount = Grid.GetReactionCount();
	return State;
}
