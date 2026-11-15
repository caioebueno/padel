#include "UI/Model/PadelUiGlyphs.h"

#include <array>
#include <cctype>

namespace Padel::Ui
{
	namespace
	{
		struct FKeyLabels
		{
			std::string_view Key;
			std::string_view Xbox;
			std::string_view PlayStation;
			std::string_view Switch;
		};

		constexpr std::array<FKeyLabels, 16> KeyLabelTable{{
			{"Gamepad_FaceButton_Bottom", "A", "Cross", "B"},
			{"Gamepad_FaceButton_Right", "B", "Circle", "A"},
			{"Gamepad_FaceButton_Left", "X", "Square", "Y"},
			{"Gamepad_FaceButton_Top", "Y", "Triangle", "X"},
			{"Gamepad_LeftShoulder", "LB", "L1", "L"},
			{"Gamepad_RightShoulder", "RB", "R1", "R"},
			{"Gamepad_LeftTrigger", "LT", "L2", "ZL"},
			{"Gamepad_RightTrigger", "RT", "R2", "ZR"},
			{"Gamepad_LeftTriggerAxis", "LT", "L2", "ZL"},
			{"Gamepad_RightTriggerAxis", "RT", "R2", "ZR"},
			{"Gamepad_DPad_Up", "D-pad up", "D-pad up", "D-pad up"},
			{"Gamepad_DPad_Down", "D-pad down", "D-pad down", "D-pad down"},
			{"Gamepad_DPad_Left", "D-pad left", "D-pad left", "D-pad left"},
			{"Gamepad_DPad_Right", "D-pad right", "D-pad right", "D-pad right"},
			{"Gamepad_Special_Right", "Menu", "Options", "+"},
			{"Gamepad_Left2D", "Left stick", "Left stick", "Left stick"},
		}};

		bool Contains(const std::string& Haystack, std::string_view Needle)
		{
			return Haystack.find(Needle) != std::string::npos;
		}
	}

	EPadFamily FamilyFromDeviceName(std::string_view DeviceName)
	{
		if (DeviceName.empty())
		{
			return EPadFamily::KeyboardMouse;
		}
		std::string Lower(DeviceName);
		for (char& C : Lower)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		if (Lower == "kbm" || Contains(Lower, "keyboard") || Contains(Lower, "mouse"))
		{
			return EPadFamily::KeyboardMouse;
		}
		for (const std::string_view Word : {"dualsense", "dualshock", "playstation", "ps4", "ps5", "sony"})
		{
			if (Contains(Lower, Word))
			{
				return EPadFamily::PlayStation;
			}
		}
		for (const std::string_view Word : {"switch", "nintendo", "joycon", "joy-con"})
		{
			if (Contains(Lower, Word))
			{
				return EPadFamily::Switch;
			}
		}
		return EPadFamily::Xbox;
	}

	std::string GamepadKeyLabel(std::string_view KeyName, EPadFamily Family)
	{
		for (const FKeyLabels& Entry : KeyLabelTable)
		{
			if (Entry.Key != KeyName)
			{
				continue;
			}
			switch (Family)
			{
			case EPadFamily::PlayStation:
				return std::string(Entry.PlayStation);
			case EPadFamily::Switch:
				return std::string(Entry.Switch);
			case EPadFamily::Xbox:
			case EPadFamily::KeyboardMouse:
				return std::string(Entry.Xbox);
			}
		}
		return "";
	}
}
