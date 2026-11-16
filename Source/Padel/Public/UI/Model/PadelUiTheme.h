// The menu design tokens (docs/ui/MENU_SPEC.md sections 4, 5 and 9): colours, type,
// layout, motion and input timings. Data/UiTheme.json is the source of truth; the
// defaults here are the spec's values, so a missing or broken file still shows the
// menus (spec 7.12). The engine side (UI/PadelUiTokens.h) loads the file into this and
// turns the plain values into FLinearColor and FSlateFontInfo.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Padel::Ui
{
	struct FUiColour
	{
		float R = 0.0f;
		float G = 0.0f;
		float B = 0.0f;
		float A = 1.0f;
	};

	// sRGB "#RRGGBB" (or "RRGGBB") to 0..1 channels, unconverted; false if malformed.
	bool ParseHexColour(std::string_view Hex, FUiColour& Out);

	enum class EUiColour
	{
		BgDeep,
		BgScrim,
		BgScrimPause,
		BgScrimTitle,
		BgScrimDialog,
		BgVignette,
		GlassFill,
		GlassFillRaised,
		GlassEdge,
		GlassEdgeTop,
		GlassShadow,
		SurfaceCharcoal,
		Accent,
		AccentPressed,
		AccentGlow,
		OnAccent,
		TextPrimary,
		TextSecondary,
		TextDisabled,
		TeamLagoon,
		TeamEmber,
		StateGood,
		StateWarning,
		StateDanger,
		Lock,
		Count,
	};

	// The JSON key of a colour token, e.g. "Glass.FillRaised".
	const char* ColourKey(EUiColour Colour);

	// Font files under SourceArt/Fonts/. The *Tabular faces have tabular digits baked in
	// (Tools/AssetPipeline/fonts/bake_tabular_figures.py), since Slate can't turn on tnum.
	enum class EUiFontFace
	{
		Medium,
		CondensedSemiBold,
		CondensedBold,
		CondensedExtraBold,
		CondensedExtraBoldItalic,
		CondensedBoldTabular,
		CondensedExtraBoldTabular,
		Count,
	};

	const char* FontFaceKey(EUiFontFace Face);
	// File name under SourceArt/Fonts/, e.g. "BarlowCondensed-Bold.ttf".
	const char* FontFaceFile(EUiFontFace Face);
	bool FontFaceFromKey(std::string_view Key, EUiFontFace& Out);

	enum class EUiType
	{
		Hero,
		Display,
		Score,
		Title,
		Heading,
		Body,
		BodySmall,
		Label,
		Stat,
		Overall,
		Count,
	};

	const char* TypeKey(EUiType Type);

	struct FUiTypeStyle
	{
		EUiFontFace Face = EUiFontFace::Medium;
		float Size = 28.0f;        // px at 1080p
		float LineHeight = 36.0f;  // px at 1080p
		float TrackingPercent = 0.0f;
		bool bUpper = false;
		// Body, BodySmall and Label follow the Text size setting (spec 4.2).
		bool bScales = false;
	};

	// Spec 7.7 Text size.
	enum class ETextSize
	{
		Default,
		Large,
		Largest,
	};

	// Numeric tokens, by their JSON key ("Layout.Safe.X", "Motion.Focus", "Input.EntryLock").
	struct FUiLayout
	{
		std::array<float, 8> Space = {8, 16, 24, 32, 48, 64, 96, 128};
		float SafeX = 96;
		float SafeY = 64;
		float GridColumns = 12;
		float GridColumn = 122;
		float GridGutter = 24;
		float HeaderHeight = 96;
		float HintBarHeight = 56;
		float HintBarPauseStrip = 88;
		float BandGap = 24;
		float RadiusPanel = 12;
		float RadiusControl = 8;
		float RadiusPill = 999;
		float ShadowOffsetY = 12;
		float ShadowBlur = 32;
		float FocusRing = 3;
		float FocusGap = 2;
		float FocusGlowBlur = 12;
		float FocusScale = 1.03f;
		float FocusRowSlide = 8;
		float FocusRowBar = 6;
		float FocusRowBarPressed = 10;
		float PressScale = 0.98f;
		float PressScaleTile = 0.99f;
		float PressScaleArrow = 0.85f;
		float BlurStrength = 10;
	};

	struct FUiMotion
	{
		float Focus = 0.12f;
		float FocusOut = 0.08f;
		float Press = 0.08f;
		float Value = 0.14f;
		float ScreenIn = 0.24f;
		float ScreenOut = 0.20f;
		float ScreenSlideIn = 48;
		float ScreenSlideOut = 32;
		float Stagger = 0.04f;
		float StaggerMax = 6;
		float DialogIn = 0.16f;
		float DialogOut = 0.10f;
		float DialogScaleFrom = 0.96f;
		float Pulse = 1.6f;
		float PushIn = 6.0f;
		float PushInScale = 1.04f;
		float ReduceMotionFade = 0.16f;
		float ToastSeconds = 2.5f;
		float ToastFade = 0.2f;
		float HintFlash = 0.12f;
	};

	struct FUiInputTiming
	{
		float EntryLock = 0.12f;
		float RepeatDelay = 0.4f;
		float RepeatRate = 0.11f;
		float SliderRepeatRate = 0.06f;
		float SliderFastAfter = 1.0f;
		float SliderStep = 5;
		float SliderFineStep = 1;
		float StickDeadZone = 0.5f;
		float PressMinimum = 0.08f;
		float StatsConfirmLockSeconds = 1.0f;
		float GoldenPromptLockSeconds = 0.5f;
		float CursorHideSeconds = 3.0f;
		float LoadingIndicatorAfter = 0.3f;
		float VersusSeconds = 2.5f;
		float MatchStartTimeout = 20.0f;
	};

	struct FUiTheme
	{
		std::array<FUiColour, static_cast<size_t>(EUiColour::Count)> Colours{};
		std::array<FUiTypeStyle, static_cast<size_t>(EUiType::Count)> Types{};
		std::array<float, 3> TextScale = {1.0f, 1.15f, 1.3f};
		FUiLayout Layout;
		FUiMotion Motion;
		FUiInputTiming Input;
		// Spec 6.6: card stat label -> the Data/Players.json attributes it averages.
		std::vector<std::pair<std::string, std::vector<std::string>>> PlayerCardStats;

		// The spec's values (sections 4, 5, 9).
		static FUiTheme Defaults();

		const FUiColour& Colour(EUiColour Token) const { return Colours[static_cast<size_t>(Token)]; }
		const FUiTypeStyle& Type(EUiType Token) const { return Types[static_cast<size_t>(Token)]; }
		// The font size to draw at, with the Text size setting applied where the token scales.
		float FontSize(EUiType Token, ETextSize TextSize) const;
		float Space(int Step) const;  // Space.1 .. Space.8

		// Grid (spec 4.3): x of column C (1-based) from the screen's left edge, and the
		// width of a span of N columns (146n - 24 at the defaults).
		float ColumnX(int Column) const;
		float SpanWidth(int Columns) const;

		// Setters used by the loader. Each returns false for a key it doesn't know, so
		// the loader can warn about typos instead of silently ignoring them.
		bool SetColour(std::string_view Key, std::string_view Hex, float Alpha);
		bool SetType(std::string_view Key, const FUiTypeStyle& Style);
		bool SetNumber(std::string_view Group, std::string_view Key, float Value);
		bool SetTextScale(std::string_view Key, float Value);
	};

	// Spec 9 easings, T in 0..1.
	inline float EaseOutCubic(float T)
	{
		const float U = 1.0f - (T < 0.0f ? 0.0f : (T > 1.0f ? 1.0f : T));
		return 1.0f - U * U * U;
	}
	inline float EaseOutQuad(float T)
	{
		const float U = 1.0f - (T < 0.0f ? 0.0f : (T > 1.0f ? 1.0f : T));
		return 1.0f - U * U;
	}
	inline float EaseOutQuint(float T)
	{
		const float U = 1.0f - (T < 0.0f ? 0.0f : (T > 1.0f ? 1.0f : T));
		return 1.0f - U * U * U * U * U;
	}
	inline float EaseInQuad(float T)
	{
		const float C = T < 0.0f ? 0.0f : (T > 1.0f ? 1.0f : T);
		return C * C;
	}

	// Every numeric key the theme knows, as "Group.Key" ("Layout.Safe.X"), for tests and
	// for the loader's "unknown key" warning.
	std::vector<std::string> NumberKeys();
	bool GetNumber(const FUiTheme& Theme, std::string_view Group, std::string_view Key, float& Out);
}
