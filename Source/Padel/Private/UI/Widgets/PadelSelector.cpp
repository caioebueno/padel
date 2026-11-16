#include "UI/Widgets/PadelSelector.h"

#include "InputCoreTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelIcon.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	constexpr float SelectorArrow = 32.0f;
	constexpr float SelectorArrowStroke = 3.0f;
	constexpr float SelectorValueWidth = 240.0f;
	constexpr float SelectorSlidePx = 24.0f;
	constexpr float SelectorDot = 8.0f;
	constexpr float SelectorDotGap = 8.0f;
	constexpr float SelectorDotsTop = 6.0f;
}

void UPadelSelector::SetOptions(const TArray<FText>& InOptions, const TArray<FText>& InDescriptions)
{
	Options = InOptions;
	Descriptions = InDescriptions;
	Model.Count = Options.Num();
	Model.Index = FMath::Clamp(Model.Index, 0, FMath::Max(0, Model.Count - 1));
	RebuildDots();
	RefreshVisuals();
}

void UPadelSelector::SetIndex(int32 InIndex)
{
	Model.Index = FMath::Clamp(InIndex, 0, FMath::Max(0, Model.Count - 1));
	SlideTween.Snap(0.0f);
	RefreshVisuals();
}

FText UPadelSelector::GetValueText() const
{
	return Options.IsValidIndex(Model.Index) ? Options[Model.Index] : FText::GetEmpty();
}

FText UPadelSelector::CurrentDescription() const
{
	if (Descriptions.IsValidIndex(Model.Index) && !Descriptions[Model.Index].IsEmpty())
	{
		return Descriptions[Model.Index];
	}
	return Description;
}

bool UPadelSelector::HandleInput(EUiInput Input, bool bRepeat)
{
	if (IsDisabled() || (Input != EUiInput::Left && Input != EUiInput::Right))
	{
		return false;
	}
	StepValue(Input == EUiInput::Right ? 1 : -1, false, bRepeat);
	return true;
}

void UPadelSelector::Activate()
{
	if (IsDisabled())
	{
		return;
	}
	StepValue(1, true, false);
	Super::Activate();
}

void UPadelSelector::StepValue(int32 Direction, bool bWrap, bool bRepeat)
{
	const int32 Before = Model.Index;
	if (bWrap)
	{
		Model.StepForwardWrapping();
	}
	else if (!Model.Step(Direction))
	{
		if (!bRepeat)
		{
			OnEdgeReached.Broadcast(this);
		}
		return;
	}
	if (Model.Index == Before)
	{
		return;
	}
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	float& ArrowTimer = Direction > 0 ? RightPressTimer : LeftPressTimer;
	ArrowTimer = FMath::Max(Theme.Motion.Press, Theme.Input.PressMinimum);
	// The new value comes in from the side opposite the arrow pressed.
	SlideFrom = Direction > 0 ? SelectorSlidePx : -SelectorSlidePx;
	SlideTween.Snap(1.0f);
	SlideTween.Start(0.0f, PadelUi::Options().bReduceMotion ? 0.0f : Theme.Motion.Value);
	RefreshVisuals();
	OnValueChanged.Broadcast(this);
}

void UPadelSelector::BuildValue(UHorizontalBox* Row)
{
	LeftArrow = WidgetTree->ConstructWidget<UPadelIcon>(UPadelIcon::StaticClass());
	LeftArrow->SetIcon(EPadelIcon::ChevronLeft);
	LeftArrow->SetStroke(SelectorArrowStroke);
	LeftArrowBox = MakeSized(LeftArrow, SelectorArrow, SelectorArrow);
	Row->AddChildToHorizontalBox(LeftArrowBox)->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* ValueColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ValueText = MakeTokenText(FText::GetEmpty(), EUiType::Heading, EUiColour::TextPrimary);
	ValueText->SetJustification(ETextJustify::Center);
	ValueText->SetClipping(EWidgetClipping::ClipToBounds);
	ValueColumn->AddChildToVerticalBox(ValueText)->SetHorizontalAlignment(HAlign_Fill);
	Dots = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* DotsSlot = ValueColumn->AddChildToVerticalBox(Dots);
	DotsSlot->SetHorizontalAlignment(HAlign_Center);
	DotsSlot->SetPadding(FMargin(0.0f, SelectorDotsTop, 0.0f, 0.0f));
	Row->AddChildToHorizontalBox(MakeSized(ValueColumn, SelectorValueWidth, 0.0f))->SetVerticalAlignment(VAlign_Center);

	RightArrow = WidgetTree->ConstructWidget<UPadelIcon>(UPadelIcon::StaticClass());
	RightArrow->SetIcon(EPadelIcon::ChevronRight);
	RightArrow->SetStroke(SelectorArrowStroke);
	RightArrowBox = MakeSized(RightArrow, SelectorArrow, SelectorArrow);
	Row->AddChildToHorizontalBox(RightArrowBox)->SetVerticalAlignment(VAlign_Center);
	RebuildDots();
}

void UPadelSelector::RebuildDots()
{
	if (!Dots)
	{
		return;
	}
	Dots->ClearChildren();
	if (!Model.ShowsDots())
	{
		return;
	}
	for (int32 Index = 0; Index < Model.Count; ++Index)
	{
		UImage* Dot = MakeImage(PadelUiPaint::Rounded(FLinearColor::White, SelectorDot * 0.5f));
		UHorizontalBoxSlot* DotSlot = Dots->AddChildToHorizontalBox(MakeSized(Dot, SelectorDot, SelectorDot));
		DotSlot->SetPadding(FMargin(Index == 0 ? 0.0f : SelectorDotGap, 0.0f, 0.0f, 0.0f));
	}
}

void UPadelSelector::RefreshValue()
{
	if (!ValueText || !LeftArrow || !RightArrow || !Dots)
	{
		return;
	}
	const bool bDisabled = IsDisabled();
	const bool bFocused = IsElementFocused() && !bDisabled;
	ValueText->SetText(GetValueText());
	ValueText->SetColorAndOpacity(FSlateColor(PadelUi::Colour(bDisabled ? EUiColour::TextDisabled : EUiColour::TextPrimary)));

	auto ArrowColour = [bFocused](bool bAtEnd)
	{
		if (bAtEnd)
		{
			return PadelUi::Colour(EUiColour::TextDisabled);
		}
		return PadelUi::Colour(bFocused ? EUiColour::Accent : EUiColour::TextSecondary);
	};
	LeftArrow->SetTint(ArrowColour(Model.AtStart()));
	RightArrow->SetTint(ArrowColour(Model.AtEnd()));
	// Disabled: the arrows are hidden, keeping their space so the value doesn't move.
	const ESlateVisibility ArrowVisibility = bDisabled ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible;
	LeftArrowBox->SetVisibility(ArrowVisibility);
	RightArrowBox->SetVisibility(ArrowVisibility);

	for (int32 Index = 0; Index < Dots->GetChildrenCount(); ++Index)
	{
		USizeBox* DotBox = Cast<USizeBox>(Dots->GetChildAt(Index));
		UImage* Dot = DotBox ? Cast<UImage>(DotBox->GetContent()) : nullptr;
		if (Dot)
		{
			Dot->SetColorAndOpacity(PadelUi::Colour(Index == Model.Index && !bDisabled ? EUiColour::Accent : EUiColour::TextDisabled));
		}
	}
}

void UPadelSelector::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SlideTween.Tick(InDeltaTime);
	LeftPressTimer = FMath::Max(0.0f, LeftPressTimer - InDeltaTime);
	RightPressTimer = FMath::Max(0.0f, RightPressTimer - InDeltaTime);
	if (ValueText)
	{
		ValueText->SetRenderTranslation(FVector2D(SlideFrom * SlideTween.Value(), 0.0f));
		ValueText->SetRenderOpacity(1.0f - SlideTween.Value());
	}
	// Spec 6.8: the pressed arrow scales to 0.85 for 80 ms.
	const bool bReduceMotion = PadelUi::Options().bReduceMotion;
	const float ArrowScale = PadelUi::Theme().Layout.PressScaleArrow;
	if (LeftArrowBox)
	{
		const float Scale = LeftPressTimer > 0.0f && !bReduceMotion ? ArrowScale : 1.0f;
		LeftArrowBox->SetRenderScale(FVector2D(Scale, Scale));
	}
	if (RightArrowBox)
	{
		const float Scale = RightPressTimer > 0.0f && !bReduceMotion ? ArrowScale : 1.0f;
		RightArrowBox->SetRenderScale(FVector2D(Scale, Scale));
	}
}

void UPadelSelector::OnClicked(const FPointerEvent& InMouseEvent)
{
	// A click on an arrow steps that way (no wrap); anywhere else on the row is A.
	const FVector2D At = InMouseEvent.GetScreenSpacePosition();
	if (LeftArrowBox && LeftArrowBox->GetCachedGeometry().IsUnderLocation(At))
	{
		StepValue(-1, false, false);
	}
	else if (RightArrowBox && RightArrowBox->GetCachedGeometry().IsUnderLocation(At))
	{
		StepValue(1, false, false);
	}
	else
	{
		Activate();
	}
}
