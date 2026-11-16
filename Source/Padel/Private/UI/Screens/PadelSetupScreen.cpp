#include "UI/Screens/PadelSetupScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/PadelShotData.h"
#include "UI/Model/PadelUiPlayerCard.h"
#include "UI/Model/PadelUiQuickMatch.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Widgets/PadelButton.h"
#include "UI/Widgets/PadelPlayerCard.h"
#include "UI/Widgets/PadelScreenHeader.h"
#include "UI/Widgets/PadelSegmented.h"
#include "UI/Widgets/PadelSelector.h"
#include "UI/Widgets/PadelToggle.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	// Spec 7.3's layout and the "Setup" board, in 1080p px. Panels sit on the grid: your
	// pair on C1-C6, the opponents and the rules on C8-C12, "VS" on C7.
	constexpr float SetupContentTop = 184.0f;
	constexpr float SetupContentBottom = 936.0f;
	constexpr float SetupPanelPad = 24.0f;
	constexpr float SetupPanelTitleH = 36.0f;          // Type.Heading's line
	constexpr float SetupPairCardsTop = 256.0f;        // inside the pair panel, under its title
	constexpr float SetupPairCardGap = 24.0f;
	constexpr float SetupVsCentreY = 520.0f;
	constexpr float SetupOpponentsBottom = 448.0f;
	constexpr float SetupOpponentsPadY = 14.0f;        // the board's tighter top padding
	constexpr float SetupOpponentCardsTop = 240.0f;
	constexpr float SetupOpponentCardGap = 16.0f;
	constexpr float SetupRulesTop = 472.0f;
	constexpr float SetupRulesListGap = 12.0f;         // under the panel title
	constexpr float SetupRulesRowGap = 4.0f;
	constexpr float SetupStartHeight = 64.0f;
	constexpr float SetupStartGap = 8.0f;              // between the last rule and Start
	constexpr float SetupHeaderGap = 16.0f;            // "LAGOON  vs  EMBER"

	FText SetupTeamName(int32 Team)
	{
		return FText::FromString(UTF8_TO_TCHAR(Padel::Ui::TeamName(Team).c_str()));
	}

	/** Data/Players.json's row for a player, through the same loader as the match. */
	Padel::Ui::FPlayerCardView SetupLoadCard(int32 PlayerId)
	{
		Padel::Shots::FPlayerRatings Ratings;
		const std::string Row = Padel::Ui::PlayerRowName(PlayerId);
		const bool bLoaded = !Row.empty() && FPadelShotData::LoadPlayer(FName(UTF8_TO_TCHAR(Row.c_str())), Ratings);
		return Padel::Ui::MakePlayerCard(PlayerId, bLoaded ? &Ratings : nullptr, PadelUi::Theme().PlayerCardStats);
	}

	FText SetupFormatDescription(int32 Index)
	{
		return Index == 0
			? LOCTEXT("Setup.Format.OneSet.Desc", "First to 6 games, by two clear. Tie-break at 6–6.")
			: LOCTEXT("Setup.Format.BestOf3.Desc", "First to win 2 sets. Each set is first to 6 games, by two clear, with a tie-break at 6–6.");
	}

	FText SetupGoldenPointDescription(bool bOn)
	{
		return bOn ? LOCTEXT("Setup.GoldenPoint.On.Desc", "At deuce, one deciding point. The receiving pair picks who receives.")
			: LOCTEXT("Setup.GoldenPoint.Off.Desc", "Advantage scoring: win by two points from deuce.");
	}
}

Padel::Ui::FMatchSetup& UPadelSetupScreen::Setup()
{
	IPadelMenuHost* Host = GetHost();
	return Host ? Host->GetMenuMatchSetup() : LocalSetup;
}

UPadelPlayerCard* UPadelSetupScreen::GetPairCard(int32 Index) const
{
	return PairCards.IsValidIndex(Index) ? PairCards[Index].Get() : nullptr;
}

UPadelPlayerCard* UPadelSetupScreen::GetOpponentCard(int32 Index) const
{
	return OpponentCards.IsValidIndex(Index) ? OpponentCards[Index].Get() : nullptr;
}

void UPadelSetupScreen::BuildScreen()
{
	SetHeader(LOCTEXT("Setup.Title", "Quick Match"));
	BuildHeaderMatchup();
	BuildPairPanel();
	BuildOpponentsPanel();
	BuildRulesPanel();
	FocusMap = Padel::Ui::SetupFocusMap();
	UpdateCardRoles(false);
	UpdateDescriptions();
}

void UPadelSetupScreen::BuildHeaderMatchup()
{
	UPadelScreenHeader* Head = GetHeader();
	if (!Head)
	{
		return;
	}
	// Spec 7.3: "LAGOON  vs  EMBER" in team colours, right of the title.
	UHorizontalBox* Matchup = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Lagoon = MakeTokenText(SetupTeamName(0), EUiType::Heading, EUiColour::TeamLagoon);
	UTextBlock* Vs = MakeTokenText(LOCTEXT("Versus.Vs", "vs"), EUiType::Heading, EUiColour::TextSecondary);
	UTextBlock* Ember = MakeTokenText(SetupTeamName(1), EUiType::Heading, EUiColour::TeamEmber);
	Matchup->AddChildToHorizontalBox(Lagoon)->SetVerticalAlignment(VAlign_Center);
	Matchup->AddChildToHorizontalBox(Vs)->SetPadding(FMargin(SetupHeaderGap, 0.0f));
	Matchup->AddChildToHorizontalBox(Ember)->SetVerticalAlignment(VAlign_Center);
	Head->EnsureBuilt();
	Head->SetRightContent(Matchup);
}

UPadelPlayerCard* UPadelSetupScreen::MakeCard(int32 PlayerId, bool bLarge)
{
	UPadelPlayerCard* Card = WidgetTree->ConstructWidget<UPadelPlayerCard>(UPadelPlayerCard::StaticClass());
	Card->SetLarge(bLarge);
	Card->SetCard(SetupLoadCard(PlayerId));
	return Card;
}

void UPadelSetupScreen::BuildPairPanel()
{
	const float Left = PadelUi::ColumnX(1);
	const float Width = PadelUi::SpanWidth(6);
	UTextBlock* Title = MakeTokenText(FText::Format(LOCTEXT("Setup.YourPair", "Your pair · {0}"), SetupTeamName(0)),
		EUiType::Heading, EUiColour::TeamLagoon);
	Place(MakeGlassPanel(Title, FMargin(SetupPanelPad), PadelUi::Theme().Layout.RadiusPanel), Left, SetupContentTop, Width,
		SetupContentBottom - SetupContentTop);

	const FVector2D CardSize = UPadelPlayerCard::LargeSize();
	const std::array<int, 2> Order = Padel::Ui::SetupPairOrder();
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UPadelPlayerCard* Card = MakeCard(Order[Index], true);
		// Spec 7.3: a click on either large card is the swap, like X.
		Card->OnCardClicked.AddWeakLambda(this, [this](UPadelPlayerCard*)
		{
			SwapPlayer();
		});
		Place(Card, Left + SetupPanelPad + Index * (CardSize.X + SetupPairCardGap), SetupPairCardsTop, CardSize.X, CardSize.Y);
		PairCards.Add(Card);
	}

	// "VS" between the two sides, centred on column 7.
	UTextBlock* Vs = MakeTokenText(LOCTEXT("Versus.Vs", "vs"), EUiType::Display, EUiColour::TextSecondary);
	Vs->SetJustification(ETextJustify::Center);
	const float VsHeight = PadelUi::Theme().Type(EUiType::Display).LineHeight;
	Place(Vs, PadelUi::ColumnX(7), SetupVsCentreY - VsHeight * 0.5f, PadelUi::SpanWidth(1), VsHeight);
}

void UPadelSetupScreen::BuildOpponentsPanel()
{
	const float Left = PadelUi::ColumnX(8);
	const float Width = PadelUi::SpanWidth(5);
	UTextBlock* Title = MakeTokenText(FText::Format(LOCTEXT("Setup.Opponents", "Opponents · {0}"), SetupTeamName(1)),
		EUiType::Heading, EUiColour::TeamEmber);
	Place(MakeGlassPanel(Title, FMargin(SetupPanelPad, SetupOpponentsPadY), PadelUi::Theme().Layout.RadiusPanel), Left,
		SetupContentTop, Width, SetupOpponentsBottom - SetupContentTop);

	const FVector2D CardSize = UPadelPlayerCard::SmallSize();
	const std::array<int, 2> Order = Padel::Ui::SetupOpponentOrder();
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UPadelPlayerCard* Card = MakeCard(Order[Index], false);
		Place(Card, Left + SetupPanelPad + Index * (CardSize.X + SetupOpponentCardGap), SetupOpponentCardsTop, CardSize.X,
			CardSize.Y);
		OpponentCards.Add(Card);
	}
}

void UPadelSetupScreen::BuildRulesPanel()
{
	const float Left = PadelUi::ColumnX(8);
	const float Width = PadelUi::SpanWidth(5);
	const float Inner = Width - 2.0f * SetupPanelPad;
	UTextBlock* Title = MakeTokenText(LOCTEXT("Setup.Rules", "Match rules"), EUiType::Heading, EUiColour::TextPrimary);
	Place(MakeGlassPanel(Title, FMargin(SetupPanelPad), PadelUi::Theme().Layout.RadiusPanel), Left, SetupRulesTop, Width,
		SetupContentBottom - SetupRulesTop);

	Padel::Ui::FMatchSetup& Current = Setup();
	Difficulty = MakeElement<UPadelSelector>(FName(Padel::Ui::SetupDifficultyId));
	Difficulty->SetLabel(LOCTEXT("Setup.Difficulty", "Difficulty"));
	Difficulty->SetOptions(
		{LOCTEXT("Setup.Difficulty.Easy", "Easy"), LOCTEXT("Setup.Difficulty.Medium", "Medium"), LOCTEXT("Setup.Difficulty.Hard", "Hard")},
		{LOCTEXT("Setup.Difficulty.Easy.Desc", "The AI reads your shots late and plays it safe."),
			LOCTEXT("Setup.Difficulty.Medium.Desc", "A fair match. The AI plays smart padel and makes some mistakes."),
			LOCTEXT("Setup.Difficulty.Hard.Desc", "The AI reads the ball early, attacks the net and rarely misses.")});
	Difficulty->SetIndex(Padel::Ui::DifficultyIndex(Current));
	Difficulty->SetRowWidth(Inner);

	Format = MakeElement<UPadelSegmented>(FName(Padel::Ui::SetupFormatId));
	Format->SetLabel(LOCTEXT("Setup.Format", "Format"));
	Format->SetOptions({LOCTEXT("Setup.Format.OneSet", "1 set"), LOCTEXT("Setup.Format.BestOf3", "Best of 3")});
	Format->SetIndex(Padel::Ui::FormatIndex(Current));
	Format->SetRowWidth(Inner);

	GoldenPoint = MakeElement<UPadelToggle>(FName(Padel::Ui::SetupGoldenPointId));
	GoldenPoint->SetLabel(LOCTEXT("Setup.GoldenPoint", "Golden point"));
	GoldenPoint->SetOn(Current.bGoldenPoint);
	GoldenPoint->SetRowWidth(Inner);

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Rows->AddChildToVerticalBox(Difficulty);
	Rows->AddChildToVerticalBox(Format)->SetPadding(FMargin(0.0f, SetupRulesRowGap, 0.0f, 0.0f));
	Rows->AddChildToVerticalBox(GoldenPoint)->SetPadding(FMargin(0.0f, SetupRulesRowGap, 0.0f, 0.0f));
	// The focused row's description line pushes the rows below it down (board
	// "SetupDifficulty"). At Text size Largest a long one could reach Start, so the list
	// shrinks to fit its band instead of overlapping (spec 10).
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Fit->SetStretch(EStretch::ScaleToFit);
	Fit->SetStretchDirection(EStretchDirection::DownOnly);
	if (UScaleBoxSlot* FitSlot = Cast<UScaleBoxSlot>(Fit->SetContent(Rows)))
	{
		FitSlot->SetHorizontalAlignment(HAlign_Fill);
		FitSlot->SetVerticalAlignment(VAlign_Top);
	}
	const float ListTop = SetupRulesTop + SetupPanelPad + SetupPanelTitleH + SetupRulesListGap;
	const float StartTop = SetupContentBottom - SetupPanelPad - SetupStartHeight;
	Place(Fit, Left + SetupPanelPad, ListTop, Inner, StartTop - SetupStartGap - ListTop);

	StartButton = MakeElement<UPadelButton>(FName(Padel::Ui::SetupStartId));
	StartButton->SetKind(EPadelButtonKind::Primary);
	StartButton->SetLabel(LOCTEXT("Setup.Start", "Start match"));
	StartButton->SetSize(Inner, SetupStartHeight);
	StartButton->SetShowGlyph(true, GetPadFamily());
	Place(StartButton, Left + SetupPanelPad, StartTop, Inner, SetupStartHeight);
}

void UPadelSetupScreen::UpdateCardRoles(bool bAnimate)
{
	const Padel::Ui::FMatchSetup& Current = Setup();
	for (UPadelPlayerCard* Card : PairCards)
	{
		if (Card)
		{
			Card->SetRole(Card->GetPlayerId() == Current.HumanPlayerId ? EPadelCardRole::You : EPadelCardRole::Partner, bAnimate);
		}
	}
	for (UPadelPlayerCard* Card : OpponentCards)
	{
		if (Card)
		{
			Card->SetRole(EPadelCardRole::None, false);
		}
	}
}

void UPadelSetupScreen::UpdateDescriptions()
{
	if (Format)
	{
		Format->SetDescription(SetupFormatDescription(Format->GetIndex()));
	}
	if (GoldenPoint)
	{
		GoldenPoint->SetDescription(SetupGoldenPointDescription(GoldenPoint->IsOn()));
	}
}

void UPadelSetupScreen::SwapPlayer()
{
	Setup().CyclePlayer();
	UpdateCardRoles(true);
	if (UPadelMenuRoot* Root = GetMenuRoot())
	{
		Root->PlayValueCue();
	}
}

void UPadelSetupScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	// The rules rows handle A themselves (the selector steps, the toggle flips); only
	// Start leaves. Spec 7.3: Start goes to the Versus card.
	if (Element && Element == StartButton)
	{
		if (UPadelMenuRoot* Root = GetMenuRoot())
		{
			Root->ShowScreen(EPadelScreen::Versus);
		}
	}
}

void UPadelSetupScreen::OnElementValueChanged(UPadelFocusableWidget* Element)
{
	Padel::Ui::FMatchSetup& Current = Setup();
	if (Element == Difficulty)
	{
		Padel::Ui::SetDifficultyIndex(Current, Difficulty->GetIndex());
	}
	else if (Element == Format)
	{
		Padel::Ui::SetFormatIndex(Current, Format->GetIndex());
	}
	else if (Element == GoldenPoint)
	{
		Current.bGoldenPoint = GoldenPoint->IsOn();
	}
	UpdateDescriptions();
}

bool UPadelSetupScreen::OnScreenInput(EUiInput Input, bool bRepeat)
{
	// Spec 7.3: X swaps the player whatever is focused. A held X doesn't keep swapping.
	if (Input == EUiInput::Secondary)
	{
		if (!bRepeat)
		{
			SwapPlayer();
		}
		return true;
	}
	return false;
}

TArray<FPadelHint> UPadelSetupScreen::GetHints() const
{
	// Spec 7.3: A Start match (A Change on a rules row), X Swap player, B Back.
	TArray<FPadelHint> Hints;
	const bool bOnStart = GetFocusedId() == FName(Padel::Ui::SetupStartId);
	Hints.Add({EUiInput::Confirm, bOnStart ? LOCTEXT("Hint.StartMatch", "Start match") : LOCTEXT("Hint.Change", "Change")});
	Hints.Add({EUiInput::Secondary, LOCTEXT("Hint.Swap", "Swap player")});
	Hints.Add({EUiInput::Back, LOCTEXT("Hint.Back", "Back")});
	return Hints;
}

#undef LOCTEXT_NAMESPACE
