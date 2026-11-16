#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "UI/Widgets/PadelBadge.h"
#include "PadelNameStrap.generated.h"

class UBorder;
class UImage;
class UPadelBadge;
class USizeBox;
class UTextBlock;

/**
 * The broadcast name strap on top of a player card (spec 6.6): the full card width in the
 * team colour at 0.9, a 4 px white top edge, the first name (Type.Label) over the surname
 * (Type.Title), both in OnAccent, and an optional You / Partner badge top right. 96 px on
 * the large card, 72 px on the small one. Our own lower-third look, not a trading card.
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
	bool bShowBadge = false;
	EPadelBadgeKind BadgeKind = EPadelBadgeKind::You;

	UPROPERTY()
	TObjectPtr<USizeBox> SizeRoot;

	UPROPERTY()
	TObjectPtr<UBorder> Band;

	UPROPERTY()
	TObjectPtr<UTextBlock> FirstText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SurnameText;

	UPROPERTY()
	TObjectPtr<UPadelBadge> Badge;
};
