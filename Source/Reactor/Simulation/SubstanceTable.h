// Reactor — Copyright (c) 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ReactorTypes.h"

/**
 * Owns the substance table and the pairwise reaction table.
 *
 * Table 1 of the two tables the settlement loop reads. Loaded from JSON at
 * run time so that editing a reaction does not require recompiling the module —
 * see the data-driven architecture note in production/handoff-2026-10-05.md.
 *
 * This class is pure data plus lookups. It performs no simulation.
 */
class REACTOR_API FSubstanceTable
{
public:
	FSubstanceTable() = default;

	/**
	 * Load the table from a JSON document.
	 *
	 * Accepts either an absolute filesystem path or a project-relative path
	 * rooted at the project directory. Returns false when the file is missing or
	 * malformed; OutError carries the reason and is safe to log.
	 */
	bool LoadFromJsonFile(const FString& InPath, FString& OutError);

	/** Build a table directly from already-parsed data — the path automated tests use. */
	static bool FromJsonString(const FString& InJson, FSubstanceTable& OutTable, FString& OutError);

	const FReactorSimulationData& GetData() const { return Data; }
	const FSimulationParams& GetParams() const { return Data.Params; }

	/** Look up one substance definition. Returns nullptr when the id is unknown. */
	const FSubstanceDef* FindSubstance(const FSubstanceId& Id) const;

	/** True when the id appears in the substance table. */
	bool IsKnownSubstance(const FSubstanceId& Id) const { return FindSubstance(Id) != nullptr; }

	/**
	 * True when the substance is a recursion base case.
	 * An unknown id is NOT primitive: callers must not treat a typo as a valid leaf.
	 */
	bool IsPrimitive(const FSubstanceId& Id) const;

	/**
	 * Look up the outcome of the ordered pair (A, B).
	 *
	 * Order matters — see FReactionPair. Returns nullptr when the pair has no entry.
	 */
	const FReactionOutcome* FindReaction(const FSubstanceId& A, const FSubstanceId& B) const;

	const TArray<FSubstanceDef>& GetSubstances() const { return Data.Substances; }
	int32 NumReactions() const { return Data.Reactions.Num(); }

	/**
	 * Every ordered pair of distinct substances, which the default JSON must cover.
	 * Keys are written sorted for a stable report; this does not affect lookup.
	 */
	void GetMissingReactionPairs(TArray<FReactionPair>& OutMissing) const;

	/**
	 * Structural invariants the table must hold to be usable.
	 *
	 * Checks: at least one substance; every id valid and unique; at least one
	 * primitive substance (without one there is no recursion base case, so a
	 * recursive composition could never terminate); grid dimensions positive.
	 * Appends one human-readable line per violation.
	 */
	void Validate(TArray<FString>& OutProblems) const;

private:
	FReactorSimulationData Data;
};
