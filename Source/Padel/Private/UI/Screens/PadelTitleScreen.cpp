#include "UI/Screens/PadelTitleScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/Model/PadelUiFrontEnd.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiStyle.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Screens/PadelTitleCatcher.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	// Board "Title" (1080p px). Text tops are the boards' line-box tops: the wordmark's
	// 108 px line centred on y 360 (spec 7.1), the tagline on 492, the prompt on 820.
	constexpr float TitleScreenWidth = 1920.0f;
	constexpr float TitleLogoTop = 306.0f;
	constexpr float TitleTaglineTop = 474.0f;
	constexpr float TitlePromptTop = 802.0f;
	// The lime stroke under STREET: 6 px tall, 8 px under the line box, inset 4 px on the
	// left and 12 px on the right to sit under the italic letters.
	constexpr float TitleUnderlineHeight = 6.0f;
	constexpr float TitleUnderlineGap = 8.0f;
	constexpr float TitleUnderlineInsetLeft = 4.0f;
	constexpr float TitleUnderlineInsetRight = 12.0f;
	// A word space at Type.Hero between PADEL and STREET.
	constexpr float TitleWordGap = 28.0f;

	const FName TitleContinueId(TEXT("Title.Continue"));

	/** A widget centred across the full screen width. */
	UOverlay* TitleCentred(UWidgetTree* Tree, UWidget* Content)
	{
		UOverlay* Row = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UOverlaySlot* RowSlot = Row->AddChildToOverlay(Content);
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetVerticalAlignment(VAlign_Top);
		// Clicks pass through the text to the catcher behind it.
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Row;
	}

	FText TitleBuildVersion()
	{
		FString Version;
		if (!GConfig || !GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni)
			|| Version.IsEmpty())
		{
			Version = TEXT("dev");
		}
		return FText::FromString(Version);
	}
}

void UPadelTitleScreen::BuildScreen()
{
	SetHeaderVisible(false);
	SetHintBarVisible(false);

	// The catcher first, so it sits behind the text and takes clicks anywhere.
	UPadelTitleCatcher* Catcher = MakeElement<UPadelTitleCatcher>(TitleContinueId);
	Place(Catcher, 0.0f, 0.0f, TitleScreenWidth, 1080.0f);

	// The working-title wordmark in our own font, not a logo asset (spec 7.1, GDD 13).
	UHorizontalBox* Logo = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* First = MakeTokenText(LOCTEXT("Title.Logo.First", "Padel"), EUiType::Hero, EUiColour::TextPrimary);
	Logo->AddChildToHorizontalBox(First)->SetPadding(FMargin(0.0f, 0.0f, TitleWordGap, 0.0f));
	UVerticalBox* SecondWord = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SecondWord->AddChildToVerticalBox(MakeTokenText(LOCTEXT("Title.Logo.Second", "Street"), EUiType::Hero, EUiColour::TextPrimary));
	Underline = MakeImage(PadelUiPaint::Solid(PadelUi::Colour(EUiColour::Accent)));
	// Draws left to right: scale X from the left edge.
	Underline->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
	USizeBox* UnderlineBox = MakeSized(Underline, 0.0f, TitleUnderlineHeight);
	UVerticalBoxSlot* UnderlineSlot = SecondWord->AddChildToVerticalBox(UnderlineBox);
	UnderlineSlot->SetHorizontalAlignment(HAlign_Fill);
	UnderlineSlot->SetPadding(FMargin(TitleUnderlineInsetLeft, TitleUnderlineGap, TitleUnderlineInsetRight, 0.0f));
	Logo->AddChildToHorizontalBox(SecondWord);
	Place(TitleCentred(WidgetTree, Logo), 0.0f, TitleLogoTop, TitleScreenWidth, 0.0f);

	UTextBlock* Tagline = MakeTokenText(LOCTEXT("Title.Tagline", "2v2 padel"), EUiType::Heading, EUiColour::TextSecondary);
	Place(TitleCentred(WidgetTree, Tagline), 0.0f, TitleTaglineTop, TitleScreenWidth, 0.0f);

	Prompt = MakeTokenText(FText::GetEmpty(), EUiType::Heading, EUiColour::TextPrimary);
	Place(TitleCentred(WidgetTree, Prompt), 0.0f, TitlePromptTop, TitleScreenWidth, 0.0f);

	// Bottom-left of the safe area. The board draws it in Text.Secondary (board wins on looks).
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	UTextBlock* BuildLine = MakeTokenText(FText::Format(LOCTEXT("Title.Build", "Vertical slice · build {0}"), TitleBuildVersion()),
		EUiType::BodySmall, EUiColour::TextSecondary);
	BuildLine->SetVisibility(ESlateVisibility::HitTestInvisible);
	const float BuildLineHeight = Theme.Type(EUiType::BodySmall).LineHeight;
	Place(BuildLine, Theme.Layout.SafeX, 1080.0f - Theme.Layout.SafeY - BuildLineHeight, 0.0f, 0.0f, true);

	FocusMap = Padel::Ui::FFocusMap::VerticalList({TCHAR_TO_UTF8(*TitleContinueId.ToString())});
	FocusMap.SetDefault(TCHAR_TO_UTF8(*TitleContinueId.ToString()));

	bPadPrompt = ShowsPadPrompt();
	ApplyPrompt();
}

bool UPadelTitleScreen::ShowsPadPrompt() const
{
	const bool bAttached = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsGamepadAttached();
	const Padel::Ui::EPadFamily Family = GetMenuRoot() ? GetMenuRoot()->GetPadFamily() : GetPadFamily();
	return Padel::Ui::TitleShowsPadPrompt(Family, bAttached);
}

FText UPadelTitleScreen::GetPromptText() const
{
	return bPadPrompt ? LOCTEXT("Title.PressAnyButton", "Press any button") : LOCTEXT("Title.PressAnyKey", "Press any key");
}

void UPadelTitleScreen::ApplyPrompt()
{
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	const bool bReduceMotion = PadelUi::Options().bReduceMotion;
	if (Prompt)
	{
		Prompt->SetText(GetPromptText());
		// Spec 7.1: the prompt turns Accent on the press that continues; until then it pulses.
		Prompt->SetColorAndOpacity(FSlateColor(PadelUi::Colour(bLeaving ? EUiColour::Accent : EUiColour::TextPrimary)));
		Prompt->SetRenderOpacity(bLeaving ? 1.0f : Padel::Ui::PulseAlpha(Seconds, Motion.Pulse, Motion.PulseMinAlpha, bReduceMotion));
	}
	if (Underline)
	{
		const float Drawn = Padel::Ui::UnderlineProgress(Seconds, Motion.TitleUnderline, bReduceMotion);
		Underline->SetRenderScale(FVector2D(Drawn, 1.0f));
	}
}

void UPadelTitleScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceTitle(InDeltaTime);
}

void UPadelTitleScreen::AdvanceTitle(float DeltaSeconds)
{
	Seconds += DeltaSeconds;
	// A pad plugged in (or used) while the title shows changes the wording at once.
	bPadPrompt = ShowsPadPrompt();
	ApplyPrompt();
}

void UPadelTitleScreen::Continue()
{
	UPadelMenuRoot* Root = GetMenuRoot();
	if (bLeaving || !Root)
	{
		return;
	}
	// The root already took the device family from this press (spec 5.5). The prompt's
	// Accent flash and the logo's exit (spec 7.1) need an outgoing-screen animation the
	// root doesn't have yet, so the main menu's own entry carries the move.
	bLeaving = true;
	ApplyPrompt();
	Root->PlayConfirmCue();
	Root->ShowScreen(EPadelScreen::MainMenu);
}

bool UPadelTitleScreen::OnAnyKey(const FString& KeyName)
{
	if (!Padel::Ui::CountsAsAnyButton(PadelUiStyle::ToUtf8(KeyName)))
	{
		return false;
	}
	Continue();
	return true;
}

void UPadelTitleScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	Continue();
}

bool UPadelTitleScreen::OnBack()
{
	// No B on the title (spec 3), but B is a button like any other: it continues.
	Continue();
	return true;
}

bool UPadelTitleScreen::OnScreenInput(EUiInput Input, bool bRepeat)
{
	if (bRepeat)
	{
		return false;
	}
	Continue();
	return true;
}

TArray<FPadelHint> UPadelTitleScreen::GetHints() const
{
	// The hint bar is hidden (spec 7.1); this names what A does for the tests and tools.
	return {{EUiInput::Confirm, LOCTEXT("Hint.Continue", "Continue")}};
}

#undef LOCTEXT_NAMESPACE
