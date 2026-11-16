#include "UI/Model/PadelUiNavigation.h"

#include <cmath>
#include <set>

namespace Padel::Ui
{
	namespace
	{
		struct FNavKeyBinding
		{
			const char* Key;
			EUiInput Input;
		};

		// Spec 5.1. Keyboard WASD navigates as well as the arrows (5.2).
		constexpr FNavKeyBinding NavKeyBindings[] = {
			{"Up", EUiInput::Up},
			{"Down", EUiInput::Down},
			{"Left", EUiInput::Left},
			{"Right", EUiInput::Right},
			{"W", EUiInput::Up},
			{"S", EUiInput::Down},
			{"A", EUiInput::Left},
			{"D", EUiInput::Right},
			{"Gamepad_DPad_Up", EUiInput::Up},
			{"Gamepad_DPad_Down", EUiInput::Down},
			{"Gamepad_DPad_Left", EUiInput::Left},
			{"Gamepad_DPad_Right", EUiInput::Right},
			{"Enter", EUiInput::Confirm},
			{"SpaceBar", EUiInput::Confirm},
			{"J", EUiInput::Confirm},
			{"Gamepad_FaceButton_Bottom", EUiInput::Confirm},
			{"Escape", EUiInput::Back},
			{"BackSpace", EUiInput::Back},
			{"K", EUiInput::Back},
			{"Gamepad_FaceButton_Right", EUiInput::Back},
			{"RightMouseButton", EUiInput::Back},
			{"ThumbMouseButton", EUiInput::Back},
			{"Q", EUiInput::TabLeft},
			{"Gamepad_LeftShoulder", EUiInput::TabLeft},
			{"E", EUiInput::TabRight},
			{"Gamepad_RightShoulder", EUiInput::TabRight},
			{"R", EUiInput::Secondary},
			{"Gamepad_FaceButton_Left", EUiInput::Secondary},
			{"F", EUiInput::Tertiary},
			{"Gamepad_FaceButton_Top", EUiInput::Tertiary},
			{"Gamepad_Special_Right", EUiInput::Pause},
		};

		const std::string EmptyNavId;
	}

	bool IsDirection(EUiInput Input)
	{
		return Input == EUiInput::Up || Input == EUiInput::Down || Input == EUiInput::Left || Input == EUiInput::Right;
	}

	const char* PadKeyName(EUiInput Input)
	{
		switch (Input)
		{
		case EUiInput::Confirm:
			return "Gamepad_FaceButton_Bottom";
		case EUiInput::Back:
			return "Gamepad_FaceButton_Right";
		case EUiInput::Secondary:
			return "Gamepad_FaceButton_Left";
		case EUiInput::Tertiary:
			return "Gamepad_FaceButton_Top";
		case EUiInput::TabLeft:
			return "Gamepad_LeftShoulder";
		case EUiInput::TabRight:
			return "Gamepad_RightShoulder";
		case EUiInput::Pause:
			return "Gamepad_Special_Right";
		case EUiInput::Up:
			return "Gamepad_DPad_Up";
		case EUiInput::Down:
			return "Gamepad_DPad_Down";
		case EUiInput::Left:
			return "Gamepad_DPad_Left";
		case EUiInput::Right:
			return "Gamepad_DPad_Right";
		default:
			return "";
		}
	}

	const char* KeyboardHintLabel(EUiInput Input)
	{
		switch (Input)
		{
		case EUiInput::Confirm:
			return "Enter";
		case EUiInput::Back:
			return "Esc";
		case EUiInput::Secondary:
			return "R";
		case EUiInput::Tertiary:
			return "F";
		case EUiInput::TabLeft:
			return "Q";
		case EUiInput::TabRight:
			return "E";
		case EUiInput::Pause:
			return "Esc";
		case EUiInput::Up:
			return "W";
		case EUiInput::Down:
			return "S";
		case EUiInput::Left:
			return "A";
		case EUiInput::Right:
			return "D";
		default:
			return "";
		}
	}

	bool FocusDirFor(EUiInput Input, EFocusDir& Out)
	{
		switch (Input)
		{
		case EUiInput::Up:
			Out = EFocusDir::Up;
			return true;
		case EUiInput::Down:
			Out = EFocusDir::Down;
			return true;
		case EUiInput::Left:
			Out = EFocusDir::Left;
			return true;
		case EUiInput::Right:
			Out = EFocusDir::Right;
			return true;
		default:
			return false;
		}
	}

	EUiInput InputForKey(std::string_view KeyName)
	{
		for (const FNavKeyBinding& Binding : NavKeyBindings)
		{
			if (KeyName == Binding.Key)
			{
				return Binding.Input;
			}
		}
		return EUiInput::None;
	}

	const std::string& FFocusLinks::Get(EFocusDir Dir) const
	{
		switch (Dir)
		{
		case EFocusDir::Up:
			return Up;
		case EFocusDir::Down:
			return Down;
		case EFocusDir::Left:
			return Left;
		case EFocusDir::Right:
			return Right;
		}
		return EmptyNavId;
	}

	void FFocusMap::Add(const std::string& Id, const FFocusLinks& InLinks)
	{
		if (!Links.count(Id))
		{
			Order.push_back(Id);
		}
		Links[Id] = InLinks;
		Enabled.emplace(Id, true);
		if (DefaultId.empty())
		{
			DefaultId = Id;
		}
	}

	void FFocusMap::SetEnabled(const std::string& Id, bool bEnabled)
	{
		if (Links.count(Id))
		{
			Enabled[Id] = bEnabled;
		}
	}

	bool FFocusMap::IsEnabled(const std::string& Id) const
	{
		const auto It = Enabled.find(Id);
		return It != Enabled.end() && It->second;
	}

	std::optional<std::string> FFocusMap::Move(const std::string& From, EFocusDir Dir) const
	{
		std::string Current = From;
		// Pass through disabled elements, but never loop forever on a ring of them.
		for (size_t Step = 0; Step <= Links.size(); ++Step)
		{
			const auto It = Links.find(Current);
			if (It == Links.end())
			{
				return std::nullopt;
			}
			const std::string& Next = It->second.Get(Dir);
			if (Next.empty() || Next == From)
			{
				return std::nullopt;
			}
			if (IsEnabled(Next))
			{
				return Next;
			}
			Current = Next;
		}
		return std::nullopt;
	}

	std::string FFocusMap::FirstFocus() const
	{
		if (IsEnabled(DefaultId))
		{
			return DefaultId;
		}
		for (const std::string& Id : Order)
		{
			if (IsEnabled(Id))
			{
				return Id;
			}
		}
		return std::string();
	}

	std::vector<std::string> FFocusMap::Validate() const
	{
		std::vector<std::string> Problems;
		if (!Links.count(DefaultId))
		{
			Problems.push_back("default focus '" + DefaultId + "' is not in the map");
		}
		for (const auto& [Id, ElementLinks] : Links)
		{
			for (EFocusDir Dir : {EFocusDir::Up, EFocusDir::Down, EFocusDir::Left, EFocusDir::Right})
			{
				const std::string& Target = ElementLinks.Get(Dir);
				if (!Target.empty() && !Links.count(Target))
				{
					Problems.push_back("'" + Id + "' links to unknown '" + Target + "'");
				}
			}
		}
		// Every element must be reachable from the default focus (spec 11.5).
		std::set<std::string> Seen;
		std::vector<std::string> Frontier;
		if (Links.count(DefaultId))
		{
			Frontier.push_back(DefaultId);
			Seen.insert(DefaultId);
		}
		while (!Frontier.empty())
		{
			const std::string Id = Frontier.back();
			Frontier.pop_back();
			for (EFocusDir Dir : {EFocusDir::Up, EFocusDir::Down, EFocusDir::Left, EFocusDir::Right})
			{
				const std::string& Target = Links.at(Id).Get(Dir);
				if (!Target.empty() && Links.count(Target) && Seen.insert(Target).second)
				{
					Frontier.push_back(Target);
				}
			}
		}
		for (const std::string& Id : Order)
		{
			if (!Seen.count(Id))
			{
				Problems.push_back("'" + Id + "' can't be reached from the default focus");
			}
		}
		return Problems;
	}

	FFocusMap FFocusMap::VerticalList(const std::vector<std::string>& Ids, bool bWrap)
	{
		FFocusMap Map;
		const size_t Count = Ids.size();
		for (size_t Index = 0; Index < Count; ++Index)
		{
			FFocusLinks ElementLinks;
			if (Index > 0)
			{
				ElementLinks.Up = Ids[Index - 1];
			}
			else if (bWrap && Count > 1)
			{
				ElementLinks.Up = Ids[Count - 1];
			}
			if (Index + 1 < Count)
			{
				ElementLinks.Down = Ids[Index + 1];
			}
			else if (bWrap && Count > 1)
			{
				ElementLinks.Down = Ids[0];
			}
			Map.Add(Ids[Index], ElementLinks);
		}
		return Map;
	}

	FFocusMap FFocusMap::Row(const std::vector<std::string>& Ids)
	{
		FFocusMap Map;
		for (size_t Index = 0; Index < Ids.size(); ++Index)
		{
			FFocusLinks ElementLinks;
			if (Index > 0)
			{
				ElementLinks.Left = Ids[Index - 1];
			}
			if (Index + 1 < Ids.size())
			{
				ElementLinks.Right = Ids[Index + 1];
			}
			Map.Add(Ids[Index], ElementLinks);
		}
		return Map;
	}

	void FDirectionRepeat::Press(EUiInput Direction)
	{
		if (!IsDirection(Direction))
		{
			return;
		}
		HeldDirection = Direction;
		HeldTime = 0.0f;
		NextFire = Delay;
	}

	void FDirectionRepeat::Release(EUiInput Direction)
	{
		if (Direction == HeldDirection)
		{
			Reset();
		}
	}

	void FDirectionRepeat::Reset()
	{
		HeldDirection = EUiInput::None;
		HeldTime = 0.0f;
		NextFire = 0.0f;
	}

	int FDirectionRepeat::Tick(float DeltaSeconds)
	{
		if (HeldDirection == EUiInput::None || Rate <= 0.0f)
		{
			return 0;
		}
		HeldTime += DeltaSeconds;
		int Fired = 0;
		while (HeldTime >= NextFire)
		{
			++Fired;
			NextFire += Rate;
		}
		return Fired;
	}

	EUiInput FStickDirection::Update(float X, float Y)
	{
		LastReleased = EUiInput::None;
		LastX = X;
		LastY = Y;
		const float Release = DeadZone * 0.8f;
		if (Active != EUiInput::None)
		{
			const float Along = (Active == EUiInput::Left || Active == EUiInput::Right) ? std::fabs(X) : std::fabs(Y);
			if (Along >= Release)
			{
				return EUiInput::None;
			}
			LastReleased = Active;
			Active = EUiInput::None;
		}
		const float AbsX = std::fabs(X);
		const float AbsY = std::fabs(Y);
		if (std::max(AbsX, AbsY) < DeadZone)
		{
			return EUiInput::None;
		}
		// The dominant axis wins, so a diagonal flick moves one step, not two.
		if (AbsX > AbsY)
		{
			Active = X > 0.0f ? EUiInput::Right : EUiInput::Left;
		}
		else
		{
			Active = Y > 0.0f ? EUiInput::Up : EUiInput::Down;
		}
		return Active;
	}

	void FInputLock::Begin(float EntrySeconds, float ConfirmSeconds)
	{
		Elapsed = 0.0f;
		Entry = EntrySeconds;
		ConfirmUntil = std::max(EntrySeconds, ConfirmSeconds);
	}
}
