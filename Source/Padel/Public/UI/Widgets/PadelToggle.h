#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiControls.h"
#include "UI/Widgets/PadelRowControl.h"
#include "PadelToggle.generated.h"

class UBorder;
class UHorizontalBox;
class USizeBox;
class UTextBlock;

/**
 * Spec 6.7: a settings row with a 72 x 36 switch on the right. The word ON or OFF is
 * always drawn beside it, so the state doesn't rest on colour. A toggles; Left means Off and Right
 * means On, with the edge sound if it's already there.
 */
UCLASS()
class PADEL_API UPadelToggle : public UPadelRowControl
{
	GENERATED_BODY()

public:
	void SetOn(bool bInOn);
	bool IsOn() const { return Model.bOn; }

	virtual bool HandleInput(Padel::Ui::EUiInput Input, bool bRepeat) override;
	virtual void Activate() override;

protected:
	virtual void BuildValue(UHorizontalBox* Row) override;
	virtual void RefreshValue() override;

private:
	Padel::Ui::FToggleModel Model;

	UPROPERTY()
	TObjectPtr<UBorder> Track;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> TrackRow;

	UPROPERTY()
	TObjectPtr<UBorder> Knob;

	UPROPERTY()
	TObjectPtr<USizeBox> KnobBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> Word;
};
