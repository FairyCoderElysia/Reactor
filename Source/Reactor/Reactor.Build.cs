// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Reactor : ModuleRules
{
	public Reactor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// The settlement tables are authored as JSON so editing a rule costs no
			// recompile. FJsonObject / FJsonValue / the serializer all live in the
			// Json module, which is not pulled in by Core — without this the code
			// compiles and then fails at link with ~33 unresolved externals.
			"Json",
			"JsonUtilities",
		});

		// Without this the module root is not on the include path, and a
		// subdirectory-relative include such as "Simulation/GridSimulation.h"
		// does not resolve from a file inside Simulation/ — the compiler looks
		// for Simulation/Simulation/GridSimulation.h. Adding the module directory
		// makes the module-root-relative form work from anywhere in the module,
		// which is the form every file here uses so the layout stays obvious in
		// the source text.
		PublicIncludePaths.Add(ModuleDirectory);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
