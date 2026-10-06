// Reactor — Copyright (c) 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FReactorSimulation;

/**
 * Repeatable per-tick timing for the settlement kernel.
 *
 * Exists because "how expensive is a tick at the target grid size" is a real
 * acceptance criterion of the first kernel story, and the answer has to be a
 * number produced by a run rather than an estimate. `project.yaml`'s
 * `performance` block was left unset on purpose until this could fill it.
 *
 * The extra passes taken here are the same whole-grid settlement passes the real
 * loop performs, so a measurement at WorkMultiplier = N is a usable stand-in for
 * a grid N+1 times larger. That is how the 512x512 figure is reached without
 * allocating a 512x512 buffer.
 */
class REACTOR_API FSimulationBenchmark
{
public:
	struct FResult
	{
		int32 GridWidth = 0;
		int32 GridHeight = 0;
		int32 WorkMultiplier = 0;

		/** Whole-cell updates performed by one step, matching FGridSimulation::GetCellUpdatesPerStep. */
		int64 CellUpdatesPerStep = 0;

		int32 StepsMeasured = 0;

		/** Wall-clock milliseconds per step, averaged over StepsMeasured. */
		double MillisecondsPerStep = 0.0;

		/** Cell updates per second implied by MillisecondsPerStep. */
		double UpdatesPerSecond = 0.0;

		/**
		 * The frame-rate target this measurement is reported against, read from the
		 * simulation parameters. Carried in the result so Format() never needs a
		 * literal — a result printed against a different target than the project
		 * committed to is worse than no number.
		 */
		double TargetFramerate = 0.0;

		/** Effective ratio of this step's work to one baseline 256x256 grid. */
		double EquivalentGridsPerStep = 0.0;
	};

	/**
	 * Time StepsToMeasure settlement steps on the simulation's current grid.
	 *
	 * A few steps are run and discarded first: the first step after a seed pays
	 * for cache misses and page faults that later steps do not, and including it
	 * would make a short measurement report a cost the game never sees twice.
	 */
	static FResult Measure(FReactorSimulation& Simulation, int32 StepsToMeasure = 30, int32 WarmupSteps = 3);

	/** Format a result as a plain-text report, suitable for writing into a log or a doc. */
	static FString Format(const FResult& Result);
};
