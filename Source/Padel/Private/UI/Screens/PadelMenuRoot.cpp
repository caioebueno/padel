#include "UI/Screens/PadelMenuRoot.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "PadelLog.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiStyle.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelPlaceholderScreen.h"
#include "UI/Screens/PadelScreenRegistry.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "UI/Widgets/PadelDialog.h"
#include "UI/Widgets/PadelToast.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiNavKind;
using Padel::Ui::EUiTransition;

namespace
{
	constexpr float RootStageWidth = 1920.0f;
	constexpr float RootStageMinHeight = 1080.0f;
	// The toast sits 16 px above the hint bar band (y 960, spec 6.13).
	constexpr float RootToastBottom = 1080.0f - 960.0f + 16.0f;

	std::string RootUtf8(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}
}

void UPadelMenuRoot::SetHost(UObject* InHost)
{
	HostObject = InHost;
	if (InHost && !Cast<IPadelMenuHost>(InHost))
	{
		UE_LOG(LogPadel, Error, TEXT("%s doesn't implement IPadelMenuHost; menu actions will do nothing."), *InHost->GetName());
	}
}

IPadelMenuHost* UPadelMenuRoot::GetHost() const
{
	return Cast<IPadelMenuHost>(HostObject.Get());
}

void UPadelMenuRoot::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	const Padel::Ui::FUiInputTiming& Input = PadelUi::Theme().Input;
	Repeat.Configure(Input.RepeatDelay, Input.RepeatRate);
	Stick.SetDeadZone(Input.StickDeadZone);
	OptionsHandle = PadelUi::OnOptionsChanged().AddUObject(this, &UPadelMenuRoot::RebuildForOptions);
}

void UPadelMenuRoot::NativeDestruct()
{
	PadelUi::OnOptionsChanged().Remove(OptionsHandle);
	Super::NativeDestruct();
}

void UPadelMenuRoot::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;
	auto AddFull = [Root](UWidget* Widget)
	{
		UOverlaySlot* FullSlot = Root->AddChildToOverlay(Widget);
		FullSlot->SetHorizontalAlignment(HAlign_Fill);
		FullSlot->SetVerticalAlignment(VAlign_Fill);
	};

	DeepFill = MakeImage(PadelUiPaint::Solid(PadelUi::Colour(EUiColour::BgDeep)));
	AddFull(DeepFill);

	Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass());
	Blur->SetBlurStrength(PadelUi::Theme().Layout.BlurStrength);
	AddFull(Blur);

	Scrim = MakeImage(PadelUiPaint::Solid(PadelUi::Colour(EUiColour::BgScrim)));
	AddFull(Scrim);

	Vignette = MakeImage(PadelUiPaint::Vignette(PadelUi::Colour(EUiColour::BgVignette)));
	AddFull(Vignette);

	// The stage: spec coordinates are 1920 x 1080; ScaleToFit keeps them on any viewport.
	UOverlay* Stage = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	ScreenLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UOverlaySlot* ScreenSlot = Stage->AddChildToOverlay(ScreenLayer);
	ScreenSlot->SetHorizontalAlignment(HAlign_Fill);
	ScreenSlot->SetVerticalAlignment(VAlign_Fill);

	Toast = WidgetTree->ConstructWidget<UPadelToast>(UPadelToast::StaticClass());
	UOverlaySlot* ToastSlot = Stage->AddChildToOverlay(Toast);
	ToastSlot->SetHorizontalAlignment(HAlign_Center);
	ToastSlot->SetVerticalAlignment(VAlign_Bottom);
	ToastSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RootToastBottom));

	Dialog = WidgetTree->ConstructWidget<UPadelDialog>(UPadelDialog::StaticClass());
	UOverlaySlot* DialogSlot = Stage->AddChildToOverlay(Dialog);
	DialogSlot->SetHorizontalAlignment(HAlign_Fill);
	DialogSlot->SetVerticalAlignment(VAlign_Fill);

	StageBox = MakeSized(Stage, RootStageWidth, StageHeight);
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetContent(StageBox);
	AddFull(Scale);

	// A screen shown before the tree existed.
	if (ScreenWidget)
	{
		UOverlaySlot* EarlySlot = ScreenLayer->AddChildToOverlay(ScreenWidget);
		EarlySlot->SetHorizontalAlignment(HAlign_Fill);
		EarlySlot->SetVerticalAlignment(VAlign_Fill);
	}
	ApplyScreenLayers();
}

bool UPadelMenuRoot::IsShowingMenu() const
{
	return Padel::Ui::IsMenuScreen(Flow.Current());
}

void UPadelMenuRoot::ShowScreen(EPadelScreen Screen, EUiNavKind Kind)
{
	const FString Leaving = ScreenWidget ? ScreenWidget->GetFocusedId().ToString() : FString();
	const EUiTransition Transition = Flow.GoTo(ToUiScreen(Screen), Kind, ScreenWidget ? RootUtf8(Leaving) : std::string());
	SwapScreen(Screen, Transition);
}

void UPadelMenuRoot::ReturnToOpener()
{
	const FString Leaving = ScreenWidget ? ScreenWidget->GetFocusedId().ToString() : FString();
	const Padel::Ui::EUiScreen Target = Flow.ReturnToOpener(RootUtf8(Leaving));
	PlayBackCue();
	SwapScreen(ToPadelScreen(Target), EUiTransition::SlideFromLeft);
}

bool UPadelMenuRoot::DefaultBack(FName FocusedId)
{
	switch (Padel::Ui::BackAction(Flow.Current()))
	{
	case Padel::Ui::EUiBackAction::None:
		return false;
	case Padel::Ui::EUiBackAction::QuitGameDialog:
		PlayBackCue();
		ShowQuitGameConfirm();
		return true;
	case Padel::Ui::EUiBackAction::MainMenu:
		PlayBackCue();
		ShowScreen(EPadelScreen::MainMenu, EUiNavKind::Back);
		return true;
	case Padel::Ui::EUiBackAction::Resume:
		PlayBackCue();
		if (IPadelMenuHost* Host = GetHost())
		{
			Host->MenuResumeMatch();
		}
		return true;
	case Padel::Ui::EUiBackAction::Opener:
		ReturnToOpener();
		return true;
	}
	return false;
}

void UPadelMenuRoot::SwapScreen(EPadelScreen Screen, EUiTransition Transition)
{
	if (Dialog)
	{
		Dialog->Dismiss();
	}
	if (ScreenWidget)
	{
		ScreenWidget->RemoveFromParent();
		ScreenWidget = nullptr;
	}
	Repeat.Reset();

	const TSubclassOf<UPadelScreenWidget> ScreenClass = PadelScreenRegistry::ScreenClass(Screen);
	if (Padel::Ui::IsMenuScreen(ToUiScreen(Screen)) && ScreenClass)
	{
		ScreenWidget = WidgetTree->ConstructWidget<UPadelScreenWidget>(ScreenClass);
		ScreenWidget->InitScreen(this, Screen);
		ScreenWidget->SetPadFamily(PadFamily);
		if (ScreenLayer)
		{
			UOverlaySlot* ScreenSlot = ScreenLayer->AddChildToOverlay(ScreenWidget);
			ScreenSlot->SetHorizontalAlignment(HAlign_Fill);
			ScreenSlot->SetVerticalAlignment(VAlign_Fill);
		}
		ScreenWidget->EnterScreen(UTF8_TO_TCHAR(Flow.FocusOnArrival().c_str()), Transition);
	}
	ApplyScreenLayers();
	OnScreenChanged.Broadcast(Screen);
}

void UPadelMenuRoot::ApplyScreenLayers()
{
	const bool bMenu = IsShowingMenu() && ScreenWidget;
	SetVisibility(bMenu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!Scrim || !Blur || !DeepFill || !Vignette)
	{
		return;
	}
	const EUiColour ScrimColour = ScreenWidget ? ScreenWidget->GetScrimColour() : EUiColour::BgScrim;
	Scrim->SetBrush(PadelUiPaint::Solid(PadelUi::Colour(ScrimColour)));
	// Reduce transparency: no blur; the glass itself turns solid (spec 7.7).
	const bool bBlur = ScreenWidget && ScreenWidget->WantsBackgroundBlur() && !PadelUi::Options().bReduceTransparency;
	Blur->SetVisibility(bBlur ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DeepFill->SetVisibility(bDeepBackground ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Scrim->SetVisibility(ESlateVisibility::HitTestInvisible);
	Vignette->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPadelMenuRoot::SetDeepBackground(bool bDeep)
{
	bDeepBackground = bDeep;
	ApplyScreenLayers();
}

void UPadelMenuRoot::RebuildForOptions()
{
	// Text size, Reduce motion and Reduce transparency change how every widget is built:
	// rebuild the current screen in place, keeping its focus (spec 7.7: they apply at once).
	if (!ScreenWidget)
	{
		ApplyScreenLayers();
		return;
	}
	const EPadelScreen Screen = ScreenWidget->GetScreenId();
	const FString Focus = ScreenWidget->GetFocusedId().ToString();
	ScreenWidget->RemoveFromParent();
	ScreenWidget = WidgetTree->ConstructWidget<UPadelScreenWidget>(PadelScreenRegistry::ScreenClass(Screen));
	ScreenWidget->InitScreen(this, Screen);
	ScreenWidget->SetPadFamily(PadFamily);
	if (ScreenLayer)
	{
		UOverlaySlot* ScreenSlot = ScreenLayer->AddChildToOverlay(ScreenWidget);
		ScreenSlot->SetHorizontalAlignment(HAlign_Fill);
		ScreenSlot->SetVerticalAlignment(VAlign_Fill);
	}
	ScreenWidget->EnterScreen(Focus, EUiTransition::None);
	ScreenWidget->SkipEntryAnimation();
	ApplyScreenLayers();
}

void UPadelMenuRoot::ShowConfirm(const FPadelConfirm& Confirm)
{
	if (Dialog)
	{
		Repeat.Reset();
		Dialog->Open(Confirm);
	}
}

void UPadelMenuRoot::ShowQuitGameConfirm()
{
	FPadelConfirm Confirm;
	// "Padel Street" is the working title (GDD 13 decision 2).
	Confirm.Title = LOCTEXT("Dlg.QuitGame.Title", "Quit Padel Street?");
	Confirm.ActionLabel = LOCTEXT("Dlg.QuitGame.Action", "Quit game");
	Confirm.SafeLabel = LOCTEXT("Dlg.Cancel", "Cancel");
	Confirm.bDanger = true;
	Confirm.OnAction = [this]()
	{
		if (IPadelMenuHost* Host = GetHost())
		{
			Host->MenuQuitGame();
		}
	};
	ShowConfirm(Confirm);
}

bool UPadelMenuRoot::IsDialogOpen() const
{
	return Dialog && Dialog->IsOpen();
}

void UPadelMenuRoot::ShowToast(const FText& Message)
{
	if (Toast)
	{
		Toast->Show(Message);
	}
}

void UPadelMenuRoot::SetPadFamily(Padel::Ui::EPadFamily InFamily)
{
	if (PadFamily == InFamily)
	{
		return;
	}
	PadFamily = InFamily;
	if (ScreenWidget)
	{
		ScreenWidget->SetPadFamily(PadFamily);
	}
}

void UPadelMenuRoot::HandleInput(EUiInput Input, bool bRepeat)
{
	if (Input == EUiInput::None)
	{
		return;
	}
	if (IsDialogOpen())
	{
		if (!bRepeat || Padel::Ui::IsDirection(Input))
		{
			Dialog->HandleInput(Input);
		}
		return;
	}
	UPadelScreenWidget* Screen = ScreenWidget;
	if (!Screen)
	{
		return;
	}
	if (Screen->HandleMenuInput(Input, bRepeat))
	{
		return;
	}
	// Start on Pause resumes (spec 7.5); every other screen ignores it (G13).
	if (Input == EUiInput::Pause && !bRepeat
		&& (Flow.Current() == Padel::Ui::EUiScreen::Paused || Flow.Current() == Padel::Ui::EUiScreen::PracticePaused))
	{
		if (IPadelMenuHost* Host = GetHost())
		{
			Host->MenuResumeMatch();
		}
	}
}

void UPadelMenuRoot::PressDirection(EUiInput Input)
{
	// Spec 5.3: left/right held on a slider repeats at its own rate; everything else at 110 ms.
	const Padel::Ui::FUiInputTiming& Timing = PadelUi::Theme().Input;
	float Rate = Timing.RepeatRate;
	const UPadelFocusableWidget* Focused = ScreenWidget ? ScreenWidget->GetFocusedElement() : nullptr;
	if (Focused && (Input == EUiInput::Left || Input == EUiInput::Right) && Focused->GetHeldRepeatRate() > 0.0f)
	{
		Rate = Focused->GetHeldRepeatRate();
	}
	Repeat.Configure(Timing.RepeatDelay, Rate);
	Repeat.Press(Input);
	HandleInput(Input, false);
}

void UPadelMenuRoot::ReleaseDirection(EUiInput Input)
{
	Repeat.Release(Input);
}

void UPadelMenuRoot::UpdateFamilyFromKey(const FKey& Key, int32 UserIndex)
{
	if (!Key.IsGamepadKey())
	{
		SetPadFamily(Padel::Ui::EPadFamily::KeyboardMouse);
		return;
	}
	// Spec 5.5: glyphs follow the last device used, by its hardware name.
	Padel::Ui::EPadFamily Family = Padel::Ui::EPadFamily::Xbox;
	if (const UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get())
	{
		const APlayerController* PC = GetOwningPlayer();
		const FPlatformUserId User = PC ? PC->GetPlatformUserId() : FPlatformUserId::CreateFromInternalId(UserIndex);
		const FHardwareDeviceIdentifier Device = Devices->GetMostRecentlyUsedHardwareDevice(User);
		Family = Padel::Ui::FamilyFromDeviceName(PadelUiStyle::ToUtf8(Device.HardwareDeviceIdentifier.ToString()));
		if (Family == Padel::Ui::EPadFamily::KeyboardMouse)
		{
			// A pad whose name we don't know: Xbox labels, by position (GDD 7).
			Family = Padel::Ui::EPadFamily::Xbox;
		}
	}
	SetPadFamily(Family);
}

FReply UPadelMenuRoot::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const EUiInput Input = Padel::Ui::InputForKey(RootUtf8(InKeyEvent.GetKey().GetFName().ToString()));
	UpdateFamilyFromKey(InKeyEvent.GetKey(), InKeyEvent.GetUserIndex());
	if (Input == EUiInput::None)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	// The OS's own key repeat is ignored: held directions repeat on the spec's timing.
	if (!InKeyEvent.IsRepeat())
	{
		if (Padel::Ui::IsDirection(Input))
		{
			PressDirection(Input);
		}
		else
		{
			HandleInput(Input, false);
		}
	}
	return FReply::Handled();
}

FReply UPadelMenuRoot::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const EUiInput Input = Padel::Ui::InputForKey(RootUtf8(InKeyEvent.GetKey().GetFName().ToString()));
	if (Padel::Ui::IsDirection(Input))
	{
		ReleaseDirection(Input);
		return FReply::Handled();
	}
	return Input == EUiInput::None ? Super::NativeOnKeyUp(InGeometry, InKeyEvent) : FReply::Handled();
}

FReply UPadelMenuRoot::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	const FKey Key = InAnalogEvent.GetKey();
	if (Key == EKeys::Gamepad_LeftX)
	{
		StickX = InAnalogEvent.GetAnalogValue();
	}
	else if (Key == EKeys::Gamepad_LeftY)
	{
		StickY = InAnalogEvent.GetAnalogValue();
	}
	else
	{
		return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
	}
	const EUiInput Pressed = Stick.Update(StickX, StickY);
	if (Stick.Released() != EUiInput::None)
	{
		ReleaseDirection(Stick.Released());
	}
	if (Pressed != EUiInput::None)
	{
		UpdateFamilyFromKey(Key, InAnalogEvent.GetUserIndex());
		PressDirection(Pressed);
	}
	return FReply::Handled();
}

FReply UPadelMenuRoot::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!InMouseEvent.GetCursorDelta().IsNearlyZero())
	{
		SetPadFamily(Padel::Ui::EPadFamily::KeyboardMouse);
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UPadelMenuRoot::UpdateStageSize(const FGeometry& Geometry)
{
	// Spec 4.3: design at 1920 x 1080; narrower than 16:9 grows the stage taller, so the
	// header and hint bar stay at the edges.
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0.0f || Size.Y <= 0.0f || !StageBox)
	{
		return;
	}
	const float Wanted = FMath::Max(RootStageMinHeight, RootStageWidth * Size.Y / Size.X);
	if (!FMath::IsNearlyEqual(Wanted, StageHeight, 0.5f))
	{
		StageHeight = Wanted;
		StageBox->SetHeightOverride(StageHeight);
	}
}

void UPadelMenuRoot::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateStageSize(MyGeometry);

	const int32 Fires = Repeat.Tick(InDeltaTime);
	for (int32 Index = 0; Index < Fires; ++Index)
	{
		HandleInput(Repeat.Held(), true);
	}

	// Keep keyboard focus on the root while a menu shows, so a click on empty space or a
	// focus steal can't leave the pad dead (spec 5.6). Only with a local player: headless
	// tests have none.
	if (IsShowingMenu() && GetOwningLocalPlayer() && !HasAnyUserFocus() && FSlateApplication::IsInitialized())
	{
		SetFocus();
	}
}

#undef LOCTEXT_NAMESPACE
