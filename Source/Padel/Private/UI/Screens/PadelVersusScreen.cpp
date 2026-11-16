#include "UI/Screens/PadelVersusScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Widgets/PadelBadge.h"
#include "UI/Widgets/PadelLoadingIndicator.h"
#include "UI/Widgets/PadelSilhouette.h"
#include "UI/Widgets/PadelSkipArea.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	// Spec 7.4 and the "Versus" board, in 1080p px.
	constexpr const char* VersusSkipId = "Versus.Skip";
	constexpr float VersusBlockTop = 380.0f;
	constexpr float VersusBlockH = 320.0f;
	constexpr int32 VersusBlockSpan = 5;               // C1-C5 and C8-C12
	constexpr float VersusTeamBar = 8.0f;
	constexpr float VersusTextSide = 40.0f;
	constexpr float VersusTextBottom = 48.0f;
	constexpr float VersusNameGap = 16.0f;
	constexpr float VersusFiguresW = 240.0f;           // two figures at 0.75, overlapping
	constexpr float VersusFigureW = 150.0f;
	constexpr float VersusFigureH = 210.0f;
	constexpr float VersusFigureStep = 90.0f;
	constexpr float VersusFiguresSide = 24.0f;
	constexpr float VersusVsLeft = 810.0f;
	constexpr float VersusVsTop = 472.0f;
	constexpr float VersusVsW = 300.0f;
	constexpr float VersusVsPx = 96.0f;                // Type.Hero at 96 px
	constexpr float VersusRulesTop = 742.0f;

	FText VersusSurname(int32 PlayerId)
	{
		return FText::FromString(UTF8_TO_TCHAR(Padel::Ui::Surname(PlayerId).c_str()));
	}

	FText VersusDifficulty(Padel::Ui::EDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case Padel::Ui::EDifficulty::Easy: return LOCTEXT("Setup.Difficulty.Easy", "Easy");
		case Padel::Ui::EDifficulty::Hard: return LOCTEXT("Setup.Difficulty.Hard", "Hard");
		case Padel::Ui::EDifficulty::Medium: break;
		}
		return LOCTEXT("Setup.Difficulty.Medium", "Medium");
	}
}

void UPadelVersusScreen::BuildScreen()
{
	SetHeaderVisible(false);
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	Clock.CardSeconds = Theme.Input.VersusSeconds;
	Clock.LoadingAfter = Theme.Input.LoadingIndicatorAfter;
	Clock.StartTimeout = Theme.Input.MatchStartTimeout;

	// Under everything: A, Enter or a click anywhere is "Skip" (spec 5.1).
	SkipArea = MakeElement<UPadelSkipArea>(FName(VersusSkipId));
	Place(SkipArea, 0.0f, 0.0f, 1920.0f, 1080.0f);

	const float BlockW = PadelUi::SpanWidth(VersusBlockSpan);
	TeamBlocks.Add(Place(MakeTeamBlock(0), PadelUi::ColumnX(1), VersusBlockTop, BlockW, VersusBlockH));
	TeamBlocks.Add(Place(MakeTeamBlock(1), PadelUi::ColumnX(8), VersusBlockTop, BlockW, VersusBlockH));
	// Nothing on the card takes the mouse, so a click anywhere reaches the skip area.
	for (UWidget* Block : TeamBlocks)
	{
		Block->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	VsText = MakeTokenText(LOCTEXT("Versus.Vs", "vs"), EUiType::Hero, EUiColour::TextSecondary);
	VsText->SetFont(PadelUi::FontAtPx(EUiType::Hero, VersusVsPx));
	VsText->SetJustification(ETextJustify::Center);
	VsText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	VsText->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(VsText, VersusVsLeft, VersusVsTop, VersusVsW, VersusVsPx);

	RulesText = MakeTokenText(GetRulesLine(), EUiType::Heading, EUiColour::TextSecondary);
	RulesText->SetJustification(ETextJustify::Center);
	RulesText->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(RulesText, Theme.Layout.SafeX, VersusRulesTop, 1920.0f - 2.0f * Theme.Layout.SafeX,
		Theme.Type(EUiType::Heading).LineHeight);

	// Spec 6.16: bottom right of the safe area, in the hint bar's band, which it replaces.
	UBorder* LoadingAlign = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LoadingAlign->SetBrush(PadelUiPaint::Solid(FLinearColor::Transparent));
	LoadingAlign->SetPadding(FMargin(0.0f));
	LoadingAlign->SetHorizontalAlignment(HAlign_Right);
	LoadingAlign->SetVerticalAlignment(VAlign_Center);
	LoadingAlign->SetContent(WidgetTree->ConstructWidget<UPadelLoadingIndicator>(UPadelLoadingIndicator::StaticClass()));
	LoadingBox = Place(LoadingAlign, Theme.Layout.SafeX, 960.0f, 1920.0f - 2.0f * Theme.Layout.SafeX, Theme.Layout.HintBarHeight, true);
	LoadingBox->SetVisibility(ESlateVisibility::Collapsed);

	FocusMap = Padel::Ui::FFocusMap::VerticalList({VersusSkipId});
	FocusMap.SetDefault(VersusSkipId);
	ApplyEntryMotion();
}

UWidget* UPadelVersusScreen::MakeTeamBlock(int32 Team)
{
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	const bool bLeft = Team == 0;
	const float Radius = Theme.Layout.RadiusPanel;
	UOverlay* Block = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	UOverlay* Glass = MakeGlassPanel(nullptr, FMargin(0.0f), Radius);
	UOverlaySlot* GlassSlot = Block->AddChildToOverlay(Glass);
	GlassSlot->SetHorizontalAlignment(HAlign_Fill);
	GlassSlot->SetVerticalAlignment(VAlign_Fill);

	// Two team-colour figures at the outer side, standing on the block's bottom edge.
	UCanvasPanel* Figures = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UPadelSilhouette* Figure = WidgetTree->ConstructWidget<UPadelSilhouette>(UPadelSilhouette::StaticClass());
		Figure->SetTeam(Team);
		UCanvasPanelSlot* FigureSlot = Figures->AddChildToCanvas(Figure);
		FigureSlot->SetPosition(FVector2D(Index * VersusFigureStep, 0.0f));
		FigureSlot->SetSize(FVector2D(VersusFigureW, VersusFigureH));
	}
	UOverlaySlot* FiguresSlot = Block->AddChildToOverlay(MakeSized(Figures, VersusFiguresW, VersusFigureH));
	FiguresSlot->SetHorizontalAlignment(bLeft ? HAlign_Right : HAlign_Left);
	FiguresSlot->SetVerticalAlignment(VAlign_Bottom);
	FiguresSlot->SetPadding(bLeft ? FMargin(0.0f, 0.0f, VersusFiguresSide, 0.0f) : FMargin(VersusFiguresSide, 0.0f, 0.0f, 0.0f));

	// The 8 px team bar along the top.
	UImage* Bar = MakeImage(FSlateRoundedBoxBrush(PadelUi::TeamColour(Team), FVector4(Radius, Radius, 0.0f, 0.0f)));
	UOverlaySlot* BarSlot = Block->AddChildToOverlay(MakeSized(Bar, 0.0f, VersusTeamBar));
	BarSlot->SetHorizontalAlignment(HAlign_Fill);
	BarSlot->SetVerticalAlignment(VAlign_Top);

	// The team name, then the pair: the human first with the YOU badge (spec 7.4).
	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* TeamName = MakeTokenText(FText::FromString(UTF8_TO_TCHAR(Padel::Ui::TeamName(Team).c_str())), EUiType::Display,
		bLeft ? EUiColour::TeamLagoon : EUiColour::TeamEmber);
	Text->AddChildToVerticalBox(TeamName)->SetHorizontalAlignment(bLeft ? HAlign_Left : HAlign_Right);

	UHorizontalBox* Names = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	IPadelMenuHost* Host = GetHost();
	const Padel::Ui::FMatchSetup Current = Host ? Host->GetMenuMatchSetup() : Padel::Ui::FMatchSetup();
	const std::array<int, 2> Order = Padel::Ui::VersusPairOrder(Team, Current);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (Index > 0)
		{
			UTextBlock* Dot = MakeTokenText(FText::FromString(TEXT("·")), EUiType::Title, EUiColour::TextSecondary);
			Names->AddChildToHorizontalBox(Dot)->SetPadding(FMargin(VersusNameGap, 0.0f, 0.0f, 0.0f));
		}
		UTextBlock* Name = MakeTokenText(VersusSurname(Order[Index]), EUiType::Title, EUiColour::TextPrimary);
		UHorizontalBoxSlot* NameSlot = Names->AddChildToHorizontalBox(Name);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(Index > 0 ? VersusNameGap : 0.0f, 0.0f, 0.0f, 0.0f));
		PairTexts.Add(Name);
		if (Team == 0 && Order[Index] == Current.HumanPlayerId)
		{
			UPadelBadge* You = WidgetTree->ConstructWidget<UPadelBadge>(UPadelBadge::StaticClass());
			You->SetKind(EPadelBadgeKind::You);
			UHorizontalBoxSlot* BadgeSlot = Names->AddChildToHorizontalBox(You);
			BadgeSlot->SetVerticalAlignment(VAlign_Center);
			BadgeSlot->SetPadding(FMargin(VersusNameGap, 0.0f, 0.0f, 0.0f));
		}
	}
	UVerticalBoxSlot* NamesSlot = Text->AddChildToVerticalBox(Names);
	NamesSlot->SetHorizontalAlignment(bLeft ? HAlign_Left : HAlign_Right);
	NamesSlot->SetPadding(FMargin(0.0f, VersusNameGap, 0.0f, 0.0f));

	UOverlaySlot* TextSlot = Block->AddChildToOverlay(Text);
	TextSlot->SetHorizontalAlignment(bLeft ? HAlign_Left : HAlign_Right);
	TextSlot->SetVerticalAlignment(VAlign_Bottom);
	TextSlot->SetPadding(FMargin(VersusTextSide, 0.0f, VersusTextSide, VersusTextBottom));
	return Block;
}

FText UPadelVersusScreen::GetRulesLine() const
{
	// Spec 7.4: "MEDIUM · BEST OF 3 · GOLDEN POINT"; "ADVANTAGE" with golden point off (7.12).
	IPadelMenuHost* Host = GetHost();
	const Padel::Ui::FMatchSetup Current = Host ? Host->GetMenuMatchSetup() : Padel::Ui::FMatchSetup();
	const FText Format = Current.SetsToWin == 1 ? LOCTEXT("Setup.Format.OneSet", "1 set") : LOCTEXT("Setup.Format.BestOf3", "Best of 3");
	const FText Deuce = Current.bGoldenPoint ? LOCTEXT("Setup.GoldenPoint", "Golden point") : LOCTEXT("Versus.Advantage", "Advantage");
	return FText::Format(LOCTEXT("Versus.Rules", "{0} · {1} · {2}"), VersusDifficulty(Current.Difficulty), Format, Deuce);
}

FString UPadelVersusScreen::GetPairText(int32 Team) const
{
	const int32 First = Team * 2;
	if (!PairTexts.IsValidIndex(First + 1))
	{
		return FString();
	}
	return PairTexts[First]->GetText().ToString() + TEXT(" · ") + PairTexts[First + 1]->GetText().ToString();
}

bool UPadelVersusScreen::IsMatchReady() const
{
	// TODO(W5): read the match owner's "ready" signal (spec 7.4's gap). Until then the
	// match is built when MenuStartMatch is called, so it's always ready.
	return true;
}

void UPadelVersusScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceCard(InDeltaTime);
}

void UPadelVersusScreen::AdvanceCard(float DeltaSeconds)
{
	if (Clock.bStarted)
	{
		return;
	}
	EntryElapsed += DeltaSeconds;
	ApplyEntryMotion();
	// The clock doesn't run under a dialog (the timeout's own question).
	if (UPadelMenuRoot* Root = GetMenuRoot(); Root && Root->IsDialogOpen())
	{
		return;
	}
	Clock.Tick(DeltaSeconds);
	const bool bReady = IsMatchReady();
	if (Clock.ShouldAutoStart(bReady))
	{
		StartMatch();
		return;
	}
	if (Clock.TakeTimeout(bReady))
	{
		ShowStartTimeout();
	}
	UpdateLoading();
}

void UPadelVersusScreen::UpdateLoading()
{
	const bool bShow = IsLoadingShown();
	if (bShow == bLoadingShown)
	{
		return;
	}
	bLoadingShown = bShow;
	if (LoadingBox)
	{
		LoadingBox->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// The board shows the indicator in the hint bar's place, with no hints (A can't skip yet).
	SetHintBarVisible(!bShow);
	RefreshHints();
}

bool UPadelVersusScreen::IsLoadingShown() const
{
	return Clock.ShowsLoading(IsMatchReady());
}

void UPadelVersusScreen::Skip()
{
	if (Clock.CanSkip(IsMatchReady()))
	{
		StartMatch();
	}
}

void UPadelVersusScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	if (Element == SkipArea)
	{
		Skip();
	}
}

void UPadelVersusScreen::StartMatch()
{
	if (Clock.bStarted)
	{
		return;
	}
	Clock.MarkStarted();
	RefreshHints();
	if (IPadelMenuHost* Host = GetHost())
	{
		Host->MenuStartMatch();
	}
}

void UPadelVersusScreen::ShowStartTimeout()
{
	UPadelMenuRoot* Root = GetMenuRoot();
	if (!Root)
	{
		return;
	}
	// Spec 7.12: "The match didn't start." with a way back to the menu.
	FPadelConfirm Confirm;
	Confirm.Title = LOCTEXT("Versus.Timeout.Title", "The match didn't start.");
	Confirm.ActionLabel = LOCTEXT("Versus.Timeout.Back", "Back to menu");
	Confirm.SafeLabel = LOCTEXT("Versus.Timeout.Wait", "Keep waiting");
	Confirm.OnAction = [Root]()
	{
		Root->ShowScreen(EPadelScreen::MainMenu, Padel::Ui::EUiNavKind::Back);
	};
	Root->ShowConfirm(Confirm);
}

void UPadelVersusScreen::ApplyEntryMotion()
{
	// Spec 7.4: the blocks slide in from their sides (Motion.ScreenIn) while "VS" settles
	// from Motion.VsScaleFrom to 1. Reduce motion: no slides or scales; the screen's fade
	// stays.
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	const bool bReduce = PadelUi::Options().bReduceMotion;
	const float SlideT = Motion.ScreenIn > 0.0f ? Padel::Ui::EaseOutQuint(EntryElapsed / Motion.ScreenIn) : 1.0f;
	const float Slide = bReduce ? 0.0f : Motion.ScreenSlideIn * (1.0f - SlideT);
	for (int32 Index = 0; Index < TeamBlocks.Num(); ++Index)
	{
		if (TeamBlocks[Index])
		{
			TeamBlocks[Index]->SetRenderTranslation(FVector2D(Index == 0 ? -Slide : Slide, 0.0f));
		}
	}
	if (VsText)
	{
		const float ScaleT = Motion.VsScale > 0.0f ? Padel::Ui::EaseOutCubic(EntryElapsed / Motion.VsScale) : 1.0f;
		const float Scale = bReduce ? 1.0f : FMath::Lerp(Motion.VsScaleFrom, 1.0f, ScaleT);
		VsText->SetRenderScale(FVector2D(Scale, Scale));
	}
}

TArray<FPadelHint> UPadelVersusScreen::GetHints() const
{
	// Spec 7.4: A Skip, only once the match is ready. No B.
	TArray<FPadelHint> Hints;
	if (Clock.CanSkip(IsMatchReady()))
	{
		Hints.Add({EUiInput::Confirm, LOCTEXT("Hint.Skip", "Skip")});
	}
	return Hints;
}

#undef LOCTEXT_NAMESPACE
