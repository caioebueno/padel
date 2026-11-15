// The AI inside the engine build: its data files load from the project directory, and
// the D-pad actions map to the GDD 7 partner commands. The AI's behaviour is tested in
// the native suite (Tests/Native/Ai*.cpp), which CI runs on every PR.
#include "Misc/AutomationTest.h"

#include "AI/PadelAiData.h"
#include "AI/PadelPartnerCommands.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelAiDataTest, "Padel.AI.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelAiDataTest::RunTest(const FString& Parameters)
{
	Padel::AI::FAiTactics Tactics;
	TestTrue(TEXT("AiTactics.json loads"), Padel::AIData::LoadTactics(Tactics));
	TestTrue(TEXT("Net depth is set"), Tactics.NetDepthM > 0.0);

	std::vector<Padel::AI::FAiDifficulty> Rows;
	TestTrue(TEXT("Difficulty.json loads"), Padel::AIData::LoadDifficulties(Rows));
	for (const TCHAR* Name : { TEXT("Easy"), TEXT("Medium"), TEXT("Hard") })
	{
		TestNotNull(FString::Printf(TEXT("Difficulty %s"), Name), Padel::AIData::FindDifficulty(Rows, Name));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelAiPartnerCommandsTest, "Padel.AI.PartnerCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelAiPartnerCommandsTest::RunTest(const FString& Parameters)
{
	using Padel::AI::EPartnerCommand;
	using Padel::AIInput::CommandForAction;
	TestTrue(TEXT("Net"), CommandForAction(EPadelInputAction::CmdNet) == EPartnerCommand::GoNet);
	TestTrue(TEXT("Back"), CommandForAction(EPadelInputAction::CmdBack) == EPartnerCommand::StayBack);
	TestTrue(TEXT("Middle"), CommandForAction(EPadelInputAction::CmdMiddle) == EPartnerCommand::TakeMiddle);
	TestTrue(TEXT("Yours"), CommandForAction(EPadelInputAction::CmdYours) == EPartnerCommand::Yours);
	TestFalse(TEXT("Shots aren't commands"), CommandForAction(EPadelInputAction::ShotDrive).has_value());
	return true;
}

#endif
