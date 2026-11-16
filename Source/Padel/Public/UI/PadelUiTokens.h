// The menu design tokens on the engine side (docs/ui/MENU_SPEC.md 4, 9): loads
// Data/UiTheme.json into Padel::Ui::FUiTheme once, and hands widgets FLinearColors and
// Barlow FSlateFontInfos. Every menu widget takes its look from here, never from numbers
// of its own (CLAUDE.md: no hard-coded tuning numbers).
//
// The in-match HUD still uses PadelUiStyle.h until the HUD pass (spec 7.11).

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "UI/Model/PadelUiTheme.h"

namespace PadelUi
{
	using Padel::Ui::EUiColour;
	using Padel::Ui::EUiType;

	/** The loaded theme (spec 11.2); falls back to the spec's defaults, with one warning. */
	const Padel::Ui::FUiTheme& Theme();
	/** Re-read Data/UiTheme.json (tests, live tuning). Broadcasts OnOptionsChanged. */
	void ReloadTheme();
	/** Load from a given file; false (and the defaults) if it's missing or invalid. */
	bool LoadThemeFile(const FString& Path, Padel::Ui::FUiTheme& OutTheme);

	FLinearColor Colour(EUiColour Token);
	FLinearColor Colour(EUiColour Token, float AlphaScale);
	/** Team.Lagoon for team 0, Team.Ember for team 1, Text.Primary otherwise. */
	FLinearColor TeamColour(int32 Team);

	/** The token's Barlow face at its size (Text size applied where it scales), in Slate units. */
	FSlateFontInfo Font(EUiType Token);
	/** The token's face and tracking at another pixel size (e.g. the Hero tile's 56 px label). */
	FSlateFontInfo FontAtPx(EUiType Token, float Px);
	bool IsUpper(EUiType Token);

	/**
	 * UMG font sizes are points at 96 DPI, so size N draws N * 4/3 px tall. The spec gives
	 * px at 1080p; this converts.
	 */
	inline float PxToFontSize(float Px)
	{
		return Px * 0.75f;
	}

	/** True when Barlow loaded from SourceArt/Fonts; false means the engine font stands in. */
	bool HasBarlow();

	/** Settings that change how every menu draws (spec 7.7). Persisted by the Settings screen's owner. */
	struct FUiOptions
	{
		Padel::Ui::ETextSize TextSize = Padel::Ui::ETextSize::Default;
		bool bReduceMotion = false;
		bool bReduceTransparency = false;
	};

	const FUiOptions& Options();
	/** Applies at once; screens rebuild on OnOptionsChanged (Text size must hold at Largest). */
	void SetOptions(const FUiOptions& NewOptions);
	FSimpleMulticastDelegate& OnOptionsChanged();

	/** Grid helpers in px (spec 4.3): column C's left edge, a span of N columns. */
	inline float ColumnX(int32 Column) { return Theme().ColumnX(Column); }
	inline float SpanWidth(int32 Columns) { return Theme().SpanWidth(Columns); }
	inline float Space(int32 Step) { return Theme().Space(Step); }
}
