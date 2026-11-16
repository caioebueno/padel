#include "UI/Screens/PadelTitleCatcher.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "UI/PadelUiPaint.h"

void UPadelTitleCatcher::BuildTree()
{
	// Hit-testable but clear: clicks land here, nothing shows.
	UImage* Fill = MakeImage(PadelUiPaint::Solid(FLinearColor::Transparent));
	Fill->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Fill;
}
