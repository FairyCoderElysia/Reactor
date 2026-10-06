// Reactor — Copyright (c) 2026. All Rights Reserved.

#include "Reactor.h"

#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Simulation/ReactorSimulation.h"
#include "Simulation/SimulationBenchmark.h"

DEFINE_LOG_CATEGORY_STATIC(LogReactor, Log, All);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, Reactor, "Reactor");

namespace
{
	/**
	 * `Reactor.BenchmarkSimulation [workMultiplier] [steps]`
	 *
	 * Measures the settlement kernel and logs the result. Run it from the editor
	 * console, or unattended via
	 * `-ExecCmds="Reactor.BenchmarkSimulation 3 30"`.
	 *
	 * A console command rather than a code path because the number it produces is
	 * a property of the machine, not of the build: it has to be re-measurable
	 * whenever the hardware or the grid size changes.
	 */
	void BenchmarkSimulationCommand(const TArray<FString>& Args)
	{
		int32 WorkMultiplier = 0;
		int32 Steps = 30;

		if (Args.Num() > 0)
		{
			WorkMultiplier = FCString::Atoi(*Args[0]);
		}
		if (Args.Num() > 1)
		{
			Steps = FCString::Atoi(*Args[1]);
		}

		const FString TablePath = FReactorSimulation::GetDefaultTablePath();

		FReactorSimulation Simulation;
		FString Error;
		if (!Simulation.InitializeFromTableFile(TablePath, Error))
		{
			UE_LOG(LogReactor, Error, TEXT("Reactor.BenchmarkSimulation — could not load the settlement table: %s"), *Error);
			return;
		}

		// The table's own workMultiplier is what the shipped config carries; the
		// argument overrides it so one command can measure several effective grid
		// sizes without editing the data file.
		FSimulationParams Params = Simulation.GetTable().GetParams();
		const int32 TableMultiplier = Params.WorkMultiplier;
		Params.WorkMultiplier = FMath::Max(0, WorkMultiplier);
		Simulation.InitializeFromTable(Simulation.GetTable(), Params);

		Simulation.GetGrid().FillRandom(20261006, 0.55f);

		const FSimulationBenchmark::FResult Result = FSimulationBenchmark::Measure(Simulation, Steps);

		UE_LOG(LogReactor, Display, TEXT("%s"), *FSimulationBenchmark::Format(Result));
		UE_LOG(LogReactor, Display,
			TEXT("Reactor.BenchmarkSimulation — table workMultiplier was %d, this run used %d. Settled %lld steps, %d cells changed in the last one, %d reaction matches."),
			TableMultiplier,
			Params.WorkMultiplier,
			Simulation.GetGrid().GetStepIndex(),
			Simulation.GetGrid().GetChangedCellCount(),
			Simulation.GetGrid().GetReactionCount());
	}

	/**
	 * `Reactor.DumpSimulationState [seed]`
	 *
	 * Diagnostic for the "is anything actually happening" question. Prints what
	 * the loader produced and what one settlement step does with it, so a silent
	 * mismatch between the data file and the kernel shows up as a number rather
	 * than as an absence.
	 */
	void DumpSimulationStateCommand(const TArray<FString>& Args)
	{
		const int32 Seed = (Args.Num() > 0) ? FCString::Atoi(*Args[0]) : 20261006;

		FReactorSimulation Simulation;
		FString Error;
		if (!Simulation.InitializeFromTableFile(FReactorSimulation::GetDefaultTablePath(), Error))
		{
			UE_LOG(LogReactor, Error, TEXT("Reactor.DumpSimulationState — table load failed: %s"), *Error);
			return;
		}

		const FSubstanceTable& Table = Simulation.GetTable();
		UE_LOG(LogReactor, Display, TEXT("Table: %d substances, %d reactions, grid %dx%d, fixedStep %.4fs"),
			Table.GetSubstances().Num(),
			Table.NumReactions(),
			Table.GetParams().GridWidth,
			Table.GetParams().GridHeight,
			Table.GetParams().FixedStepSeconds);

		for (const FSubstanceDef& Def : Table.GetSubstances())
		{
			UE_LOG(LogReactor, Display, TEXT("  substance '%s' primitive=%s"),
				*Def.Id.Name.ToString(), Def.bIsPrimitive ? TEXT("true") : TEXT("false"));
		}

		// Minimal isolation: one cell of each primitive placed far apart. If a
		// lone cell does not survive a step, the fault is in the step's write
		// path, not in the seeding or the tables.
		{
			FReactorSimulation Isolated;
			FString IsolatedError;
			if (Isolated.InitializeFromTableFile(FReactorSimulation::GetDefaultTablePath(), IsolatedError))
			{
				FGridSimulation& IsolatedGrid = Isolated.GetGrid();
				IsolatedGrid.Fill(FSubstanceId());
				const TArray<FSubstanceDef>& Substances = Isolated.GetTable().GetSubstances();
				for (int32 Index = 0; Index < Substances.Num() && Index < 5; ++Index)
				{
					IsolatedGrid.SetCell(10 + Index * 40, 10, Substances[Index].Id);
				}

				UE_LOG(LogReactor, Display, TEXT("isolated: before step, %d non-empty"),
					IsolatedGrid.GetOccupancy().Num());
				for (const TPair<FSubstanceId, int32>& Entry : IsolatedGrid.GetOccupancy())
				{
					UE_LOG(LogReactor, Display, TEXT("  '%s' = %d"), *Entry.Key.Name.ToString(), Entry.Value);
				}

				IsolatedGrid.Step();

				UE_LOG(LogReactor, Display, TEXT("isolated: after 1 step, %d distinct non-empty, changed=%d"),
					IsolatedGrid.GetOccupancy().Num(), IsolatedGrid.GetChangedCellCount());
				for (const TPair<FSubstanceId, int32>& Entry : IsolatedGrid.GetOccupancy())
				{
					UE_LOG(LogReactor, Display, TEXT("  '%s' = %d"), *Entry.Key.Name.ToString(), Entry.Value);
				}

				IsolatedGrid.Step();
				UE_LOG(LogReactor, Display, TEXT("isolated: after 2 steps, %d distinct non-empty, changed=%d"),
					IsolatedGrid.GetOccupancy().Num(), IsolatedGrid.GetChangedCellCount());
				for (const TPair<FSubstanceId, int32>& Entry : IsolatedGrid.GetOccupancy())
				{
					UE_LOG(LogReactor, Display, TEXT("  '%s' = %d"), *Entry.Key.Name.ToString(), Entry.Value);
				}
			}
		}

		FGridSimulation& Grid = Simulation.GetGrid();
		Grid.FillRandom(Seed, 0.55f);

		UE_LOG(LogReactor, Display, TEXT("After fill: occupancy {"));
		for (const TPair<FSubstanceId, int32>& Entry : Grid.GetOccupancy())
		{
			UE_LOG(LogReactor, Display, TEXT("  '%s' = %d"), *Entry.Key.Name.ToString(), Entry.Value);
		}
		UE_LOG(LogReactor, Display, TEXT("}"));

		// Step a few times and report each one, so a reaction that fires once and
		// then stops is distinguishable from one that never fires at all.
		for (int32 Step = 1; Step <= 5; ++Step)
		{
			Grid.Step();
			UE_LOG(LogReactor, Display,
				TEXT("step %d: changed=%d reacted=%d | occupancy {"),
				Step, Grid.GetChangedCellCount(), Grid.GetReactionCount());
			for (const TPair<FSubstanceId, int32>& Entry : Grid.GetOccupancy())
			{
				UE_LOG(LogReactor, Display, TEXT("  '%s' = %d"), *Entry.Key.Name.ToString(), Entry.Value);
			}
			UE_LOG(LogReactor, Display, TEXT("}"));
		}
	}

	FAutoConsoleCommand GDumpSimulationStateCommand(
		TEXT("Reactor.DumpSimulationState"),
		TEXT("Print the loaded tables, the seeded grid, and five settlement steps. Usage: Reactor.DumpSimulationState [seed]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&DumpSimulationStateCommand));

	// UE 5.8 has no FAutoConsoleCommandWithArgs: the family was folded into
	// FAutoConsoleCommand's overloaded constructors, one of which takes an
	// FConsoleCommandWithArgsDelegate. (The WithWorld / WithOutputDevice variants
	// survive as separate classes.) Reaching for the old name costs a compile
	// error, not a deprecation warning — see docs/engine-reference/unreal/.
	FAutoConsoleCommand GBenchmarkSimulationCommand(
		TEXT("Reactor.BenchmarkSimulation"),
		TEXT("Measure the settlement kernel. Usage: Reactor.BenchmarkSimulation [workMultiplier] [steps]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&BenchmarkSimulationCommand));
}
