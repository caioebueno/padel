#include "UI/PadelScoreBugWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelUiStyle.h"

namespace
{
	// Built on use, not at static init: FText shouldn't be created before the engine starts.
	FText ServeDot()
	{
		return FText::FromString(TEXT("\u25CF"));
	}
}

void UPadelScoreBugWidget::BuildTree()
{
	UBorder* Root = MakePanel(PadelUiStyle::BugBackground, 8.0f);
	WidgetTree->RootWidget = Root;
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Root->SetContent(Column);

	for (int32 Team = 0; Team < 2; ++Team)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));

		UBorder* Bar = MakePanel(PadelUiStyle::TeamColour(Team), 0.0f);
		USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetWidthOverride(8.0f);
		BarSize->SetContent(Bar);
		Row->AddChildToHorizontalBox(BarSize);

		UTextBlock* Dot = MakeText(ServeDot(), PadelUiStyle::FontSmall, PadelUiStyle::Accent);
		Row->AddChildToHorizontalBox(Dot)->SetPadding(FMargin(10.0f, 0.0f));
		ServeDots.Add(Dot);

		UTextBlock* Names = MakeText(PadelUiStyle::ToText(Padel::Ui::PairLabel(Team)), PadelUiStyle::FontSmall, PadelUiStyle::Text);
		USizeBox* NameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NameSize->SetWidthOverride(320.0f);
		NameSize->SetContent(Names);
		Row->AddChildToHorizontalBox(NameSize)->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* Sets = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->AddChildToHorizontalBox(Sets)->SetVerticalAlignment(VAlign_Center);
		SetBoxes.Add(Sets);

		UBorder* PointsPanel = MakePanel(PadelUiStyle::PointsBackground, 4.0f);
		USizeBox* PointsSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PointsSize->SetWidthOverride(60.0f);
		PointsSize->SetContent(PointsPanel);
		UTextBlock* Points = MakeText(FText::FromString(TEXT("0")), PadelUiStyle::FontSmall, PadelUiStyle::PointsText);
		Points->SetJustification(ETextJustify::Center);
		PointsPanel->SetContent(Points);
		Row->AddChildToHorizontalBox(PointsSize)->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
		PointTexts.Add(Points);
	}

	Tag = MakeText(FText::GetEmpty(), PadelUiStyle::FontSmall, PadelUiStyle::Accent);
	Tag->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(Tag)->SetPadding(FMargin(26.0f, 4.0f, 0.0f, 0.0f));

	if (bHasPending)
	{
		UpdateScore(Pending);
	}
	else
	{
		UpdateScore(Padel::Ui::FScoreView());
	}
}

void UPadelScoreBugWidget::UpdateScore(const Padel::Ui::FScoreView& View)
{
	Pending = View;
	bHasPending = true;
	if (PointTexts.Num() < 2)
	{
		// Not built yet: applied in BuildTree.
		return;
	}
	for (int32 Team = 0; Team < 2; ++Team)
	{
		ServeDots[Team]->SetRenderOpacity(View.ServingTeam == Team && !View.bMatchOver ? 1.0f : 0.0f);
		PointTexts[Team]->SetText(View.bMatchOver ? FText::GetEmpty() : PadelUiStyle::ToText(View.Points[Team]));
		const std::vector<int> Columns = Padel::Ui::SetColumns(View, Team);
		FillSets(Team, Columns, View.bMatchOver ? -1 : static_cast<int32>(Columns.size()) - 1);
	}
	const std::string TagText = Padel::Ui::PressureTag(View);
	Tag->SetText(PadelUiStyle::ToText(TagText));
	Tag->SetVisibility(TagText.empty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UPadelScoreBugWidget::FillSets(int32 Team, const std::vector<int>& Columns, int32 LiveIndex)
{
	UHorizontalBox* Box = SetBoxes[Team];
	while (Box->GetChildrenCount() < static_cast<int32>(Columns.size()))
	{
		UTextBlock* Cell = MakeText(FText::GetEmpty(), PadelUiStyle::FontSmall, PadelUiStyle::Text);
		Cell->SetMinDesiredWidth(26.0f);
		Cell->SetJustification(ETextJustify::Center);
		Box->AddChildToHorizontalBox(Cell)->SetPadding(FMargin(4.0f, 0.0f));
	}
	while (Box->GetChildrenCount() > static_cast<int32>(Columns.size()))
	{
		Box->RemoveChildAt(Box->GetChildrenCount() - 1);
	}
	for (int32 Index = 0; Index < static_cast<int32>(Columns.size()); ++Index)
	{
		UTextBlock* Cell = Cast<UTextBlock>(Box->GetChildAt(Index));
		Cell->SetText(FText::AsNumber(Columns[Index]));
		// Finished sets are dimmed so the live set reads first.
		Cell->SetColorAndOpacity(FSlateColor(Index == LiveIndex ? PadelUiStyle::Text : PadelUiStyle::TextMuted));
	}
}

FString UPadelScoreBugWidget::GetPointsText(int32 Team) const
{
	return PointTexts.IsValidIndex(Team) ? PointTexts[Team]->GetText().ToString() : FString();
}

FString UPadelScoreBugWidget::GetGamesText(int32 Team) const
{
	if (!SetBoxes.IsValidIndex(Team))
	{
		return FString();
	}
	TArray<FString> Parts;
	for (int32 Index = 0; Index < SetBoxes[Team]->GetChildrenCount(); ++Index)
	{
		if (const UTextBlock* Cell = Cast<UTextBlock>(SetBoxes[Team]->GetChildAt(Index)))
		{
			Parts.Add(Cell->GetText().ToString());
		}
	}
	return FString::Join(Parts, TEXT(" "));
}

bool UPadelScoreBugWidget::IsServing(int32 Team) const
{
	return ServeDots.IsValidIndex(Team) && ServeDots[Team]->GetRenderOpacity() > 0.5f;
}

FString UPadelScoreBugWidget::GetTagText() const
{
	return Tag && Tag->GetVisibility() != ESlateVisibility::Collapsed ? Tag->GetText().ToString() : FString();
}
