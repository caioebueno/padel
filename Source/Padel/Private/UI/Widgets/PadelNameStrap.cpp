#include "UI/Widgets/PadelNameStrap.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	// Board "PlayerCard" (spec 6.6): strap heights, the white top edge, the band's alpha.
	constexpr float NameStrapLargeHeight = 96.0f;
	constexpr float NameStrapCompactHeight = 72.0f;
	constexpr float NameStrapEdge = 4.0f;
	constexpr float NameStrapAlpha = 0.9f;
	constexpr float NameStrapCompactSide = 20.0f;
	constexpr float NameStrapInlineGap = 12.0f;
	// Lifts the first name onto the surname's baseline when they share a line (Label 30 px
	// line against Title 48 px: the descender gap differs by about 4 px).
	constexpr float NameStrapLabelBaseline = 4.0f;
}

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

void UPadelNameStrap::SetBadgeOpacity(float Opacity)
{
	if (Badge)
	{
		Badge->SetRenderOpacity(Opacity);
	}
}

void UPadelNameStrap::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeRoot = MakeSized(Root, 0.0f, NameStrapLargeHeight);
	WidgetTree->RootWidget = SizeRoot;

	// Names on the left (filling), the badge on the right, both centred on the band.
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Band->SetVerticalAlignment(VAlign_Center);
	Band->SetContent(Row);
	UOverlaySlot* BandSlot = Root->AddChildToOverlay(Band);
	BandSlot->SetHorizontalAlignment(HAlign_Fill);
	BandSlot->SetVerticalAlignment(VAlign_Fill);

	UOverlay* Names = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UHorizontalBoxSlot* NamesSlot = Row->AddChildToHorizontalBox(Names);
	NamesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NamesSlot->SetVerticalAlignment(VAlign_Center);
	StackedNames = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Names->AddChildToOverlay(StackedNames)->SetVerticalAlignment(VAlign_Center);
	InlineNames = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Names->AddChildToOverlay(InlineNames)->SetVerticalAlignment(VAlign_Center);

	FirstText = MakeTokenText(FirstName, EUiType::Label, EUiColour::OnAccent);
	SurnameText = MakeTokenText(Surname, EUiType::Title, EUiColour::OnAccent);
	SurnameText->SetClipping(EWidgetClipping::ClipToBounds);

	Badge = WidgetTree->ConstructWidget<UPadelBadge>(UPadelBadge::StaticClass());
	UHorizontalBoxSlot* BadgeSlot = Row->AddChildToHorizontalBox(Badge);
	BadgeSlot->SetVerticalAlignment(VAlign_Center);
	BadgeSlot->SetPadding(FMargin(PadelUi::Space(2), 0.0f, 0.0f, 0.0f));

	// The thin white top edge of a broadcast strap, over the band.
	FSlateBrush EdgeBrush = PadelUiPaint::Solid(FLinearColor::White);
	EdgeBrush.ImageSize = FVector2D(NameStrapEdge, NameStrapEdge);
	UOverlaySlot* EdgeSlot = Root->AddChildToOverlay(MakeImage(EdgeBrush));
	EdgeSlot->SetHorizontalAlignment(HAlign_Fill);
	EdgeSlot->SetVerticalAlignment(VAlign_Top);
	Refresh();
}

void UPadelNameStrap::Refresh()
{
	if (!Band || !FirstText || !SurnameText || !Badge || !SizeRoot || !StackedNames || !InlineNames)
	{
		return;
	}
	// Board "PlayerCard": the large strap is 96 px with 24 px sides; the small one 72 px
	// with 20 px sides and 4 px more at the top, clearing the white edge.
	SizeRoot->SetHeightOverride(bCompact ? NameStrapCompactHeight : NameStrapLargeHeight);
	Band->SetPadding(bCompact ? FMargin(NameStrapCompactSide, NameStrapEdge, NameStrapCompactSide, 0.0f)
		: FMargin(PadelUi::Space(3), 0.0f, PadelUi::Space(3), 0.0f));
	Band->SetBrush(PadelUiPaint::Solid(PadelUi::TeamColour(Team) * FLinearColor(1.0f, 1.0f, 1.0f, NameStrapAlpha)));

	// Move the two names into the stacked or the inline box.
	FirstText->RemoveFromParent();
	SurnameText->RemoveFromParent();
	if (bCompact)
	{
		UHorizontalBoxSlot* FirstSlot = InlineNames->AddChildToHorizontalBox(FirstText);
		FirstSlot->SetVerticalAlignment(VAlign_Bottom);
		FirstSlot->SetPadding(FMargin(0.0f, 0.0f, NameStrapInlineGap, NameStrapLabelBaseline));
		UHorizontalBoxSlot* SurnameSlot = InlineNames->AddChildToHorizontalBox(SurnameText);
		SurnameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SurnameSlot->SetVerticalAlignment(VAlign_Bottom);
	}
	else
	{
		StackedNames->AddChildToVerticalBox(FirstText);
		StackedNames->AddChildToVerticalBox(SurnameText);
	}

	FirstText->SetText(FirstName);
	FirstText->SetVisibility(FirstName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	SurnameText->SetText(Surname);
	Badge->SetKind(BadgeKind);
	Badge->SetVisibility(bShowBadge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
