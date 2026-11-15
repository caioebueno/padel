// The spawned APadelCourt matches PadelCore's layout: one component per box, at the
// right place and size in world units, with the right collision and surface tags.
#include "Misc/AutomationTest.h"

#include "Components/StaticMeshComponent.h"
#include "Court/PadelCourt.h"
#include "Court/PadelCourtUnits.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelCourtActorTest, "Padel.Court.Actor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelCourtActorTest::RunTest(const FString& Parameters)
{
	using namespace Padel::Court;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);

	APadelCourt* Court = World->SpawnActor<APadelCourt>();
	if (!TestNotNull(TEXT("Court spawned"), Court))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	const std::vector<FCourtBox>& Layout = BuildCourtLayout();
	const TArray<TObjectPtr<UStaticMeshComponent>>& Components = Court->GetBoxComponents();
	TestEqual(TEXT("One component per layout box"), Components.Num(), static_cast<int32>(Layout.size()));

	const int32 Count = FMath::Min(Components.Num(), static_cast<int32>(Layout.size()));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FCourtBox& Box = Layout[Index];
		const UStaticMeshComponent* Component = Components[Index];
		const FString Name = UTF8_TO_TCHAR(Box.Name.c_str());

		const FVector ExpectedCentre = Padel::MetresToWorld(FVector(Box.Centre.X, Box.Centre.Y, Box.Centre.Z));
		const FVector ExpectedSize = Padel::MetresToWorld(FVector(Box.Size.X, Box.Size.Y, Box.Size.Z));
		const FBox Bounds = Component->Bounds.GetBox();
		TestTrue(FString::Printf(TEXT("%s centre"), *Name), Bounds.GetCenter().Equals(ExpectedCentre, 0.5));
		TestTrue(FString::Printf(TEXT("%s size"), *Name), Bounds.GetSize().Equals(ExpectedSize, 0.5));

		const bool bBlocks = Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		TestTrue(FString::Printf(TEXT("%s collides"), *Name), bBlocks == Box.bCollides);
		TestTrue(FString::Printf(TEXT("%s surface tag"), *Name), APadelCourt::GetSurface(Component) == Box.Surface);
		TestTrue(FString::Printf(TEXT("%s visible"), *Name), Component->IsVisible() == Box.bVisible);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
