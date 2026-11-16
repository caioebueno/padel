// APadelVenueLighting turns every Data/Lighting.json preset into the lights the native
// tests measured (same places, candela, cones, shadows) and a locked broadcast exposure;
// APadelLookDevStage assembles the parts Data/LookDev.json lists for the active venue.
#include "Misc/AutomationTest.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SpotLightComponent.h"
#include "Court/PadelCourtUnits.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Presentation/Model/PadelLightingModel.h"
#include "Presentation/PadelBroadcastCamera.h"
#include "Presentation/PadelLookDevStage.h"
#include "Presentation/PadelVenueLighting.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}

		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelLightingPresetsTest, "Padel.Presentation.Lighting.Presets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelLightingPresetsTest::RunTest(const FString& Parameters)
{
	using namespace Padel::Presentation::Lighting;
	FTestWorld Test;
	APadelVenueLighting* Lighting = Test.World->SpawnActor<APadelVenueLighting>();
	if (!TestNotNull(TEXT("Lighting spawned"), Lighting))
	{
		return false;
	}

	const TArray<FString> Names = Lighting->GetPresetNames();
	TestTrue(TEXT("Data/Lighting.json has the three presets"), Names.Contains(TEXT("IndoorClub"))
		&& Names.Contains(TEXT("OutdoorDay")) && Names.Contains(TEXT("OutdoorNight")));

	for (const FString& Name : Names)
	{
		TestTrue(FString::Printf(TEXT("%s applies"), *Name), Lighting->ApplyPreset(Name));
		TestEqual(TEXT("Active preset"), Lighting->GetActivePresetName(), Name);
		const FLightingPreset* Preset = Lighting->FindPreset(Name);
		if (!TestNotNull(TEXT("Preset found"), Preset))
		{
			continue;
		}

		const std::vector<FFixture> Fixtures = BuildFixtures(*Preset);
		const TArray<TObjectPtr<USpotLightComponent>>& Lights = Lighting->GetFloodlights();
		TestEqual(FString::Printf(TEXT("%s floodlights"), *Name), Lights.Num(),
			Preset->bFloodlightsOn ? static_cast<int32>(Fixtures.size()) : 0);
		int32 Shadowed = 0;
		for (int32 Index = 0; Index < Lights.Num() && Index < static_cast<int32>(Fixtures.size()); ++Index)
		{
			const USpotLightComponent* Light = Lights[Index];
			const FFixture& F = Fixtures[Index];
			const FVector Expected = Padel::MetresToWorld(FVector(F.Position.X, F.Position.Y, F.Position.Z));
			// Within the 5 cm the light sits in front of the lens.
			TestTrue(FString::Printf(TEXT("%s light %d position"), *Name, Index),
				Light->GetComponentLocation().Equals(Expected, 6.0));
			TestTrue(FString::Printf(TEXT("%s light %d aim"), *Name, Index),
				Light->GetForwardVector().Equals(FVector(F.Direction.X, F.Direction.Y, F.Direction.Z), 1e-3));
			TestTrue(TEXT("Candela units"), Light->IntensityUnits == ELightUnits::Candelas);
			TestEqual(TEXT("Intensity"), Light->Intensity, static_cast<float>(F.IntensityCd));
			TestEqual(TEXT("Outer cone"), Light->OuterConeAngle, static_cast<float>(F.OuterConeDeg));
			TestTrue(TEXT("Movable"), Light->Mobility == EComponentMobility::Movable);
			Shadowed += Light->CastShadows ? 1 : 0;
		}
		if (Preset->bFloodlightsOn)
		{
			TestTrue(FString::Printf(TEXT("%s shadow budget"), *Name), Shadowed >= 1 && Shadowed <= Preset->MaxShadowedFixtures);
		}

		TestTrue(FString::Printf(TEXT("%s sun"), *Name), Lighting->GetSun()->IsVisible() == Preset->Sun.bEnabled);
		const FPostProcessSettings& Post = Lighting->GetPostProcess()->Settings;
		TestTrue(TEXT("Post-process is unbound"), Lighting->GetPostProcess()->bUnbound);
		TestTrue(TEXT("Manual exposure"), Post.AutoExposureMethod == EAutoExposureMethod::AEM_Manual);
		TestTrue(TEXT("Physical camera exposure"), Post.AutoExposureApplyPhysicalCameraExposure != 0);
		const double EV100 = FMath::Log2(Post.DepthOfFieldFstop * Post.DepthOfFieldFstop * Post.CameraShutterSpeed)
			- FMath::Log2(Post.CameraISO / 100.0);
		TestTrue(FString::Printf(TEXT("%s exposure is EV100 %.1f"), *Name, Preset->Post.ExposureEV100),
			FMath::IsNearlyEqual(EV100, Preset->Post.ExposureEV100, 0.01));
		TestEqual(TEXT("White balance"), Post.WhiteTemp, static_cast<float>(Preset->Post.WhiteBalanceK));
	}

	// An unknown name keeps the current preset.
	const FString Before = Lighting->GetActivePresetName();
	TestFalse(TEXT("Unknown preset is refused"), Lighting->ApplyPreset(TEXT("NoSuchPreset")));
	TestEqual(TEXT("Preset unchanged"), Lighting->GetActivePresetName(), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelLookDevStageTest, "Padel.Presentation.LookDev.Stage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelLookDevStageTest::RunTest(const FString& Parameters)
{
	using Padel::Presentation::Lighting::EVenue;
	TArray<FPadelLookDevPart> Parts;
	TArray<FString> Fallback;
	TArray<FPadelLookDevCamera> Cameras;
	if (!TestTrue(TEXT("Data/LookDev.json loads"), APadelLookDevStage::LoadConfig(Parts, Fallback, Cameras)))
	{
		return false;
	}

	// All five court-art parts are listed, each with an owner.
	for (const TCHAR* Part : { TEXT("Surface"), TEXT("Glass"), TEXT("MeshAndFrame"), TEXT("Net") })
	{
		TestTrue(FString::Printf(TEXT("%s is a part"), Part), Parts.ContainsByPredicate(
			[Part](const FPadelLookDevPart& P) { return P.Part == Part && P.Group == TEXT("Court") && !P.Owner.IsEmpty(); }));
	}

	TestTrue(TEXT("Any fits indoors"), APadelLookDevStage::PartFitsVenue(TEXT("Any"), EVenue::Indoor));
	TestTrue(TEXT("Indoor fits indoors"), APadelLookDevStage::PartFitsVenue(TEXT("Indoor"), EVenue::Indoor));
	TestFalse(TEXT("Indoor doesn't fit outdoors"), APadelLookDevStage::PartFitsVenue(TEXT("Indoor"), EVenue::Outdoor));

	// The broadcast bookmark is the match camera's pose (GDD 9).
	const FPadelLookDevCamera* Broadcast = Cameras.FindByPredicate([](const FPadelLookDevCamera& C) { return C.Name == TEXT("Broadcast"); });
	if (TestNotNull(TEXT("Broadcast camera bookmark"), Broadcast))
	{
		const FVector Match = Padel::WorldToMetres(APadelBroadcastCamera::GetDefaultTransform().GetLocation());
		TestTrue(TEXT("Bookmark matches the broadcast camera"), Broadcast->LocationM.Equals(Match, 0.01));
	}

	FTestWorld Test;
	APadelLookDevStage* Stage = Test.World->SpawnActor<APadelLookDevStage>();
	if (!TestNotNull(TEXT("Stage spawned"), Stage))
	{
		return false;
	}
	for (const EVenue Venue : { EVenue::Indoor, EVenue::Outdoor })
	{
		Stage->Assemble(Venue);
		int32 Expected = 0;
		int32 CourtPlaced = 0;
		for (const FPadelLookDevPart& Part : Parts)
		{
			if (APadelLookDevStage::PartFitsVenue(Part.Venue, Venue))
			{
				Expected += Part.Meshes.Num();
			}
		}
		for (const FPadelLookDevPlacement& Placement : Stage->GetPlacements())
		{
			const FPadelLookDevPart* Part = Parts.FindByPredicate([&Placement](const FPadelLookDevPart& P) { return P.Part == Placement.Part; });
			TestTrue(FString::Printf(TEXT("%s belongs here"), *Placement.Mesh), Placement.Part == TEXT("CourtFallback")
				|| (Part && APadelLookDevStage::PartFitsVenue(Part->Venue, Venue)));
			CourtPlaced += Placement.bPlaced && (Placement.Part == TEXT("CourtFallback") || (Part && Part->Group == TEXT("Court"))) ? 1 : 0;
		}
		TestTrue(TEXT("Every listed mesh was tried"), Stage->GetPlacements().Num() >= Expected);
		// Whatever is imported on this machine, there is always a court to look at.
		TestTrue(TEXT("A court is on stage"), CourtPlaced > 0 || Stage->IsShowingPlaceholderCourt());
		TestEqual(TEXT("Placed components match placements"), Stage->GetPlacedComponents().Num(),
			Stage->GetPlacements().FilterByPredicate([](const FPadelLookDevPlacement& P) { return P.bPlaced; }).Num());
	}
	return true;
}

#endif
