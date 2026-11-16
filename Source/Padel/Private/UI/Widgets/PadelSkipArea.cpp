#include "UI/Widgets/PadelSkipArea.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "UI/PadelUiPaint.h"

void UPadelSkipArea::BuildTree()
{
	// A clear fill, so the whole area takes hover and clicks.
	WidgetTree->RootWidget = MakeImage(PadelUiPaint::Solid(FLinearColor::Transparent));
}
