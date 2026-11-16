#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "PadelButton.generated.h"

class UBorder;
class UImage;
class UPadelButtonGlyph;
class UPadelIcon;
class USizeBox;
class UTextBlock;

/** Spec 6.2's three buttons, plus the Danger variant of the primary one. */
UENUM()
enum class EPadelButtonKind : uint8
{
	/** The one main action on a screen: lime when focused. 448 x 80, or 320 x 72 in dialogs. */
	Primary,
	/** The alternative beside a primary ("Main menu", a dialog's safe option). */
	Secondary,
	/** A row in a vertical menu: a lime bar and an 8 px slide instead of the ring. */
	List,
	/** A destructive dialog action ("Quit game"): State.Danger when focused. */
	Danger,
};

/**
 * UPadelPrimaryButton, UPadelSecondaryButton and UPadelListButton in spec 6.2, as one
 * class with a kind: their anatomy is the same and only the state table differs.
 */
UCLASS()
class PADEL_API UPadelButton : public UPadelFocusableWidget
{
	GENERATED_BODY()

public:
	void SetKind(EPadelButtonKind InKind);
	EPadelButtonKind GetKind() const { return Kind; }
	void SetLabel(const FText& InLabel);
	FText GetLabel() const { return Label; }
	/** Fixed size in px; 0 for an axis keeps the kind's default (a list row fills its column). */
	void SetSize(float InWidth, float InHeight);
	/** Primary only: the leading A glyph (spec 6.2). */
	void SetShowGlyph(bool bShow, Padel::Ui::EPadFamily InFamily = Padel::Ui::EPadFamily::Xbox);
	/** List only: a value on the right ("ON", "Medium"), or a chevron if bChevron. */
	void SetTrailing(const FText& InValue, bool bChevron = false);

protected:
	virtual void BuildTree() override;
	virtual void RefreshVisuals() override;
	virtual float FocusedScale() const override;

private:
	EPadelButtonKind Kind = EPadelButtonKind::Primary;
	FText Label;
	FText Trailing;
	bool bTrailingChevron = false;
	bool bShowGlyph = false;
	Padel::Ui::EPadFamily GlyphFamily = Padel::Ui::EPadFamily::Xbox;
	float Width = 0.0f;
	float Height = 0.0f;

	UPROPERTY()
	TObjectPtr<USizeBox> SizeRoot;

	UPROPERTY()
	TObjectPtr<UBorder> Fill;

	UPROPERTY()
	TObjectPtr<UImage> ListBar;

	UPROPERTY()
	TObjectPtr<USizeBox> ListBarBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TrailingText;

	UPROPERTY()
	TObjectPtr<UPadelIcon> Chevron;

	UPROPERTY()
	TObjectPtr<USizeBox> ChevronBox;

	UPROPERTY()
	TObjectPtr<UPadelButtonGlyph> Glyph;
};
