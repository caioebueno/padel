#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "UI/Widgets/PadelBadge.h"
#include "PadelNameStrap.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UPadelBadge;
class USizeBox;
class UTextBlock;
class UVerticalBox;

/**
 * The broadcast name strap on top of a player card (spec 6.6): the full card width in the
 * team colour at 0.9, a 4 px white top edge, the first name (Type.Label) and the surname
 * (Type.Title), both in OnAccent, and an optional You / Partner badge on the right. The
 * large card's strap is 96 px with the first name over the surname; the small card's is
 * 72 px with them side by side (design board "PlayerCard"). Our own lower-third look, not
 * a trading card.
 */
UCLASS()
class PADEL_API UPadelNameStrap : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void SetPlayer(const FText& InFirstName, const FText& InSurname, int32 InTeam);
	void SetCompact(bool bInCompact);
	/** Shows a badge in the top-right corner; bShow false hides it. */
	void SetBadge(bool bShow, EPadelBadgeKind InKind = EPadelBadgeKind::You);
	/**
	 * Round the band's top corners (the small card's strap sits at the card's rounded top).
	 * The white edge is inset by the radius, as the glass panel's light catch is.
	 */
	void SetTopRadius(float InRadius);
	/** The badge's opacity, for the card's You / Partner cross-fade (spec 7.3). */
	void SetBadgeOpacity(float Opacity);

	FString GetSurnameText() const;
	bool IsBadgeShown() const { return bShowBadge; }
	EPadelBadgeKind GetBadgeKind() const { return BadgeKind; }

protected:
	virtual void BuildTree() override;

private:
	void Refresh();

	FText FirstName;
	FText Surname;
	int32 Team = 0;
	bool bCompact = false;
	float TopRadius = 0.0f;
	bool bShowBadge = false;
	EPadelBadgeKind BadgeKind = EPadelBadgeKind::You;

	UPROPERTY()
	TObjectPtr<USizeBox> SizeRoot;

	UPROPERTY()
	TObjectPtr<UBorder> Band;

	/** The large strap stacks the names; the compact one lines them up. */
	UPROPERTY()
	TObjectPtr<UVerticalBox> StackedNames;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> InlineNames;

	UPROPERTY()
	TObjectPtr<UTextBlock> FirstText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SurnameText;

	UPROPERTY()
	TObjectPtr<UPadelBadge> Badge;

	UPROPERTY()
	TObjectPtr<UImage> TopEdge;
};
