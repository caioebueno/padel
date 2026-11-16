// UPadelNetReactionSubsystem inside the engine build: a net bounce from UPadelMatchEvents
// reaches the model, other surfaces don't, and Data/Net.json loads. The reaction's shape
// is tested in the native suite (Tests/Native/UI/NetReactionTests.cpp).
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#include "Presentation/PadelMatchEvents.h"
#include "Presentation/PadelNetReactionSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelNetReactionTest, "Padel.Presentation.NetReaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelNetReactionTest::RunTest(const FString& Parameters)
{
	using Padel::Court::EHalf;
	using Padel::Court::ESurface;

	const Padel::Presentation::FNetReactionSettings Loaded = UPadelNetReactionSubsystem::LoadSettings();
	TestEqual(TEXT("Data/Net.json loads"), Loaded.DurationSeconds, Padel::Presentation::FNetReactionSettings().DurationSeconds);

	UPadelMatchEvents* Events = NewObject<UPadelMatchEvents>(GetTransientPackage());
	UPadelNetReactionSubsystem* Net = NewObject<UPadelNetReactionSubsystem>(GetTransientPackage());
	Net->BindTo(*Events);

	Events->OnBallBounced.Broadcast(ESurface::Ground, EHalf::A, FVector(300.0, 0.0, 3.25));
	TestFalse(TEXT("A ground bounce leaves the net alone"), Net->GetModel().IsActive());

	Events->OnBallHit.Broadcast(0, Padel::EShotType::Drive, Padel::ETimingGrade::Perfect, 20.0, FVector::ZeroVector);
	// Into the net from the +X half: the simulation reports it a ball radius on that side.
	Events->OnBallBounced.Broadcast(ESurface::Net, EHalf::A, FVector(3.25, 120.0, 45.0));
	TestTrue(TEXT("A net bounce moves the net"), Net->GetModel().IsActive());
	const Padel::Presentation::FNetImpact& Impact = Net->GetModel().GetImpacts()[0];
	TestEqual(TEXT("Hit position across the net, metres"), Impact.Y, 1.2, 1e-6);
	TestTrue(TEXT("The net gives way toward -X"), Impact.Amplitude < 0.0);

	// No world and no collection here: Advance still steps the model safely.
	Net->Advance(Net->GetModel().GetSettings().DurationSeconds + 0.1);
	TestFalse(TEXT("The net settles"), Net->GetModel().IsActive());

	Net->Unbind();
	Events->OnBallBounced.Broadcast(ESurface::Net, EHalf::A, FVector(3.25, 0.0, 45.0));
	TestFalse(TEXT("Nothing arrives after Unbind"), Net->GetModel().IsActive());
	return true;
}

#endif
