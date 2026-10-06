// Reactor — Copyright (c) 2026. All Rights Reserved.

#include "Simulation/SubstanceTable.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogReactorSubstances, Log, All);

namespace
{
	/** Read a dotted path out of a parsed JSON object, returning false when absent or of the wrong type. */
	bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& OutValue)
	{
		return Object.IsValid() && Object->TryGetStringField(Field, OutValue);
	}

	FSubstanceId MakeSubstanceId(const FString& Raw)
	{
		// FName is case-insensitive and trims, which is what makes a hand-edited
		// JSON table forgiving about "Water" vs "water" in two places.
		FString Trimmed = Raw;
		Trimmed.TrimStartAndEndInline();
		return Trimmed.IsEmpty() ? FSubstanceId() : FSubstanceId(FName(*Trimmed));
	}
}

bool FSubstanceTable::LoadFromJsonFile(const FString& InPath, FString& OutError)
{
	const FString Resolved = FPaths::IsRelative(InPath) ? FPaths::Combine(FPaths::ProjectDir(), InPath) : InPath;

	if (!IFileManager::Get().FileExists(*Resolved))
	{
		OutError = FString::Printf(TEXT("Substance table not found at '%s'."), *Resolved);
		return false;
	}

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Resolved))
	{
		OutError = FString::Printf(TEXT("Substance table at '%s' could not be read."), *Resolved);
		return false;
	}

	if (!FromJsonString(Json, *this, OutError))
	{
		OutError = FString::Printf(TEXT("%s (file: '%s')"), *OutError, *Resolved);
		return false;
	}
	return true;
}

bool FSubstanceTable::FromJsonString(const FString& InJson, FSubstanceTable& OutTable, FString& OutError)
{
	OutTable.Data = FReactorSimulationData();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Substance table is not valid JSON.");
		return false;
	}

	// ---- params ------------------------------------------------------------
	{
		const TSharedPtr<FJsonObject>* ParamsObject = nullptr;
		if (Root->TryGetObjectField(TEXT("params"), ParamsObject) && ParamsObject != nullptr && ParamsObject->IsValid())
		{
			const TSharedPtr<FJsonObject>& Params = *ParamsObject;
			Params->TryGetNumberField(TEXT("gridWidth"), OutTable.Data.Params.GridWidth);
			Params->TryGetNumberField(TEXT("gridHeight"), OutTable.Data.Params.GridHeight);
			Params->TryGetNumberField(TEXT("fixedStepSeconds"), OutTable.Data.Params.FixedStepSeconds);
			Params->TryGetNumberField(TEXT("maxStepsPerFrame"), OutTable.Data.Params.MaxStepsPerFrame);
			Params->TryGetNumberField(TEXT("targetFramerate"), OutTable.Data.Params.TargetFramerate);
			Params->TryGetNumberField(TEXT("reactionRateScale"), OutTable.Data.Params.ReactionRateScale);
			Params->TryGetNumberField(TEXT("workMultiplier"), OutTable.Data.Params.WorkMultiplier);
		}
	}

	// ---- substances --------------------------------------------------------
	const TArray<TSharedPtr<FJsonValue>>* SubstanceArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("substances"), SubstanceArray) || SubstanceArray == nullptr)
	{
		OutError = TEXT("Substance table has no 'substances' array.");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *SubstanceArray)
	{
		const TSharedPtr<FJsonObject>* SubstanceObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(SubstanceObject) || !SubstanceObject->IsValid())
		{
			OutError = TEXT("A 'substances' entry is not an object.");
			return false;
		}

		FString RawId;
		if (!ReadString(*SubstanceObject, TEXT("id"), RawId))
		{
			OutError = TEXT("A 'substances' entry has no 'id'.");
			return false;
		}

		FSubstanceDef Def;
		Def.Id = MakeSubstanceId(RawId);
		if (!Def.Id.IsValid())
		{
			OutError = TEXT("A 'substances' entry has an empty 'id'.");
			return false;
		}

		FString DisplayName;
		if (ReadString(*SubstanceObject, TEXT("displayName"), DisplayName))
		{
			Def.DisplayName = FText::FromString(DisplayName);
		}
		else
		{
			// Fall back to the id so a missing label is a cosmetic gap, not a load failure.
			Def.DisplayName = FText::FromName(Def.Id.Name);
		}

		FString ColorHex;
		if (ReadString(*SubstanceObject, TEXT("color"), ColorHex))
		{
			Def.PlaceholderColor = FLinearColor::FromSRGBColor(FColor::FromHex(ColorHex));
		}

		// Absent means primitive. The default is the safe direction: an id that
		// forgot to declare itself never becomes a non-terminating recursion branch.
		bool bIsPrimitive = true;
		(*SubstanceObject)->TryGetBoolField(TEXT("bIsPrimitive"), bIsPrimitive);
		Def.bIsPrimitive = bIsPrimitive;

		OutTable.Data.Substances.Add(MoveTemp(Def));
	}

	// ---- reactions ---------------------------------------------------------
	const TArray<TSharedPtr<FJsonValue>>* ReactionArray = nullptr;
	if (Root->TryGetArrayField(TEXT("reactions"), ReactionArray) && ReactionArray != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ReactionArray)
		{
			const TSharedPtr<FJsonObject>* ReactionObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ReactionObject) || !ReactionObject->IsValid())
			{
				OutError = TEXT("A 'reactions' entry is not an object.");
				return false;
			}

			FString RawA;
			FString RawB;
			if (!ReadString(*ReactionObject, TEXT("a"), RawA) || !ReadString(*ReactionObject, TEXT("b"), RawB))
			{
				OutError = TEXT("A 'reactions' entry must name both 'a' and 'b'.");
				return false;
			}

			FReactionOutcome Outcome;
			FString RawResultA;
			if (ReadString(*ReactionObject, TEXT("resultA"), RawResultA))
			{
				Outcome.ResultA = MakeSubstanceId(RawResultA);
			}

			FString RawResultB;
			if (ReadString(*ReactionObject, TEXT("resultB"), RawResultB))
			{
				Outcome.ResultB = MakeSubstanceId(RawResultB);
				Outcome.bProduceB = Outcome.ResultB.IsValid();
			}

			(*ReactionObject)->TryGetBoolField(TEXT("produceB"), Outcome.bProduceB);

			const FReactionPair Key(MakeSubstanceId(RawA), MakeSubstanceId(RawB));
			if (!Key.A.IsValid() || !Key.B.IsValid())
			{
				OutError = TEXT("A 'reactions' entry has an empty 'a' or 'b'.");
				return false;
			}

			OutTable.Data.Reactions.Add(Key, Outcome);
		}
	}

	// Referential integrity is checked here rather than at use time so a typo
	// surfaces as one clear load failure instead of a silently inert rule.
	TArray<FString> Problems;
	OutTable.Validate(Problems);
	for (const FString& Problem : Problems)
	{
		if (Problem.Contains(TEXT("unknown substance")))
		{
			OutError = Problem;
			return false;
		}
	}

	return true;
}

const FSubstanceDef* FSubstanceTable::FindSubstance(const FSubstanceId& Id) const
{
	if (!Id.IsValid())
	{
		return nullptr;
	}
	return Data.Substances.FindByPredicate([&Id](const FSubstanceDef& Def) { return Def.Id == Id; });
}

bool FSubstanceTable::IsPrimitive(const FSubstanceId& Id) const
{
	const FSubstanceDef* Def = FindSubstance(Id);
	return Def != nullptr && Def->bIsPrimitive;
}

const FReactionOutcome* FSubstanceTable::FindReaction(const FSubstanceId& A, const FSubstanceId& B) const
{
	if (!A.IsValid() || !B.IsValid())
	{
		return nullptr;
	}
	return Data.Reactions.Find(FReactionPair(A, B));
}

void FSubstanceTable::GetMissingReactionPairs(TArray<FReactionPair>& OutMissing) const
{
	OutMissing.Reset();

	for (int32 A = 0; A < Data.Substances.Num(); ++A)
	{
		for (int32 B = A + 1; B < Data.Substances.Num(); ++B)
		{
			const FSubstanceId& IdA = Data.Substances[A].Id;
			const FSubstanceId& IdB = Data.Substances[B].Id;

			// A pair counts as covered when either direction has an entry: the
			// table is allowed to state only one direction and mean both.
			if (FindReaction(IdA, IdB) == nullptr && FindReaction(IdB, IdA) == nullptr)
			{
				OutMissing.Add(FReactionPair(IdA, IdB));
			}
		}
	}
}

void FSubstanceTable::Validate(TArray<FString>& OutProblems) const
{
	if (Data.Substances.Num() == 0)
	{
		OutProblems.Add(TEXT("Substance table is empty — the grid would have nothing to place."));
		return;
	}

	TSet<FName> SeenIds;
	int32 PrimitiveCount = 0;

	for (const FSubstanceDef& Def : Data.Substances)
	{
		if (!Def.Id.IsValid())
		{
			OutProblems.Add(TEXT("Substance table contains an entry with an invalid id."));
			continue;
		}

		if (SeenIds.Contains(Def.Id.Name))
		{
			OutProblems.Add(FString::Printf(TEXT("Substance '%s' is declared more than once."), *Def.Id.Name.ToString()));
			continue;
		}

		SeenIds.Add(Def.Id.Name);
		if (Def.bIsPrimitive)
		{
			++PrimitiveCount;
		}
	}

	if (PrimitiveCount == 0)
	{
		// Without a base case a recursive composition could never terminate, so
		// this is a structural error rather than a content preference.
		OutProblems.Add(TEXT("No substance is marked bIsPrimitive — recursion would have no base case."));
	}

	for (const TPair<FReactionPair, FReactionOutcome>& Entry : Data.Reactions)
	{
		auto CheckReferenced = [this, &OutProblems](const FSubstanceId& Id)
		{
			if (Id.IsValid() && !IsKnownSubstance(Id))
			{
				OutProblems.Add(FString::Printf(
					TEXT("Reaction references unknown substance '%s'."), *Id.Name.ToString()));
			}
		};

		CheckReferenced(Entry.Key.A);
		CheckReferenced(Entry.Key.B);
		CheckReferenced(Entry.Value.ResultA);
		CheckReferenced(Entry.Value.ResultB);
	}

	if (Data.Params.GridWidth <= 0 || Data.Params.GridHeight <= 0)
	{
		OutProblems.Add(FString::Printf(
			TEXT("Grid dimensions must be positive (got %dx%d)."),
			Data.Params.GridWidth, Data.Params.GridHeight));
	}

	if (Data.Params.FixedStepSeconds <= 0.0f)
	{
		OutProblems.Add(FString::Printf(
			TEXT("fixedStepSeconds must be positive (got %f)."), Data.Params.FixedStepSeconds));
	}
}
