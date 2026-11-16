#include "UI/Screens/PadelSettingsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelSettings.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Widgets/PadelSelector.h"
#include "UI/Widgets/PadelSlider.h"
#include "UI/Widgets/PadelTabBar.h"
#include "UI/Widgets/PadelToggle.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::ESetting;
using Padel::Ui::ESettingControl;
using Padel::Ui::ESettingsTab;
using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	// Spec 7.7 and boards "Settings" / "SettingsAccess", in 1080p px.
	constexpr float SettingsTabsTop = 176.0f;
	constexpr float SettingsPanelTop = 248.0f;
	constexpr float SettingsListHeight = 688.0f;
	constexpr int32 SettingsListColumns = 7;
	constexpr int32 SettingsDescColumn = 8;
	constexpr int32 SettingsDescColumns = 5;
	constexpr float SettingsDescMinHeight = 352.0f;
	constexpr float SettingsListPadding = 16.0f;
	constexpr float SettingsRowGap = 8.0f;
	constexpr float SettingsDescPadding = 40.0f;
	constexpr float SettingsDescGap = 12.0f;

	// Changing Text size, Reduce motion or Reduce transparency makes the menu root rebuild
	// this screen at once (PadelUi::OnOptionsChanged). The rebuilt screen reopens this tab,
	// so the root can restore the focused row; any other build starts on the first tab.
	int32 SettingsRebuildTab = INDEX_NONE;
}

FName UPadelSettingsScreen::RowId(ESetting Setting)
{
	return FName(UTF8_TO_TCHAR(Padel::Ui::SettingInfo(Setting).Id));
}

FText UPadelSettingsScreen::TabLabel(ESettingsTab InTab)
{
	switch (InTab)
	{
	case ESettingsTab::Audio:
		return LOCTEXT("Settings.Tab.Audio", "Audio");
	case ESettingsTab::Accessibility:
		return LOCTEXT("Settings.Tab.Accessibility", "Accessibility");
	case ESettingsTab::Count:
		break;
	}
	return FText::GetEmpty();
}

FText UPadelSettingsScreen::SettingLabel(ESetting Setting)
{
	switch (Setting)
	{
	case ESetting::MasterVolume:
		return LOCTEXT("Settings.Vol.Master", "Master volume");
	case ESetting::EffectsVolume:
		return LOCTEXT("Settings.Vol.Effects", "Ball and court");
	case ESetting::CrowdVolume:
		return LOCTEXT("Settings.Vol.Crowd", "Crowd");
	case ESetting::MenuVolume:
		return LOCTEXT("Settings.Vol.Ui", "Menu sounds");
	case ESetting::TextSize:
		return LOCTEXT("Settings.TextSize", "Text size");
	case ESetting::ReduceMotion:
		return LOCTEXT("Settings.ReduceMotion", "Reduce motion");
	case ESetting::ReduceTransparency:
		return LOCTEXT("Settings.ReduceTransparency", "Reduce transparency");
	case ESetting::Count:
		break;
	}
	return FText::GetEmpty();
}

FText UPadelSettingsScreen::SettingDescription(ESetting Setting)
{
	// Spec 7.7: the volumes other than Ball and court have no description ("—").
	switch (Setting)
	{
	case ESetting::EffectsVolume:
		return LOCTEXT("Settings.Vol.Effects.Desc", "Racket, glass, mesh and footsteps. These help you read the point.");
	case ESetting::TextSize:
		return LOCTEXT("Settings.TextSize.Desc", "Makes text bigger in menus and the HUD.");
	case ESetting::ReduceMotion:
		return LOCTEXT("Settings.ReduceMotion.Desc", "Stops the menu camera, tile push-ins, pulses and slides; screens cross-fade instead.");
	case ESetting::ReduceTransparency:
		return LOCTEXT("Settings.ReduceTransparency.Desc", "Solid panels instead of glass.");
	default:
		return FText::GetEmpty();
	}
}

bool UPadelSettingsScreen::OpenedFromPause() const
{
	const UPadelMenuRoot* Root = GetMenuRoot();
	const Padel::Ui::EUiScreen Opener = Root ? Root->GetFlow().Opener() : Padel::Ui::EUiScreen::Count;
	return Opener == Padel::Ui::EUiScreen::Paused || Opener == Padel::Ui::EUiScreen::PracticePaused;
}

EUiColour UPadelSettingsScreen::GetScrimColour() const
{
	// Board "SettingsAccess": over the frozen match it keeps the Pause scrim and blur.
	return OpenedFromPause() ? EUiColour::BgScrimPause : EUiColour::BgScrim;
}

bool UPadelSettingsScreen::WantsBackgroundBlur() const
{
	return OpenedFromPause();
}

void UPadelSettingsScreen::BuildScreen()
{
	SetHeader(LOCTEXT("Settings.Title", "Settings"),
		OpenedFromPause() ? LOCTEXT("Settings.Breadcrumb.Pause", "Pause / Settings") : FText::GetEmpty());

	Values = UPadelSettings::Get()->GetMenuValues();
	bMenuSounds = GetMenuRoot() && GetMenuRoot()->OnCue.IsBound();

	TabBar = WidgetTree->ConstructWidget<UPadelTabBar>(UPadelTabBar::StaticClass());
	TArray<FText> TabNames;
	for (int32 Index = 0; Index < static_cast<int32>(ESettingsTab::Count); ++Index)
	{
		TabNames.Add(TabLabel(static_cast<ESettingsTab>(Index)));
	}
	TabBar->SetTabs(TabNames);
	TabBar->SetFamily(GetPadFamily());
	TabBar->OnTabChanged.AddUObject(this, &UPadelSettingsScreen::HandleTabChanged);
	Place(TabBar, PadelUi::Theme().Layout.SafeX, SettingsTabsTop, 0.0f, 0.0f);

	// The list: one vertical box of rows per tab, stacked; only the current tab shows.
	UOverlay* Lists = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	for (int32 Index = 0; Index < static_cast<int32>(ESettingsTab::Count); ++Index)
	{
		UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (ESetting Setting : Padel::Ui::TabSettings(static_cast<ESettingsTab>(Index), bMenuSounds))
		{
			BuildRow(Setting, List);
		}
		UOverlaySlot* ListSlot = Lists->AddChildToOverlay(List);
		ListSlot->SetHorizontalAlignment(HAlign_Fill);
		ListSlot->SetVerticalAlignment(VAlign_Top);
		TabLists.Add(List);
	}
	const float Radius = PadelUi::Theme().Layout.RadiusPanel;
	PlaceInColumns(MakeGlassPanel(Lists, FMargin(SettingsListPadding), Radius), 1, SettingsListColumns, SettingsPanelTop, SettingsListHeight);

	// The description panel: the focused row's name and what it does.
	UVerticalBox* Description = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DescriptionTitle = MakeTokenText(FText::GetEmpty(), EUiType::Title, EUiColour::TextPrimary);
	DescriptionTitle->SetAutoWrapText(true);
	Description->AddChildToVerticalBox(DescriptionTitle);
	DescriptionBody = MakeTokenText(FText::GetEmpty(), EUiType::Body, EUiColour::TextSecondary);
	DescriptionBody->SetAutoWrapText(true);
	Description->AddChildToVerticalBox(DescriptionBody)->SetPadding(FMargin(0.0f, SettingsDescGap, 0.0f, 0.0f));
	USizeBox* DescriptionBox = MakeSized(MakeGlassPanel(Description, FMargin(SettingsDescPadding), Radius), 0.0f, 0.0f);
	DescriptionBox->SetMinDesiredHeight(SettingsDescMinHeight);
	PlaceInColumns(DescriptionBox, SettingsDescColumn, SettingsDescColumns, SettingsPanelTop, 0.0f);

	ESettingsTab StartTab = ESettingsTab::Audio;
	if (SettingsRebuildTab >= 0 && SettingsRebuildTab < static_cast<int32>(ESettingsTab::Count))
	{
		StartTab = static_cast<ESettingsTab>(SettingsRebuildTab);
	}
	SettingsRebuildTab = INDEX_NONE;
	TabBar->SetIndex(static_cast<int32>(StartTab));
	ShowTab(StartTab, false);
}

void UPadelSettingsScreen::BuildRow(ESetting Setting, UVerticalBox* List)
{
	const Padel::Ui::FSettingInfo& Info = Padel::Ui::SettingInfo(Setting);
	const FName Id = RowId(Setting);
	UPadelRowControl* Row = nullptr;
	switch (Info.Control)
	{
	case ESettingControl::Slider:
	{
		UPadelSlider* Slider = MakeElement<UPadelSlider>(Id);
		Slider->SetRange(Info.Min, Info.Max);
		Slider->SetValue(Values.Get(Setting));
		Row = Slider;
		break;
	}
	case ESettingControl::Selector:
	{
		UPadelSelector* Selector = MakeElement<UPadelSelector>(Id);
		// Text size is the only selector in the slice (spec 7.7).
		Selector->SetOptions({LOCTEXT("Settings.TextSize.Default", "Default"), LOCTEXT("Settings.TextSize.Large", "Large"),
			LOCTEXT("Settings.TextSize.Largest", "Largest")});
		Selector->SetIndex(Values.Get(Setting));
		Row = Selector;
		break;
	}
	case ESettingControl::Toggle:
	{
		UPadelToggle* Toggle = MakeElement<UPadelToggle>(Id);
		Toggle->SetOn(Values.Get(Setting) != 0);
		Row = Toggle;
		break;
	}
	}
	if (!Row)
	{
		return;
	}
	// Board: the description lives in the panel on the right, not under the row.
	Row->SetLabel(SettingLabel(Setting));
	UVerticalBoxSlot* RowSlot = List->AddChildToVerticalBox(Row);
	RowSlot->SetHorizontalAlignment(HAlign_Fill);
	RowSlot->SetPadding(FMargin(0.0f, List->GetChildrenCount() > 1 ? SettingsRowGap : 0.0f, 0.0f, 0.0f));
}

void UPadelSettingsScreen::ShowTab(ESettingsTab NewTab, bool bFocusFirst)
{
	Tab = NewTab;
	for (int32 Index = 0; Index < TabLists.Num(); ++Index)
	{
		TabLists[Index]->SetVisibility(Index == static_cast<int32>(Tab) ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// Spec 7.7: a vertical list per tab, wrapping.
	std::vector<std::string> Ids;
	for (ESetting Setting : Padel::Ui::TabSettings(Tab, bMenuSounds))
	{
		Ids.push_back(Padel::Ui::SettingInfo(Setting).Id);
	}
	FocusMap = Padel::Ui::FFocusMap::VerticalList(Ids);
	if (bFocusFirst && !Ids.empty())
	{
		// Spec 6.11: changing tab focuses the new tab's first row.
		FocusElement(FName(UTF8_TO_TCHAR(FocusMap.FirstFocus().c_str())));
	}
	RefreshHints();
}

void UPadelSettingsScreen::HandleTabChanged(int32 NewIndex)
{
	ShowTab(static_cast<ESettingsTab>(FMath::Clamp(NewIndex, 0, static_cast<int32>(ESettingsTab::Count) - 1)), true);
}

bool UPadelSettingsScreen::FindSetting(FName Id, ESetting& Out) const
{
	for (int32 Index = 0; Index < static_cast<int32>(ESetting::Count); ++Index)
	{
		if (RowId(static_cast<ESetting>(Index)) == Id)
		{
			Out = static_cast<ESetting>(Index);
			return true;
		}
	}
	return false;
}

void UPadelSettingsScreen::OnElementValueChanged(UPadelFocusableWidget* Element)
{
	ESetting Setting;
	if (!Element || !FindSetting(Element->GetElementId(), Setting))
	{
		return;
	}
	int32 NewValue = Values.Get(Setting);
	if (const UPadelSlider* Slider = Cast<UPadelSlider>(Element))
	{
		NewValue = Slider->GetValue();
	}
	else if (const UPadelSelector* Selector = Cast<UPadelSelector>(Element))
	{
		NewValue = Selector->GetIndex();
	}
	else if (const UPadelToggle* Toggle = Cast<UPadelToggle>(Element))
	{
		NewValue = Toggle->IsOn() ? 1 : 0;
	}
	if (Values.Set(Setting, NewValue))
	{
		ApplyValues();
	}
}

void UPadelSettingsScreen::ApplyValues()
{
	// A menu option rebuilds this screen inside SetMenuValues; the new one reopens this tab.
	SettingsRebuildTab = static_cast<int32>(Tab);
	UPadelSettings::Get()->SetMenuValues(Values);
	SettingsRebuildTab = INDEX_NONE;
}

void UPadelSettingsScreen::SyncRows()
{
	for (int32 Index = 0; Index < static_cast<int32>(ESetting::Count); ++Index)
	{
		const ESetting Setting = static_cast<ESetting>(Index);
		UPadelFocusableWidget* Element = FindElement(RowId(Setting));
		if (UPadelSlider* Slider = Cast<UPadelSlider>(Element))
		{
			Slider->SetValue(Values.Get(Setting));
		}
		else if (UPadelSelector* Selector = Cast<UPadelSelector>(Element))
		{
			Selector->SetIndex(Values.Get(Setting));
		}
		else if (UPadelToggle* Toggle = Cast<UPadelToggle>(Element))
		{
			Toggle->SetOn(Values.Get(Setting) != 0);
		}
	}
}

void UPadelSettingsScreen::ConfirmResetTab()
{
	UPadelMenuRoot* Root = GetMenuRoot();
	if (!Root)
	{
		return;
	}
	// Spec 7.10, Dlg.ResetTab: "{tab}" is the tab's name inside a sentence.
	const FText TabName = TabLabel(Tab).ToLower();
	FPadelConfirm Confirm;
	Confirm.Title = FText::Format(LOCTEXT("Dlg.ResetTab.Title", "Reset {0} settings?"), TabName);
	Confirm.Body = FText::Format(LOCTEXT("Dlg.ResetTab.Body", "All {0} settings go back to their defaults."), TabName);
	Confirm.ActionLabel = LOCTEXT("Dlg.ResetTab.Action", "Reset");
	Confirm.SafeLabel = LOCTEXT("Dlg.Cancel", "Cancel");
	TWeakObjectPtr<UPadelSettingsScreen> WeakThis(this);
	Confirm.OnAction = [WeakThis]()
	{
		UPadelSettingsScreen* Screen = WeakThis.Get();
		if (!Screen || !Screen->Values.ResetTab(Screen->Tab))
		{
			return;
		}
		Screen->SyncRows();
		Screen->RefreshHints();
		Screen->ApplyValues();
	};
	Root->ShowConfirm(Confirm);
}

void UPadelSettingsScreen::OnFocusChanged(UPadelFocusableWidget* Element)
{
	UpdateDescription(Element);
}

void UPadelSettingsScreen::UpdateDescription(UPadelFocusableWidget* Element)
{
	if (!DescriptionTitle || !DescriptionBody)
	{
		return;
	}
	ESetting Setting;
	if (!Element || !FindSetting(Element->GetElementId(), Setting))
	{
		DescriptionTitle->SetText(FText::GetEmpty());
		DescriptionBody->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	DescriptionTitle->SetText(SettingLabel(Setting));
	const FText Body = SettingDescription(Setting);
	DescriptionBody->SetText(Body);
	DescriptionBody->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

bool UPadelSettingsScreen::OnBack()
{
	// Spec 7.7: changes are already applied; they're written when leaving the screen.
	UPadelSettings::Get()->SaveMenuSettings();
	return Super::OnBack();
}

bool UPadelSettingsScreen::OnScreenInput(EUiInput Input, bool bRepeat)
{
	if (bRepeat)
	{
		return false;
	}
	if (Input == EUiInput::TabLeft || Input == EUiInput::TabRight)
	{
		// Tabs don't wrap (spec 6.11): at an end, the edge sound.
		if (TabBar && !TabBar->HandleInput(Input) && GetMenuRoot())
		{
			GetMenuRoot()->PlayEdgeCue();
		}
		return true;
	}
	if (Input == EUiInput::Secondary)
	{
		// X lives only while something on the tab isn't default (spec 6.1).
		if (!Values.IsTabDefault(Tab, bMenuSounds))
		{
			ConfirmResetTab();
			return true;
		}
		return false;
	}
	return false;
}

TArray<FPadelHint> UPadelSettingsScreen::GetHints() const
{
	// Spec 7.7: A Change (toggles and selectors) · X Reset (only if off-default) · LB RB Change tab · B Back.
	TArray<FPadelHint> Hints;
	const UPadelFocusableWidget* Focused = GetFocusedElement();
	if (Focused && (Focused->IsA<UPadelToggle>() || Focused->IsA<UPadelSelector>()))
	{
		Hints.Add({EUiInput::Confirm, LOCTEXT("Hint.Change", "Change")});
	}
	if (!Values.IsTabDefault(Tab, bMenuSounds))
	{
		Hints.Add({EUiInput::Secondary, LOCTEXT("Hint.Reset", "Reset")});
	}
	Hints.Add({EUiInput::TabLeft, LOCTEXT("Hint.ChangeTab", "Change tab")});
	Hints.Add({EUiInput::Back, LOCTEXT("Hint.Back", "Back")});
	return Hints;
}

#undef LOCTEXT_NAMESPACE
