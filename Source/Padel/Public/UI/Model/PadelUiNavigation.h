// Menu input and focus (docs/ui/MENU_SPEC.md section 5): which key means what, explicit
// focus maps per screen, held-direction repeat, the gamepad stick's dead zone and the
// input locks. Screens move a "virtual" focus through an FFocusMap instead of trusting
// Slate's geometric navigation, so focus never jumps somewhere surprising (5.3) and the
// maps can be checked without an engine.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Padel::Ui
{
	// What a press means in a menu (spec 5.1), independent of the device.
	enum class EUiInput
	{
		None,
		Up,
		Down,
		Left,
		Right,
		Confirm,   // A, Enter, Space, J
		Back,      // B, Esc, Backspace, K
		TabLeft,   // LB, Q
		TabRight,  // RB, E
		Secondary, // X, R
		Tertiary,  // Y, F
		Pause,     // Start (Gamepad_Special_Right); only Pause and Match listen (G13)
	};

	bool IsDirection(EUiInput Input);

	// From an Unreal key name ("Gamepad_FaceButton_Bottom", "Enter", "W"). The left
	// stick's virtual direction keys return None: the stick goes through FStickDirection
	// so the dead zone is the spec's 0.5, not the platform's.
	EUiInput InputForKey(std::string_view KeyName);

	// The Unreal gamepad key that a hint glyph shows for an input ("Gamepad_FaceButton_Bottom"),
	// for GamepadKeyLabel. Empty for inputs with no single pad button.
	const char* PadKeyName(EUiInput Input);
	// The key cap text for the keyboard hint glyph ("Enter", "Esc", "Q E").
	const char* KeyboardHintLabel(EUiInput Input);

	enum class EFocusDir
	{
		Up,
		Down,
		Left,
		Right,
	};

	// The focus direction of a direction input; false for any other input.
	bool FocusDirFor(EUiInput Input, EFocusDir& Out);

	// Spec 5.3: every focusable element names its target in each direction, or none.
	struct FFocusLinks
	{
		std::string Up;
		std::string Down;
		std::string Left;
		std::string Right;

		const std::string& Get(EFocusDir Dir) const;
	};

	class FFocusMap
	{
	public:
		void Add(const std::string& Id, const FFocusLinks& Links);
		void SetDefault(const std::string& Id) { DefaultId = Id; }
		// Disabled elements are skipped by navigation (spec 5.3); locked ones are not.
		void SetEnabled(const std::string& Id, bool bEnabled);

		bool Contains(const std::string& Id) const { return Links.count(Id) > 0; }
		bool IsEnabled(const std::string& Id) const;
		const std::string& Default() const { return DefaultId; }
		const std::vector<std::string>& Ids() const { return Order; }

		// The element focus moves to, or nullopt at an edge (the caller plays the edge
		// sound). A disabled target is passed through in the same direction.
		std::optional<std::string> Move(const std::string& From, EFocusDir Dir) const;

		// The default, or the first enabled element if the default is disabled or missing.
		std::string FirstFocus() const;

		// Problems with the map, for tests: links to unknown ids, a missing default,
		// elements unreachable from the default. Empty means the map is sound.
		std::vector<std::string> Validate() const;

		// Builders for the common shapes. A vertical list wraps top and bottom; a row
		// doesn't wrap (spec 5.3).
		static FFocusMap VerticalList(const std::vector<std::string>& Ids, bool bWrap = true);
		static FFocusMap Row(const std::vector<std::string>& Ids);

	private:
		std::map<std::string, FFocusLinks> Links;
		std::map<std::string, bool> Enabled;
		std::vector<std::string> Order;
		std::string DefaultId;
	};

	// Holding a direction repeats after RepeatDelay, then every RepeatRate (spec 5.3).
	// Sliders repeat faster and step further after FastAfter.
	class FDirectionRepeat
	{
	public:
		void Configure(float InDelay, float InRate) { Delay = InDelay; Rate = InRate; }
		void Press(EUiInput Direction);
		void Release(EUiInput Direction);
		void Reset();
		// Repeats that fire this frame (usually 0 or 1).
		int Tick(float DeltaSeconds);
		EUiInput Held() const { return HeldDirection; }
		float HeldSeconds() const { return HeldTime; }

	private:
		EUiInput HeldDirection = EUiInput::None;
		float Delay = 0.4f;
		float Rate = 0.11f;
		float HeldTime = 0.0f;
		float NextFire = 0.0f;
	};

	// The left stick as four directions with the spec's dead zone (5.2). A direction
	// presses once when the stick passes the dead zone and releases when it falls back
	// under 80% of it, so a stick resting on the threshold doesn't chatter.
	class FStickDirection
	{
	public:
		void SetDeadZone(float InDeadZone) { DeadZone = InDeadZone; }
		// Feed the latest X and Y (-1..1, +Y up). Returns the newly pressed direction, or
		// None. Released() reports a direction that just let go.
		EUiInput Update(float X, float Y);
		EUiInput Released() const { return LastReleased; }
		EUiInput Current() const { return Active; }

	private:
		float DeadZone = 0.5f;
		float LastX = 0.0f;
		float LastY = 0.0f;
		EUiInput Active = EUiInput::None;
		EUiInput LastReleased = EUiInput::None;
	};

	// Input locks (spec 3, 7.8). A screen ignores input for its entry lock; some screens
	// (Stats, the golden point prompt) also refuse Confirm until a longer lock ends and
	// then only take a fresh press that started after it, so a held A never lands.
	class FInputLock
	{
	public:
		// Lock all input for EntrySeconds and Confirm for ConfirmSeconds (>= entry).
		void Begin(float EntrySeconds, float ConfirmSeconds = 0.0f);
		void Tick(float DeltaSeconds) { Elapsed += DeltaSeconds; }
		bool AcceptsInput() const { return Elapsed >= Entry; }
		bool IsConfirmLocked() const { return Elapsed < ConfirmUntil; }
		// Call on a key-down (never a repeat). True if this Confirm press may act.
		bool AcceptConfirmPress() const { return !IsConfirmLocked(); }
		float Seconds() const { return Elapsed; }

	private:
		float Elapsed = 0.0f;
		float Entry = 0.0f;
		float ConfirmUntil = 0.0f;
	};
}
