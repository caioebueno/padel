#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiPlayerCard.h"
#include "UI/PadelWidgetBase.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "PadelPlayerCard.generated.h"

class UCanvasPanel;
class UImage;
class UPadelNameStrap;
class UPadelPlayerCard;
class UPadelSilhouette;
class USpacer;
class UTextBlock;

/** Who the card's player is to the human (spec 6.6's states). */
UENUM()
enum class EPadelCardRole : uint8
{
	/** An opponent: no badge, plain frame. */
	None,
	/** The player you control: "YOU" badge and a 2 px Accent frame (Selected). */
	You,
	/** Your AI partner: "PARTNER" badge, plain frame. */
	Partner,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelCardClicked, UPadelPlayerCard* /*Card*/);

/**
 * Spec 6.6's player card, to the design board "PlayerCard": our own broadcast "player
 * graphic" built round the lower-third name strap, not a trading card.
 *
 * Large (390 x 560, Setup's pair): the team name in outline type behind a waist-up
 * portrait (a team-colour silhouette until W12), the profile line in words beside it, the
 * name strap with the You / Partner badge, four stat rows and the Overall meter band.
 * Small (321 x 200, the opponents): the strap and the Overall band only.
 *
 * Not focusable in the slice (X swaps from anywhere on Setup), but a large card can be
 * clicked, which Setup treats as the swap (spec 7.3).
 */
UCLASS()
class PADEL_API UPadelPlayerCard : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	/** Large or small. Set before the card is built. */
	void SetLarge(bool bInLarge) { bLarge = bInLarge; }
	bool IsLarge() const { return bLarge; }
	void SetCard(const Padel::Ui::FPlayerCardView& InCard);
	const Padel::Ui::FPlayerCardView& GetCard() const { return Card; }
	int32 GetPlayerId() const { return Card.Id; }

	/** bAnimate cross-fades the badge and the Selected frame over Motion.BadgeSwap (spec 7.3). */
	void SetRole(EPadelCardRole InRole, bool bAnimate);
	EPadelCardRole GetRole() const { return Role; }
	bool IsSelectedCard() const { return Role == EPadelCardRole::You; }

	/** The numbers as drawn, for tests: the overall, then each stat row ("--" when unknown). */
	FString GetOverallText() const;
	FString GetStatText(int32 Index) const;
	FString GetProfileText() const;

	FOnPadelCardClicked OnCardClicked;

	/** Sizes on the board (spec 6.6). */
	static FVector2D LargeSize();
	static FVector2D SmallSize();

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	/** A 1-99 meter: a pill track with a fill sized by weight against a spacer. */
	UWidget* MakeMeter(float Height, const FLinearColor& FillColour, UImage*& OutFill, USpacer*& OutRest);
	static void SetMeter(UImage* Fill, USpacer* Rest, int32 Value);
	UWidget* MakeOverallBand(float Radius);
	void BuildLarge(UCanvasPanel* Canvas);
	void BuildSmall(UCanvasPanel* Canvas);
	void Refresh();
	void ApplyRoleVisuals();

	bool bLarge = true;
	Padel::Ui::FPlayerCardView Card;
	EPadelCardRole Role = EPadelCardRole::None;
	/** The role whose badge shows now: it changes halfway through the cross-fade. */
	EPadelCardRole ShownRole = EPadelCardRole::None;
	FPadelTween SwapTween{1.0f, 1.0f};
	FPadelTween FrameTween{0.0f, 0.0f};
	bool bPointerDown = false;

	UPROPERTY()
	TObjectPtr<UPadelNameStrap> Strap;

	UPROPERTY()
	TObjectPtr<UTextBlock> TeamOutline;

	UPROPERTY()
	TObjectPtr<UPadelSilhouette> Portrait;

	UPROPERTY()
	TObjectPtr<UTextBlock> ProfileText;

	UPROPERTY()
	TObjectPtr<UImage> SelectedFrame;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> StatLabels;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> StatValues;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> StatFills;

	UPROPERTY()
	TArray<TObjectPtr<USpacer>> StatRests;

	UPROPERTY()
	TObjectPtr<UTextBlock> OverallValue;

	UPROPERTY()
	TObjectPtr<UImage> OverallFill;

	UPROPERTY()
	TObjectPtr<USpacer> OverallRest;
};
