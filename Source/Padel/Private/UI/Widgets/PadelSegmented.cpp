#include "UI/Widgets/PadelSegmented.h"

#include "InputCoreTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	constexpr float SegmentedHeight = 56.0f;
	constexpr float SegmentedInset = 4.0f;
	constexpr float SegmentMinWidth = 160.0f;
	constexpr float SegmentPaddingX = 24.0f;
}

void UPadelSegmented::SetOptions(const TArray<FText>& InOptions)
{
	Options = InOptions;
	Model.Count = Options.Num();
	Model.Index = FMath::Clamp(Model.Index, 0, FMath::Max(0, Model.Count - 1));
	RebuildSegments();
	RefreshVisuals();
}

void UPadelSegmented::SetIndex(int32 InIndex)
{
	Model.Index = FMath::Clamp(InIndex, 0, FMath::Max(0, Model.Count - 1));
	RefreshVisuals();
}

void UPadelSegmented::Select(int32 InIndex)
{
	if (IsDisabled() || InIndex == Model.Index || InIndex < 0 || InIndex >= Model.Count)
	{
		return;
	}
	Model.Index = InIndex;
	RefreshVisuals();
	OnValueChanged.Broadcast(this);
}

bool UPadelSegmented::HandleInput(EUiInput Input, bool bRepeat)
{
	if (IsDisabled() || (Input != EUiInput::Left && Input != EUiInput::Right))
	{
		return false;
	}
	if (Model.Step(Input == EUiInput::Right ? 1 : -1))
	{
		RefreshVisuals();
		OnValueChanged.Broadcast(this);
	}
	else if (!bRepeat)
	{
		OnEdgeReached.Broadcast(this);
	}
	return true;
}

void UPadelSegmented::OnClicked(const FPointerEvent& InMouseEvent)
{
	for (int32 Index = 0; Index < SegmentFills.Num(); ++Index)
	{
		if (SegmentFills[Index] && SegmentFills[Index]->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			Select(Index);
			return;
		}
	}
}

void UPadelSegmented::BuildValue(UHorizontalBox* Row)
{
	const Padel::Ui::FUiLayout& Layout = PadelUi::Theme().Layout;
	UOverlay* PillRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Segments = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UBorder* Pill = MakeRoundedBorder(PadelUi::Colour(EUiColour::GlassFill), Layout.RadiusPill, PadelUi::Colour(EUiColour::GlassEdge), 1.0f,
		FMargin(SegmentedInset), Segments);
	UOverlaySlot* PillSlot = PillRoot->AddChildToOverlay(Pill);
	PillSlot->SetHorizontalAlignment(HAlign_Fill);
	PillSlot->SetVerticalAlignment(VAlign_Fill);
	AddFocusRing(PillRoot, Layout.RadiusPill);
	Row->AddChildToHorizontalBox(MakeSized(PillRoot, 0.0f, SegmentedHeight))->SetVerticalAlignment(VAlign_Center);
	RebuildSegments();
}

void UPadelSegmented::RebuildSegments()
{
	if (!Segments)
	{
		return;
	}
	Segments->ClearChildren();
	SegmentFills.Reset();
	SegmentLabels.Reset();
	const float Radius = PadelUi::Theme().Layout.RadiusPill;
	for (const FText& Option : Options)
	{
		UTextBlock* LabelBlock = MakeTokenText(Option, EUiType::Heading, EUiColour::TextSecondary);
		LabelBlock->SetJustification(ETextJustify::Center);
		UBorder* SegmentFill = MakeRoundedBorder(FLinearColor::Transparent, Radius, FLinearColor::Transparent, 0.0f,
			FMargin(SegmentPaddingX, 0.0f), LabelBlock);
		SegmentFill->SetHorizontalAlignment(HAlign_Center);
		SegmentFill->SetVerticalAlignment(VAlign_Center);
		USizeBox* SegmentBox = MakeSized(SegmentFill, 0.0f, 0.0f);
		SegmentBox->SetMinDesiredWidth(SegmentMinWidth);
		Segments->AddChildToHorizontalBox(SegmentBox)->SetVerticalAlignment(VAlign_Fill);
		SegmentFills.Add(SegmentFill);
		SegmentLabels.Add(LabelBlock);
	}
}

void UPadelSegmented::RefreshValue()
{
	const bool bDisabled = IsDisabled();
	const float Radius = PadelUi::Theme().Layout.RadiusPill;
	for (int32 Index = 0; Index < SegmentFills.Num(); ++Index)
	{
		const bool bSelected = Index == Model.Index;
		if (SegmentFills[Index])
		{
			SegmentFills[Index]->SetBrush(PadelUiPaint::Rounded(
				bSelected ? PadelUi::Colour(bDisabled ? EUiColour::GlassEdge : EUiColour::Accent) : FLinearColor::Transparent, Radius));
		}
		if (SegmentLabels.IsValidIndex(Index) && SegmentLabels[Index])
		{
			const EUiColour Colour = bDisabled ? EUiColour::TextDisabled : (bSelected ? EUiColour::OnAccent : EUiColour::TextSecondary);
			SegmentLabels[Index]->SetColorAndOpacity(FSlateColor(PadelUi::Colour(Colour)));
		}
	}
	ShowFocusRing(IsElementFocused() && !bDisabled);
}
