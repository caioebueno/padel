// The input map (GDD section 7): every action exists with its IA_ name, has a gamepad
// and a keyboard binding, and the key layout matches the GDD table.
#include "Misc/AutomationTest.h"

#include "Gameplay/PadelInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelInputTests
{
	bool HasBinding(const UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action == Action && Mapping.Key == Key)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelInputActionsTest, "Padel.Input.Actions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelInputActionsTest::RunTest(const FString& Parameters)
{
	UPadelInputConfig* Config = NewObject<UPadelInputConfig>(GetTransientPackage());
	Config->Build();

	const UInputMappingContext* Context = Config->GetMatchContext();
	if (!TestNotNull(TEXT("IMC_Match exists"), Context))
	{
		return false;
	}
	TestEqual(TEXT("Context name"), Context->GetName(), FString(TEXT("IMC_Match")));

	const TArray<EPadelInputAction> All = UPadelInputConfig::GetAllActions();
	TestEqual(TEXT("13 actions (GDD 7)"), All.Num(), 13);

	for (const EPadelInputAction Id : All)
	{
		const UInputAction* Action = Config->GetAction(Id);
		const FString Name = UPadelInputConfig::GetActionAssetName(Id).ToString();
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), *Name), Action))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("%s object name"), *Name), Action->GetName(), Name);

		bool bGamepad = false;
		bool bKeyboard = false;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action == Action)
			{
				bGamepad |= Mapping.Key.IsGamepadKey();
				bKeyboard |= !Mapping.Key.IsGamepadKey() && !Mapping.Key.IsMouseButton();
			}
		}
		TestTrue(*FString::Printf(TEXT("%s has a gamepad binding"), *Name), bGamepad);
		TestTrue(*FString::Printf(TEXT("%s has a keyboard binding"), *Name), bKeyboard);
	}

	TestTrue(TEXT("Move is 2D"), Config->GetAction(EPadelInputAction::Move)->ValueType == EInputActionValueType::Axis2D);
	TestTrue(TEXT("Shots are buttons"), Config->GetAction(EPadelInputAction::ShotDrive)->ValueType == EInputActionValueType::Boolean);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelInputLayoutTest, "Padel.Input.GddLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelInputLayoutTest::RunTest(const FString& Parameters)
{
	using PadelInputTests::HasBinding;
	using A = EPadelInputAction;

	UPadelInputConfig* Config = NewObject<UPadelInputConfig>(GetTransientPackage());
	Config->Build();
	const UInputMappingContext* Context = Config->GetMatchContext();
	auto Check = [&](A Action, const FKey& Pad, const FKey& Key)
	{
		const FString Name = UPadelInputConfig::GetActionAssetName(Action).ToString();
		TestTrue(*FString::Printf(TEXT("%s on %s"), *Name, *Pad.ToString()), HasBinding(Context, Config->GetAction(Action), Pad));
		TestTrue(*FString::Printf(TEXT("%s on %s"), *Name, *Key.ToString()), HasBinding(Context, Config->GetAction(Action), Key));
	};

	Check(A::Move, EKeys::Gamepad_Left2D, EKeys::W);
	Check(A::Sprint, EKeys::Gamepad_RightTrigger, EKeys::LeftShift);
	Check(A::ShotDrive, EKeys::Gamepad_FaceButton_Bottom, EKeys::J);
	Check(A::ShotLob, EKeys::Gamepad_FaceButton_Right, EKeys::K);
	Check(A::ShotOverhead, EKeys::Gamepad_FaceButton_Top, EKeys::L);
	Check(A::ShotDrop, EKeys::Gamepad_FaceButton_Left, EKeys::I);
	Check(A::ModAttack, EKeys::Gamepad_RightShoulder, EKeys::E);
	Check(A::ModTouch, EKeys::Gamepad_LeftShoulder, EKeys::Q);
	Check(A::CmdNet, EKeys::Gamepad_DPad_Up, EKeys::One);
	Check(A::CmdBack, EKeys::Gamepad_DPad_Down, EKeys::Two);
	Check(A::CmdMiddle, EKeys::Gamepad_DPad_Left, EKeys::Three);
	Check(A::CmdYours, EKeys::Gamepad_DPad_Right, EKeys::Four);
	Check(A::Pause, EKeys::Gamepad_Special_Right, EKeys::Escape);

	// WASD all drive the one 2D move action.
	for (const FKey& Key : { EKeys::A, EKeys::S, EKeys::D })
	{
		TestTrue(*FString::Printf(TEXT("IA_Move on %s"), *Key.ToString()), HasBinding(Context, Config->GetAction(A::Move), Key));
	}
	return true;
}

#endif
