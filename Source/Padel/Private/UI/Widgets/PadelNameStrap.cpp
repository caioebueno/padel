#include "UI/Widgets/PadelNameStrap.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

void UPadelNameStrap::SetPlayer(const FText& InFirstName, const FText& InSurname, int32 InTeam)
{
	FirstName = InFirstName;
	Surname = InSurname;
	Team = InTeam;
	Refresh();
}

void UPadelNameStrap::SetCompact(bool bInCompact)
{
	bCompact = bInCompact;
	Refresh();
}

void UPadelNameStrap::SetBadge(bool bShow, EPadelBadgeKind InKind)
{
	bShowBadge = bShow;
	BadgeKind = InKind;
	Refresh();
}

FString UPadelNameStrap::GetSurnameText() const
{
	return Surname.ToString();
}

void UPadelNameStrap::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeRoot = MakeSized(Root, 0.0f, 96.0f);
	WidgetTree->RootWidget = SizeRoot;

	UVerticalBox* Names = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Band->SetPadding(FMargin(24.0f, 4.0f, 24.0f, 0.0f));
	Band->SetVerticalAlignment(VAlign_Center);
	Band->SetContent(Names);
	UOverlaySlot* BandSlot = Root->AddChildToOverlay(Band);
	BandSlot->SetHorizontalAlignment(HAlign_Fill);
	BandSlot->SetVerticalAlignment(VAlign_Fill);

	// The thin white top edge of a broadcast strap.
	FSlateBrush EdgeBrush = PadelUiPaint::Solid(FLinearColor::White);
	EdgeBrush.ImageSize = FVector2D(4.0f, 4.0f);
	UOverlaySlot* EdgeSlot = Root->AddChildToOverlay(MakeImage(EdgeBrush));
	EdgeSlot->SetHorizontalAlignment(HAlign_Fill);
	EdgeSlot->SetVerticalAlignment(VAlign_Top);

	FirstText = MakeTokenText(FirstName, EUiType::Label, EUiColour::OnAccent);
	Names->AddChildToVerticalBox(FirstText);
	SurnameText = MakeTokenText(Surname, EUiType::Title, EUiColour::OnAccent);
	SurnameText->SetClipping(EWidgetClipping::ClipToBounds);
	Names->AddChildToVerticalBox(SurnameText);

	Badge = WidgetTree->ConstructWidget<UPadelBadge>(UPadelBadge::StaticClass());
	UOverlaySlot* BadgeSlot = Root->AddChildToOverlay(Badge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Right);
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	BadgeSlot->SetPadding(FMargin(0.0f, 16.0f, 16.0f, 0.0f));
	Refresh();
}

void UPadelNameStrap::Refresh()
{
	if (!Band || !FirstText || !SurnameText || !Badge || !SizeRoot)
	{
		return;
	}
	SizeRoot->SetHeightOverride(bCompact ? 72.0f : 96.0f);
	Band->SetBrush(PadelUiPaint::Solid(PadelUi::TeamColour(Team) * FLinearColor(1.0f, 1.0f, 1.0f, 0.9f)));
	FirstText->SetText(FirstName);
	FirstText->SetVisibility(FirstName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	SurnameText->SetText(Surname);
	// The small card's strap is shorter: the surname drops to Type.Heading.
	SurnameText->SetFont(PadelUi::Font(bCompact ? EUiType::Heading : EUiType::Title));
	Badge->SetKind(BadgeKind);
	Badge->SetVisibility(bShowBadge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
