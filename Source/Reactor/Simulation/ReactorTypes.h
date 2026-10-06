// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReactorTypes.generated.h"

/**
 * Stable identifier for a substance.
 *
 * Wraps FName rather than using FName directly so the type can be changed later
 * (FName -> soft object path, data asset id, ...) without touching every call
 * site. A primitive substance is the recursion base case described in
 * design/composition-model.md: it cannot be decomposed any further, which is
 * what lets a recursive composition terminate.
 */
USTRUCT(BlueprintType)
struct REACTOR_API FSubstanceId
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	FName Name;

	FSubstanceId() = default;
	explicit FSubstanceId(FName InName) : Name(InName) {}

	bool IsValid() const { return !Name.IsNone(); }
	bool operator==(const FSubstanceId& Other) const { return Name == Other.Name; }
	bool operator!=(const FSubstanceId& Other) const { return !(*this == Other); }
};

FORCEINLINE uint32 GetTypeHash(const FSubstanceId& Id)
{
	return GetTypeHash(Id.Name);
}

/**
 * One row of the substance table.
 *
 * bIsPrimitive is the recursion base case. It is present from the first grid
 * story onward even though nothing reads it yet: adding it later would change
 * the data format and invalidate every already-saved substance table.
 */
USTRUCT(BlueprintType)
struct REACTOR_API FSubstanceDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	FSubstanceId Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	FText DisplayName;

	/** Placeholder colour only — real art is explicitly out of scope for the prototype. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	FLinearColor PlaceholderColor = FLinearColor::White;

	/** True means this substance cannot be decomposed further — the recursion base case. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	bool bIsPrimitive = true;
};

/**
 * Ordered pair of substances, used as the reaction-table key.
 *
 * The pair is deliberately NOT normalised. A reaction table entry is written in
 * the order the player's rule states it, and "A then B" differing from "B then
 * A" is a legitimate rule, not a duplicate. Normalising here would silently
 * collapse those into one entry and make the difference unexpressible.
 *
 * Plain (non-USTRUCT) C++ type on purpose: it is a map key used only inside the
 * simulation, never serialised on its own and never exposed to the editor. The
 * JSON loader builds it from the table's ordered pair of names. Keeping it out
 * of UHT's reach avoids carrying reflection metadata nothing reads.
 */
struct REACTOR_API FReactionPair
{
	FSubstanceId A;
	FSubstanceId B;

	FReactionPair() = default;
	FReactionPair(const FSubstanceId& InA, const FSubstanceId& InB) : A(InA), B(InB) {}

	bool operator==(const FReactionPair& Other) const { return A == Other.A && B == Other.B; }
	bool operator!=(const FReactionPair& Other) const { return !(*this == Other); }
};

FORCEINLINE uint32 GetTypeHash(const FReactionPair& Pair)
{
	uint32 Hash = GetTypeHash(Pair.A);
	Hash = HashCombine(Hash, GetTypeHash(Pair.B));
	return Hash;
}

/**
 * Result of one reaction: what A+B turns into.
 *
 * Both outputs are optional. Leaving B empty means the pair collapses to a
 * single cell, which is how "consume" behaviour is expressed without a second
 * concept. ResultId empty with bDestroyA set means the pair annihilates.
 */
USTRUCT(BlueprintType)
struct REACTOR_API FReactionOutcome
{
	GENERATED_BODY()

	/** What A becomes. Invalid means A is destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	FSubstanceId ResultA;

	/** What B becomes. Invalid means B is destroyed (see bProduceB). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	FSubstanceId ResultB;

	/** When false the reaction is a merge: B's cell is emptied and ResultA only is written. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	bool bProduceB = false;
};

/**
 * Tunables for the settlement loop.
 *
 * Every value here is a config/asset value, never a hardcoded gameplay constant
 * — the project's architecture note (production/handoff-2026-10-05.md) makes
 * data-driven rules a hard requirement so that iterating on rules does not cost
 * a recompile.
 */
USTRUCT(BlueprintType)
struct REACTOR_API FSimulationParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	int32 GridWidth = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	int32 GridHeight = 256;

	/** Fixed settlement step. The sim advances in whole steps of this size, never per render frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	float FixedStepSeconds = 1.0f / 60.0f;

	/** Upper bound on catch-up steps per Advance() call, so a long hitch cannot spiral. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	int32 MaxStepsPerFrame = 8;

	/**
	 * The frame rate this project commits to, and therefore the budget every
	 * measurement is reported against.
	 *
	 * Here rather than hardcoded in the benchmark tool because the tool's whole
	 * output is "how much of the frame did this cost" — a literal 60 in that
	 * arithmetic goes stale the moment the target changes, and reports a healthy
	 * headroom against a budget nobody committed to. It is also not free of
	 * consequence: it is the same number as `performance.target_framerate` in
	 * project.yaml, and a drift between the two makes the report lie.
	 *
	 * Kept as a float so a 144Hz or 30fps target needs no code change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	float TargetFramerate = 60.0f;

	/** Multiplier on the reaction probability. 1.0 = reactions always fire when a pair matches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	float ReactionRateScale = 1.0f;

	/**
	 * Runtime-only stress knob, used to fill the performance budget.
	 * 0 disables it. When greater than 0 the grid is subdivided horizontally into
	 * this many columns and each column runs one extra full settlement pass,
	 * which lets the measured per-tick cost be extrapolated to larger grids
	 * (e.g. 512x512 = 4x the cells of 256x256) without allocating a bigger
	 * buffer. This is NOT a gameplay value and must stay 0 in a shipped config.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactor")
	int32 WorkMultiplier = 0;
};

/** Observable grid state for one cell. */
UENUM(BlueprintType)
enum class ECellChange : uint8
{
	/** The cell was already this substance and nothing touched it. */
	Unchanged = 0,
	/** The cell holds a different substance than it did last step. */
	Changed = 1,
	/** The cell matched a reaction rule this step but its substance is unchanged (e.g. a catalyst). */
	Reacted = 2
};
/**
 * Snapshot of the grid after a step.
 *
 * Occupancy is the only cross-cell view the loop needs; it exists so that the
 * "is anything happening at all" question — the whole point of the build-order
 * step-1 prototype — is answerable without scanning the grid again.
 */
USTRUCT(BlueprintType)
struct REACTOR_API FSimulationState
{
	GENERATED_BODY()

	/** Substance per cell, row-major, length = Width * Height. */
	UPROPERTY(BlueprintReadOnly, Category = "Reactor")
	TArray<FSubstanceId> Cells;

	/** Parallel to Cells, length = Width * Height. */
	UPROPERTY(BlueprintReadOnly, Category = "Reactor")
	TArray<ECellChange> Changes;

	UPROPERTY(BlueprintReadOnly, Category = "Reactor")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Reactor")
	int32 Height = 0;

	/** Count of cells per substance id after the step. */
	UPROPERTY(BlueprintReadOnly, Category = "Reactor")
	TMap<FSubstanceId, int32> Occupancy;

	/** Number of cells that changed, summed over the last step. */
	UPROPERTY(BlueprintReadOnly, Category = "Reactor")
	int32 ChangedCellCount = 0;

	/** Number of cell pairs that matched a reaction rule during the last step. */
	UPROPERTY(BlueprintReadOnly, Category = "Reactor")
	int32 ReactionCount = 0;

	int32 Num() const { return Width * Height; }

	bool IsValidIndex(int32 X, int32 Y) const
	{
		return X >= 0 && Y >= 0 && X < Width && Y < Height;
	}

	int32 IndexOf(int32 X, int32 Y) const { return Y * Width + X; }

	FSubstanceId GetAt(int32 X, int32 Y) const
	{
		return IsValidIndex(X, Y) ? Cells[IndexOf(X, Y)] : FSubstanceId();
	}

	/** Empty cells are represented by an invalid substance id, never by a null entry in Cells. */
	static FSubstanceId EmptyCell() { return FSubstanceId(); }
};

/**
 * Everything needed to build a simulation, as plain run-time data.
 *
 * Deliberately NOT a USTRUCT/UCLASS. Its reaction map is keyed by FReactionPair,
 * which is a plain C++ type — a TMap with a non-USTRUCT key cannot appear in a
 * reflected UPROPERTY, and UHT rejects it. Reflection is genuinely not wanted
 * here: the editor never authors this struct directly, it is built by the JSON
 * loader from Content/Data/Reactor/DefaultSubstances.json.
 *
 * DESIGN-CRITICAL: the ordered pair is preserved as the key (see FReactionPair).
 */
struct REACTOR_API FReactorSimulationData
{
	TArray<FSubstanceDef> Substances;

	/** Keyed by ordered pair. See FReactionPair for why the order is preserved. */
	TMap<FReactionPair, FReactionOutcome> Reactions;

	FSimulationParams Params;

	TMap<FSubstanceId, FSubstanceId> BuildReactionIndex() const
	{
		TMap<FSubstanceId, FSubstanceId> Index;
		for (const TPair<FReactionPair, FReactionOutcome>& Pair : Reactions)
		{
			if (Pair.Value.ResultA.IsValid())
			{
				Index.Add(Pair.Key.A, Pair.Value.ResultA);
			}
		}
		return Index;
	}

	int32 Num() const { return Substances.Num(); }
};
