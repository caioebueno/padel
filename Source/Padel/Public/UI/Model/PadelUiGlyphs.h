// Button names for prompts (GDD 7): "Labels use the Xbox layout and must show the
// right glyphs for PlayStation and Switch pads." Text labels for now; icon glyphs
// can replace them later without changing callers.
//
// Unreal names gamepad buttons by position (Gamepad_FaceButton_Bottom...), so the same
// action is "A" on Xbox, "Cross" on PlayStation and "B" on Switch.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include <string>
#include <string_view>

namespace Padel::Ui
{
	enum class EPadFamily
	{
		KeyboardMouse,
		Xbox,
		PlayStation,
		Switch,
	};

	// From the hardware device name Unreal reports for the last used device
	// (e.g. "XInputController", "DualSense", "SwitchProController"). Empty = keyboard.
	EPadFamily FamilyFromDeviceName(std::string_view DeviceName);

	// Label for an Unreal gamepad key name on a pad family, e.g.
	// ("Gamepad_FaceButton_Bottom", PlayStation) -> "Cross". Returns "" for a key that
	// isn't a known gamepad key, so the caller can fall back to the engine's key name.
	std::string GamepadKeyLabel(std::string_view KeyName, EPadFamily Family);
}
