#include "UI/Screens/PadelControlsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Model/PadelUiPadDiagram.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Widgets/PadelButtonGlyph.h"
#include "UI/Widgets/PadelPadDiagram.h"
#include "UI/Widgets/PadelTabBar.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EPadFamily;
using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

const FName UPadelControlsScreen::SheetId(TEXT("Controls.Sheet"));

namespace
{
	// Board "Controls7" and spec 7.6, in 1080p px.
	constexpr float ControlsTabsTop = 176.0f;
	constexpr float ControlsCalloutHeight = 80.0f;
	constexpr float ControlsGlyph = 40.0f;
	constexpr float ControlsGlyphGap = 16.0f;
	constexpr float ControlsTopGlyphGap = 12.0f;
	constexpr float ControlsLaterGap = 8.0f;
	// The keyboard tab: a glass table over C2-C11, two columns of rows.
	constexpr int32 ControlsKeysColumn = 2;
	constexpr int32 ControlsKeysColumns = 10;
	constexpr float ControlsKeysTop = 248.0f;
	constexpr float ControlsKeysPadding = 24.0f;
	constexpr float ControlsKeysColumnGap = 48.0f;
	constexpr float ControlsKeysRowHeight = 64.0f;
	constexpr float ControlsKeysDetailGap = 12.0f;
	constexpr float ControlsKeyCapGap = 8.0f;

	enum class EControlsTab : int32
	{
		Gamepad,
		Keyboard,
	};

	/** The pad key an action is bound to (the first gamepad row of the binding table). */
	FName ControlsPadKey(EPadelInputAction Action)
	{
		for (const FPadelInputBinding& Binding : UPadelInputConfig::GetDefaultBindings())
		{
			if (Binding.Action == Action && Binding.Key.IsGamepadKey())
			{
				return Binding.Key.GetFName();
			}
		}
		return NAME_None;
	}

	int32 ControlsAxisOrder(EPadelAxisMapping Axis)
	{
		// Spec 7.6: Move reads W A S D.
		switch (Axis)
		{
		case EPadelAxisMapping::PositiveY:
			return 0;
		case EPadelAxisMapping::NegativeX:
			return 1;
		case EPadelAxisMapping::NegativeY:
			return 2;
		case EPadelAxisMapping::PositiveX:
			return 3;
		case EPadelAxisMapping::None:
			break;
		}
		return 4;
	}

	TArray<FPadelInputBinding> ControlsKeyboardKeys(EPadelInputAction Action)
	{
		TArray<FPadelInputBinding> Keys;
		for (const FPadelInputBinding& Binding : UPadelInputConfig::GetDefaultBindings())
		{
			if (Binding.Action == Action && !Binding.Key.IsGamepadKey())
			{
				Keys.Add(Binding);
			}
		}
		Keys.StableSort([](const FPadelInputBinding& A, const FPadelInputBinding& B)
		{
			return ControlsAxisOrder(A.Axis) < ControlsAxisOrder(B.Axis);
		});
		return Keys;
	}

	EPadelInputAction ControlsActionForPadKey(const FString& KeyName, bool& bFound)
	{
		for (EPadelInputAction Action : UPadelInputConfig::GetAllActions())
		{
			if (ControlsPadKey(Action).ToString() == KeyName)
			{
				bFound = true;
				return Action;
			}
		}
		bFound = false;
		return EPadelInputAction::Move;
	}
}

bool UPadelControlsScreen::IsLater(EPadelInputAction Action)
{
	// GDD 7: the drop shot is bound but post-slice.
	return Action == EPadelInputAction::ShotDrop;
}

FText UPadelControlsScreen::GetActionLabel(EPadelInputAction Action) const
{
	if (bPractice)
	{
		// Spec 7.6 "In practice": APadelPracticeDirector's D-pad controls.
		switch (Action)
		{
		case EPadelInputAction::CmdNet:
			return LOCTEXT("Controls.Practice.PauseMachine", "Practice: pause machine");
		case EPadelInputAction::CmdBack:
			return LOCTEXT("Controls.Practice.Feed", "Practice: feed a ball");
		case EPadelInputAction::CmdMiddle:
		case EPadelInputAction::CmdYours:
			return LOCTEXT("Controls.Practice.ChangeDrill", "Practice: change drill");
		default:
			break;
		}
	}
	switch (Action)
	{
	case EPadelInputAction::Move:
		return LOCTEXT("Controls.Action.Move", "Move · aim");
	case EPadelInputAction::Sprint:
		return LOCTEXT("Controls.Action.Sprint", "Sprint");
	case EPadelInputAction::ShotDrive:
		return LOCTEXT("Controls.Action.Drive", "Drive · volley · serve");
	case EPadelInputAction::ShotLob:
		return LOCTEXT("Controls.Action.Lob", "Lob");
	case EPadelInputAction::ShotOverhead:
		return LOCTEXT("Controls.Action.Overhead", "Overhead");
	case EPadelInputAction::ShotDrop:
		return LOCTEXT("Controls.Action.Drop", "Drop shot");
	case EPadelInputAction::ModAttack:
		return LOCTEXT("Controls.Action.Attack", "Attack modifier");
	case EPadelInputAction::ModTouch:
		return LOCTEXT("Controls.Action.Touch", "Touch modifier");
	case EPadelInputAction::CmdNet:
		return LOCTEXT("Controls.Action.Net", "Partner: go to the net");
	case EPadelInputAction::CmdBack:
		return LOCTEXT("Controls.Action.Back", "Partner: stay back");
	case EPadelInputAction::CmdMiddle:
		return LOCTEXT("Controls.Action.Middle", "Partner: take the middle");
	case EPadelInputAction::CmdYours:
		return LOCTEXT("Controls.Action.Yours", "Partner: yours");
	case EPadelInputAction::Pause:
		return LOCTEXT("Controls.Action.Pause", "Pause");
	}
	return FText::GetEmpty();
}

FText UPadelControlsScreen::ActionDetail(EPadelInputAction Action) const
{
	// Board "Controls7": the shots a modifier or button makes, on a second line.
	switch (Action)
	{
	case EPadelInputAction::ShotOverhead:
		return LOCTEXT("Controls.Action.Overhead.Detail", "bandeja, smash");
	case EPadelInputAction::ModAttack:
		return LOCTEXT("Controls.Action.Attack.Detail", "víbora, kick serve");
	case EPadelInputAction::ModTouch:
		return LOCTEXT("Controls.Action.Touch.Detail", "chiquita");
	default:
		return FText::GetEmpty();
	}
}

bool UPadelControlsScreen::OpenedFrom(Padel::Ui::EUiScreen Screen) const
{
	const UPadelMenuRoot* Root = GetMenuRoot();
	return Root && Root->GetFlow().Opener() == Screen;
}

bool UPadelControlsScreen::OpenedFromPause() const
{
	return OpenedFrom(Padel::Ui::EUiScreen::Paused) || OpenedFrom(Padel::Ui::EUiScreen::PracticePaused);
}

EUiColour UPadelControlsScreen::GetScrimColour() const
{
	return OpenedFromPause() ? EUiColour::BgScrimPause : EUiColour::BgScrim;
}

bool UPadelControlsScreen::WantsBackgroundBlur() const
{
	return OpenedFromPause();
}

void UPadelControlsScreen::BuildScreen()
{
	bPractice = OpenedFrom(Padel::Ui::EUiScreen::PracticePaused);
	SetHeader(LOCTEXT("Controls.Title", "Controls"),
		OpenedFromPause() ? LOCTEXT("Controls.Breadcrumb.Pause", "Pause / Controls") : FText::GetEmpty());
	SeenFamily = GetPadFamily();
	GlyphFamily = SeenFamily == EPadFamily::KeyboardMouse ? EPadFamily::Xbox : SeenFamily;

	// The drawing first, so the labels sit over it. It never takes clicks.
	Diagram = MakeElement<UPadelPadDiagram>(SheetId);
	Diagram->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Diagram, 0.0f, 0.0f, 1920.0f, 1080.0f);

	TabBar = WidgetTree->ConstructWidget<UPadelTabBar>(UPadelTabBar::StaticClass());
	TabBar->SetTabs({LOCTEXT("Controls.Tab.Gamepad", "Gamepad"), LOCTEXT("Controls.Tab.Keyboard", "Keyboard")});
	TabBar->SetFamily(GetPadFamily());
	TabBar->OnTabChanged.AddUObject(this, &UPadelControlsScreen::HandleTabChanged);
	Place(TabBar, PadelUi::Theme().Layout.SafeX, ControlsTabsTop, 0.0f, 0.0f);

	BuildGamepadTab();
	BuildKeyboardTab();

	const Padel::Ui::FPadDiagramLayout& Layout = Padel::Ui::PadDiagramLayout();
	UTextBlock* Footer = MakeTokenText(LOCTEXT("Controls.Footer",
		"Hold a shot button to charge it. Release to swing. Aim with the left stick when you release."), EUiType::BodySmall,
		EUiColour::TextSecondary);
	Footer->SetJustification(ETextJustify::Center);
	Footer->SetAutoWrapText(true);
	const float SafeX = PadelUi::Theme().Layout.SafeX;
	Place(Footer, SafeX, Layout.FooterY, 1920.0f - 2.0f * SafeX, 0.0f);

	// Spec 7.6: nothing to focus; the drawing holds focus so LB/RB and B work.
	FocusMap = Padel::Ui::FFocusMap::VerticalList({TCHAR_TO_UTF8(*SheetId.ToString())}, false);

	// Opens on the tab for the device used last.
	const int32 StartTab = static_cast<int32>(GetPadFamily() == EPadFamily::KeyboardMouse ? EControlsTab::Keyboard : EControlsTab::Gamepad);
	TabBar->SetIndex(StartTab);
	ShowTab(StartTab);
}

UWidget* UPadelControlsScreen::MakeActionText(EPadelInputAction Action, bool bAlignRight)
{
	const bool bLater = IsLater(Action);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const EHorizontalAlignment Align = bAlignRight ? HAlign_Right : HAlign_Left;

	UHorizontalBox* LabelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Label = MakeTokenText(GetActionLabel(Action), EUiType::Body, bLater ? EUiColour::TextDisabled : EUiColour::TextPrimary);
	LabelRow->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
	if (bLater)
	{
		// Board: "Drop shot — LATER", greyed, the word in Type.Label.
		UTextBlock* Later = MakeTokenText(FText::Format(LOCTEXT("Controls.LaterSuffix", "— {0}"), LOCTEXT("Controls.Later", "Later")),
			EUiType::Label, EUiColour::TextDisabled);
		UHorizontalBoxSlot* LaterSlot = LabelRow->AddChildToHorizontalBox(Later);
		LaterSlot->SetVerticalAlignment(VAlign_Center);
		LaterSlot->SetPadding(FMargin(ControlsLaterGap, 0.0f, 0.0f, 0.0f));
	}
	Column->AddChildToVerticalBox(LabelRow)->SetHorizontalAlignment(Align);

	const FText Detail = ActionDetail(Action);
	if (!Detail.IsEmpty())
	{
		UTextBlock* DetailText = MakeTokenText(Detail, EUiType::BodySmall, EUiColour::TextSecondary);
		Column->AddChildToVerticalBox(DetailText)->SetHorizontalAlignment(Align);
	}
	return Column;
}

UWidget* UPadelControlsScreen::MakeCallout(EPadelInputAction Action, FName PadKey, bool bGlyphFirst)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UPadelButtonGlyph* Glyph = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
	Glyph->SetDiameter(ControlsGlyph);
	Glyph->SetFamily(GlyphFamily);
	Glyph->SetPadKey(PadKey);
	PadGlyphs.Add(Glyph);

	UWidget* Text = MakeActionText(Action, !bGlyphFirst);
	if (bGlyphFirst)
	{
		Row->AddChildToHorizontalBox(Glyph)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text);
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetPadding(FMargin(ControlsGlyphGap, 0.0f, 0.0f, 0.0f));
	}
	else
	{
		Row->AddChildToHorizontalBox(Text)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* GlyphSlot = Row->AddChildToHorizontalBox(Glyph);
		GlyphSlot->SetVerticalAlignment(VAlign_Center);
		GlyphSlot->SetPadding(FMargin(ControlsGlyphGap, 0.0f, 0.0f, 0.0f));
	}
	return Row;
}

void UPadelControlsScreen::BuildGamepadTab()
{
	const Padel::Ui::FPadDiagramLayout& Layout = Padel::Ui::PadDiagramLayout();
	const float SafeX = PadelUi::Theme().Layout.SafeX;
	const float RightEdge = 1920.0f - SafeX;
	TArray<FString> Labelled;
	for (const Padel::Ui::FPadCallout& Callout : Padel::Ui::PadCallouts())
	{
		const FString KeyName = UTF8_TO_TCHAR(Callout.KeyName.c_str());
		bool bBound = false;
		const EPadelInputAction Action = ControlsActionForPadKey(KeyName, bBound);
		if (!bBound)
		{
			continue;
		}
		Labelled.Add(KeyName);

		// Each label sits in a box beside the drawing, aligned toward it, centred on its row.
		UOverlay* Box = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UOverlaySlot* RowSlot = nullptr;
		switch (Callout.Side)
		{
		case Padel::Ui::ECalloutSide::Left:
			RowSlot = Box->AddChildToOverlay(MakeCallout(Action, FName(*KeyName), false));
			RowSlot->SetHorizontalAlignment(HAlign_Right);
			Place(Box, SafeX, Callout.LabelY - ControlsCalloutHeight * 0.5f, Layout.LeftLabelRight - SafeX, ControlsCalloutHeight);
			break;
		case Padel::Ui::ECalloutSide::Right:
			RowSlot = Box->AddChildToOverlay(MakeCallout(Action, FName(*KeyName), true));
			RowSlot->SetHorizontalAlignment(HAlign_Left);
			Place(Box, Layout.RightLabelLeft, Callout.LabelY - ControlsCalloutHeight * 0.5f, RightEdge - Layout.RightLabelLeft,
				ControlsCalloutHeight);
			break;
		case Padel::Ui::ECalloutSide::Top:
		{
			// Board: "Start  Pause", centred over the pad, glyph first and a tighter gap.
			UWidget* Row = MakeCallout(Action, FName(*KeyName), true);
			if (UHorizontalBox* Horizontal = Cast<UHorizontalBox>(Row))
			{
				if (UHorizontalBoxSlot* TextSlot = Cast<UHorizontalBoxSlot>(Horizontal->GetChildAt(1)->Slot))
				{
					TextSlot->SetPadding(FMargin(ControlsTopGlyphGap, 0.0f, 0.0f, 0.0f));
				}
			}
			RowSlot = Box->AddChildToOverlay(Row);
			RowSlot->SetHorizontalAlignment(HAlign_Center);
			const float Width = Layout.LeftLabelRight - SafeX;
			Place(Box, Layout.TopLabelX - Width * 0.5f, Layout.TopLabelY - ControlsCalloutHeight * 0.5f, Width, ControlsCalloutHeight);
			break;
		}
		}
		if (RowSlot)
		{
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}
		GamepadWidgets.Add(Box);
	}
	Diagram->SetLabelledKeys(Labelled);
}

UWidget* UPadelControlsScreen::MakeKeyboardRow(EPadelInputAction Action)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UHorizontalBox* Words = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Words->AddChildToHorizontalBox(MakeActionText(Action, false))->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* WordsSlot = Row->AddChildToHorizontalBox(Words);
	WordsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	WordsSlot->SetVerticalAlignment(VAlign_Center);
	WordsSlot->SetPadding(FMargin(0.0f, 0.0f, ControlsKeysDetailGap, 0.0f));

	for (const FPadelInputBinding& Binding : ControlsKeyboardKeys(Action))
	{
		UPadelButtonGlyph* Cap = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
		Cap->SetDiameter(ControlsGlyph);
		Cap->SetKeyCap(Binding.Key.GetDisplayName(false));
		UHorizontalBoxSlot* CapSlot = Row->AddChildToHorizontalBox(Cap);
		CapSlot->SetVerticalAlignment(VAlign_Center);
		CapSlot->SetPadding(FMargin(ControlsKeyCapGap, 0.0f, 0.0f, 0.0f));
	}
	return MakeSized(Row, 0.0f, ControlsKeysRowHeight);
}

void UPadelControlsScreen::BuildKeyboardTab()
{
	// Spec 7.6: the same rows in the same order; here split over two columns so the
	// table ends above the footer note.
	const TArray<EPadelInputAction> Actions = UPadelInputConfig::GetAllActions();
	const int32 FirstColumnRows = (Actions.Num() + 1) / 2;
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 ColumnIndex = 0; ColumnIndex < 2; ++ColumnIndex)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		const int32 From = ColumnIndex == 0 ? 0 : FirstColumnRows;
		const int32 To = ColumnIndex == 0 ? FirstColumnRows : Actions.Num();
		for (int32 Index = From; Index < To; ++Index)
		{
			Column->AddChildToVerticalBox(MakeKeyboardRow(Actions[Index]))->SetHorizontalAlignment(HAlign_Fill);
		}
		UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(Column);
		ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ColumnSlot->SetVerticalAlignment(VAlign_Top);
		ColumnSlot->SetPadding(FMargin(ColumnIndex == 0 ? 0.0f : ControlsKeysColumnGap, 0.0f, 0.0f, 0.0f));
	}
	UWidget* Panel = MakeGlassPanel(Columns, FMargin(ControlsKeysPadding), PadelUi::Theme().Layout.RadiusPanel);
	KeyboardWidgets.Add(PlaceInColumns(Panel, ControlsKeysColumn, ControlsKeysColumns, ControlsKeysTop, 0.0f));
}

void UPadelControlsScreen::ShowTab(int32 NewIndex)
{
	TabIndex = FMath::Clamp(NewIndex, 0, 1);
	const bool bPad = TabIndex == static_cast<int32>(EControlsTab::Gamepad);
	if (Diagram)
	{
		Diagram->SetVisibility(bPad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	for (UWidget* Widget : GamepadWidgets)
	{
		Widget->SetVisibility(bPad ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	for (UWidget* Widget : KeyboardWidgets)
	{
		Widget->SetVisibility(bPad ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UPadelControlsScreen::HandleTabChanged(int32 NewIndex)
{
	ShowTab(NewIndex);
	if (GetMenuRoot())
	{
		GetMenuRoot()->PlayFocusCue();
	}
}

void UPadelControlsScreen::RefreshGlyphFamily()
{
	// The Gamepad tab names the pad in use; after keyboard input it keeps the last pad.
	const EPadFamily Family = GetPadFamily();
	if (Family == SeenFamily)
	{
		return;
	}
	SeenFamily = Family;
	if (TabBar)
	{
		TabBar->SetFamily(Family);
	}
	if (Family != EPadFamily::KeyboardMouse && Family != GlyphFamily)
	{
		GlyphFamily = Family;
		for (UPadelButtonGlyph* Glyph : PadGlyphs)
		{
			Glyph->SetFamily(GlyphFamily);
		}
	}
}

void UPadelControlsScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// SetPadFamily isn't virtual; the glyphs follow the device from here (spec 5.5).
	RefreshGlyphFamily();
}

bool UPadelControlsScreen::OnScreenInput(EUiInput Input, bool bRepeat)
{
	if (bRepeat || (Input != EUiInput::TabLeft && Input != EUiInput::TabRight))
	{
		return false;
	}
	if (TabBar && !TabBar->HandleInput(Input) && GetMenuRoot())
	{
		GetMenuRoot()->PlayEdgeCue();
	}
	return true;
}

TArray<FPadelHint> UPadelControlsScreen::GetHints() const
{
	// Spec 7.6: LB RB Change tab · B Back.
	return {{EUiInput::TabLeft, LOCTEXT("Hint.ChangeTab", "Change tab")}, {EUiInput::Back, LOCTEXT("Hint.Back", "Back")}};
}

#undef LOCTEXT_NAMESPACE
