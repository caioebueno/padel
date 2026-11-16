#include "UI/Widgets/PadelLoadingIndicator.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	// Board "VersusLoading": a 32 x 40 box, the 20 px ball at (6, 4), a 2 px line 4 px
	// above the bottom, 16 px before the word.
	constexpr float LoadingBoxW = 32.0f;
	constexpr float LoadingBoxH = 40.0f;
	constexpr float LoadingBall = 20.0f;
	constexpr float LoadingBallX = 6.0f;
	constexpr float LoadingLine = 2.0f;
	constexpr float LoadingLineInset = 4.0f;
}

void UPadelLoadingIndicator::BuildTree()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WidgetTree->RootWidget = Row;

	UOverlay* Stage = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	FSlateBrush LineBrush = PadelUiPaint::Solid(PadelUi::Colour(EUiColour::TextSecondary));
	LineBrush.ImageSize = FVector2D(LoadingLine, LoadingLine);
	UOverlaySlot* LineSlot = Stage->AddChildToOverlay(MakeImage(LineBrush));
	LineSlot->SetHorizontalAlignment(HAlign_Fill);
	LineSlot->SetVerticalAlignment(VAlign_Bottom);
	LineSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, LoadingLineInset));

	// The ball rests just above the line; the bounce lifts it with a render offset.
	Ball = MakeImage(PadelUiPaint::Rounded(PadelUi::Colour(EUiColour::TextPrimary), LoadingBall * 0.5f));
	UOverlaySlot* BallSlot = Stage->AddChildToOverlay(MakeSized(Ball, LoadingBall, LoadingBall));
	BallSlot->SetHorizontalAlignment(HAlign_Left);
	BallSlot->SetVerticalAlignment(VAlign_Bottom);
	BallSlot->SetPadding(FMargin(LoadingBallX, 0.0f, 0.0f, LoadingLineInset + LoadingLine));
	UHorizontalBoxSlot* StageSlot = Row->AddChildToHorizontalBox(MakeSized(Stage, LoadingBoxW, LoadingBoxH));
	StageSlot->SetVerticalAlignment(VAlign_Center);

	UTextBlock* Label = MakeTokenText(LOCTEXT("Loading.Label", "Loading"), EUiType::Label, EUiColour::TextPrimary);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetPadding(FMargin(PadelUi::Space(2), 0.0f, 0.0f, 0.0f));
}

void UPadelLoadingIndicator::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Ball)
	{
		return;
	}
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	if (PadelUi::Options().bReduceMotion || Motion.LoadingLoop <= 0.0f)
	{
		Ball->SetRenderTranslation(FVector2D(0.0f, -Motion.LoadingBounce));
		return;
	}
	Phase = FMath::Fmod(Phase + InDeltaTime / Motion.LoadingLoop, 1.0f);
	// A ball's arc: touching the line at the loop's ends, highest halfway.
	const float Height = 4.0f * Phase * (1.0f - Phase);
	Ball->SetRenderTranslation(FVector2D(0.0f, -Motion.LoadingBounce * Height));
}

#undef LOCTEXT_NAMESPACE
