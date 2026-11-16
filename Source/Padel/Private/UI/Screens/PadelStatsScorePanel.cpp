#include "UI/Screens/PadelStatsScorePanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/Model/PadelUiStatsView.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelIcon.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	// The "Stats" board, relative to the panel (which starts at x 96, y 184).
	constexpr float ScorePanelHeight = 216.0f;
	constexpr float ScoreRowTop = 12.0f;
	constexpr float ScoreRowHeight = 96.0f;
	constexpr float ScoreNameInset = 32.0f;
	constexpr float ScoreNameTop = 6.0f;
	constexpr int32 ScoreFirstSetColumn = 7;
	constexpr float ScoreCheckSize = 40.0f;
	constexpr float ScoreCheckStroke = 5.0f;
	constexpr float ScoreCheckTop = 28.0f;

	FText ScoreUtf8(const std::string& Text)
	{
		return FText::FromString(UTF8_TO_TCHAR(Text.c_str()));
	}

	// Panel-relative x of a grid column.
	float ScorePanelX(int32 Column)
	{
		return PadelUi::ColumnX(Column) - PadelUi::Theme().Layout.SafeX;
	}

	void ScorePlace(UCanvasPanel* Canvas, UWidget* Widget, float X, float Y, float Width, float Height)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetPosition(FVector2D(X, Y));
		CanvasSlot->SetAutoSize(Width <= 0.0f || Height <= 0.0f);
		if (Width > 0.0f && Height > 0.0f)
		{
			CanvasSlot->SetSize(FVector2D(Width, Height));
		}
	}
}

void UPadelStatsScorePanel::SetStats(const Padel::Ui::FMatchStats& InStats, int32 InHumanPlayerId)
{
	Stats = InStats;
	HumanPlayerId = InHumanPlayerId;
}

void UPadelStatsScorePanel::BuildTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = MakeSized(MakeGlassPanel(Canvas, FMargin(0.0f), PadelUi::Theme().Layout.RadiusPanel), 0.0f, ScorePanelHeight);

	Columns.SetNum(Padel::Ui::SetCount(Stats));
	for (int32 Team = Padel::Ui::TeamA; Team <= Padel::Ui::TeamB; ++Team)
	{
		BuildTeamRow(Canvas, Team);
	}
}

void UPadelStatsScorePanel::BuildTeamRow(UCanvasPanel* Canvas, int32 Team)
{
	const float RowTop = ScoreRowTop + static_cast<float>(Team) * ScoreRowHeight;

	// Team name (Type.Title, team colour) over the pair's surnames (Type.Body, secondary).
	UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* TeamText = MakeTokenText(ScoreUtf8(Padel::Ui::TeamName(Team)), EUiType::Title, EUiColour::TextPrimary);
	TeamText->SetColorAndOpacity(FSlateColor(PadelUi::TeamColour(Team)));
	Names->AddChildToVerticalBox(TeamText);
	int32 First = Team * 2;
	int32 Second = Team * 2 + 1;
	if (Padel::Ui::TeamOfPlayer(HumanPlayerId) == Team && HumanPlayerId == Second)
	{
		Swap(First, Second);
	}
	Names->AddChildToVerticalBox(MakeTokenText(FText::Format(LOCTEXT("Stats.Pair", "{0} · {1}"),
		ScoreUtf8(Padel::Ui::Surname(First)), ScoreUtf8(Padel::Ui::Surname(Second))), EUiType::Body, EUiColour::TextSecondary));
	ScorePlace(Canvas, Names, ScoreNameInset, RowTop + ScoreNameTop, 0.0f, 0.0f);

	const float ColumnWidth = PadelUi::SpanWidth(1);
	for (int32 Set = 0; Set < Columns.Num(); ++Set)
	{
		UWidget* Cell = MakeSetCell(Set, Team);
		ScorePlace(Canvas, Cell, ScorePanelX(ScoreFirstSetColumn + Set), RowTop, ColumnWidth, ScoreRowHeight);
		Columns[Set].Add(Cell);
		ColumnCells.Add(Cell);
	}

	// Spec 7.8: a check in the winner's colour after the winning team, past the last set.
	if (Stats.MatchWinner == Team)
	{
		UPadelIcon* Check = WidgetTree->ConstructWidget<UPadelIcon>(UPadelIcon::StaticClass());
		Check->SetIcon(EPadelIcon::Check);
		Check->SetTint(PadelUi::TeamColour(Team));
		Check->SetStroke(ScoreCheckStroke);
		const float CheckX = ScorePanelX(ScoreFirstSetColumn + Columns.Num()) + (ColumnWidth - ScoreCheckSize) * 0.5f;
		ScorePlace(Canvas, MakeSized(Check, ScoreCheckSize, ScoreCheckSize), CheckX, RowTop + ScoreCheckTop, ScoreCheckSize, ScoreCheckSize);
	}
}

UWidget* UPadelStatsScorePanel::MakeSetCell(int32 Set, int32 Team)
{
	const Padel::Ui::FSetCell Cell = Padel::Ui::SetCell(Stats, Set, Team);
	const EUiColour Colour = Cell.bWonSet ? EUiColour::TextPrimary : EUiColour::TextSecondary;

	// The games, centred in the column, with the tie-break points raised beside them in
	// Type.Label ("7⁷ 6⁵", spec 7.8).
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->AddChildToHorizontalBox(MakeTokenText(FText::AsNumber(Cell.Games), EUiType::Score, Colour))->SetVerticalAlignment(VAlign_Top);
	if (Cell.TiebreakPoints)
	{
		UTextBlock* Tiebreak = MakeTokenText(FText::AsNumber(*Cell.TiebreakPoints), EUiType::Label, Colour);
		UHorizontalBoxSlot* TiebreakSlot = Row->AddChildToHorizontalBox(Tiebreak);
		TiebreakSlot->SetVerticalAlignment(VAlign_Top);
		TiebreakSlot->SetPadding(FMargin(PadelUi::Space(1) * 0.5f, 0.0f, 0.0f, 0.0f));
	}
	USizeBox* Box = MakeSized(Row, PadelUi::SpanWidth(1), ScoreRowHeight);
	if (USizeBoxSlot* RowSlot = Cast<USizeBoxSlot>(Row->Slot))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetVerticalAlignment(VAlign_Top);
	}
	return Box;
}

void UPadelStatsScorePanel::SetColumnAlpha(int32 Column, float Alpha)
{
	if (!Columns.IsValidIndex(Column))
	{
		return;
	}
	for (UWidget* Cell : Columns[Column])
	{
		if (Cell)
		{
			Cell->SetRenderOpacity(Alpha);
		}
	}
}

#undef LOCTEXT_NAMESPACE
