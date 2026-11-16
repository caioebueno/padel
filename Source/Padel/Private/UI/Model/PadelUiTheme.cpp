#include "UI/Model/PadelUiTheme.h"

#include <algorithm>
#include <cmath>

namespace Padel::Ui
{
	namespace
	{
		constexpr const char* ThemeColourKeys[] = {
			"Bg.Deep", "Bg.Scrim", "Bg.ScrimPause", "Bg.ScrimTitle", "Bg.ScrimDialog", "Bg.Vignette",
			"Glass.Fill", "Glass.FillRaised", "Glass.Edge", "Glass.EdgeTop", "Glass.Shadow",
			"Surface.Charcoal", "Accent", "Accent.Pressed", "Accent.Glow", "OnAccent",
			"Text.Primary", "Text.Secondary", "Text.Disabled", "Team.Lagoon", "Team.Ember",
			"State.Good", "State.Warning", "State.Danger", "Lock",
		};
		static_assert(std::size(ThemeColourKeys) == static_cast<size_t>(EUiColour::Count));

		constexpr const char* ThemeTypeKeys[] = {
			"Hero", "Display", "Score", "Title", "Heading", "Body", "BodySmall", "Label", "Stat", "Overall",
		};
		static_assert(std::size(ThemeTypeKeys) == static_cast<size_t>(EUiType::Count));

		constexpr const char* ThemeFaceKeys[] = {
			"Medium", "CondensedSemiBold", "CondensedBold", "CondensedExtraBold", "CondensedExtraBoldItalic",
			"CondensedBoldTabular", "CondensedExtraBoldTabular",
		};
		constexpr const char* ThemeFaceFiles[] = {
			"Barlow-Medium.ttf", "BarlowCondensed-SemiBold.ttf", "BarlowCondensed-Bold.ttf",
			"BarlowCondensed-ExtraBold.ttf", "BarlowCondensed-ExtraBoldItalic.ttf",
			"BarlowCondensedTabular-Bold.ttf", "BarlowCondensedTabular-ExtraBold.ttf",
		};
		static_assert(std::size(ThemeFaceKeys) == static_cast<size_t>(EUiFontFace::Count));
		static_assert(std::size(ThemeFaceFiles) == static_cast<size_t>(EUiFontFace::Count));

		constexpr const char* ThemeTextScaleKeys[] = {"Default", "Large", "Largest"};

		struct FThemeNumberField
		{
			const char* Group;
			const char* Key;
			float& (*Get)(FUiTheme&);
		};

#define PADEL_UI_FIELD(GroupName, KeyName, Member) \
		FThemeNumberField{GroupName, KeyName, [](FUiTheme& T) -> float& { return T.Member; }}

		const FThemeNumberField ThemeNumberFields[] = {
			PADEL_UI_FIELD("Layout", "Space.1", Layout.Space[0]),
			PADEL_UI_FIELD("Layout", "Space.2", Layout.Space[1]),
			PADEL_UI_FIELD("Layout", "Space.3", Layout.Space[2]),
			PADEL_UI_FIELD("Layout", "Space.4", Layout.Space[3]),
			PADEL_UI_FIELD("Layout", "Space.5", Layout.Space[4]),
			PADEL_UI_FIELD("Layout", "Space.6", Layout.Space[5]),
			PADEL_UI_FIELD("Layout", "Space.7", Layout.Space[6]),
			PADEL_UI_FIELD("Layout", "Space.8", Layout.Space[7]),
			PADEL_UI_FIELD("Layout", "Safe.X", Layout.SafeX),
			PADEL_UI_FIELD("Layout", "Safe.Y", Layout.SafeY),
			PADEL_UI_FIELD("Layout", "Grid.Columns", Layout.GridColumns),
			PADEL_UI_FIELD("Layout", "Grid.Column", Layout.GridColumn),
			PADEL_UI_FIELD("Layout", "Grid.Gutter", Layout.GridGutter),
			PADEL_UI_FIELD("Layout", "Header.Height", Layout.HeaderHeight),
			PADEL_UI_FIELD("Layout", "HintBar.Height", Layout.HintBarHeight),
			PADEL_UI_FIELD("Layout", "HintBar.PauseStrip", Layout.HintBarPauseStrip),
			PADEL_UI_FIELD("Layout", "Band.Gap", Layout.BandGap),
			PADEL_UI_FIELD("Layout", "Radius.Panel", Layout.RadiusPanel),
			PADEL_UI_FIELD("Layout", "Radius.Control", Layout.RadiusControl),
			PADEL_UI_FIELD("Layout", "Radius.Pill", Layout.RadiusPill),
			PADEL_UI_FIELD("Layout", "Shadow.OffsetY", Layout.ShadowOffsetY),
			PADEL_UI_FIELD("Layout", "Shadow.Blur", Layout.ShadowBlur),
			PADEL_UI_FIELD("Layout", "Focus.Ring", Layout.FocusRing),
			PADEL_UI_FIELD("Layout", "Focus.Gap", Layout.FocusGap),
			PADEL_UI_FIELD("Layout", "Focus.GlowBlur", Layout.FocusGlowBlur),
			PADEL_UI_FIELD("Layout", "Focus.Scale", Layout.FocusScale),
			PADEL_UI_FIELD("Layout", "Focus.RowSlide", Layout.FocusRowSlide),
			PADEL_UI_FIELD("Layout", "Focus.RowBar", Layout.FocusRowBar),
			PADEL_UI_FIELD("Layout", "Focus.RowBarPressed", Layout.FocusRowBarPressed),
			PADEL_UI_FIELD("Layout", "Press.Scale", Layout.PressScale),
			PADEL_UI_FIELD("Layout", "Press.ScaleTile", Layout.PressScaleTile),
			PADEL_UI_FIELD("Layout", "Press.ScaleArrow", Layout.PressScaleArrow),
			PADEL_UI_FIELD("Layout", "Blur.Strength", Layout.BlurStrength),

			PADEL_UI_FIELD("Motion", "Focus", Motion.Focus),
			PADEL_UI_FIELD("Motion", "FocusOut", Motion.FocusOut),
			PADEL_UI_FIELD("Motion", "Press", Motion.Press),
			PADEL_UI_FIELD("Motion", "Value", Motion.Value),
			PADEL_UI_FIELD("Motion", "ScreenIn", Motion.ScreenIn),
			PADEL_UI_FIELD("Motion", "ScreenOut", Motion.ScreenOut),
			PADEL_UI_FIELD("Motion", "ScreenSlideIn", Motion.ScreenSlideIn),
			PADEL_UI_FIELD("Motion", "ScreenSlideOut", Motion.ScreenSlideOut),
			PADEL_UI_FIELD("Motion", "Stagger", Motion.Stagger),
			PADEL_UI_FIELD("Motion", "StaggerMax", Motion.StaggerMax),
			PADEL_UI_FIELD("Motion", "DialogIn", Motion.DialogIn),
			PADEL_UI_FIELD("Motion", "DialogOut", Motion.DialogOut),
			PADEL_UI_FIELD("Motion", "DialogScaleFrom", Motion.DialogScaleFrom),
			PADEL_UI_FIELD("Motion", "Pulse", Motion.Pulse),
			PADEL_UI_FIELD("Motion", "PushIn", Motion.PushIn),
			PADEL_UI_FIELD("Motion", "PushInScale", Motion.PushInScale),
			PADEL_UI_FIELD("Motion", "ReduceMotionFade", Motion.ReduceMotionFade),
			PADEL_UI_FIELD("Motion", "ToastSeconds", Motion.ToastSeconds),
			PADEL_UI_FIELD("Motion", "ToastFade", Motion.ToastFade),
			PADEL_UI_FIELD("Motion", "HintFlash", Motion.HintFlash),

			PADEL_UI_FIELD("Input", "EntryLock", Input.EntryLock),
			PADEL_UI_FIELD("Input", "RepeatDelay", Input.RepeatDelay),
			PADEL_UI_FIELD("Input", "RepeatRate", Input.RepeatRate),
			PADEL_UI_FIELD("Input", "SliderRepeatRate", Input.SliderRepeatRate),
			PADEL_UI_FIELD("Input", "SliderFastAfter", Input.SliderFastAfter),
			PADEL_UI_FIELD("Input", "SliderStep", Input.SliderStep),
			PADEL_UI_FIELD("Input", "SliderFineStep", Input.SliderFineStep),
			PADEL_UI_FIELD("Input", "StickDeadZone", Input.StickDeadZone),
			PADEL_UI_FIELD("Input", "PressMinimum", Input.PressMinimum),
			PADEL_UI_FIELD("Input", "StatsConfirmLockSeconds", Input.StatsConfirmLockSeconds),
			PADEL_UI_FIELD("Input", "GoldenPromptLockSeconds", Input.GoldenPromptLockSeconds),
			PADEL_UI_FIELD("Input", "CursorHideSeconds", Input.CursorHideSeconds),
			PADEL_UI_FIELD("Input", "LoadingIndicatorAfter", Input.LoadingIndicatorAfter),
			PADEL_UI_FIELD("Input", "VersusSeconds", Input.VersusSeconds),
			PADEL_UI_FIELD("Input", "MatchStartTimeout", Input.MatchStartTimeout),
		};
#undef PADEL_UI_FIELD

		int ThemeHexDigit(char C)
		{
			if (C >= '0' && C <= '9')
			{
				return C - '0';
			}
			if (C >= 'a' && C <= 'f')
			{
				return C - 'a' + 10;
			}
			if (C >= 'A' && C <= 'F')
			{
				return C - 'A' + 10;
			}
			return -1;
		}

		FUiColour ThemeHex(const char* Text, float Alpha)
		{
			FUiColour Colour;
			ParseHexColour(Text, Colour);
			Colour.A = Alpha;
			return Colour;
		}

		template <size_t N>
		int ThemeIndexOf(const char* const (&Keys)[N], std::string_view Key)
		{
			for (size_t Index = 0; Index < N; ++Index)
			{
				if (Key == Keys[Index])
				{
					return static_cast<int>(Index);
				}
			}
			return -1;
		}
	}

	bool ParseHexColour(std::string_view Text, FUiColour& Out)
	{
		if (!Text.empty() && Text.front() == '#')
		{
			Text.remove_prefix(1);
		}
		if (Text.size() != 6)
		{
			return false;
		}
		float Channels[3];
		for (int Channel = 0; Channel < 3; ++Channel)
		{
			const int High = ThemeHexDigit(Text[Channel * 2]);
			const int Low = ThemeHexDigit(Text[Channel * 2 + 1]);
			if (High < 0 || Low < 0)
			{
				return false;
			}
			Channels[Channel] = static_cast<float>(High * 16 + Low) / 255.0f;
		}
		Out.R = Channels[0];
		Out.G = Channels[1];
		Out.B = Channels[2];
		return true;
	}

	const char* ColourKey(EUiColour Colour)
	{
		const size_t Index = static_cast<size_t>(Colour);
		return Index < std::size(ThemeColourKeys) ? ThemeColourKeys[Index] : "";
	}

	const char* TypeKey(EUiType Type)
	{
		const size_t Index = static_cast<size_t>(Type);
		return Index < std::size(ThemeTypeKeys) ? ThemeTypeKeys[Index] : "";
	}

	const char* FontFaceKey(EUiFontFace Face)
	{
		const size_t Index = static_cast<size_t>(Face);
		return Index < std::size(ThemeFaceKeys) ? ThemeFaceKeys[Index] : "";
	}

	const char* FontFaceFile(EUiFontFace Face)
	{
		const size_t Index = static_cast<size_t>(Face);
		return Index < std::size(ThemeFaceFiles) ? ThemeFaceFiles[Index] : "";
	}

	bool FontFaceFromKey(std::string_view Key, EUiFontFace& Out)
	{
		const int Index = ThemeIndexOf(ThemeFaceKeys, Key);
		if (Index < 0)
		{
			return false;
		}
		Out = static_cast<EUiFontFace>(Index);
		return true;
	}

	FUiTheme FUiTheme::Defaults()
	{
		FUiTheme Theme;
		auto Set = [&Theme](EUiColour Token, const char* HexText, float Alpha)
		{
			Theme.Colours[static_cast<size_t>(Token)] = ThemeHex(HexText, Alpha);
		};
		// Spec 4.1.
		Set(EUiColour::BgDeep, "#070B14", 1.0f);
		Set(EUiColour::BgScrim, "#070B14", 0.55f);
		Set(EUiColour::BgScrimPause, "#070B14", 0.70f);
		Set(EUiColour::BgScrimTitle, "#070B14", 0.35f);
		Set(EUiColour::BgScrimDialog, "#070B14", 0.60f);
		Set(EUiColour::BgVignette, "#000000", 0.60f);
		Set(EUiColour::GlassFill, "#111A2B", 0.72f);
		Set(EUiColour::GlassFillRaised, "#1A2540", 0.82f);
		Set(EUiColour::GlassEdge, "#FFFFFF", 0.10f);
		Set(EUiColour::GlassEdgeTop, "#FFFFFF", 0.22f);
		Set(EUiColour::GlassShadow, "#000000", 0.45f);
		Set(EUiColour::SurfaceCharcoal, "#151922", 0.94f);
		Set(EUiColour::Accent, "#C8F03C", 1.0f);
		Set(EUiColour::AccentPressed, "#A6CC22", 1.0f);
		Set(EUiColour::AccentGlow, "#C8F03C", 0.35f);
		Set(EUiColour::OnAccent, "#0A1020", 1.0f);
		Set(EUiColour::TextPrimary, "#F4F1E8", 1.0f);
		Set(EUiColour::TextSecondary, "#F4F1E8", 0.68f);
		Set(EUiColour::TextDisabled, "#F4F1E8", 0.34f);
		Set(EUiColour::TeamLagoon, "#2EC4B6", 1.0f);
		Set(EUiColour::TeamEmber, "#FF7A3D", 1.0f);
		Set(EUiColour::StateGood, "#7CFF6B", 1.0f);
		Set(EUiColour::StateWarning, "#FFB347", 1.0f);
		Set(EUiColour::StateDanger, "#FF5D5D", 1.0f);
		Set(EUiColour::Lock, "#8C95A8", 1.0f);

		// Spec 4.2.
		auto Type = [&Theme](EUiType Token, EUiFontFace Face, float Size, float Line, float Tracking, bool bUpper, bool bScales)
		{
			Theme.Types[static_cast<size_t>(Token)] = FUiTypeStyle{Face, Size, Line, Tracking, bUpper, bScales};
		};
		Type(EUiType::Hero, EUiFontFace::CondensedExtraBoldItalic, 120, 108, -1, true, false);
		Type(EUiType::Display, EUiFontFace::CondensedBold, 72, 72, 0, true, false);
		Type(EUiType::Score, EUiFontFace::CondensedBoldTabular, 96, 96, 0, false, false);
		Type(EUiType::Title, EUiFontFace::CondensedSemiBold, 44, 48, 1, true, false);
		Type(EUiType::Heading, EUiFontFace::CondensedSemiBold, 32, 36, 2, true, false);
		Type(EUiType::Body, EUiFontFace::Medium, 28, 36, 0, false, true);
		Type(EUiType::BodySmall, EUiFontFace::Medium, 26, 34, 0, false, true);
		Type(EUiType::Label, EUiFontFace::CondensedBold, 26, 30, 8, true, true);
		Type(EUiType::Stat, EUiFontFace::CondensedBoldTabular, 40, 40, 0, false, false);
		Type(EUiType::Overall, EUiFontFace::CondensedExtraBoldTabular, 88, 80, -2, false, false);

		// Spec 6.6.
		Theme.PlayerCardStats = {
			{"Attack", {"Overhead", "Bandeja", "Vibora", "Power"}},
			{"Defence", {"WallPlay", "Lob", "Drive", "Backhand"}},
			{"Net", {"Volley", "Touch", "Anticipation", "Positioning"}},
			{"Movement", {"Speed", "Acceleration", "Agility", "Stamina"}},
		};
		return Theme;
	}

	float FUiTheme::FontSize(EUiType Token, ETextSize TextSize) const
	{
		const FUiTypeStyle& Style = Type(Token);
		const float Scale = Style.bScales ? TextScale[static_cast<size_t>(TextSize)] : 1.0f;
		return std::round(Style.Size * Scale);
	}

	float FUiTheme::Space(int Step) const
	{
		return Layout.Space[static_cast<size_t>(std::clamp(Step, 1, 8) - 1)];
	}

	float FUiTheme::ColumnX(int Column) const
	{
		return Layout.SafeX + static_cast<float>(Column - 1) * (Layout.GridColumn + Layout.GridGutter);
	}

	float FUiTheme::SpanWidth(int Columns) const
	{
		return static_cast<float>(Columns) * (Layout.GridColumn + Layout.GridGutter) - Layout.GridGutter;
	}

	bool FUiTheme::SetColour(std::string_view Key, std::string_view HexText, float Alpha)
	{
		const int Index = ThemeIndexOf(ThemeColourKeys, Key);
		FUiColour Colour;
		if (Index < 0 || !ParseHexColour(HexText, Colour))
		{
			return false;
		}
		Colour.A = std::clamp(Alpha, 0.0f, 1.0f);
		Colours[static_cast<size_t>(Index)] = Colour;
		return true;
	}

	bool FUiTheme::SetType(std::string_view Key, const FUiTypeStyle& Style)
	{
		const int Index = ThemeIndexOf(ThemeTypeKeys, Key);
		if (Index < 0 || Style.Size <= 0.0f)
		{
			return false;
		}
		Types[static_cast<size_t>(Index)] = Style;
		return true;
	}

	bool FUiTheme::SetNumber(std::string_view Group, std::string_view Key, float Value)
	{
		for (const FThemeNumberField& Field : ThemeNumberFields)
		{
			if (Group == Field.Group && Key == Field.Key)
			{
				Field.Get(*this) = Value;
				return true;
			}
		}
		return false;
	}

	bool FUiTheme::SetTextScale(std::string_view Key, float Value)
	{
		const int Index = ThemeIndexOf(ThemeTextScaleKeys, Key);
		if (Index < 0 || Value <= 0.0f)
		{
			return false;
		}
		TextScale[static_cast<size_t>(Index)] = Value;
		return true;
	}

	std::vector<std::string> NumberKeys()
	{
		std::vector<std::string> Keys;
		for (const FThemeNumberField& Field : ThemeNumberFields)
		{
			Keys.push_back(std::string(Field.Group) + "." + Field.Key);
		}
		return Keys;
	}

	bool GetNumber(const FUiTheme& Theme, std::string_view Group, std::string_view Key, float& Out)
	{
		for (const FThemeNumberField& Field : ThemeNumberFields)
		{
			if (Group == Field.Group && Key == Field.Key)
			{
				Out = Field.Get(const_cast<FUiTheme&>(Theme));
				return true;
			}
		}
		return false;
	}
}
