// Reactor — Copyright (c) 2026. All Rights Reserved.

#include "Simulation/GridSimulation.h"

#include "Math/RandomStream.h"
#include "Simulation/SubstanceTable.h"

namespace
{
	/** The fixed four-neighbourhood probe order. Part of the determinism contract — see StepPass. */
	const int32 GNeighbourOffsets[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
}

void FGridSimulation::Initialize(const FSubstanceTable& InTable, const FSimulationParams& InParams)
{
	Table = &InTable;
	Params = InParams;

	Width = FMath::Max(1, Params.GridWidth);
	Height = FMath::Max(1, Params.GridHeight);

	const int32 CellCount = Width * Height;
	CellsPrev.Init(FSubstanceId(), CellCount);
	CellsNext.Init(FSubstanceId(), CellCount);
	Changes.Init(ECellChange::Unchanged, CellCount);

	SeedState();
}

void FGridSimulation::SeedState()
{
	// Seeding is a new starting point, so the step counter restarts with it.
	// Leaving it running would make two "identical" runs differ in StepIndex,
	// and StepIndex is an input to the step even where nothing reads it yet.
	StepIndex = 0;
	AccumulatorSeconds = 0.0f;
	WorkPassChecksum = 0;
	ChangedCellCount = 0;
	ReactionCount = 0;

	CellsNext = CellsPrev;
	Changes.Init(ECellChange::Unchanged, CellsPrev.Num());
	RebuildOccupancy();
}

void FGridSimulation::Fill(const FSubstanceId& Substance)
{
	CellsPrev.Init(Substance, Width * Height);
	SeedState();
}

void FGridSimulation::FillRows(int32 FirstRow, int32 RowCount, const FSubstanceId& Substance)
{
	for (int32 Y = FirstRow; Y < FirstRow + RowCount; ++Y)
	{
		if (Y < 0 || Y >= Height)
		{
			continue;
		}
		for (int32 X = 0; X < Width; ++X)
		{
			CellsPrev[IndexOf(X, Y)] = Substance;
		}
	}
	SeedState();
}

void FGridSimulation::SetCell(int32 X, int32 Y, const FSubstanceId& Substance)
{
	if (IsValidIndex(X, Y))
	{
		CellsPrev[IndexOf(X, Y)] = Substance;
		RebuildOccupancy();
	}
}

FSubstanceId FGridSimulation::GetCell(int32 X, int32 Y) const
{
	return IsValidIndex(X, Y) ? CellsPrev[IndexOf(X, Y)] : FSubstanceId();
}

void FGridSimulation::FillRandom(int32 Seed, float Density)
{
	const TArray<FSubstanceDef>* Substances = (Table != nullptr) ? &Table->GetSubstances() : nullptr;
	if (Substances == nullptr || Substances->Num() == 0)
	{
		return;
	}

	// One stream for the whole fill, seeded deterministically and drawn in a fixed
	// index order. Single-threaded and order-fixed means the sequence is
	// reproducible; a per-cell stream would be more obviously order-independent
	// but is not needed here, and it invited the bug below.
	//
	// DO NOT call GenerateNewSeed() on this stream. Its implementation is
	// Initialize(FMath::Rand()), and FMath::Rand() draws from the process-global
	// RNG, which is seeded from platform entropy. Calling it silently replaces the
	// deterministic seed just set — the fill then differs between two runs of the
	// same build, which is exactly what the determinism test caught.
	//
	// Occupancy is decided with integer arithmetic, not `FRand() < Density`: that
	// comparison is a floating-point one and FRand()'s low bits are not stable
	// across runs (FMA contraction, optimisation settings), so a cell sitting on
	// the threshold flips. Density stays a float in the public API but is quantised
	// to whole percent here so no float comparison reaches the decision.
	const int32 DensityPercent = FMath::Clamp(FMath::RoundToInt(Density * 100.0f), 0, 100);

	FRandomStream Stream(Seed);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const bool bOccupied = Stream.RandRange(1, 100) <= DensityPercent;
			CellsPrev[IndexOf(X, Y)] = bOccupied
				? (*Substances)[Stream.RandRange(0, Substances->Num() - 1)].Id
				: FSubstanceId();
		}
	}

	SeedState();
}

int32 FGridSimulation::Advance(float ElapsedSeconds)
{
	if (!(Params.FixedStepSeconds > 0.0f))
	{
		return 0;
	}

	AccumulatorSeconds += FMath::Max(0.0f, ElapsedSeconds);

	const int32 MaxSteps = FMath::Max(0, Params.MaxStepsPerFrame);
	int32 StepsRun = 0;

	while (AccumulatorSeconds >= Params.FixedStepSeconds && StepsRun < MaxSteps)
	{
		Step();
		AccumulatorSeconds -= Params.FixedStepSeconds;
		++StepsRun;
	}

	if (StepsRun >= MaxSteps && AccumulatorSeconds >= Params.FixedStepSeconds)
	{
		// Deliberately drop the backlog rather than carry it: catching up over
		// many frames turns one hitch into a stutter that lasts, and nothing in
		// the design depends on the simulation being wall-clock accurate.
		AccumulatorSeconds = 0.0f;
	}

	return StepsRun;
}

void FGridSimulation::Step()
{
	if (Table == nullptr || CellsPrev.Num() == 0)
	{
		return;
	}

	StepPass();

	// The swap is what makes this step's output the next step's input.
	Swap(CellsPrev, CellsNext);
	++StepIndex;
	RebuildOccupancy();
}

void FGridSimulation::StepPass()
{
	const int32 CellCount = CellsPrev.Num();
	ChangedCellCount = 0;
	ReactionCount = 0;

	// Seed the write buffer from the read buffer, then let the scan overwrite only
	// the cells a reaction actually touched. This is the half of double buffering
	// that is easy to get wrong: clearing the write buffer instead of copying it
	// looks like the cleaner version and silently empties the entire grid in one
	// step, because every cell nobody wrote to becomes empty in the next frame.
	//
	// There is no stale-data hazard in copying here — the whole buffer is rebuilt
	// from CellsPrev at the top of every step, so nothing a previous step left in
	// CellsNext can survive into a read. Reads still come only from CellsPrev, so
	// the sweep direction still cannot leak into the result.
	CellsNext = CellsPrev;

	if (Params.WorkMultiplier > 0)
	{
		StepPassWorkOnly(CellCount);
	}

	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const int32 X = Index % Width;
		const int32 Y = Index / Width;

		const FSubstanceId Current = CellsPrev[Index];
		if (!Current.IsValid())
		{
			continue;
		}

		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			const int32 NX = X + GNeighbourOffsets[Direction][0];
			const int32 NY = Y + GNeighbourOffsets[Direction][1];
			if (!IsValidIndex(NX, NY))
			{
				continue;
			}

			// Reads always come from CellsPrev, never CellsNext: a cell already
			// written this step must not be visible to its neighbours, or the
			// sweep direction leaks into the result and the run stops being
			// reproducible.
			const FSubstanceId Neighbour = CellsPrev[IndexOf(NX, NY)];
			if (!Neighbour.IsValid())
			{
				continue;
			}

			const FReactionOutcome* Outcome = Table->FindReaction(Current, Neighbour);
			if (Outcome == nullptr)
			{
				continue;
			}

			if (Outcome->ResultA == Current && !Outcome->bProduceB)
			{
				// A catalyst-style rule that leaves both sides as they were.
				continue;
			}

			++ReactionCount;
			WriteNext(X, Y, Outcome->ResultA);

			if (Outcome->bProduceB)
			{
				WriteNext(NX, NY, Outcome->ResultB);
			}
			else
			{
				// Merge semantics: the pair collapses into one cell.
				WriteNext(NX, NY, FSubstanceId());
			}

			// First matching neighbour wins; the cell is consumed by the reaction.
			break;
		}
	}

	// Recompute changed flags by comparing this frame against the previous one.
	ChangedCellCount = 0;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		Changes[Index] = (CellsNext[Index] == CellsPrev[Index]) ? ECellChange::Unchanged : ECellChange::Changed;
		if (Changes[Index] != ECellChange::Unchanged)
		{
			++ChangedCellCount;
		}
	}
}

void FGridSimulation::StepPassWorkOnly(int32 CellCount)
{
	// Extra whole-grid settlement passes, used to reach a larger effective cell
	// count on the same allocation. A pass performs the same neighbour probes and
	// table lookups as the real one — a cheaper loop would make the extrapolation
	// to 512x512 dishonest — but writes nothing, so it cannot change the result.
	//
	// This is a measurement knob, never a gameplay value: it must stay 0 in a
	// shipped config. Nothing here is read by the step's own state.
	for (int32 Pass = 0; Pass < Params.WorkMultiplier; ++Pass)
	{
		uint32 PassAccumulator = 0;

		for (int32 Index = 0; Index < CellCount; ++Index)
		{
			const int32 X = Index % Width;
			const int32 Y = Index / Width;

			const FSubstanceId Current = CellsPrev[Index];
			if (!Current.IsValid())
			{
				continue;
			}

			for (int32 Direction = 0; Direction < 4; ++Direction)
			{
				const int32 NX = X + GNeighbourOffsets[Direction][0];
				const int32 NY = Y + GNeighbourOffsets[Direction][1];
				if (!IsValidIndex(NX, NY))
				{
					continue;
				}

				const FSubstanceId Neighbour = CellsPrev[IndexOf(NX, NY)];
				if (!Neighbour.IsValid())
				{
					continue;
				}

				const FReactionOutcome* Outcome = Table->FindReaction(Current, Neighbour);
				if (Outcome != nullptr)
				{
					PassAccumulator += 1;
					break;
				}
			}
		}

		// The accumulator is carried out of the loop so the compiler cannot elide
		// the passes in a Shipping build.
		WorkPassChecksum += PassAccumulator;
	}
}

void FGridSimulation::WriteNext(int32 X, int32 Y, const FSubstanceId& Substance)
{
	if (IsValidIndex(X, Y))
	{
		CellsNext[IndexOf(X, Y)] = Substance;
	}
}

void FGridSimulation::RebuildOccupancy()
{
	Occupancy.Reset();

	// Iterate in index order. Counts are order-independent, so this cannot leak a
	// container-ordering difference into anything the step reads.
	for (const FSubstanceId& Substance : CellsPrev)
	{
		if (Substance.IsValid())
		{
			Occupancy.FindOrAdd(Substance) += 1;
		}
	}
}
