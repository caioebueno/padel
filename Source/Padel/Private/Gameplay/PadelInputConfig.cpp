#include "Gameplay/PadelInputConfig.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

const FName UPadelInputConfig::MatchContextName(TEXT("IMC_Match"));

const TArray<FPadelInputBinding>& UPadelInputConfig::GetDefaultBindings()
{
	using A = EPadelInputAction;
	using M = EPadelAxisMapping;
	// GDD section 7. Xbox labels; Unreal's gamepad keys map to the same buttons on
	// PlayStation and Switch pads.
	static const TArray<FPadelInputBinding> Bindings = {
		{ A::Move, EKeys::Gamepad_Left2D, M::None },
		{ A::Move, EKeys::W, M::PositiveY },
		{ A::Move, EKeys::S, M::NegativeY },
		{ A::Move, EKeys::A, M::NegativeX },
		{ A::Move, EKeys::D, M::PositiveX },

		{ A::Sprint, EKeys::Gamepad_RightTrigger },
		{ A::Sprint, EKeys::LeftShift },

		{ A::ShotDrive, EKeys::Gamepad_FaceButton_Bottom },   // A
		{ A::ShotDrive, EKeys::J },
		{ A::ShotLob, EKeys::Gamepad_FaceButton_Right },      // B
		{ A::ShotLob, EKeys::K },
		{ A::ShotOverhead, EKeys::Gamepad_FaceButton_Top },   // Y
		{ A::ShotOverhead, EKeys::L },
		{ A::ShotDrop, EKeys::Gamepad_FaceButton_Left },      // X
		{ A::ShotDrop, EKeys::I },
		{ A::ModAttack, EKeys::Gamepad_RightShoulder },       // RB
		{ A::ModAttack, EKeys::E },
		{ A::ModTouch, EKeys::Gamepad_LeftShoulder },         // LB
		{ A::ModTouch, EKeys::Q },

		{ A::CmdNet, EKeys::Gamepad_DPad_Up },
		{ A::CmdNet, EKeys::One },
		{ A::CmdBack, EKeys::Gamepad_DPad_Down },
		{ A::CmdBack, EKeys::Two },
		{ A::CmdMiddle, EKeys::Gamepad_DPad_Left },
		{ A::CmdMiddle, EKeys::Three },
		{ A::CmdYours, EKeys::Gamepad_DPad_Right },
		{ A::CmdYours, EKeys::Four },

		{ A::Pause, EKeys::Gamepad_Special_Right },           // Start / Menu
		{ A::Pause, EKeys::Escape },
	};
	return Bindings;
}

TArray<EPadelInputAction> UPadelInputConfig::GetAllActions()
{
	TArray<EPadelInputAction> Out;
	const UEnum* Enum = StaticEnum<EPadelInputAction>();
	// NumEnums() includes the generated _MAX entry.
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
	{
		Out.Add(static_cast<EPadelInputAction>(Enum->GetValueByIndex(Index)));
	}
	return Out;
}

FName UPadelInputConfig::GetActionAssetName(EPadelInputAction Action)
{
	const FString Name = StaticEnum<EPadelInputAction>()->GetNameStringByValue(static_cast<int64>(Action));
	return FName(*(TEXT("IA_") + Name));
}

void UPadelInputConfig::Build()
{
	if (MatchContext)
	{
		return;
	}

	MatchContext = NewObject<UInputMappingContext>(this, MatchContextName);

	for (const EPadelInputAction Id : GetAllActions())
	{
		UInputAction* Action = NewObject<UInputAction>(this, GetActionAssetName(Id));
		Action->ValueType = Id == EPadelInputAction::Move ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
		Actions.Add(Id, Action);
	}

	for (const FPadelInputBinding& Binding : GetDefaultBindings())
	{
		FEnhancedActionKeyMapping& Mapping = MatchContext->MapKey(GetAction(Binding.Action), Binding.Key);

		if (Binding.Key == EKeys::Gamepad_Left2D)
		{
			UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(MatchContext);
			DeadZone->LowerThreshold = StickDeadZone;
			Mapping.Modifiers.Add(DeadZone);
		}

		// A single key reports its value on X. Swizzle moves it to Y; Negate flips it.
		const bool bSwizzle = Binding.Axis == EPadelAxisMapping::PositiveY || Binding.Axis == EPadelAxisMapping::NegativeY;
		const bool bNegate = Binding.Axis == EPadelAxisMapping::NegativeX || Binding.Axis == EPadelAxisMapping::NegativeY;
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(MatchContext);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MatchContext));
		}
	}
}

UInputAction* UPadelInputConfig::GetAction(EPadelInputAction Action) const
{
	const TObjectPtr<UInputAction>* Found = Actions.Find(Action);
	return Found ? Found->Get() : nullptr;
}
