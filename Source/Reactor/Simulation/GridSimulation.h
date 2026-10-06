// Reactor — Copyright (c) 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ReactorTypes.h"

class FSubstanceTable;

/**
 * The settlement kernel: a grid of cells plus a fixed-step, deterministic step.
 *
 * Deliberately engine-agnostic apart from the container/JSON types — it owns no
 * UObject, spawns nothing, and touches no tick. That is what lets the whole
 * kernel be exercised from an automated test with no world.
 *
 * Two invariants the step must never lose:
 *
 *  1. DETERMINISM. The result of a step is a pure function of the previous
 *     state, the tables and the step index — never of wall-clock time, frame
 *     rate, iteration order of a hash container, or address-dependent values.
 *     design/game-brief.md requires puzzles to be saved self-contained and
 *     re-runnable, which is only meaningful if the same input re-settles the
 *     same way.
 *  2. DOUBLE BUFFERING. A step reads FrameN and writes FrameN+1; it never reads
 *     a cell it has already written this step. Single-buffering makes the result
 *     depend on scan order, which breaks invariant 1 and produces reactions that
 *     visibly "flow" in one direction.
 */
class REACTOR_API FGridSimulation
{
public:
	FGridSimulation() = default;

	/**
	 * Build a grid and seed it. The table must already be validated — call
	 * FSubstanceTable::Validate first; a kernel handed an invalid table cannot
	 * report anything useful.
	 */
	void Initialize(const FSubstanceTable& InTable, const FSimulationParams& InParams);

	/** Seed the whole grid with one substance. */
	void Fill(const FSubstanceId& Substance);

	/** Seed a band of rows. Clamped to the grid; out-of-range input is ignored, not fatal. */
	void FillRows(int32 FirstRow, int32 RowCount, const FSubstanceId& Substance);

	void SetCell(int32 X, int32 Y, const FSubstanceId& Substance);
	FSubstanceId GetCell(int32 X, int32 Y) const;

	/**
	 * Fill from a fixed-seed PRNG over the table's substances.
	 *
	 * Uses FRandomStream, not FMath::Rand: the latter draws from global engine
	 * state shared with everything else in the process, so a seed there does not
	 * actually pin the outcome. A seeded FRandomStream is reproducible, and each
	 * index is advanced by its own fixed number of draws so the result does not
	 * depend on call order either.
	 */
	void FillRandom(int32 Seed, float Density);

	/**
	 * Run exactly one settlement step.
	 *
	 * The step is unconditional and stateless with respect to time — it is the
	 * caller (a test, or Advance) that decides whether a step is due. Keeping the
	 * accumulator out of here is what makes "N steps" reproducible.
	 */
	void Step();

	/**
	 * Account for ElapsedSeconds of real time and run however many whole fixed
	 * steps are due, capped at MaxStepsPerFrame.
	 *
	 * Returns the number of steps actually run. The cap exists so a long hitch
	 * cannot make the simulation try to catch up unboundedly; dropping time is
	 * the deliberate trade. Not used by automated tests, which call Step().
	 */
	int32 Advance(float ElapsedSeconds);

	/** Cell-change flags produced by the most recent Step(). Length = width * height. */
	const TArray<ECellChange>& GetChanges() const { return Changes; }

	/** Occupancy counts produced by the most recent Step(), rebuilt for the visualisation layer. */
	const TMap<FSubstanceId, int32>& GetOccupancy() const { return Occupancy; }

	int32 GetChangedCellCount() const { return ChangedCellCount; }
	int32 GetReactionCount() const { return ReactionCount; }

	/** Steps executed since Initialize. The step index is part of the step's input, not wall time. */
	uint64 GetStepIndex() const { return StepIndex; }

	int32 GetWidth() const { return Width; }
	int32 GetHeight() const { return Height; }
	int32 GetCellCount() const { return Width * Height; }

	/** Seconds of un-consumed real time left over from the last Advance. */
	float GetPendingSeconds() const { return AccumulatorSeconds; }

	const FSimulationParams& GetParams() const { return Params; }

	/**
	 * Number of whole-cell updates performed by one Step().
	 *
	 * Equals the cell count at WorkMultiplier = 0. The multiplier makes the extra
	 * passes a separate measured quantity so the settlement cost can be
	 * extrapolated to a 512x512 grid (4x the cells) without allocating one —
	 * filling the performance budget was an acceptance criterion of story 001.
	 * The index arithmetic mirrors the scan loop, so the measured loop has the
	 * same memory access pattern as a genuinely larger grid.
	 */
	int64 GetCellUpdatesPerStep() const { return static_cast<int64>(Width) * Height * (1 + FMath::Max(0, Params.WorkMultiplier)); }

	/**
	 * Updated by the WorkMultiplier passes so the compiler cannot elide them.
	 * Read only by tests asserting the stress knob actually did work.
	 */
	uint32 GetWorkPassChecksum() const { return WorkPassChecksum; }

private:
	/** Run the settlement scan once over the whole grid. */
	void StepPass();

	/** The WorkMultiplier-only extra passes. Measurement knob — see GetCellUpdatesPerStep. */
	void StepPassWorkOnly(int32 CellCount);

	/** Reset the step counter, accumulator and change flags after a seed operation. */
	void SeedState();

	/** Recompute occupancy from Cells. */
	void RebuildOccupancy();

	int32 IndexOf(int32 X, int32 Y) const { return Y * Width + X; }
	bool IsValidIndex(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }

	/** Read the previous frame. */
	FSubstanceId ReadPrev(int32 X, int32 Y) const;

	/** Write the next frame. Only ever called during a step. */
	void WriteNext(int32 X, int32 Y, const FSubstanceId& Substance);

	const FSubstanceTable* Table = nullptr;
	FSimulationParams Params;

	int32 Width = 0;
	int32 Height = 0;

	/** Frame N — what the current step reads. */
	TArray<FSubstanceId> CellsPrev;

	/** Frame N+1 — what the current step writes. Swapped with CellsPrev at the end of the step. */
	TArray<FSubstanceId> CellsNext;

	TArray<ECellChange> Changes;
	TMap<FSubstanceId, int32> Occupancy;

	int32 ChangedCellCount = 0;
	int32 ReactionCount = 0;
	uint64 StepIndex = 0;
	float AccumulatorSeconds = 0.0f;

	/**
	 * Updated by the WorkMultiplier passes so the compiler cannot elide them.
	 * Read only by tests asserting the stress knob actually did work.
	 */
	uint32 WorkPassChecksum = 0;
};
