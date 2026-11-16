#include "UI/Screens/PadelScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "PadelLog.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Widgets/PadelHintBar.h"
#include "UI/Widgets/PadelScreenHeader.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiInput;
using Padel::Ui::EUiTransition;

namespace
{
	// Spec 4.3's 1080p reference frame.
	constexpr float ScreenRefHeight = 1080.0f;
	constexpr float ScreenHeaderTop = 64.0f;
	constexpr float ScreenHintBarTop = 960.0f;

	std::string ScreenIdString(FName Id)
	{
		return std::string(TCHAR_TO_UTF8(*Id.ToString()));
	}

	FName ScreenIdName(const std::string& Id)
	{
		return FName(UTF8_TO_TCHAR(Id.c_str()));
	}
}

void UPadelScreenWidget::InitScreen(UPadelMenuRoot* InRoot, EPadelScreen InScreen)
{
	MenuRoot = InRoot;
	ScreenId = InScreen;
}

IPadelMenuHost* UPadelScreenWidget::GetHost() const
{
	return MenuRoot ? MenuRoot->GetHost() : nullptr;
}

void UPadelScreenWidget::BuildTree()
{
	Stage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Stage;
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	const float SafeWidth = 1920.0f - 2.0f * Theme.Layout.SafeX;

	Header = WidgetTree->ConstructWidget<UPadelScreenHeader>(UPadelScreenHeader::StaticClass());
	Place(Header, Theme.Layout.SafeX, ScreenHeaderTop, SafeWidth, Theme.Layout.HeaderHeight);

	HintBar = WidgetTree->ConstructWidget<UPadelHintBar>(UPadelHintBar::StaticClass());
	HintBar->SetFamily(PadFamily);
	HintBar->OnHintClicked.AddWeakLambda(this, [this](EUiInput Input)
	{
		HandleMenuInput(Input, false);
	});
	Place(HintBar, Theme.Layout.SafeX, ScreenHintBarTop, SafeWidth, Theme.Layout.HintBarHeight, true);

	BuildScreen();

	for (const std::string& Problem : FocusMap.Validate())
	{
		UE_LOG(LogPadel, Warning, TEXT("%s focus map: %s"), UTF8_TO_TCHAR(Padel::Ui::ScreenName(ToUiScreen(ScreenId))),
			UTF8_TO_TCHAR(Problem.c_str()));
	}
	RefreshHints();
}

UWidget* UPadelScreenWidget::Place(UWidget* Widget, float X, float Y, float Width, float Height, bool bFromBottom)
{
	if (!Widget || !Stage)
	{
		return Widget;
	}
	UWidget* Placed = Widget;
	const bool bAutoSize = Width <= 0.0f || Height <= 0.0f;
	if (bAutoSize && (Width > 0.0f || Height > 0.0f))
	{
		Placed = MakeSized(Widget, Width, Height);
	}
	UCanvasPanelSlot* CanvasSlot = Stage->AddChildToCanvas(Placed);
	CanvasSlot->SetAnchors(FAnchors(0.0f, bFromBottom ? 1.0f : 0.0f));
	CanvasSlot->SetAlignment(FVector2D::ZeroVector);
	CanvasSlot->SetPosition(FVector2D(X, bFromBottom ? Y - ScreenRefHeight : Y));
	CanvasSlot->SetAutoSize(bAutoSize);
	if (!bAutoSize)
	{
		CanvasSlot->SetSize(FVector2D(Width, Height));
	}
	return Placed;
}

UWidget* UPadelScreenWidget::PlaceInColumns(UWidget* Widget, int32 Column, int32 Span, float Y, float Height, bool bFromBottom)
{
	return Place(Widget, PadelUi::ColumnX(Column), Y, PadelUi::SpanWidth(Span), Height, bFromBottom);
}

void UPadelScreenWidget::RegisterElement(UPadelFocusableWidget* Element, FName Id)
{
	if (!Element)
	{
		return;
	}
	Element->SetElementId(Id);
	Elements.Add(Id, Element);
	Element->OnActivated.AddUObject(this, &UPadelScreenWidget::HandleElementActivated);
	Element->OnHovered.AddUObject(this, &UPadelScreenWidget::HandleElementHovered);
	Element->OnValueChanged.AddUObject(this, &UPadelScreenWidget::HandleElementValueChanged);
	Element->OnEdgeReached.AddWeakLambda(this, [this](UPadelFocusableWidget*)
	{
		if (MenuRoot)
		{
			MenuRoot->PlayEdgeCue();
		}
	});
}

void UPadelScreenWidget::SetElementEnabled(FName Id, bool bEnabled)
{
	FocusMap.SetEnabled(ScreenIdString(Id), bEnabled);
	if (UPadelFocusableWidget* Element = FindElement(Id))
	{
		Element->SetDisabled(!bEnabled);
	}
	if (!bEnabled && FocusedId == Id)
	{
		FocusElement(ScreenIdName(FocusMap.FirstFocus()));
	}
}

UPadelFocusableWidget* UPadelScreenWidget::FindElement(FName Id) const
{
	const TObjectPtr<UPadelFocusableWidget>* Found = Elements.Find(Id);
	return Found ? Found->Get() : nullptr;
}

UPadelFocusableWidget* UPadelScreenWidget::GetFocusedElement() const
{
	return FindElement(FocusedId);
}

bool UPadelScreenWidget::FocusElement(FName Id)
{
	UPadelFocusableWidget* Target = FindElement(Id);
	if (!Target || !FocusMap.Contains(ScreenIdString(Id)) || !FocusMap.IsEnabled(ScreenIdString(Id)))
	{
		return false;
	}
	if (FocusedId == Id)
	{
		return true;
	}
	if (UPadelFocusableWidget* Previous = GetFocusedElement())
	{
		Previous->SetElementFocused(false);
	}
	FocusedId = Id;
	Target->SetElementFocused(true);
	if (MenuRoot && !bQuietFocus)
	{
		MenuRoot->PlayFocusCue();
	}
	OnFocusChanged(Target);
	RefreshHints();
	return true;
}

void UPadelScreenWidget::EnterScreen(const FString& RestoreFocus, EUiTransition Transition)
{
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	InputLock.Begin(Theme.Input.EntryLock, FMath::Max(Theme.Input.EntryLock, GetConfirmLockSeconds()));

	EnsureBuilt();
	// Focus memory (spec 5.4): the remembered element if it still exists and is enabled.
	// No focus sound for the arrival itself.
	bQuietFocus = true;
	const FName Wanted = RestoreFocus.IsEmpty() ? NAME_None : FName(*RestoreFocus);
	if (Wanted.IsNone() || !FocusElement(Wanted))
	{
		FocusElement(ScreenIdName(FocusMap.FirstFocus()));
	}
	bQuietFocus = false;

	EntryTransition = Transition;
	const bool bReduceMotion = PadelUi::Options().bReduceMotion;
	const float Seconds = Transition == EUiTransition::None ? 0.0f : (bReduceMotion ? Theme.Motion.ReduceMotionFade : Theme.Motion.ScreenIn);
	EntryTween.Snap(0.0f);
	EntryTween.Start(1.0f, Seconds);
	ApplyEntryAnimation();
}

void UPadelScreenWidget::SkipEntryAnimation()
{
	InputLock.Begin(0.0f, 0.0f);
	EntryTween.Snap(1.0f);
	ApplyEntryAnimation();
}

void UPadelScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	InputLock.Tick(InDeltaTime);
	if (!EntryTween.IsDone())
	{
		EntryTween.Tick(InDeltaTime);
		ApplyEntryAnimation();
	}
}

void UPadelScreenWidget::ApplyEntryAnimation()
{
	// Spec 9: forward slides in 48 px from the right, back from the left; Pause, the match
	// and Reduce motion fade only.
	const float T = EntryTween.Value();
	float Slide = 0.0f;
	if (!PadelUi::Options().bReduceMotion && UsesStageSlide())
	{
		const float Distance = PadelUi::Theme().Motion.ScreenSlideIn;
		if (EntryTransition == EUiTransition::SlideFromRight)
		{
			Slide = Distance * (1.0f - T);
		}
		else if (EntryTransition == EUiTransition::SlideFromLeft)
		{
			Slide = -Distance * (1.0f - T);
		}
	}
	if (Stage)
	{
		Stage->SetRenderTranslation(FVector2D(Slide, 0.0f));
	}
	SetRenderOpacity(T);
}

bool UPadelScreenWidget::HandleMenuInput(EUiInput Input, bool bRepeat)
{
	if (Input == EUiInput::None || !InputLock.AcceptsInput())
	{
		return false;
	}
	UPadelFocusableWidget* Focused = GetFocusedElement();

	// Value controls use left/right themselves (spec 6.7-6.10).
	if (Focused && Padel::Ui::IsDirection(Input) && Focused->HandleInput(Input, bRepeat))
	{
		return true;
	}

	Padel::Ui::EFocusDir Dir;
	if (Padel::Ui::FocusDirFor(Input, Dir))
	{
		if (FocusedId.IsNone())
		{
			return FocusElement(ScreenIdName(FocusMap.FirstFocus()));
		}
		const std::optional<std::string> Next = FocusMap.Move(ScreenIdString(FocusedId), Dir);
		if (Next)
		{
			FocusElement(ScreenIdName(*Next));
		}
		else if (!bRepeat && MenuRoot)
		{
			MenuRoot->PlayEdgeCue();
		}
		return true;
	}

	switch (Input)
	{
	case EUiInput::Confirm:
		if (bRepeat || InputLock.IsConfirmLocked())
		{
			return false;
		}
		FlashConfirmHint();
		if (Focused)
		{
			Focused->Activate();
			return true;
		}
		return OnScreenInput(Input, bRepeat);
	case EUiInput::Back:
		return !bRepeat && OnBack();
	default:
		return OnScreenInput(Input, bRepeat);
	}
}

bool UPadelScreenWidget::HandleAnyKey(const FString& KeyName)
{
	return InputLock.AcceptsInput() && OnAnyKey(KeyName);
}

bool UPadelScreenWidget::OnBack()
{
	return MenuRoot && MenuRoot->DefaultBack(FocusedId);
}

void UPadelScreenWidget::HandleElementActivated(UPadelFocusableWidget* Element)
{
	if (Element && Element->GetElementId() != FocusedId)
	{
		FocusElement(Element->GetElementId());
	}
	if (MenuRoot)
	{
		MenuRoot->PlayConfirmCue();
	}
	OnElementActivated(Element);
}

void UPadelScreenWidget::HandleElementHovered(UPadelFocusableWidget* Element)
{
	// Hover = focus (spec 5.6), but not during the entry lock, so a resting cursor doesn't
	// steal the default focus as the screen slides in under it.
	if (Element && InputLock.AcceptsInput())
	{
		FocusElement(Element->GetElementId());
	}
}

void UPadelScreenWidget::HandleElementValueChanged(UPadelFocusableWidget* Element)
{
	if (MenuRoot)
	{
		MenuRoot->PlayValueCue();
	}
	OnElementValueChanged(Element);
	RefreshHints();
}

TArray<FPadelHint> UPadelScreenWidget::GetHints() const
{
	// The common pair: A Select whenever something is focused, B Back where B does anything.
	TArray<FPadelHint> Hints;
	if (!FocusedId.IsNone())
	{
		Hints.Add({EUiInput::Confirm, LOCTEXT("Hint.Select", "Select")});
	}
	switch (Padel::Ui::BackAction(ToUiScreen(ScreenId)))
	{
	case Padel::Ui::EUiBackAction::None:
		break;
	case Padel::Ui::EUiBackAction::QuitGameDialog:
		Hints.Add({EUiInput::Back, LOCTEXT("Hint.Quit", "Quit")});
		break;
	case Padel::Ui::EUiBackAction::Resume:
		Hints.Add({EUiInput::Back, LOCTEXT("Hint.Resume", "Resume")});
		break;
	default:
		Hints.Add({EUiInput::Back, LOCTEXT("Hint.Back", "Back")});
		break;
	}
	return Hints;
}

void UPadelScreenWidget::RefreshHints()
{
	if (HintBar)
	{
		HintBar->SetHints(GetHints());
	}
}

void UPadelScreenWidget::SetPadFamily(Padel::Ui::EPadFamily InFamily)
{
	PadFamily = InFamily;
	if (HintBar)
	{
		HintBar->SetFamily(InFamily);
	}
}

void UPadelScreenWidget::FlashConfirmHint()
{
	if (HintBar)
	{
		HintBar->FlashConfirm();
	}
}

void UPadelScreenWidget::SetHeader(const FText& Title, const FText& Breadcrumb)
{
	if (Header)
	{
		Header->SetTitle(Title);
		Header->SetBreadcrumb(Breadcrumb);
	}
}

void UPadelScreenWidget::SetHeaderVisible(bool bVisible)
{
	if (Header)
	{
		Header->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UPadelScreenWidget::SetHintBarVisible(bool bVisible)
{
	if (HintBar)
	{
		HintBar->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

#undef LOCTEXT_NAMESPACE
