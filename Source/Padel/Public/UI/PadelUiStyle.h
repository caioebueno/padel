// The broadcast look (GDD 10 "UI: a clean broadcast look: a compact score bug in one
// corner, big type for the score on change-overs"). One strong colour per team (GDD 10
// readability). An original palette; swap for real fonts and art when the UI pass lands.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"

#include <string>

namespace PadelUiStyle
{
	inline const FLinearColor TeamColours[2] = {FLinearColor(FColor(0x2E, 0xC4, 0xB6)), FLinearColor(FColor(0xFF, 0x7A, 0x3D))};
	inline const FLinearColor BugBackground(0.02f, 0.03f, 0.05f, 0.88f);
	inline const FLinearColor Panel(0.015f, 0.02f, 0.035f, 0.94f);
	inline const FLinearColor ButtonNormal(0.06f, 0.08f, 0.11f, 1.0f);
	inline const FLinearColor ButtonFocused(0.16f, 0.2f, 0.26f, 1.0f);
	inline const FLinearColor PointsBackground(FColor(0xF4, 0xF1, 0xE8));
	inline const FLinearColor PointsText(FColor(0x12, 0x16, 0x1C));
	inline const FLinearColor Text(FColor(0xF4, 0xF1, 0xE8));
	inline const FLinearColor TextMuted(0.9f, 0.88f, 0.84f, 0.55f);
	inline const FLinearColor Accent(FColor(0xFF, 0xD2, 0x3F));
	inline const FLinearColor Good(FColor(0x7C, 0xFF, 0x6B));
	inline const FLinearColor Warning(FColor(0xFF, 0xB3, 0x47));
	inline const FLinearColor Bad(FColor(0xFF, 0x5D, 0x5D));

	inline constexpr int32 FontSmall = 22;
	inline constexpr int32 FontBody = 28;
	inline constexpr int32 FontTitle = 56;
	inline constexpr int32 FontHuge = 96;
	inline constexpr float SafeMargin = 48.0f;

	inline FSlateFontInfo Font(int32 Size, bool bBold = true)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	inline FLinearColor TeamColour(int32 Team)
	{
		return (Team == 0 || Team == 1) ? TeamColours[Team] : Text;
	}

	/** The UI model speaks UTF-8 std::string; widgets want FText. */
	inline FText ToText(const std::string& Utf8)
	{
		return FText::FromString(FString(UTF8_TO_TCHAR(Utf8.c_str())));
	}

	inline FString ToFString(const std::string& Utf8)
	{
		return FString(UTF8_TO_TCHAR(Utf8.c_str()));
	}

	inline std::string ToUtf8(const FString& Source)
	{
		return std::string(TCHAR_TO_UTF8(*Source));
	}
}
