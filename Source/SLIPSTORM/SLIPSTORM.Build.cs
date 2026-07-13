// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7

using UnrealBuildTool;

public class SLIPSTORM : ModuleRules
{
    public SLIPSTORM(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Add private dependencies as needed
        });

        // Add the module root as a public include path so intra-module includes like
        // #include "Player/EPlayerLane.h" resolve against Source/SLIPSTORM/. Required
        // because this project uses the flat module layout (no Public/Private/Classes
        // subfolders) — UBT auto-adds those when present, but not the module root.
        PublicIncludePaths.Add(ModuleDirectory);

        // UnrealEd is required for FAutomationEditorCommonUtils::CreateNewMap()
        // used in PMLifecycleAndSeamTest.cpp (Story 001a).
        // Gated to editor builds only — never links into Shipping or Game builds.
        // Note: this also fixes a latent Story 001 link error; AutomationEditorCommon.h
        // was already #included in the original test file without this module dep.
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
