// The value logic of the menu controls (docs/ui/MENU_SPEC.md section 6): selector,
// toggle, segmented control, slider, tab bar, and the hint bar's ordering. Widgets draw
// these; the rules about wrapping and edges live here so the native suite checks them.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include "UI/Model/PadelUiNavigation.h"

#include <string>
#include <vector>

namespace Padel::Ui
{
	// Spec 6.8: left/right step without wrapping (the arrow at an end greys out); A steps
	// forward and wraps, so one-button and mouse users reach every value.
	struct FSelectorModel
	{
		int Count = 0;
		int Index = 0;

		// -1 or +1. False at an end (the caller plays the edge sound).
		bool Step(int Direction);
		void StepForwardWrapping();
		bool AtStart() const { return Index <= 0; }
		bool AtEnd() const { return Index >= Count - 1; }
		// Dots under the value only for 2 to 5 options.
		bool ShowsDots() const { return Count >= 2 && Count <= 5; }
	};

	// Spec 6.7: A toggles; Left means Off and Right means On.
	struct FToggleModel
	{
		bool bOn = false;

		void Toggle() { bOn = !bOn; }
		// False if it was already on that side (edge sound).
		bool Push(int Direction);
	};

	// Spec 6.9: left/right select directly, no wrap; A does nothing.
	struct FSegmentedModel
	{
		int Count = 0;
		int Index = 0;

		bool Step(int Direction);
	};

	// Spec 6.10 and 5.3: a press steps by Step (5); held, it repeats by FineStep (1), then
	// by Step again after FastAfter seconds.
	struct FSliderModel
	{
		int Min = 0;
		int Max = 100;
		int Value = 0;
		int StepSize = 5;
		int FineStep = 1;
		float FastAfter = 1.0f;

		// A fresh press. False if already at that end.
		bool Press(int Direction);
		// One repeat while held for HeldSeconds.
		bool Repeat(int Direction, float HeldSeconds);
		void Set(int NewValue);
		float Fraction() const;
	};

	// Spec 6.11: LB/RB change tab, no wrap. Changing tab resets focus to its first element.
	struct FTabModel
	{
		int Count = 0;
		int Index = 0;

		bool Step(int Direction);
	};

	// One item in the button hint bar (spec 6.1).
	struct FHint
	{
		// Confirm, Secondary, Tertiary, TabLeft (the "LB RB" pair), Pause or Back.
		EUiInput Input = EUiInput::None;
		std::string Label;
	};

	// Spec 6.1: A first, then X, Y, the LB RB pair, Start, and B last, whatever order the
	// screen listed them in. A TabRight item is folded into the TabLeft pair.
	std::vector<FHint> OrderHints(const std::vector<FHint>& Hints);
}
