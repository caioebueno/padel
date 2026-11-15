// Runs a smoke check of the court layout inside Unreal's Automation framework, so the
// engine build is known to see the same geometry as the native suite (Tests/Native),
// which holds the full set of court tests.
#include "Misc/AutomationTest.h"
#include "CourtLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelCourtSpecSmokeTest, "Padel.Court.SpecSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelCourtSpecSmokeTest::RunTest(const FString& Parameters)
{
	using namespace Padel::Court;

	TestEqual(TEXT("Length"), Length, 20.0);
	TestEqual(TEXT("Width"), Width, 10.0);
	TestEqual(TEXT("Net centre"), NetHeightAt(0.0), 0.88, 1e-6);
	TestEqual(TEXT("Net at posts"), NetHeightAt(HalfWidth), 0.92, 1e-6);
	TestEqual(TEXT("Exit width"), ExitFarX - ExitNearX, 2.0, 1e-6);

	const std::optional<FRayHit> Glass = RaycastLayout(BuildCourtLayout(), { 0.0, 0.0, 1.5 }, { 20.0, 0.0, 1.5 });
	TestTrue(TEXT("Back glass is hit"), Glass.has_value());
	if (Glass)
	{
		TestTrue(TEXT("Back glass surface"), Glass->Surface == ESurface::Glass);
		TestEqual(TEXT("Back glass face"), Glass->Point.X, 10.0, 0.01);
	}

	const std::optional<FRayHit> Exit = RaycastLayout(BuildCourtLayout(), { 5.0, 0.0, 1.0 }, { 5.0, 20.0, 1.0 });
	TestFalse(TEXT("Exit is open"), Exit.has_value());
	return true;
}

#endif
