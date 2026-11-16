#include "UI/Screens/PadelPracticeControlList.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"
#include "UI/Model/PadelUiFrontEnd.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelButtonGlyph.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EPracticeControl;
using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	// Boards "Practice" and "PracticePause": 48 px rows 8 px apart, a 104 px glyph
	// column of 40 px glyphs 8 px apart, 16 px before the label.
	constexpr float PracticeListRowHeight = 48.0f;
	constexpr float PracticeListRowGap = 8.0f;
	constexpr float PracticeListGlyphColumn = 104.0f;
	constexpr float PracticeListGlyphSize = 40.0f;
	constexpr float PracticeListGlyphGap = 8.0f;
	constexpr float PracticeListLabelGap = 16.0f;

	FText PracticeControlLabel(EPracticeControl Control)
	{
		switch (Control)
		{
		case EPracticeControl::ChangeDrill:
			return LOCTEXT("Practice.Control.ChangeDrill", "Change drill");
		case EPracticeControl::PauseMachine:
			return LOCTEXT("Practice.Control.PauseMachine", "Pause the machine");
		case EPracticeControl::FeedNow:
			return LOCTEXT("Practice.Control.FeedNow", "Feed a ball now");
		case EPracticeControl::Pause:
			return LOCTEXT("Practice.Control.Pause", "Pause");
		}
		return FText::GetEmpty();
	}
}

void UPadelPracticeControlList::BuildTree()
{
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = List;
	Glyphs.Reset();
	RowCount = 0;
	for (const Padel::Ui::FPracticeControlRow& Row : Padel::Ui::PracticeControlRows())
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBox* Keys = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (size_t Index = 0; Index < Row.PadKeys.size(); ++Index)
		{
			UPadelButtonGlyph* Glyph = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
			const FKey KeyboardKey(FName(UTF8_TO_TCHAR(Row.Keys[Index])));
			Glyph->SetKey(UTF8_TO_TCHAR(Row.PadKeys[Index]), KeyboardKey.GetDisplayName(false));
			Glyph->SetDiameter(PracticeListGlyphSize);
			Glyph->SetFamily(Family);
			UHorizontalBoxSlot* GlyphSlot = Keys->AddChildToHorizontalBox(Glyph);
			GlyphSlot->SetVerticalAlignment(VAlign_Center);
			GlyphSlot->SetPadding(FMargin(Index == 0 ? 0.0f : PracticeListGlyphGap, 0.0f, 0.0f, 0.0f));
			Glyphs.Add(Glyph);
		}
		// At least the board's column, wider if a long key cap ("Options") or Text size needs it.
		USizeBox* KeyColumn = MakeSized(Keys, 0.0f, 0.0f);
		KeyColumn->SetMinDesiredWidth(PracticeListGlyphColumn);
		Line->AddChildToHorizontalBox(KeyColumn)->SetVerticalAlignment(VAlign_Center);

		UTextBlock* Label = MakeTokenText(PracticeControlLabel(Row.Control), EUiType::Body, EUiColour::TextPrimary);
		UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(Label);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(PracticeListLabelGap, 0.0f, 0.0f, 0.0f));

		// 48 px rows, taller only if Text size Largest needs it (spec 10).
		USizeBox* LineBox = MakeSized(Line, 0.0f, 0.0f);
		LineBox->SetMinDesiredHeight(PracticeListRowHeight);
		UVerticalBoxSlot* LineSlot = List->AddChildToVerticalBox(LineBox);
		LineSlot->SetPadding(FMargin(0.0f, RowCount == 0 ? 0.0f : PracticeListRowGap, 0.0f, 0.0f));
		++RowCount;
	}
}

void UPadelPracticeControlList::SetFamily(Padel::Ui::EPadFamily InFamily)
{
	Family = InFamily;
	for (UPadelButtonGlyph* Glyph : Glyphs)
	{
		if (Glyph)
		{
			Glyph->SetFamily(Family);
		}
	}
}

#undef LOCTEXT_NAMESPACE
