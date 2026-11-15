#include "UI/PadelHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelScoreBugWidget.h"
#include "UI/PadelUiStyle.h"

namespace
{
	// Golden point choices: Team A's players (ids 0 and 1).
	constexpr int32 GoldenChoices[2] = {0, 1};
	// Where the timing cue sits until the player's screen position is known.
	constexpr float TimingCueBottomOffset = 260.0f;
}

void UPadelHudWidget::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const float Margin = PadelUiStyle::SafeMargin;

	ScoreBug = WidgetTree->ConstructWidget<UPadelScoreBugWidget>(UPadelScoreBugWidget::StaticClass());
	UOverlaySlot* BugSlot = Root->AddChildToOverlay(ScoreBug);
	BugSlot->SetHorizontalAlignment(HAlign_Left);
	BugSlot->SetVerticalAlignment(VAlign_Top);
	BugSlot->SetPadding(FMargin(Margin));

	struct FMessageLayout
	{
		EHorizontalAlignment H;
		EVerticalAlignment V;
		FMargin Padding;
		int32 FontSize;
	};
	const FMessageLayout Layouts[static_cast<int32>(EPadelHudMessage::Count)] = {
		{HAlign_Center, VAlign_Top, FMargin(0.0f, Margin + 140.0f, 0.0f, 0.0f), PadelUiStyle::FontTitle},
		{HAlign_Center, VAlign_Bottom, FMargin(0.0f, 0.0f, 0.0f, TimingCueBottomOffset), PadelUiStyle::FontBody},
		{HAlign_Left, VAlign_Bottom, FMargin(Margin, 0.0f, 0.0f, Margin), PadelUiStyle::FontBody},
	};
	for (const FMessageLayout& Layout : Layouts)
	{
		UBorder* Panel = MakePanel(PadelUiStyle::Panel, 12.0f);
		UTextBlock* Text = MakeText(FText::GetEmpty(), Layout.FontSize, PadelUiStyle::Text);
		Text->SetJustification(ETextJustify::Center);
		Panel->SetContent(Text);
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* MessageSlot = Root->AddChildToOverlay(Panel);
		MessageSlot->SetHorizontalAlignment(Layout.H);
		MessageSlot->SetVerticalAlignment(Layout.V);
		MessageSlot->SetPadding(Layout.Padding);
		MessagePanels.Add(Panel);
		MessageTexts.Add(Text);
		MessageTimeLeft.Add(0.0f);
	}

	// Change-over card: big type, centred.
	ScoreCard = MakePanel(PadelUiStyle::Panel, 36.0f);
	ScoreCard->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* CardSlot = Root->AddChildToOverlay(ScoreCard);
	CardSlot->SetHorizontalAlignment(HAlign_Center);
	CardSlot->SetVerticalAlignment(VAlign_Center);
	UVerticalBox* CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ScoreCard->SetContent(CardColumn);
	ScoreCardTitle = MakeText(FText::GetEmpty(), PadelUiStyle::FontBody, PadelUiStyle::Accent);
	ScoreCardTitle->SetJustification(ETextJustify::Center);
	CardColumn->AddChildToVerticalBox(ScoreCardTitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	for (int32 Team = 0; Team < 2; ++Team)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		CardColumn->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 8.0f));
		UBorder* Bar = MakePanel(PadelUiStyle::TeamColour(Team), 0.0f);
		USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetWidthOverride(14.0f);
		BarSize->SetContent(Bar);
		Row->AddChildToHorizontalBox(BarSize);
		UTextBlock* Names = MakeText(PadelUiStyle::ToText(Padel::Ui::PairLabel(Team)), PadelUiStyle::FontTitle, PadelUiStyle::Text);
		USizeBox* NameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NameSize->SetWidthOverride(720.0f);
		NameSize->SetContent(Names);
		Row->AddChildToHorizontalBox(NameSize)->SetPadding(FMargin(24.0f, 0.0f));
		UTextBlock* Score = MakeText(FText::GetEmpty(), PadelUiStyle::FontHuge, PadelUiStyle::Text);
		Row->AddChildToHorizontalBox(Score)->SetVerticalAlignment(VAlign_Center);
		ScoreCardRows.Add(Score);
	}

	// Golden point prompt: two big buttons, so stick left/right + A works through Slate's
	// own gamepad navigation (GDD 7 "Left stick + A").
	GoldenPrompt = MakePanel(PadelUiStyle::Panel, 24.0f);
	GoldenPrompt->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* GoldenSlot = Root->AddChildToOverlay(GoldenPrompt);
	GoldenSlot->SetHorizontalAlignment(HAlign_Center);
	GoldenSlot->SetVerticalAlignment(VAlign_Bottom);
	GoldenSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 360.0f));
	UVerticalBox* GoldenColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	GoldenPrompt->SetContent(GoldenColumn);
	UTextBlock* GoldenTitle = MakeText(NSLOCTEXT("PadelHud", "GoldenTitle", "GOLDEN POINT: who receives?"), PadelUiStyle::FontBody, PadelUiStyle::Accent);
	GoldenTitle->SetJustification(ETextJustify::Center);
	GoldenColumn->AddChildToVerticalBox(GoldenTitle)->SetHorizontalAlignment(HAlign_Center);
	UHorizontalBox* GoldenRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	GoldenColumn->AddChildToVerticalBox(GoldenRow)->SetPadding(FMargin(0.0f, 12.0f));
	for (const int32 PlayerId : GoldenChoices)
	{
		UButton* Choice = MakeMenuButton(PadelUiStyle::ToText(Padel::Ui::DisplayName(PlayerId)));
		GoldenRow->AddChildToHorizontalBox(Choice)->SetPadding(FMargin(16.0f, 0.0f));
		GoldenButtons.Add(Choice);
	}
	GoldenButtons[0]->OnClicked.AddDynamic(this, &UPadelHudWidget::HandleGoldenFirst);
	GoldenButtons[1]->OnClicked.AddDynamic(this, &UPadelHudWidget::HandleGoldenSecond);
	GoldenHint = MakeText(FText::GetEmpty(), PadelUiStyle::FontSmall, PadelUiStyle::TextMuted);
	GoldenColumn->AddChildToVerticalBox(GoldenHint)->SetHorizontalAlignment(HAlign_Center);
}

void UPadelHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickMessages(InDeltaTime);
}

void UPadelHudWidget::TickMessages(float DeltaSeconds)
{
	for (int32 Index = 0; Index < MessageTimeLeft.Num(); ++Index)
	{
		if (MessageTimeLeft[Index] > 0.0f)
		{
			MessageTimeLeft[Index] -= DeltaSeconds;
			if (MessageTimeLeft[Index] <= 0.0f)
			{
				MessagePanels[Index]->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	if (ScoreCardTimeLeft > 0.0f)
	{
		ScoreCardTimeLeft -= DeltaSeconds;
		if (ScoreCardTimeLeft <= 0.0f)
		{
			ScoreCard->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UPadelHudWidget::UpdateScore(const Padel::Ui::FScoreView& View)
{
	if (ScoreBug)
	{
		ScoreBug->UpdateScore(View);
	}
}

void UPadelHudWidget::ShowMessage(EPadelHudMessage Which, const FText& Text, const FLinearColor& Colour, float Seconds)
{
	const int32 Index = static_cast<int32>(Which);
	if (!MessagePanels.IsValidIndex(Index))
	{
		return;
	}
	MessageTexts[Index]->SetText(Text);
	MessageTexts[Index]->SetColorAndOpacity(FSlateColor(Colour));
	MessagePanels[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
	MessageTimeLeft[Index] = Seconds;
}

void UPadelHudWidget::ShowScoreCard(const FText& Title, const Padel::Ui::FScoreView& View, float Seconds)
{
	if (!ScoreCard)
	{
		return;
	}
	ScoreCardTitle->SetText(Title);
	for (int32 Team = 0; Team < 2; ++Team)
	{
		TArray<FString> Parts;
		for (const int Games : Padel::Ui::SetColumns(View, Team))
		{
			Parts.Add(FString::FromInt(Games));
		}
		ScoreCardRows[Team]->SetText(FText::FromString(FString::Join(Parts, TEXT("   "))));
	}
	ScoreCard->SetVisibility(ESlateVisibility::HitTestInvisible);
	ScoreCardTimeLeft = Seconds;
}

void UPadelHudWidget::OpenGoldenPrompt(int32 DefaultPlayerId, const FText& ConfirmHint)
{
	if (!GoldenPrompt)
	{
		return;
	}
	GoldenHint->SetText(ConfirmHint);
	GoldenPrompt->SetVisibility(ESlateVisibility::Visible);
	const int32 Index = DefaultPlayerId == GoldenChoices[1] ? 1 : 0;
	GoldenButtons[Index]->SetFocus();
}

void UPadelHudWidget::CloseGoldenPrompt()
{
	if (GoldenPrompt)
	{
		GoldenPrompt->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UPadelHudWidget::IsGoldenPromptOpen() const
{
	return GoldenPrompt && GoldenPrompt->GetVisibility() == ESlateVisibility::Visible;
}

void UPadelHudWidget::SetTimingCueScreenPosition(const FVector2D& ScreenPosition)
{
	const int32 Index = static_cast<int32>(EPadelHudMessage::TimingCue);
	if (!MessagePanels.IsValidIndex(Index))
	{
		return;
	}
	// The cue's slot sits bottom-centre; offset it so it appears above the player.
	const FVector2D Size = GetCachedGeometry().GetLocalSize();
	const FVector2D Default(Size.X * 0.5f, Size.Y - TimingCueBottomOffset);
	MessagePanels[Index]->SetRenderTranslation(ScreenPosition - Default - FVector2D(0.0f, 80.0f));
}

FString UPadelHudWidget::GetMessageText(EPadelHudMessage Which) const
{
	const int32 Index = static_cast<int32>(Which);
	if (!MessagePanels.IsValidIndex(Index) || MessagePanels[Index]->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return FString();
	}
	return MessageTexts[Index]->GetText().ToString();
}

FString UPadelHudWidget::GetScoreCardTitle() const
{
	return ScoreCard && ScoreCard->GetVisibility() != ESlateVisibility::Collapsed ? ScoreCardTitle->GetText().ToString() : FString();
}

void UPadelHudWidget::HandleGoldenFirst()
{
	ChooseGolden(0);
}

void UPadelHudWidget::HandleGoldenSecond()
{
	ChooseGolden(1);
}

void UPadelHudWidget::ChooseGolden(int32 Index)
{
	CloseGoldenPrompt();
	OnGoldenReceiverChosen.Broadcast(GoldenChoices[Index]);
}
