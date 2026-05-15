// Copyright 2025 Onur Altuntaş. All Rights Reserved.


using UnrealBuildTool;

public class NexxNote : ModuleRules
{
        public NexxNote(ReadOnlyTargetRules Target) : base(Target)
        {
                PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

                PublicIncludePaths.AddRange(
                        new string[] {
                                // ... add public include paths required here ...
                        }
                        );


                PrivateIncludePaths.AddRange(
                        new string[] {
                                // Editor özel header dosyalarına erişim için
                                System.IO.Path.Combine(EngineDirectory, "Source/Editor"),
                                System.IO.Path.Combine(EngineDirectory, "Source/Editor/UnrealEd/Public"),
                                // Engine sınıflarına erişim için
                                System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Engine/Classes"),
                                // ... add other private include paths required here ...
                        }
                        );


                PublicDependencyModuleNames.AddRange(
                        new string[]
                        {
                                "Core",
                                "CoreUObject",
                                "Engine",
                                "InputCore",
                                "Slate",
                                "SlateCore",
                                "UnrealEd",
                                "BlueprintGraph",
                                "Kismet",
                                "KismetCompiler", // Comment node işlemleri için gerekli
                                "GraphEditor",
                                "KismetWidgets", // For UEdGraphNode_Comment
                                "EditorWidgets",
                                "UMG",
                                "ToolMenus",
                                "PropertyEditor",
                                "EditorSubsystem",
                                "AssetTools",
                                "ApplicationCore",
                                "AssetRegistry",
                                "EditorFramework",
                                // ... add other public dependencies that you statically link with here ...
                        }
                        );


                PrivateDependencyModuleNames.AddRange(
                        new string[]
                        {
                                "CoreUObject",
                                "Engine",
                                "Slate",
                                "SlateCore",
                                "ToolMenus",
                                "ApplicationCore",
                                "InputCore",
                                "Projects",
                                "EditorFramework",
                                "AppFramework",
                                "LevelEditor",
                                "DesktopPlatform",
                                "UnrealEd" // EdGraphNode_Comment için eklendi
                        }
                        );


                DynamicallyLoadedModuleNames.AddRange(
                        new string[]
                        {
                                // ... add any modules that your module loads dynamically here ...
                        }
                        );
        }
}
