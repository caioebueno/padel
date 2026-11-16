#include "UI/Screens/PadelStatsTable.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelTabBar.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EStatsTab;
using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;
using Padel::Ui::FStatRow;

namespace
{
	// The "Stats" board, relative to the panel (which starts at x 96, y 424).
	constexpr float TablePanelHeight = 424.0f;
	constexpr float TableInset = 32.0f;
	constexpr float TableTabsTop = 24.0f;
	constexpr float TableTeamLabelTop = 92.0f;
	constexpr float TableDividerTop = 96.0f;
	constexpr float TableDividerHeight = 312.0f;
	constexpr float TableDividerWidth = 2.0f;
	constexpr float TableRowsTop = 136.0f;
	constexpr float TableRowHeight = 44.0f;
	constexpr float TableRowGap = 2.0f;
	constexpr int32 TableVisibleRows = 6;
	constexpr float TableLabelTop = 4.0f;
	constexpr float TableNumberTop = 2.0f;
	constexpr float TableBarTop = 14.0f;
	constexpr float TableBarHeight = 16.0f;
	constexpr float TableBarRadius = 4.0f;
	// Bars start this far from the centre line and stop this far inside C6-C9.
	constexpr float TableBarCentreGap = 4.0f;
	constexpr float TableBarEndInset = 8.0f;
	constexpr int32 TableNumberColumnA = 5;
	constexpr int32 TableBarColumn = 6;
	constexpr int32 TableBarColumns = 4;
	constexpr int32 TableNumberColumnB = 10;

	FText TableUtf8(const std::string& Text)
	{
		return FText::FromString(UTF8_TO_TCHAR(Text.c_str()));
	}

	float TablePanelX(int32 Column)
	{
		return PadelUi::ColumnX(Column) - PadelUi::Theme().Layout.SafeX;
	}

	float TableCentre()
	{
		return TablePanelX(TableBarColumn) + PadelUi::SpanWidth(TableBarColumns) * 0.5f;
	}

	float TableBarMax()
	{
		return PadelUi::SpanWidth(TableBarColumns) * 0.5f - TableBarCentreGap - TableBarEndInset;
	}

	UCanvasPanelSlot* TablePlace(UCanvasPanel* Canvas, UWidget* Widget, float X, float Y, float Width, float Height)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetPosition(FVector2D(X, Y));
		CanvasSlot->SetAutoSize(Width <= 0.0f || Height <= 0.0f);
		if (Width > 0.0f && Height > 0.0f)
		{
			CanvasSlot->SetSize(FVector2D(Width, Height));
		}
		return CanvasSlot;
	}
}

void UPadelStatsTable::SetStats(const Padel::Ui::FMatchStats& InStats)
{
	Stats = InStats;
}

void UPadelStatsTable::SetFamily(Padel::Ui::EPadFamily InFamily)
{
	Family = InFamily;
	if (TabBar)
	{
		TabBar->SetFamily(InFamily);
	}
}

void UPadelStatsTable::BuildTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = MakeSized(MakeGlassPanel(Canvas, FMargin(0.0f), PadelUi::Theme().Layout.RadiusPanel), 0.0f, TablePanelHeight);

	TabBar = WidgetTree->ConstructWidget<UPadelTabBar>(UPadelTabBar::StaticClass());
	TabBar->SetFamily(Family);
	TabBar->SetTabs({LOCTEXT("Stats.Tab.Summary", "Summary"), LOCTEXT("Stats.Tab.Shots", "Shots"), LOCTEXT("Stats.Tab.Points", "Points")});
	TabBar->SetIndex(static_cast<int32>(Tab));
	TabBar->OnTabChanged.AddWeakLambda(this, [this](int32 Index) { SelectTab(Index); });
	TablePlace(Canvas, TabBar, TableInset, TableTabsTop, 0.0f, 0.0f);

	// Team labels over the number columns, in the team colours.
	const float NumberWidth = PadelUi::SpanWidth(1);
	UTextBlock* LabelA = MakeTokenText(TableUtf8(Padel::Ui::TeamName(Padel::Ui::TeamA)), EUiType::Label, EUiColour::TextPrimary);
	LabelA->SetColorAndOpacity(FSlateColor(PadelUi::TeamColour(Padel::Ui::TeamA)));
	LabelA->SetJustification(ETextJustify::Right);
	TablePlace(Canvas, LabelA, TablePanelX(TableNumberColumnA), TableTeamLabelTop, NumberWidth, TableRowHeight);
	UTextBlock* LabelB = MakeTokenText(TableUtf8(Padel::Ui::TeamName(Padel::Ui::TeamB)), EUiType::Label, EUiColour::TextPrimary);
	LabelB->SetColorAndOpacity(FSlateColor(PadelUi::TeamColour(Padel::Ui::TeamB)));
	TablePlace(Canvas, LabelB, TablePanelX(TableNumberColumnB), TableTeamLabelTop, NumberWidth, TableRowHeight);

	UImage* Divider = MakeImage(PadelUiPaint::Solid(PadelUi::Colour(EUiColour::GlassEdge)));
	TablePlace(Canvas, Divider, TableCentre() - TableDividerWidth * 0.5f, TableDividerTop, TableDividerWidth, TableDividerHeight);

	// Six rows show; POINTS can have more, which scroll (spec 7.8).
	Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	RowList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(RowList);
	const float ScrollHeight = TableVisibleRows * (TableRowHeight + TableRowGap);
	TablePlace(Canvas, Scroll, 0.0f, TableRowsTop, PadelUi::SpanWidth(static_cast<int32>(PadelUi::Theme().Layout.GridColumns)), ScrollHeight);
	RebuildRows();
}

bool UPadelStatsTable::HandleTabInput(Padel::Ui::EUiInput Input)
{
	return TabBar && TabBar->HandleInput(Input);
}

void UPadelStatsTable::SelectTab(int32 Index)
{
	Tab = static_cast<EStatsTab>(FMath::Clamp(Index, 0, static_cast<int32>(EStatsTab::Count) - 1));
	RebuildRows();
}

void UPadelStatsTable::RebuildRows()
{
	if (!RowList)
	{
		return;
	}
	RowList->ClearChildren();
	RowsA.Reset();
	RowsB.Reset();
	FractionsA.Reset();
	FractionsB.Reset();
	for (const FStatRow& Row : Padel::Ui::StatsTabRows(Stats, Tab))
	{
		AddRow(Row);
	}
	if (Scroll)
	{
		Scroll->ScrollToStart();
	}
	ShowAllBars();
}

FText UPadelStatsTable::RowLabel(const FStatRow& Row) const
{
	// Summary rows have spec 8 copy; shot and ending rows use the model's GDD labels.
	if (Row.Key == "PointsWon") return LOCTEXT("Stats.Row.PointsWon", "Points won");
	if (Row.Key == "Winners") return LOCTEXT("Stats.Row.Winners", "Winners");
	if (Row.Key == "Errors") return LOCTEXT("Stats.Row.Errors", "Errors");
	if (Row.Key == "Smashes") return LOCTEXT("Stats.Row.Smashes", "Smashes");
	if (Row.Key == "GlassPoints") return LOCTEXT("Stats.Row.GlassPoints", "Points won off the glass");
	if (Row.Key == "DoubleFaults") return LOCTEXT("Stats.Row.DoubleFaults", "Double faults");
	if (Row.Key == "GlassShots") return LOCTEXT("Stats.Row.GlassShots", "Shots off the glass");
	return TableUtf8(Row.Label);
}

void UPadelStatsTable::AddRow(const FStatRow& Row)
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	const int32 Leader = Padel::Ui::LeadingTeam(Row);
	auto NumberColour = [Leader](int32 Team)
	{
		return Leader < 0 || Leader == Team ? EUiColour::TextPrimary : EUiColour::TextSecondary;
	};
	const float NumberWidth = PadelUi::SpanWidth(1);

	UTextBlock* Label = MakeTokenText(RowLabel(Row), EUiType::Body, EUiColour::TextSecondary);
	TablePlace(Canvas, Label, TableInset, TableLabelTop, 0.0f, 0.0f);

	UTextBlock* NumberA = MakeTokenText(FText::AsNumber(Row.TeamA), EUiType::Stat, NumberColour(Padel::Ui::TeamA));
	NumberA->SetJustification(ETextJustify::Right);
	TablePlace(Canvas, NumberA, TablePanelX(TableNumberColumnA), TableNumberTop, NumberWidth, TableRowHeight);

	UImage* BarA = MakeImage(PadelUiPaint::Rounded(PadelUi::TeamColour(Padel::Ui::TeamA), TableBarRadius));
	TablePlace(Canvas, BarA, TableCentre() - TableBarCentreGap, TableBarTop, 0.0f, TableBarHeight)->SetAutoSize(false);
	UImage* BarB = MakeImage(PadelUiPaint::Rounded(PadelUi::TeamColour(Padel::Ui::TeamB), TableBarRadius));
	TablePlace(Canvas, BarB, TableCentre() + TableBarCentreGap, TableBarTop, 0.0f, TableBarHeight)->SetAutoSize(false);

	UTextBlock* NumberB = MakeTokenText(FText::AsNumber(Row.TeamB), EUiType::Stat, NumberColour(Padel::Ui::TeamB));
	TablePlace(Canvas, NumberB, TablePanelX(TableNumberColumnB), TableNumberTop, NumberWidth, TableRowHeight);

	UVerticalBoxSlot* RowSlot = RowList->AddChildToVerticalBox(MakeSized(Canvas, 0.0f, TableRowHeight));
	RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TableRowGap));

	RowsA.Add(BarA);
	RowsB.Add(BarB);
	FractionsA.Add(static_cast<float>(Padel::Ui::BarFraction(Row, Padel::Ui::TeamA)));
	FractionsB.Add(static_cast<float>(Padel::Ui::BarFraction(Row, Padel::Ui::TeamB)));
}

void UPadelStatsTable::SetRowProgress(int32 Row, float Progress)
{
	if (!RowsA.IsValidIndex(Row) || !RowsB.IsValidIndex(Row))
	{
		return;
	}
	// Spec 7.8: the bars grow out from the centre; Team A's toward the left.
	const float Eased = Padel::Ui::EaseOutCubic(Progress);
	const float WidthA = TableBarMax() * FractionsA[Row] * Eased;
	const float WidthB = TableBarMax() * FractionsB[Row] * Eased;
	if (UCanvasPanelSlot* SlotA = Cast<UCanvasPanelSlot>(RowsA[Row]->Slot))
	{
		SlotA->SetPosition(FVector2D(TableCentre() - TableBarCentreGap - WidthA, TableBarTop));
		SlotA->SetSize(FVector2D(WidthA, TableBarHeight));
	}
	if (UCanvasPanelSlot* SlotB = Cast<UCanvasPanelSlot>(RowsB[Row]->Slot))
	{
		SlotB->SetPosition(FVector2D(TableCentre() + TableBarCentreGap, TableBarTop));
		SlotB->SetSize(FVector2D(WidthB, TableBarHeight));
	}
	// A zero-length bar draws nothing at all (spec 7.12: an empty bar).
	RowsA[Row]->SetVisibility(WidthA > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	RowsB[Row]->SetVisibility(WidthB > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UPadelStatsTable::ShowAllBars()
{
	for (int32 Row = 0; Row < RowsA.Num(); ++Row)
	{
		SetRowProgress(Row, 1.0f);
	}
}

#undef LOCTEXT_NAMESPACE
