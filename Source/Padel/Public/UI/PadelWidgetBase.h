#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "Layout/Margin.h"
#include "UI/Model/PadelUiTheme.h"
#include "PadelWidgetBase.generated.h"

class UBorder;
class UButton;
class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * Base for the game's widgets. Each one builds its tree in C++ (BuildTree) instead of
 * a binary Widget Blueprint, so layout and logic stay reviewable text (CLAUDE.md:
 * logic in C++). A thin Blueprint subclass may still restyle one later.
 */
UCLASS(Abstract)
class PADEL_API UPadelWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Build the tree now if it isn't built yet (it's otherwise built with the Slate widget). */
	void EnsureBuilt();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Create the widget tree and set WidgetTree->RootWidget. Called once. */
	virtual void BuildTree() PURE_VIRTUAL(UPadelWidgetBase::BuildTree, );

	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Colour);
	UBorder* MakePanel(const FLinearColor& Colour, float InnerPadding);
	/** A big menu button with a clear focus colour, so a gamepad user always knows what A presses. */
	UButton* MakeMenuButton(const FText& Label, UTextBlock** OutLabel = nullptr);

	// --- Menu look (docs/ui/MENU_SPEC.md 4): every value comes from PadelUiTokens. ---

	/** Text in a type token and colour token; upper case is applied by the style (spec 8). */
	UTextBlock* MakeTokenText(const FText& Text, Padel::Ui::EUiType Type, Padel::Ui::EUiColour Colour);
	/** A rounded fill with an optional inner stroke, holding Content with Padding. */
	UBorder* MakeRoundedBorder(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth,
		const FMargin& Padding, UWidget* Content = nullptr);
	/**
	 * The glass panel recipe (spec 4.3): Glass.Fill, 1 px Glass.Edge, a 1 px Glass.EdgeTop
	 * light catch and a soft drop shadow. OutFill is the fill border, to raise it on focus.
	 * Reduce transparency swaps the glass for Surface.Charcoal.
	 */
	UOverlay* MakeGlassPanel(UWidget* Content, const FMargin& Padding, float Radius, UBorder** OutFill = nullptr);
	/** A fixed-size box (0 leaves that axis free). */
	USizeBox* MakeSized(UWidget* Content, float Width, float Height);
	UImage* MakeImage(const FSlateBrush& Brush);

	/** Gives keyboard/gamepad focus to the first button, so menus work without a mouse. */
	void FocusFirst();

	// Gives a button keyboard focus when a local player owns this widget. Without one
	// (headless automation tests) UWidget::SetFocus logs an error, so skip it there.
	static void FocusButton(UButton* Button);

	UPROPERTY()
	TArray<TObjectPtr<UButton>> FocusOrder;

	/** Label of each FocusOrder button, recoloured when its button has focus. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> FocusLabels;
};
