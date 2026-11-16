#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "PadelBadge.generated.h"

class UBorder;
class UPadelIcon;
class USizeBox;
class UTextBlock;

/** Spec 6.4's badge variants. */
UENUM()
enum class EPadelBadgeKind : uint8
{
	ComingSoon, // Surface.Charcoal, Lock text, a 16 px padlock
	New,        // Accent fill, OnAccent text
	You,        // 2 px Accent stroke, Accent text: the player you control
	Partner,    // Glass.Edge stroke, Text.Secondary text
};

/** A pill badge, 32 px tall, 12 px side padding, Type.Label. Not focusable. */
UCLASS()
class PADEL_API UPadelBadge : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void SetKind(EPadelBadgeKind InKind);
	EPadelBadgeKind GetKind() const { return Kind; }
	/** Overrides the variant's default copy (Badge.ComingSoon, Badge.You...). */
	void SetLabel(const FText& InLabel);
	FText GetLabel() const;

protected:
	virtual void BuildTree() override;

private:
	void Refresh();

	EPadelBadgeKind Kind = EPadelBadgeKind::ComingSoon;
	FText CustomLabel;

	UPROPERTY()
	TObjectPtr<UBorder> Pill;

	UPROPERTY()
	TObjectPtr<UTextBlock> Text;

	UPROPERTY()
	TObjectPtr<UPadelIcon> Padlock;

	UPROPERTY()
	TObjectPtr<USizeBox> PadlockBox;
};
