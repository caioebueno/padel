#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PadelWidgetBase.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * Base for the game's widgets. Each one builds its tree in C++ (BuildTree) instead of
 * a binary Widget Blueprint, so layout and logic stay reviewable text (CLAUDE.md:
 * logic in C++). A thin Blueprint subclass may still restyle one later.
 */
UCLASS(Abstract)
class PADEL_API UPadelWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Create the widget tree and set WidgetTree->RootWidget. Called once. */
	virtual void BuildTree() PURE_VIRTUAL(UPadelWidgetBase::BuildTree, );

	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Colour);
	UBorder* MakePanel(const FLinearColor& Colour, float InnerPadding);
	/** A big menu button with a clear focus colour, so a gamepad user always knows what A presses. */
	UButton* MakeMenuButton(const FText& Label, UTextBlock** OutLabel = nullptr);

	/** Gives keyboard/gamepad focus to the first button, so menus work without a mouse. */
	void FocusFirst();

	UPROPERTY()
	TArray<TObjectPtr<UButton>> FocusOrder;

	/** Label of each FocusOrder button, recoloured when its button has focus. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> FocusLabels;
};
