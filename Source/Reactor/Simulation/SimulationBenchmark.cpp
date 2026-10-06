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
	Result.TargetFramerate = Grid.GetParams().TargetFramerate;
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

	// The baseline is the grid this project actually ships at, not a literal pair
	// of numbers: the ratio is only meaningful relative to the configured baseline,
	// and a hardcoded 256x256 would keep reporting "1.00x the baseline" for a
	// project that had since moved to a different size.
	const double CellsPerBaselineGrid = static_cast<double>(Result.GridWidth) * static_cast<double>(Result.GridHeight);
	Result.EquivalentGridsPerStep = (CellsPerBaselineGrid > 0.0)
		? (static_cast<double>(Result.CellUpdatesPerStep) / CellsPerBaselineGrid)
		: 0.0;

	return Result;
}

FString FSimulationBenchmark::Format(const FResult& Result)
{
	if (Result.StepsMeasured == 0 || Result.CellUpdatesPerStep <= 0)
	{
		return TEXT("Reactor benchmark: no measurement (simulation uninitialised, or zero steps requested).");
	}

	// Read from the parameters, never a literal: a hardcoded 60 here would keep
	// reporting "headroom in a 16.67 ms frame" long after the project committed to
	// a different target, which is the failure mode this whole data-driven
	// arrangement exists to prevent. Zero means no target was configured, and the
	// ratio is then reported as unavailable rather than invented.
	const double TargetFramerate = Result.TargetFramerate;
	const double BudgetMilliseconds = (TargetFramerate > 0.0) ? (1000.0 / TargetFramerate) : 0.0;
	const double Headroom = (BudgetMilliseconds > 0.0 && Result.MillisecondsPerStep > 0.0)
		? (BudgetMilliseconds / Result.MillisecondsPerStep)
		: 0.0;

	return FString::Printf(
		TEXT("Reactor benchmark: %dx%d @ workMultiplier=%d | %lld cell-updates/step | %.4f ms/step | %.1f M updates/s | %.2fx the configured %.0fHz frame | %.2fx the baseline grid | %d steps measured"),
		Result.GridWidth,
		Result.GridHeight,
		Result.WorkMultiplier,
		Result.CellUpdatesPerStep,
		Result.MillisecondsPerStep,
		Result.UpdatesPerSecond / 1.0e6,
		Headroom,
		TargetFramerate,
		Result.EquivalentGridsPerStep,
		Result.StepsMeasured);
}
