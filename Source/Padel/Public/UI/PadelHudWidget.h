#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "UI/Model/PadelUiScore.h"
#include "PadelHudWidget.generated.h"

class UBorder;
class UButton;
class UPadelScoreBugWidget;
class UTextBlock;
class UVerticalBox;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelGoldenReceiverChosen, int32 /*PlayerId*/);

/** The HUD's timed messages. Each shows for a few seconds, then hides itself. */
UENUM()
enum class EPadelHudMessage : uint8
{
	/** Top centre: faults, lets, how the point ended, who serves. */
	Callout,
	/** Near the human player: the GDD 5.1 timing grade after each of their shots. */
	TimingCue,
	/** Bottom left: the partner command just given (GDD 8). */
	PartnerCommand,
	Count UMETA(Hidden),
};

/**
 * The in-match HUD (GDD 1 pillar 4, GDD 9, GDD 10): score bug, timing cue, partner
 * command prompt, point callouts, the big change-over card and the golden point prompt.
 * It only displays; APadelHUD decides what to show from the match events.
 */
UCLASS()
class PADEL_API UPadelHudWidget : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void UpdateScore(const Padel::Ui::FScoreView& View);
	void ShowMessage(EPadelHudMessage Which, const FText& Text, const FLinearColor& Colour, float Seconds);
	/** Big score card between games and on change of ends (GDD 9 "between points" cut-away). */
	void ShowScoreCard(const FText& Title, const Padel::Ui::FScoreView& View, float Seconds);

	/** GDD 3.1: the human pair picks who receives the golden point. Gives focus to the choice. */
	void OpenGoldenPrompt(int32 DefaultPlayerId, const FText& ConfirmHint);
	void CloseGoldenPrompt();
	bool IsGoldenPromptOpen() const;

	/** Moves the timing cue next to the human player once their screen position is known. */
	void SetTimingCueScreenPosition(const FVector2D& ScreenPosition);

	/** Steps the message timers. NativeTick calls it; tests can call it directly. */
	void TickMessages(float DeltaSeconds);

	FString GetMessageText(EPadelHudMessage Which) const;
	FString GetScoreCardTitle() const;
	UPadelScoreBugWidget* GetScoreBug() const { return ScoreBug; }

	FOnPadelGoldenReceiverChosen OnGoldenReceiverChosen;

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleGoldenFirst();

	UFUNCTION()
	void HandleGoldenSecond();

	void ChooseGolden(int32 Index);

	UPROPERTY()
	TObjectPtr<UPadelScoreBugWidget> ScoreBug;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> MessagePanels;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> MessageTexts;

	TArray<float> MessageTimeLeft;

	UPROPERTY()
	TObjectPtr<UBorder> ScoreCard;

	UPROPERTY()
	TObjectPtr<UTextBlock> ScoreCardTitle;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ScoreCardRows;

	float ScoreCardTimeLeft = 0.0f;

	UPROPERTY()
	TObjectPtr<UBorder> GoldenPrompt;

	UPROPERTY()
	TObjectPtr<UTextBlock> GoldenHint;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> GoldenButtons;
};
