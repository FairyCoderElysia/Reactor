// Reactor — Copyright (c) 2026. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Simulation/GridSimulation.h"
#include "Simulation/ReactorSimulation.h"
#include "Simulation/SubstanceTable.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Determinism of the settlement kernel.
 *
 * Story 001 carries "same input twice -> identical result" as an acceptance
 * criterion, and at `qa.level: minimal` no test file was originally written for it.
 * This exists because that criterion is the load-bearing one: design/game-brief.md
 * requires a puzzle to be saved self-contained and re-runnable, and a puzzle that
 * re-settles differently on the same machine is not a puzzle. Reasoning in prose
 * about "no shared RNG, fixed probe order" is not verification, so it is checked here.
 *
 * Deliberately engine-free apart from the test scaffolding, and fed from an
 * in-memory JSON string rather than Content/. A test that read the project's live
 * data file would start failing for the wrong reason the first time a designer
 * tunes a number.
 *
 * Naming follows the engine convention `<Project>.<System>.<Scenario>`; the
 * `Reactor.` substring filter in `commands.test` is what picks these up.
 */

namespace
{
	/** A small self-contained table. 8x8 keeps a whole run in cache and still exercises every code path. */
	const TCHAR* GDeterminismTestJson = TEXT(R"JSON(
	{
		"params": {
			"gridWidth": 8,
			"gridHeight": 8,
			"fixedStepSeconds": 0.0166667,
			"maxStepsPerFrame": 8,
			"reactionRateScale": 1.0,
			"workMultiplier": 0
		},
		"substances": [
			{ "id": "Water", "bIsPrimitive": true },
			{ "id": "Stone", "bIsPrimitive": true },
			{ "id": "Fire",  "bIsPrimitive": true },
			{ "id": "Steam", "bIsPrimitive": false },
			{ "id": "Lava",  "bIsPrimitive": false }
		],
		"reactions": [
			{ "a": "Water", "b": "Fire",  "resultA": "Steam", "produceB": false },
			{ "a": "Fire",  "b": "Water", "resultA": "Steam", "produceB": false },
			{ "a": "Stone", "b": "Fire",  "resultA": "Lava",  "produceB": false },
			{ "a": "Fire",  "b": "Stone", "resultA": "Lava",  "produceB": false },
			{ "a": "Water", "b": "Stone", "resultA": "Water", "produceB": false },
			{ "a": "Stone", "b": "Water", "resultA": "Stone", "produceB": false }
		]
	})JSON");

	/**
	 * Build a table from the fixture and seed a grid on it.
	 *
	 * Returns the table by out-parameter because FGridSimulation holds a pointer to
	 * it; the caller must keep that table alive for as long as the grid. Handing
	 * back a whole FReactorSimulation instead would not work — it owns its table by
	 * value, which makes it non-assignable, so it cannot be returned through an
	 * out-parameter on an existing object.
	 */
	bool MakeSeededGrid(int32 Seed, FSubstanceTable& OutTable, FGridSimulation& OutGrid, FAutomationTestBase& Test)
	{
		FString Error;
		if (!FSubstanceTable::FromJsonString(GDeterminismTestJson, OutTable, Error))
		{
			Test.AddError(FString::Printf(TEXT("Fixture table failed to load: %s"), *Error));
			return false;
		}

		TArray<FString> Problems;
		OutTable.Validate(Problems);
		if (Problems.Num() > 0)
		{
			Test.AddError(FString::Printf(TEXT("Fixture table failed validation: %s"),
				*FString::Join(Problems, TEXT(" | "))));
			return false;
		}

		OutGrid.Initialize(OutTable, OutTable.GetParams());
		OutGrid.FillRandom(Seed, 0.6f);
		return true;
	}

	/** Snapshot the kernel's observable state so two runs can be compared as values, not pointers. */
	struct FSnapshot
	{
		TArray<FSubstanceId> Cells;
		uint64 StepIndex = 0;
		int32 ReactionCount = 0;
		uint32 Digest = 0;
	};

	FSnapshot Capture(const FGridSimulation& Grid)
	{
		FSnapshot Result;
		Result.StepIndex = Grid.GetStepIndex();
		Result.ReactionCount = Grid.GetReactionCount();

		uint32 Digest = 0;
		for (int32 Y = 0; Y < Grid.GetHeight(); ++Y)
		{
			for (int32 X = 0; X < Grid.GetWidth(); ++X)
			{
				const FSubstanceId Cell = Grid.GetCell(X, Y);
				Result.Cells.Add(Cell);
				Digest = HashCombine(Digest, GetTypeHash(Cell));
			}
		}
		Result.Digest = Digest;
		return Result;
	}

	/** Compare two snapshots cell by cell, reporting the first divergence with its index and both digests. */
	bool ReportCellsEqual(FAutomationTestBase& Test, const FSnapshot& A, const FSnapshot& B, const FString& What)
	{
		if (A.Cells.Num() != B.Cells.Num())
		{
			Test.AddError(FString::Printf(TEXT("%s: cell counts differ (%d vs %d)."), *What, A.Cells.Num(), B.Cells.Num()));
			return false;
		}

		for (int32 Index = 0; Index < A.Cells.Num(); ++Index)
		{
			if (!(A.Cells[Index] == B.Cells[Index]))
			{
				Test.AddError(FString::Printf(
					TEXT("%s: first divergence at index %d ('%s' vs '%s'); digest %u vs %u."),
					*What,
					Index,
					*A.Cells[Index].Name.ToString(),
					*B.Cells[Index].Name.ToString(),
					A.Digest,
					B.Digest));
				return false;
			}
		}
		return true;
	}
}

/**
 * The acceptance criterion itself: same seed and same step count must produce a
 * cell-for-cell identical grid and an identical reaction count.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReactorDeterminismRepeatedRunsMatch,
	"Reactor.Simulation.Determinism.RepeatedRunsMatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FReactorDeterminismRepeatedRunsMatch::RunTest(const FString& Parameters)
{
	constexpr int32 Seed = 20261006;
	constexpr int32 Steps = 24;

	FSubstanceTable TableA;
	FSubstanceTable TableB;
	FGridSimulation GridA;
	FGridSimulation GridB;

	if (!MakeSeededGrid(Seed, TableA, GridA, *this) || !MakeSeededGrid(Seed, TableB, GridB, *this))
	{
		return false;
	}

	// Compare after every step rather than only at the end: a kernel that diverges
	// and later happens to re-converge would pass a final-only check.
	int32 TotalReactions = 0;
	for (int32 Step = 1; Step <= Steps; ++Step)
	{
		GridA.Step();
		GridB.Step();

		const FSnapshot SnapshotA = Capture(GridA);
		const FSnapshot SnapshotB = Capture(GridB);

		if (!ReportCellsEqual(*this, SnapshotA, SnapshotB, FString::Printf(TEXT("Step %d"), Step)))
		{
			return false;
		}

		TestEqual(FString::Printf(TEXT("Step %d: step index"), Step), SnapshotA.StepIndex, SnapshotB.StepIndex);
		TestEqual(FString::Printf(TEXT("Step %d: reaction count"), Step), SnapshotA.ReactionCount, SnapshotB.ReactionCount);
		TotalReactions += SnapshotA.ReactionCount;
	}

	// A kernel that never moved would satisfy determinism vacuously. The whole point
	// of this loop is that the world does something, twice, the same way.
	TestTrue(TEXT("The run actually settled something (determinism would otherwise be vacuous)"),
		TotalReactions > 0 && GridA.GetStepIndex() == Steps);

	return true;
}

/**
 * Two different seeds must NOT agree. Without this, a kernel that ignored the seed
 * — or one that returned a constant grid — would pass the test above.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReactorDeterminismDifferentSeedsDiffer,
	"Reactor.Simulation.Determinism.DifferentSeedsDiffer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FReactorDeterminismDifferentSeedsDiffer::RunTest(const FString& Parameters)
{
	FSubstanceTable TableA;
	FSubstanceTable TableB;
	FGridSimulation GridA;
	FGridSimulation GridB;

	if (!MakeSeededGrid(20261006, TableA, GridA, *this) || !MakeSeededGrid(19700101, TableB, GridB, *this))
	{
		return false;
	}

	// Compared before stepping, not after: a merge-only table settles to the same
	// equilibrium from any seed, so comparing two converged states would prove
	// nothing about whether the seed was used at all.
	TestNotEqual(TEXT("Two different seeds must produce different initial grids"),
		Capture(GridA).Digest, Capture(GridB).Digest);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
