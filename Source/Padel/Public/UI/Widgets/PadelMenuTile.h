#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "UI/Widgets/PadelIcon.h"
#include "PadelMenuTile.generated.h"

class UBorder;
class UImage;
class UPadelBadge;
class UPadelButtonGlyph;
class UPadelIcon;
class USizeBox;
class UTexture2D;
class UTextBlock;

/**
 * The main menu's picture buttons (spec 6.3). Hero is 8 columns by 520 px with a "Play"
 * chip on focus; Small is 4 columns by 248 px (208 in the bottom row). A tile without an
 * image (nothing is rendered yet, W12) or a locked one shows flat glass with a 64 px line
 * icon, so no art is commissioned for modes that don't exist.
 */
UCLASS()
class PADEL_API UPadelMenuTile : public UPadelFocusableWidget
{
	GENERATED_BODY()

public:
	void SetHero(bool bInHero);
	void SetTileSize(float InWidth, float InHeight);
	void SetLabel(const FText& InLabel);
	void SetDescription(const FText& InDescription);
	/** The tile's art; null shows flat glass with FallbackIcon. */
	void SetImage(UTexture2D* InImage);
	void SetFallbackIcon(EPadelIcon InIcon);
	/** Glyph family for the Hero's "A Play" chip. */
	void SetGlyphFamily(Padel::Ui::EPadFamily InFamily);

	FText GetLabel() const { return Label; }
	bool IsHero() const { return bHero; }

protected:
	virtual void BuildTree() override;
	virtual void RefreshVisuals() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual float PressedScale() const override;

private:
	bool bHero = false;
	float Width = 560.0f;
	float Height = 248.0f;
	FText Label;
	FText Description;
	EPadelIcon FallbackIcon = EPadelIcon::Court;
	Padel::Ui::EPadFamily GlyphFamily = Padel::Ui::EPadFamily::Xbox;
	float PushInSeconds = 0.0f;
	FPadelTween DescriptionAlpha;

	UPROPERTY()
	TObjectPtr<UTexture2D> Image;

	UPROPERTY()
	TObjectPtr<USizeBox> SizeRoot;

	UPROPERTY()
	TObjectPtr<UBorder> Frame;

	UPROPERTY()
	TObjectPtr<UImage> Art;

	UPROPERTY()
	TObjectPtr<UImage> Fade;

	UPROPERTY()
	TObjectPtr<UPadelIcon> ModeIcon;

	UPROPERTY()
	TObjectPtr<USizeBox> ModeIconBox;

	UPROPERTY()
	TObjectPtr<UPadelBadge> Badge;

	UPROPERTY()
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY()
	TObjectPtr<UPadelIcon> Padlock;

	UPROPERTY()
	TObjectPtr<USizeBox> PadlockBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY()
	TObjectPtr<UBorder> PlayChip;

	UPROPERTY()
	TObjectPtr<UPadelButtonGlyph> PlayGlyph;

	UPROPERTY()
	TObjectPtr<UBorder> Edge;
};
