#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "PadelSilhouette.generated.h"

/**
 * The flat portrait fallback (spec 6.6, 12): a waist-up figure in the team colour at 0.25
 * alpha, until the asset pipeline renders portraits (W12). Drawn as three rounded shapes
 * (head, neck, shoulders) on the design's 200 x 280 grid, scaled to the allotted size, so
 * it needs no texture. Used by the player card and the Versus card.
 */
UCLASS()
class PADEL_API UPadelSilhouette : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Team 0 (Lagoon) or 1 (Ember): the figure takes that team's colour. */
	void SetTeam(int32 InTeam) { Team = InTeam; }
	int32 GetTeam() const { return Team; }

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	int32 Team = 0;
	/** The parts' brushes, kept alive for the frame's draw elements; resized as painted. */
	mutable TArray<FSlateBrush> PartBrushes;
};
