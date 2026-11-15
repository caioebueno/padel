#include "UI/PadelWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"
#include "UI/PadelUiStyle.h"

TSharedRef<SWidget> UPadelWidgetBase::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UPadelWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Slate's own focus outline is faint on a TV; make the focused button obvious.
	for (int32 Index = 0; Index < FocusOrder.Num(); ++Index)
	{
		UButton* Button = FocusOrder[Index];
		UTextBlock* Label = FocusLabels.IsValidIndex(Index) ? FocusLabels[Index].Get() : nullptr;
		if (!Button || !Label)
		{
			continue;
		}
		const bool bFocused = Button->HasKeyboardFocus() || Button->IsHovered();
		Button->SetBackgroundColor(bFocused ? PadelUiStyle::Accent : FLinearColor::White);
		Label->SetColorAndOpacity(FSlateColor(bFocused ? PadelUiStyle::PointsText : PadelUiStyle::Text));
	}
}

UTextBlock* UPadelWidgetBase::MakeText(const FText& Text, int32 Size, const FLinearColor& Colour)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(PadelUiStyle::Font(Size));
	Block->SetColorAndOpacity(FSlateColor(Colour));
	return Block;
}

UBorder* UPadelWidgetBase::MakePanel(const FLinearColor& Colour, float InnerPadding)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Border->SetBrushColor(Colour);
	Border->SetPadding(FMargin(InnerPadding));
	return Border;
}

UButton* UPadelWidgetBase::MakeMenuButton(const FText& Label, UTextBlock** OutLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = Button->GetStyle();
	Style.Normal.TintColor = FSlateColor(PadelUiStyle::ButtonNormal);
	Style.Hovered.TintColor = FSlateColor(PadelUiStyle::ButtonFocused);
	Style.Pressed.TintColor = FSlateColor(PadelUiStyle::ButtonFocused);
	Style.NormalPadding = FMargin(24.0f, 12.0f);
	Style.PressedPadding = FMargin(24.0f, 12.0f);
	Button->SetStyle(Style);

	UTextBlock* Text = MakeText(Label, PadelUiStyle::FontBody, PadelUiStyle::Text);
	Button->AddChild(Text);
	if (OutLabel)
	{
		*OutLabel = Text;
	}
	FocusOrder.Add(Button);
	FocusLabels.Add(Text);
	return Button;
}

void UPadelWidgetBase::FocusButton(UButton* Button)
{
	if (Button && Button->GetOwningLocalPlayer())
	{
		Button->SetFocus();
	}
}

void UPadelWidgetBase::FocusFirst()
{
	for (UButton* Button : FocusOrder)
	{
		if (Button && Button->IsVisible())
		{
			FocusButton(Button);
			return;
		}
	}
}
