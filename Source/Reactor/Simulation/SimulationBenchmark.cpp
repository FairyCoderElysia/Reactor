// Reactor — Copyright (c) 2026. All Rights Reserved.

#include "Simulation/SimulationBenchmark.h"

#include "HAL/PlatformTime.h"
#include "Simulation/GridSimulation.h"
#include "Simulation/ReactorSimulation.h"

FSimulationBenchmark::FResult FSimulationBenchmark::Measure(FReactorSimulation& Simulation, int32 StepsToMeasure, int32 WarmupSteps)
{
	FResult Result;

	if (!Simulation.IsInitialized())
	{
		return Result;
	}

	FGridSimulation& Grid = Simulation.GetGrid();

	Result.GridWidth = Grid.GetWidth();
	Result.GridHeight = Grid.GetHeight();
	Result.WorkMultiplier = Grid.GetParams().WorkMultiplier;
	Result.CellUpdatesPerStep = Grid.GetCellUpdatesPerStep();
	Result.StepsMeasured = FMath::Max(0, StepsToMeasure);

	for (int32 Warmup = 0; Warmup < FMath::Max(0, WarmupSteps); ++Warmup)
	{
		Grid.Step();
	}

	if (Result.StepsMeasured == 0 || Result.CellUpdatesPerStep <= 0)
	{
		return Result;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	for (int32 Step = 0; Step < Result.StepsMeasured; ++Step)
	{
		Grid.Step();
	}
	const double ElapsedSeconds = FPlatformTime::Seconds() - StartSeconds;

	if (ElapsedSeconds <= 0.0)
	{
		// A zero or negative elapsed reading means the clock did not advance (a
		// timer resolution floor on a very small workload). Reporting an infinite
		// update rate would be worse than reporting none.
		return Result;
	}

	Result.MillisecondsPerStep = (ElapsedSeconds * 1000.0) / static_cast<double>(Result.StepsMeasured);
	Result.UpdatesPerSecond = static_cast<double>(Result.CellUpdatesPerStep) * 1000.0 / Result.MillisecondsPerStep;

	const double CellsPerBaselineGrid = 256.0 * 256.0;
	Result.EquivalentGridsPerStep = static_cast<double>(Result.CellUpdatesPerStep) / CellsPerBaselineGrid;

	return Result;
}

FString FSimulationBenchmark::Format(const FResult& Result)
{
	if (Result.StepsMeasured == 0 || Result.CellUpdatesPerStep <= 0)
	{
		return TEXT("Reactor benchmark: no measurement (simulation uninitialised, or zero steps requested).");
	}

	const double BudgetMilliseconds = 1000.0 / 60.0;
	const double Headroom = (Result.MillisecondsPerStep > 0.0) ? (BudgetMilliseconds / Result.MillisecondsPerStep) : 0.0;

	return FString::Printf(
		TEXT("Reactor benchmark: %dx%d @ workMultiplier=%d | %lld cell-updates/step | %.4f ms/step | %.1f M updates/s | %.2fx the 256x256 baseline | %.1fx headroom in a 16.67 ms frame (%d steps measured)"),
		Result.GridWidth,
		Result.GridHeight,
		Result.WorkMultiplier,
		Result.CellUpdatesPerStep,
		Result.MillisecondsPerStep,
		Result.UpdatesPerSecond / 1.0e6,
		Result.EquivalentGridsPerStep,
		Headroom,
		Result.StepsMeasured);
}
