#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/Model/PadelUiScreenFlow.h"
#include "UI/PadelScreenTypes.h"
#include "UI/PadelWidgetBase.h"
#include "PadelMenuRoot.generated.h"

class IPadelMenuHost;
class UBackgroundBlur;
class UImage;
class UOverlay;
class UPadelDialog;
class UPadelScreenWidget;
class UPadelToast;
class USizeBox;

/** Menu sounds (W10 binds them; spec 9's cue list). The root only says when. */
UENUM()
enum class EPadelMenuCue : uint8
{
	Focus,
	Confirm,
	Back,
	Value,
	Edge,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelMenuCue, EPadelMenuCue /*Cue*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelMenuScreenChanged, EPadelScreen /*NewScreen*/);

/**
 * The one widget the menus live in (spec 3, 11.1): the router between screens, and the
 * place input arrives.
 *
 * Layers, back to front: Bg.Deep (only without a court behind), the background blur and
 * scrim of the current screen, the vignette, then a 1920-wide stage scaled to fit the
 * viewport (1080 tall, or taller on narrower screens) holding the screen, the toast and
 * the dialog.
 *
 * Input: the root keeps Slate keyboard focus while a menu shows and translates keys, pad
 * buttons and the left stick (with the spec's 0.5 dead zone) into EUiInput, repeating held
 * directions itself (400 ms, then every 110 ms). A dialog takes all input while open.
 *
 * Screens move with ShowScreen and the flow model (FScreenFlow): focus memory, Controls
 * and Settings returning to their opener, and the transition to play. The screen class for
 * each EPadelScreen comes from PadelScreenRegistry; a screen not built yet shows a
 * placeholder so the flow stays walkable.
 */
UCLASS()
class PADEL_API UPadelMenuRoot : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	/** The game side (APadelHUD); must implement IPadelMenuHost. */
	void SetHost(UObject* InHost);
	IPadelMenuHost* GetHost() const;

	/** Go to a screen. Match (and any non-menu screen) hides the menus. */
	void ShowScreen(EPadelScreen Screen, Padel::Ui::EUiNavKind Kind = Padel::Ui::EUiNavKind::Forward);
	/** B on Controls or Settings: back to whichever screen opened them. */
	void ReturnToOpener();
	/** Spec 3's Back table for the current screen. False if B does nothing here. */
	bool DefaultBack(FName FocusedId);

	EPadelScreen GetCurrentScreen() const { return ToPadelScreen(Flow.Current()); }
	UPadelScreenWidget* GetScreenWidget() const { return ScreenWidget; }
	const Padel::Ui::FScreenFlow& GetFlow() const { return Flow; }
	bool IsShowingMenu() const;

	/** Spec 6.12, 7.10. Replaces any open dialog. */
	void ShowConfirm(const FPadelConfirm& Confirm);
	/** Spec 7.10's Dlg.QuitGame (main menu B). */
	void ShowQuitGameConfirm();
	UPadelDialog* GetDialog() const { return Dialog; }
	bool IsDialogOpen() const;

	/** Spec 6.13. */
	void ShowToast(const FText& Message);
	UPadelToast* GetToast() const { return Toast; }

	/** One menu input, as if pressed (tests, hint clicks). */
	void HandleInput(Padel::Ui::EUiInput Input, bool bRepeat = false);

	Padel::Ui::EPadFamily GetPadFamily() const { return PadFamily; }
	void SetPadFamily(Padel::Ui::EPadFamily InFamily);

	/** Bg.Deep behind everything when no court level is loaded (spec 6.15). */
	void SetDeepBackground(bool bDeep);

	void PlayFocusCue() { OnCue.Broadcast(EPadelMenuCue::Focus); }
	void PlayConfirmCue() { OnCue.Broadcast(EPadelMenuCue::Confirm); }
	void PlayBackCue() { OnCue.Broadcast(EPadelMenuCue::Back); }
	void PlayValueCue() { OnCue.Broadcast(EPadelMenuCue::Value); }
	void PlayEdgeCue() { OnCue.Broadcast(EPadelMenuCue::Edge); }

	FOnPadelMenuCue OnCue;
	FOnPadelMenuScreenChanged OnScreenChanged;

protected:
	virtual void BuildTree() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void SwapScreen(EPadelScreen Screen, Padel::Ui::EUiTransition Transition);
	void ApplyScreenLayers();
	void RebuildForOptions();
	void UpdateFamilyFromKey(const FKey& Key, int32 UserIndex);
	void UpdateStageSize(const FGeometry& Geometry);
	void PressDirection(Padel::Ui::EUiInput Input);
	void ReleaseDirection(Padel::Ui::EUiInput Input);

	Padel::Ui::FScreenFlow Flow;
	Padel::Ui::FDirectionRepeat Repeat;
	Padel::Ui::FStickDirection Stick;
	float StickX = 0.0f;
	float StickY = 0.0f;
	Padel::Ui::EPadFamily PadFamily = Padel::Ui::EPadFamily::KeyboardMouse;
	bool bDeepBackground = false;
	float StageHeight = 1080.0f;
	FDelegateHandle OptionsHandle;

	UPROPERTY()
	TObjectPtr<UObject> HostObject;

	UPROPERTY()
	TObjectPtr<UImage> DeepFill;

	UPROPERTY()
	TObjectPtr<UBackgroundBlur> Blur;

	UPROPERTY()
	TObjectPtr<UImage> Scrim;

	UPROPERTY()
	TObjectPtr<UImage> Vignette;

	UPROPERTY()
	TObjectPtr<USizeBox> StageBox;

	UPROPERTY()
	TObjectPtr<UOverlay> ScreenLayer;

	UPROPERTY()
	TObjectPtr<UPadelScreenWidget> ScreenWidget;

	UPROPERTY()
	TObjectPtr<UPadelToast> Toast;

	UPROPERTY()
	TObjectPtr<UPadelDialog> Dialog;
};
