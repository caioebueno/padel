#include "UI/Widgets/PadelPlayerCard.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelUiPaint.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelNameStrap.h"
#include "UI/Widgets/PadelSilhouette.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EPlayStyle;
using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;
using Padel::Ui::UnknownCardStat;

namespace
{
	// Board "PlayerCard" (spec 6.6), in px.
	constexpr float PlayerCardLargeW = 390.0f;
	constexpr float PlayerCardLargeH = 560.0f;
	constexpr float PlayerCardSmallW = 321.0f;
	constexpr float PlayerCardSmallH = 200.0f;
	constexpr float PlayerCardStrapTop = 184.0f;      // large: the strap under the portrait
	constexpr float PlayerCardStrapLarge = 96.0f;
	constexpr float PlayerCardStrapSmall = 72.0f;
	constexpr float PlayerCardBand = 72.0f;           // the Overall band
	constexpr float PlayerCardPortraitX = 24.0f;
	constexpr float PlayerCardPortraitW = 200.0f;
	constexpr float PlayerCardPortraitH = 280.0f;
	constexpr float PlayerCardOutlineX = 20.0f;
	constexpr float PlayerCardOutlineY = 16.0f;
	constexpr float PlayerCardOutlineStroke = 2.0f;
	constexpr float PlayerCardOutlineAlpha = 0.08f;
	constexpr float PlayerCardProfileX = 216.0f;
	constexpr float PlayerCardProfileTop = 24.0f;
	constexpr float PlayerCardProfileRight = 12.0f;
	constexpr float PlayerCardStatsTop = 292.0f;
	constexpr float PlayerCardStatRow = 44.0f;
	constexpr float PlayerCardStatGap = 2.0f;
	constexpr float PlayerCardStatLabelW = 150.0f;
	constexpr float PlayerCardNumberW = 48.0f;
	constexpr float PlayerCardStatNumberPx = 32.0f;   // Type.Stat at 32 px on the stat rows
	constexpr float PlayerCardStatBar = 8.0f;
	constexpr float PlayerCardOverallBar = 12.0f;
	constexpr float PlayerCardStatBarAlpha = 0.85f;   // Text.Primary at 0.85 (spec 6.6)
	constexpr float PlayerCardSelectedStroke = 2.0f;
	constexpr int32 PlayerCardMeterMax = 99;

	FText PlayerCardStatLabel(const std::string& Label)
	{
		if (Label == "Attack")
		{
			return LOCTEXT("Card.Attack", "Attack");
		}
		if (Label == "Defence")
		{
			return LOCTEXT("Card.Defence", "Defence");
		}
		if (Label == "Net")
		{
			return LOCTEXT("Card.Net", "Net");
		}
		if (Label == "Movement")
		{
			return LOCTEXT("Card.Movement", "Movement");
		}
		// A group added in Data/UiTheme.json without copy yet: show its key.
		return FText::FromString(UTF8_TO_TCHAR(Label.c_str()));
	}

	FText PlayerCardStyle(EPlayStyle Style)
	{
		switch (Style)
		{
		case EPlayStyle::Wall:     return LOCTEXT("Card.Style.Wall", "Wall play");
		case EPlayStyle::Net:      return LOCTEXT("Card.Style.Net", "Net play");
		case EPlayStyle::Power:    return LOCTEXT("Card.Style.Power", "Power play");
		case EPlayStyle::Touch:    return LOCTEXT("Card.Style.Touch", "Touch play");
		case EPlayStyle::AllRound: return LOCTEXT("Card.Style.AllRound", "All-round");
		case EPlayStyle::Unknown:  break;
		}
		return FText::GetEmpty();
	}

	FText PlayerCardNumber(int32 Value)
	{
		// Spec 7.12 and the board's missing-data card: a dash for a stat we can't show.
		return Value == UnknownCardStat ? LOCTEXT("Card.NoStat", "—") : FText::AsNumber(Value);
	}

	UCanvasPanelSlot* PlayerCardPlace(UCanvasPanel* Canvas, UWidget* Widget, float X, float Y, float W, float H)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetPosition(FVector2D(X, Y));
		CanvasSlot->SetAutoSize(W <= 0.0f || H <= 0.0f);
		if (W > 0.0f && H > 0.0f)
		{
			CanvasSlot->SetSize(FVector2D(W, H));
		}
		return CanvasSlot;
	}
}

FVector2D UPadelPlayerCard::LargeSize()
{
	return FVector2D(PlayerCardLargeW, PlayerCardLargeH);
}

FVector2D UPadelPlayerCard::SmallSize()
{
	return FVector2D(PlayerCardSmallW, PlayerCardSmallH);
}

void UPadelPlayerCard::SetCard(const Padel::Ui::FPlayerCardView& InCard)
{
	Card = InCard;
	Refresh();
}

void UPadelPlayerCard::SetRole(EPadelCardRole InRole, bool bAnimate)
{
	if (Role == InRole && SwapTween.IsDone())
	{
		return;
	}
	Role = InRole;
	const float Seconds = bAnimate ? PadelUi::Theme().Motion.BadgeSwap : 0.0f;
	// Spec 7.3: the badges cross-fade and the Selected stroke moves. The fade stays under
	// Reduce motion (spec 9 keeps fades).
	SwapTween.Snap(0.0f);
	SwapTween.Start(1.0f, Seconds);
	FrameTween.Start(IsSelectedCard() ? 1.0f : 0.0f, Seconds);
	if (Seconds <= 0.0f)
	{
		ShownRole = Role;
	}
	ApplyRoleVisuals();
}

void UPadelPlayerCard::BuildTree()
{
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	const FVector2D Size = bLarge ? LargeSize() : SmallSize();
	const float Radius = Theme.Layout.RadiusPanel;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = MakeSized(Root, Size.X, Size.Y);

	// The card's drop shadow (spec 4.3's recipe), spread half the blur past the card.
	const float Spread = Theme.Layout.ShadowBlur * 0.5f;
	UOverlaySlot* ShadowSlot = Root->AddChildToOverlay(
		MakeImage(PadelUiPaint::SoftBox(PadelUi::Colour(EUiColour::GlassShadow), Theme.Layout.ShadowBlur)));
	ShadowSlot->SetHorizontalAlignment(HAlign_Fill);
	ShadowSlot->SetVerticalAlignment(VAlign_Fill);
	ShadowSlot->SetPadding(FMargin(-Spread, -Spread + Theme.Layout.ShadowOffsetY, -Spread, -Spread - Theme.Layout.ShadowOffsetY));

	const bool bSolid = PadelUi::Options().bReduceTransparency;
	UOverlaySlot* FillSlot = Root->AddChildToOverlay(
		MakeImage(PadelUiPaint::Rounded(PadelUi::Colour(bSolid ? EUiColour::SurfaceCharcoal : EUiColour::GlassFill), Radius)));
	FillSlot->SetHorizontalAlignment(HAlign_Fill);
	FillSlot->SetVerticalAlignment(VAlign_Fill);

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	UOverlaySlot* CanvasSlot = Root->AddChildToOverlay(Canvas);
	CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
	CanvasSlot->SetVerticalAlignment(VAlign_Fill);
	if (bLarge)
	{
		BuildLarge(Canvas);
	}
	else
	{
		BuildSmall(Canvas);
	}

	// Frames over everything: the 1 px glass edge always, the 2 px Accent stroke when
	// Selected, faded in and out so it "moves" between the cards on a swap.
	UOverlaySlot* EdgeSlot = Root->AddChildToOverlay(
		MakeImage(PadelUiPaint::Rounded(FLinearColor::Transparent, Radius, PadelUi::Colour(EUiColour::GlassEdge), 1.0f)));
	EdgeSlot->SetHorizontalAlignment(HAlign_Fill);
	EdgeSlot->SetVerticalAlignment(VAlign_Fill);
	SelectedFrame = MakeImage(PadelUiPaint::Rounded(FLinearColor::Transparent, Radius, PadelUi::Colour(EUiColour::Accent),
		PlayerCardSelectedStroke));
	UOverlaySlot* SelectedSlot = Root->AddChildToOverlay(SelectedFrame);
	SelectedSlot->SetHorizontalAlignment(HAlign_Fill);
	SelectedSlot->SetVerticalAlignment(VAlign_Fill);

	SetVisibility(bLarge ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	Refresh();
}

void UPadelPlayerCard::BuildLarge(UCanvasPanel* Canvas)
{
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();

	// The team name in large outline type behind the portrait (Type.Display at 0.08).
	TeamOutline = MakeTokenText(FText::GetEmpty(), EUiType::Display, EUiColour::TextPrimary);
	FSlateFontInfo OutlineFont = PadelUi::Font(EUiType::Display);
	OutlineFont.OutlineSettings.OutlineSize = FMath::RoundToInt32(PlayerCardOutlineStroke);
	OutlineFont.OutlineSettings.OutlineColor = PadelUi::Colour(EUiColour::TextPrimary) * FLinearColor(1.0f, 1.0f, 1.0f, PlayerCardOutlineAlpha);
	OutlineFont.OutlineSettings.bSeparateFillAlpha = true;
	TeamOutline->SetFont(OutlineFont);
	TeamOutline->SetColorAndOpacity(FSlateColor(FLinearColor::Transparent));
	PlayerCardPlace(Canvas, TeamOutline, PlayerCardOutlineX, PlayerCardOutlineY, 0.0f, 0.0f);

	Portrait = WidgetTree->ConstructWidget<UPadelSilhouette>(UPadelSilhouette::StaticClass());
	PlayerCardPlace(Canvas, Portrait, PlayerCardPortraitX, 0.0f, PlayerCardPortraitW, PlayerCardPortraitH);

	// The profile line, in words. At Text size Largest it shrinks to stay above the strap.
	ProfileText = MakeTokenText(FText::GetEmpty(), EUiType::BodySmall, EUiColour::TextSecondary);
	UScaleBox* ProfileFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	ProfileFit->SetStretch(EStretch::ScaleToFit);
	ProfileFit->SetStretchDirection(EStretchDirection::DownOnly);
	if (UScaleBoxSlot* FitSlot = Cast<UScaleBoxSlot>(ProfileFit->SetContent(ProfileText)))
	{
		FitSlot->SetHorizontalAlignment(HAlign_Left);
		FitSlot->SetVerticalAlignment(VAlign_Top);
	}
	PlayerCardPlace(Canvas, ProfileFit, PlayerCardProfileX, PlayerCardProfileTop,
		PlayerCardLargeW - PlayerCardProfileX - PlayerCardProfileRight, PlayerCardStrapTop - PlayerCardProfileTop);

	Strap = WidgetTree->ConstructWidget<UPadelNameStrap>(UPadelNameStrap::StaticClass());
	PlayerCardPlace(Canvas, Strap, 0.0f, PlayerCardStrapTop, PlayerCardLargeW, PlayerCardStrapLarge);

	// Four stat rows: full-word label, a bar to number / 99, the number.
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const float Side = Theme.Space(3);
	const FLinearColor BarColour = PadelUi::Colour(EUiColour::TextPrimary) * FLinearColor(1.0f, 1.0f, 1.0f, PlayerCardStatBarAlpha);
	for (int32 Index = 0; Index < static_cast<int32>(Theme.PlayerCardStats.size()); ++Index)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Label = MakeTokenText(FText::GetEmpty(), EUiType::Body, EUiColour::TextSecondary);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeSized(Label, PlayerCardStatLabelW, 0.0f));
		LabelSlot->SetVerticalAlignment(VAlign_Center);

		UImage* Fill = nullptr;
		USpacer* Rest = nullptr;
		UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(MakeMeter(PlayerCardStatBar, BarColour, Fill, Rest));
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BarSlot->SetVerticalAlignment(VAlign_Center);
		BarSlot->SetPadding(FMargin(Theme.Space(2), 0.0f));

		UTextBlock* Value = MakeTokenText(FText::GetEmpty(), EUiType::Stat, EUiColour::TextPrimary);
		Value->SetFont(PadelUi::FontAtPx(EUiType::Stat, PlayerCardStatNumberPx));
		Value->SetJustification(ETextJustify::Right);
		UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(MakeSized(Value, PlayerCardNumberW, 0.0f));
		ValueSlot->SetVerticalAlignment(VAlign_Center);

		UVerticalBoxSlot* RowSlot = Rows->AddChildToVerticalBox(MakeSized(Row, 0.0f, PlayerCardStatRow));
		RowSlot->SetPadding(FMargin(0.0f, Index > 0 ? PlayerCardStatGap : 0.0f, 0.0f, 0.0f));
		StatLabels.Add(Label);
		StatValues.Add(Value);
		StatFills.Add(Fill);
		StatRests.Add(Rest);
	}
	PlayerCardPlace(Canvas, Rows, Side, PlayerCardStatsTop, PlayerCardLargeW - 2.0f * Side,
		PlayerCardLargeH - PlayerCardBand - PlayerCardStatsTop);

	PlayerCardPlace(Canvas, MakeOverallBand(Theme.Layout.RadiusPanel), 0.0f, PlayerCardLargeH - PlayerCardBand,
		PlayerCardLargeW, PlayerCardBand);
}

void UPadelPlayerCard::BuildSmall(UCanvasPanel* Canvas)
{
	const float Radius = PadelUi::Theme().Layout.RadiusPanel;
	Strap = WidgetTree->ConstructWidget<UPadelNameStrap>(UPadelNameStrap::StaticClass());
	Strap->SetCompact(true);
	Strap->SetTopRadius(Radius);
	PlayerCardPlace(Canvas, Strap, 0.0f, 0.0f, PlayerCardSmallW, PlayerCardStrapSmall);
	PlayerCardPlace(Canvas, MakeOverallBand(Radius), 0.0f, PlayerCardSmallH - PlayerCardBand, PlayerCardSmallW, PlayerCardBand);
}

UWidget* UPadelPlayerCard::MakeOverallBand(float Radius)
{
	// Spec 6.6: Surface.Charcoal, "Overall", a 12 px meter in the team colour, the number.
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Band->SetBrush(FSlateRoundedBoxBrush(PadelUi::Colour(EUiColour::SurfaceCharcoal), FVector4(0.0f, 0.0f, Radius, Radius)));
	Band->SetPadding(FMargin(Theme.Space(3), 0.0f));
	Band->SetVerticalAlignment(VAlign_Center);
	Band->SetContent(Row);

	UTextBlock* Label = MakeTokenText(LOCTEXT("Card.Overall", "Overall"), EUiType::Body, EUiColour::TextSecondary);
	Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
	// MakeMeter takes raw pointers; TObjectPtr members can't bind to UImage*&.
	UImage* Fill = nullptr;
	USpacer* Rest = nullptr;
	UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(
		MakeMeter(PlayerCardOverallBar, PadelUi::TeamColour(Card.Team), Fill, Rest));
	OverallFill = Fill;
	OverallRest = Rest;
	BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BarSlot->SetVerticalAlignment(VAlign_Center);
	BarSlot->SetPadding(FMargin(Theme.Space(2), 0.0f));
	OverallValue = MakeTokenText(FText::GetEmpty(), EUiType::Stat, EUiColour::TextPrimary);
	OverallValue->SetJustification(ETextJustify::Right);
	Row->AddChildToHorizontalBox(MakeSized(OverallValue, PlayerCardNumberW, 0.0f))->SetVerticalAlignment(VAlign_Center);
	return Band;
}

UWidget* UPadelPlayerCard::MakeMeter(float Height, const FLinearColor& FillColour, UImage*& OutFill, USpacer*& OutRest)
{
	const float Pill = Height * 0.5f;
	UOverlay* Meter = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UOverlaySlot* TrackSlot = Meter->AddChildToOverlay(MakeImage(PadelUiPaint::Rounded(PadelUi::Colour(EUiColour::GlassEdge), Pill)));
	TrackSlot->SetHorizontalAlignment(HAlign_Fill);
	TrackSlot->SetVerticalAlignment(VAlign_Fill);

	UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	OutFill = MakeImage(PadelUiPaint::Rounded(FillColour, Pill));
	UHorizontalBoxSlot* FillSlot = Split->AddChildToHorizontalBox(OutFill);
	FillSlot->SetVerticalAlignment(VAlign_Fill);
	OutRest = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Split->AddChildToHorizontalBox(OutRest);
	UOverlaySlot* SplitSlot = Meter->AddChildToOverlay(Split);
	SplitSlot->SetHorizontalAlignment(HAlign_Fill);
	SplitSlot->SetVerticalAlignment(VAlign_Fill);
	SetMeter(OutFill, OutRest, UnknownCardStat);
	return MakeSized(Meter, 0.0f, Height);
}

void UPadelPlayerCard::SetMeter(UImage* Fill, USpacer* Rest, int32 Value)
{
	// The fill and the rest share the track by weight: Value against 99 - Value.
	const float Filled = Padel::Ui::MeterFill(Value) * PlayerCardMeterMax;
	if (UHorizontalBoxSlot* FillSlot = Fill ? Cast<UHorizontalBoxSlot>(Fill->Slot) : nullptr)
	{
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Filled;
		FillSlot->SetSize(Size);
		Fill->SetVisibility(Filled > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UHorizontalBoxSlot* RestSlot = Rest ? Cast<UHorizontalBoxSlot>(Rest->Slot) : nullptr)
	{
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = PlayerCardMeterMax - Filled;
		RestSlot->SetSize(Size);
	}
}

void UPadelPlayerCard::Refresh()
{
	if (!Strap || !OverallValue)
	{
		return;
	}
	const FText Surname = FText::FromString(UTF8_TO_TCHAR(Card.Surname.c_str()));
	Strap->SetPlayer(FText::FromString(UTF8_TO_TCHAR(Card.FirstName.c_str())), Surname, Card.Team);

	OverallValue->SetText(PlayerCardNumber(Card.Overall));
	SetMeter(OverallFill, OverallRest, Card.Overall);

	if (Portrait)
	{
		Portrait->SetTeam(Card.Team);
	}
	if (TeamOutline)
	{
		TeamOutline->SetText(FText::FromString(UTF8_TO_TCHAR(Padel::Ui::TeamName(Card.Team).c_str())));
	}
	if (ProfileText)
	{
		// Spec 6.6: "Left side", "Right-handed", "1.86 m", "Power play". Without a
		// Players.json row only the roster's side and style are known.
		TArray<FText> Lines;
		Lines.Add(Card.bLeftSide ? LOCTEXT("Card.Side.Left", "Left side") : LOCTEXT("Card.Side.Right", "Right side"));
		if (Card.bHasRatings)
		{
			Lines.Add(Card.bLeftHanded ? LOCTEXT("Card.Hand.Left", "Left-handed") : LOCTEXT("Card.Hand.Right", "Right-handed"));
			FNumberFormattingOptions Metres;
			Metres.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
			Lines.Add(FText::Format(LOCTEXT("Card.Height", "{0} m"), FText::AsNumber(Card.HeightM, &Metres)));
		}
		const FText Style = PlayerCardStyle(Card.Style);
		if (!Style.IsEmpty())
		{
			Lines.Add(Style);
		}
		ProfileText->SetText(FText::Join(FText::FromString(TEXT("\n")), Lines));
	}
	for (int32 Index = 0; Index < StatValues.Num(); ++Index)
	{
		const bool bKnown = Card.Stats.size() > static_cast<size_t>(Index);
		const int32 Value = bKnown ? Card.Stats[Index].Value : UnknownCardStat;
		const std::string Label = bKnown ? Card.Stats[Index].Label : PadelUi::Theme().PlayerCardStats[Index].first;
		StatLabels[Index]->SetText(PlayerCardStatLabel(Label));
		StatValues[Index]->SetText(PlayerCardNumber(Value));
		SetMeter(StatFills[Index], StatRests[Index], Value);
	}
	ApplyRoleVisuals();
}

void UPadelPlayerCard::ApplyRoleVisuals()
{
	if (!Strap)
	{
		return;
	}
	Strap->SetBadge(ShownRole != EPadelCardRole::None,
		ShownRole == EPadelCardRole::Partner ? EPadelBadgeKind::Partner : EPadelBadgeKind::You);
	// Out over the first half of the swap, in over the second.
	const float T = SwapTween.Value();
	Strap->SetBadgeOpacity(SwapTween.IsDone() ? 1.0f : FMath::Abs(1.0f - 2.0f * T));
	if (SelectedFrame)
	{
		SelectedFrame->SetRenderOpacity(FrameTween.Value());
	}
}

void UPadelPlayerCard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (SwapTween.IsDone() && FrameTween.IsDone())
	{
		return;
	}
	SwapTween.Tick(InDeltaTime);
	FrameTween.Tick(InDeltaTime);
	if (SwapTween.IsDone() || SwapTween.Value() >= 0.5f)
	{
		ShownRole = Role;
	}
	ApplyRoleVisuals();
}

FReply UPadelPlayerCard::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bLarge && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPointerDown = true;
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPadelPlayerCard::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bPointerDown && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPointerDown = false;
		OnCardClicked.Broadcast(this);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FString UPadelPlayerCard::GetOverallText() const
{
	return OverallValue ? OverallValue->GetText().ToString() : FString();
}

FString UPadelPlayerCard::GetStatText(int32 Index) const
{
	return StatValues.IsValidIndex(Index) && StatValues[Index] ? StatValues[Index]->GetText().ToString() : FString();
}

FString UPadelPlayerCard::GetProfileText() const
{
	return ProfileText ? ProfileText->GetText().ToString() : FString();
}

#undef LOCTEXT_NAMESPACE
