#include "UI/PadelMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "UI/PadelMenuButtonRelay.h"
#include "UI/PadelUiStyle.h"

void UPadelMenuWidget::BuildTree()
{
	SetIsFocusable(true);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// Dim the court behind the menu.
	UBorder* Backdrop = MakePanel(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 0.0f);
	UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(Backdrop);
	BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
	BackdropSlot->SetVerticalAlignment(VAlign_Fill);

	UBorder* Card = MakePanel(PadelUiStyle::Panel, 40.0f);
	UOverlaySlot* CardSlot = Root->AddChildToOverlay(Card);
	CardSlot->SetHorizontalAlignment(HAlign_Center);
	CardSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Column);

	TitleText = MakeText(FText::GetEmpty(), PadelUiStyle::FontTitle, PadelUiStyle::Text);
	Column->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	BodyText = MakeText(FText::GetEmpty(), PadelUiStyle::FontBody, PadelUiStyle::TextMuted);
	BodyText->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(BodyText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	TableBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TableBox->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(TableBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));

	OptionsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->AddChildToVerticalBox(OptionsBox);
}

void UPadelMenuWidget::SetTitle(const FText& Title)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
}

FString UPadelMenuWidget::GetTitle() const
{
	return TitleText ? TitleText->GetText().ToString() : FString();
}

void UPadelMenuWidget::SetBody(const FText& Body)
{
	if (BodyText)
	{
		BodyText->SetText(Body);
		BodyText->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UPadelMenuWidget::SetTable(const TArray<TArray<FText>>& Rows)
{
	if (!TableBox)
	{
		return;
	}
	TableBox->ClearChildren();
	TableBox->SetVisibility(Rows.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		const bool bHeader = RowIndex == 0;
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		TableBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 4.0f));
		for (int32 Column = 0; Column < Rows[RowIndex].Num(); ++Column)
		{
			// First column is the label; the others are one per team, in team colours in the header.
			const FLinearColor Colour = bHeader && Column > 0 ? PadelUiStyle::TeamColour(Column - 1)
				: (Column == 0 ? PadelUiStyle::TextMuted : PadelUiStyle::Text);
			UTextBlock* Cell = MakeText(Rows[RowIndex][Column], PadelUiStyle::FontBody, Colour);
			Cell->SetJustification(Column == 0 ? ETextJustify::Left : ETextJustify::Center);
			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Size->SetWidthOverride(Column == 0 ? 420.0f : 260.0f);
			Size->SetContent(Cell);
			Row->AddChildToHorizontalBox(Size);
		}
	}
}

void UPadelMenuWidget::SetOptions(const TArray<FText>& Labels)
{
	if (!OptionsBox)
	{
		return;
	}
	OptionsBox->ClearChildren();
	FocusOrder.Reset();
	FocusLabels.Reset();
	Relays.Reset();
	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		UButton* Button = MakeMenuButton(Labels[Index]);
		OptionsBox->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 6.0f));
		UPadelMenuButtonRelay* Relay = NewObject<UPadelMenuButtonRelay>(this);
		Relay->Menu = this;
		Relay->Index = Index;
		Button->OnClicked.AddDynamic(Relay, &UPadelMenuButtonRelay::HandleClicked);
		Relays.Add(Relay);
	}
}

void UPadelMenuWidget::SetOptionLabel(int32 Index, const FText& Label)
{
	if (FocusLabels.IsValidIndex(Index) && FocusLabels[Index])
	{
		FocusLabels[Index]->SetText(Label);
	}
}

FString UPadelMenuWidget::GetOptionLabel(int32 Index) const
{
	return FocusLabels.IsValidIndex(Index) && FocusLabels[Index] ? FocusLabels[Index]->GetText().ToString() : FString();
}

void UPadelMenuWidget::FocusOption(int32 Index)
{
	if (FocusOrder.IsValidIndex(Index) && FocusOrder[Index])
	{
		FocusOrder[Index]->SetFocus();
	}
	else
	{
		FocusFirst();
	}
}

int32 UPadelMenuWidget::GetFocusedOption() const
{
	for (int32 Index = 0; Index < FocusOrder.Num(); ++Index)
	{
		if (FocusOrder[Index] && FocusOrder[Index]->HasKeyboardFocus())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UPadelMenuWidget::ChooseOption(int32 Index)
{
	OnOptionChosen.Broadcast(Index);
}

FReply UPadelMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		OnBack.Broadcast();
		return FReply::Handled();
	}
	const bool bLeft = Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left;
	const bool bRight = Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right;
	if (bLeft || bRight)
	{
		const int32 Focused = GetFocusedOption();
		if (Focused != INDEX_NONE && OnOptionCycled.IsBound())
		{
			OnOptionCycled.Broadcast(Focused, bRight ? 1 : -1);
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
