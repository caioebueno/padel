#include "UI/Widgets/PadelBadge.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelIcon.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	constexpr float BadgeHeight = 32.0f;

	FText BadgeDefaultLabel(EPadelBadgeKind Kind)
	{
		switch (Kind)
		{
		case EPadelBadgeKind::ComingSoon:
			return LOCTEXT("Badge.ComingSoon", "Coming soon");
		case EPadelBadgeKind::New:
			return LOCTEXT("Badge.New", "New");
		case EPadelBadgeKind::You:
			return LOCTEXT("Badge.You", "You");
		case EPadelBadgeKind::Partner:
			return LOCTEXT("Badge.Partner", "Partner");
		}
		return FText::GetEmpty();
	}
}

void UPadelBadge::SetKind(EPadelBadgeKind InKind)
{
	Kind = InKind;
	Refresh();
}

void UPadelBadge::SetLabel(const FText& InLabel)
{
	CustomLabel = InLabel;
	Refresh();
}

FText UPadelBadge::GetLabel() const
{
	return CustomLabel.IsEmpty() ? BadgeDefaultLabel(Kind) : CustomLabel;
}

void UPadelBadge::BuildTree()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Pill = MakeRoundedBorder(FLinearColor::Transparent, BadgeHeight * 0.5f, FLinearColor::Transparent, 0.0f, FMargin(12.0f, 0.0f), Row);
	Pill->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = MakeSized(Pill, 0.0f, BadgeHeight);

	Padlock = WidgetTree->ConstructWidget<UPadelIcon>(UPadelIcon::StaticClass());
	Padlock->SetIcon(EPadelIcon::Padlock);
	PadlockBox = MakeSized(Padlock, 16.0f, 16.0f);
	UHorizontalBoxSlot* LockSlot = Row->AddChildToHorizontalBox(PadlockBox);
	LockSlot->SetVerticalAlignment(VAlign_Center);
	LockSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	Text = MakeTokenText(FText::GetEmpty(), EUiType::Label, EUiColour::TextPrimary);
	Row->AddChildToHorizontalBox(Text)->SetVerticalAlignment(VAlign_Center);
	Refresh();
}

void UPadelBadge::Refresh()
{
	if (!Pill || !Text || !Padlock)
	{
		return;
	}
	FLinearColor Fill = FLinearColor::Transparent;
	FLinearColor Stroke = FLinearColor::Transparent;
	float StrokeWidth = 0.0f;
	EUiColour TextColour = EUiColour::TextPrimary;
	switch (Kind)
	{
	case EPadelBadgeKind::ComingSoon:
		Fill = PadelUi::Colour(EUiColour::SurfaceCharcoal);
		TextColour = EUiColour::Lock;
		break;
	case EPadelBadgeKind::New:
		Fill = PadelUi::Colour(EUiColour::Accent);
		TextColour = EUiColour::OnAccent;
		break;
	case EPadelBadgeKind::You:
		Stroke = PadelUi::Colour(EUiColour::Accent);
		StrokeWidth = 2.0f;
		TextColour = EUiColour::Accent;
		break;
	case EPadelBadgeKind::Partner:
		Stroke = PadelUi::Colour(EUiColour::GlassEdge);
		StrokeWidth = 2.0f;
		TextColour = EUiColour::TextSecondary;
		break;
	}
	Pill->SetBrush(PadelUiPaint::Rounded(Fill, BadgeHeight * 0.5f, Stroke, StrokeWidth));
	Text->SetText(GetLabel());
	Text->SetColorAndOpacity(FSlateColor(PadelUi::Colour(TextColour)));
	Padlock->SetTint(PadelUi::Colour(EUiColour::Lock));
	PadlockBox->SetVisibility(Kind == EPadelBadgeKind::ComingSoon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
