#include "UI/Widgets/PadelScreenHeader.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/NamedSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

void UPadelScreenHeader::SetTitle(const FText& InTitle)
{
	Title = InTitle;
	Refresh();
}

void UPadelScreenHeader::SetBreadcrumb(const FText& InBreadcrumb)
{
	Breadcrumb = InBreadcrumb;
	Refresh();
}

void UPadelScreenHeader::SetRightContent(UWidget* Content)
{
	if (RightSlot)
	{
		RightSlot->SetContent(Content);
	}
}

void UPadelScreenHeader::SetTitleColour(const FLinearColor& InColour)
{
	TitleColour = InColour;
	Refresh();
}

void UPadelScreenHeader::BuildTree()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WidgetTree->RootWidget = MakeSized(Row, 0.0f, PadelUi::Theme().Layout.HeaderHeight);

	UVerticalBox* Titles = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BreadcrumbText = MakeTokenText(Breadcrumb, EUiType::Label, EUiColour::TextSecondary);
	Titles->AddChildToVerticalBox(BreadcrumbText);
	TitleText = MakeTokenText(Title, EUiType::Display, EUiColour::TextPrimary);
	Titles->AddChildToVerticalBox(TitleText);
	UHorizontalBoxSlot* TitlesSlot = Row->AddChildToHorizontalBox(Titles);
	TitlesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitlesSlot->SetVerticalAlignment(VAlign_Center);

	RightSlot = WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass());
	Row->AddChildToHorizontalBox(RightSlot)->SetVerticalAlignment(VAlign_Center);
	Refresh();
}

void UPadelScreenHeader::Refresh()
{
	if (!TitleText || !BreadcrumbText)
	{
		return;
	}
	TitleText->SetText(Title);
	TitleText->SetColorAndOpacity(FSlateColor(TitleColour.Get(PadelUi::Colour(EUiColour::TextPrimary))));
	BreadcrumbText->SetText(Breadcrumb);
	BreadcrumbText->SetVisibility(Breadcrumb.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
