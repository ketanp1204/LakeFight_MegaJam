// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LakeFight : ModuleRules
{
	public LakeFight(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"LakeFight",
			"LakeFight/Variant_Platforming",
			"LakeFight/Variant_Platforming/Animation",
			"LakeFight/Variant_Combat",
			"LakeFight/Variant_Combat/AI",
			"LakeFight/Variant_Combat/Animation",
			"LakeFight/Variant_Combat/Gameplay",
			"LakeFight/Variant_Combat/Interfaces",
			"LakeFight/Variant_Combat/UI",
			"LakeFight/Variant_SideScrolling",
			"LakeFight/Variant_SideScrolling/AI",
			"LakeFight/Variant_SideScrolling/Gameplay",
			"LakeFight/Variant_SideScrolling/Interfaces",
			"LakeFight/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
