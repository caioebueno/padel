#include "UI/Widgets/PadelMenuTile.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelBadge.h"
#include "UI/Widgets/PadelButtonGlyph.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	// Spec 6.3: the Hero label is Type.Title at 56 px.
	constexpr float HeroLabelPx = 56.0f;
	constexpr float TilePadding = 32.0f;
	// The bottom gradient covers the lower 55% of the tile.
	constexpr float TileFadeShare = 0.55f;
	constexpr float TileFadeAlpha = 0.85f;
	// Default image: brightness 0.85 (spec 6.3). UMG can't desaturate without a material.
	constexpr float TileRestBrightness = 0.85f;
}

void UPadelMenuTile::SetHero(bool bInHero)
{
	bHero = bInHero;
	if (bHero)
	{
		Width = PadelUi::SpanWidth(8);
		Height = 520.0f;
	}
	RefreshVisuals();
}

void UPadelMenuTile::SetTileSize(float InWidth, float InHeight)
{
	Width = InWidth;
	Height = InHeight;
	RefreshVisuals();
}

void UPadelMenuTile::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	RefreshVisuals();
}

void UPadelMenuTile::SetDescription(const FText& InDescription)
{
	Description = InDescription;
	RefreshVisuals();
}

void UPadelMenuTile::SetImage(UTexture2D* InImage)
{
	Image = InImage;
	RefreshVisuals();
}

void UPadelMenuTile::SetFallbackIcon(EPadelIcon InIcon)
{
	FallbackIcon = InIcon;
	RefreshVisuals();
}

void UPadelMenuTile::SetGlyphFamily(Padel::Ui::EPadFamily InFamily)
{
	GlyphFamily = InFamily;
	if (PlayGlyph)
	{
		PlayGlyph->SetFamily(InFamily);
	}
}

float UPadelMenuTile::PressedScale() const
{
	return PadelUi::Theme().Layout.PressScaleTile;
}

void UPadelMenuTile::BuildTree()
{
	const float Radius = PadelUi::Theme().Layout.RadiusPanel;
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeRoot = MakeSized(Root, Width, Height);
	WidgetTree->RootWidget = SizeRoot;

	UOverlay* Inner = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Frame = MakeRoundedBorder(PadelUi::Colour(EUiColour::GlassFill), Radius, FLinearColor::Transparent, 0.0f, FMargin(0.0f), Inner);
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);
	// The tile clips its image (spec 6.3).
	Frame->SetClipping(EWidgetClipping::ClipToBounds);
	UOverlaySlot* FrameSlot = Root->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Fill);
	FrameSlot->SetVerticalAlignment(VAlign_Fill);

	Art = MakeImage(PadelUiPaint::Solid(FLinearColor::White));
	UOverlaySlot* ArtSlot = Inner->AddChildToOverlay(Art);
	ArtSlot->SetHorizontalAlignment(HAlign_Fill);
	ArtSlot->SetVerticalAlignment(VAlign_Fill);

	ModeIcon = WidgetTree->ConstructWidget<UPadelIcon>(UPadelIcon::StaticClass());
	ModeIconBox = MakeSized(ModeIcon, 64.0f, 64.0f);
	UOverlaySlot* IconSlot = Inner->AddChildToOverlay(ModeIconBox);
	IconSlot->SetHorizontalAlignment(HAlign_Center);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 48.0f));

	Fade = MakeImage(PadelUiPaint::FadeUp(PadelUi::Colour(EUiColour::BgDeep, TileFadeAlpha)));
	UOverlaySlot* FadeSlot = Inner->AddChildToOverlay(MakeSized(Fade, 0.0f, Height * TileFadeShare));
	FadeSlot->SetHorizontalAlignment(HAlign_Fill);
	FadeSlot->SetVerticalAlignment(VAlign_Bottom);

	Badge = WidgetTree->ConstructWidget<UPadelBadge>(UPadelBadge::StaticClass());
	Badge->SetKind(EPadelBadgeKind::ComingSoon);
	UOverlaySlot* BadgeSlot = Inner->AddChildToOverlay(Badge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Left);
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	BadgeSlot->SetPadding(FMargin(24.0f));

	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UOverlaySlot* TextSlot = Inner->AddChildToOverlay(Text);
	TextSlot->SetHorizontalAlignment(HAlign_Fill);
	TextSlot->SetVerticalAlignment(VAlign_Bottom);
	TextSlot->SetPadding(FMargin(TilePadding));

	UHorizontalBox* LabelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Text->AddChildToVerticalBox(LabelRow);
	LabelText = MakeTokenText(Label, EUiType::Heading, EUiColour::TextPrimary);
	LabelRow->AddChildToHorizontalBox(LabelText)->SetVerticalAlignment(VAlign_Center);
	Padlock = WidgetTree->ConstructWidget<UPadelIcon>(UPadelIcon::StaticClass());
	Padlock->SetIcon(EPadelIcon::Padlock);
	PadlockBox = MakeSized(Padlock, 32.0f, 32.0f);
	UHorizontalBoxSlot* LockSlot = LabelRow->AddChildToHorizontalBox(PadlockBox);
	LockSlot->SetVerticalAlignment(VAlign_Center);
	LockSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));

	DescriptionText = MakeTokenText(Description, EUiType::BodySmall, EUiColour::TextSecondary);
	DescriptionText->SetAutoWrapText(true);
	Text->AddChildToVerticalBox(DescriptionText)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	// Hero only: the "A Play" chip bottom right, shown on focus.
	UHorizontalBox* ChipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PlayChip = MakeRoundedBorder(PadelUi::Colour(EUiColour::SurfaceCharcoal), 28.0f, PadelUi::Colour(EUiColour::GlassEdge), 1.0f,
		FMargin(8.0f, 8.0f, 24.0f, 8.0f), ChipRow);
	PlayGlyph = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
	PlayGlyph->SetInput(Padel::Ui::EUiInput::Confirm);
	PlayGlyph->SetFamily(GlyphFamily);
	PlayGlyph->SetDiameter(40.0f);
	ChipRow->AddChildToHorizontalBox(PlayGlyph)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* PlaySlot = ChipRow->AddChildToHorizontalBox(
		MakeTokenText(LOCTEXT("Tile.Play", "Play"), EUiType::Heading, EUiColour::TextPrimary));
	PlaySlot->SetVerticalAlignment(VAlign_Center);
	PlaySlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
	UOverlaySlot* ChipSlot = Inner->AddChildToOverlay(PlayChip);
	ChipSlot->SetHorizontalAlignment(HAlign_Right);
	ChipSlot->SetVerticalAlignment(VAlign_Bottom);
	ChipSlot->SetPadding(FMargin(TilePadding));

	// 1 px Glass.Edge frame over everything.
	Edge = MakeRoundedBorder(FLinearColor::Transparent, Radius, PadelUi::Colour(EUiColour::GlassEdge), 1.0f, FMargin(0.0f));
	Edge->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* EdgeSlot = Root->AddChildToOverlay(Edge);
	EdgeSlot->SetHorizontalAlignment(HAlign_Fill);
	EdgeSlot->SetVerticalAlignment(VAlign_Fill);

	AddFocusRing(Root, Radius);
	DescriptionAlpha.Snap(0.0f);
	RefreshVisuals();
}

void UPadelMenuTile::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	DescriptionAlpha.Tick(InDeltaTime);
	if (DescriptionText && !IsLocked())
	{
		DescriptionText->SetRenderOpacity(DescriptionAlpha.Value());
	}
	// Focused tiles push in slowly, 1.0 -> 1.04 over 6 s (spec 9), unless Reduce motion.
	if (Art && Image)
	{
		PushInSeconds = IsElementFocused() ? FMath::Min(PushInSeconds + InDeltaTime, Motion.PushIn) : 0.0f;
		const float Alpha = Motion.PushIn > 0.0f ? PushInSeconds / Motion.PushIn : 1.0f;
		const float Scale = PadelUi::Options().bReduceMotion ? 1.0f : FMath::Lerp(1.0f, Motion.PushInScale, Alpha);
		Art->SetRenderScale(FVector2D(Scale, Scale));
	}
}

void UPadelMenuTile::RefreshVisuals()
{
	if (!SizeRoot || !Frame || !LabelText)
	{
		return;
	}
	const bool bFocused = IsElementFocused();
	const bool bLocked = IsLocked();
	const bool bHasArt = Image != nullptr && !bLocked;
	SizeRoot->SetWidthOverride(Width);
	SizeRoot->SetHeightOverride(Height);
	if (USizeBox* FadeBox = Fade ? Cast<USizeBox>(Fade->GetParent()) : nullptr)
	{
		FadeBox->SetHeightOverride(Height * TileFadeShare);
	}

	// Locked: flat glass, the fill doesn't raise even when focused.
	Frame->SetBrush(PadelUiPaint::Rounded(PadelUi::Colour(EUiColour::GlassFill), PadelUi::Theme().Layout.RadiusPanel));
	if (Art)
	{
		if (bHasArt)
		{
			Art->SetBrushFromTexture(Image);
			const float Brightness = bFocused ? 1.0f : TileRestBrightness;
			Art->SetColorAndOpacity(FLinearColor(Brightness, Brightness, Brightness, 1.0f));
			Art->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Art->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (ModeIcon && ModeIconBox)
	{
		ModeIcon->SetIcon(FallbackIcon);
		ModeIcon->SetTint(PadelUi::Colour(bLocked ? EUiColour::Lock : EUiColour::TextSecondary));
		ModeIconBox->SetVisibility(bHasArt ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Fade)
	{
		Fade->SetVisibility(bHasArt ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	LabelText->SetText(Label);
	LabelText->SetFont(bHero ? PadelUi::FontAtPx(EUiType::Title, HeroLabelPx) : PadelUi::Font(EUiType::Heading));
	// Locked labels and descriptions are Text.Secondary: meant to be read (>= 6:1).
	LabelText->SetColorAndOpacity(FSlateColor(PadelUi::Colour(bLocked ? EUiColour::TextSecondary : EUiColour::TextPrimary)));
	if (PadlockBox && Padlock)
	{
		Padlock->SetTint(PadelUi::Colour(EUiColour::Lock));
		PadlockBox->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Badge)
	{
		Badge->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
		DescriptionText->SetVisibility(Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		// The description fades in on focus; a locked tile always explains itself.
		const float Target = (bFocused || bLocked) ? 1.0f : 0.0f;
		if (DescriptionAlpha.To != Target)
		{
			DescriptionAlpha.Start(Target, PadelUi::Theme().Motion.Focus);
		}
		if (bLocked)
		{
			DescriptionText->SetRenderOpacity(1.0f);
		}
	}
	if (PlayChip)
	{
		PlayChip->SetVisibility(bHero && bFocused && !bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	ShowFocusRing(bFocused);
}

#undef LOCTEXT_NAMESPACE
