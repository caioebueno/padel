#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/PadelWidgetBase.h"
#include "PadelPracticeControlList.generated.h"

class UPadelButtonGlyph;

/**
 * The ball machine's four controls with their glyphs (spec 7.9), shared by the Practice
 * intro and the Practice pause panel: Change drill, Pause the machine, Feed a ball now,
 * Pause. Rows come from Padel::Ui::PracticeControlRows(); glyphs follow the device in use.
 */
UCLASS()
class PADEL_API UPadelPracticeControlList : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void SetFamily(Padel::Ui::EPadFamily InFamily);
	int32 GetRowCount() const { return RowCount; }

protected:
	virtual void BuildTree() override;

private:
	Padel::Ui::EPadFamily Family = Padel::Ui::EPadFamily::Xbox;
	int32 RowCount = 0;

	UPROPERTY()
	TArray<TObjectPtr<UPadelButtonGlyph>> Glyphs;
};
