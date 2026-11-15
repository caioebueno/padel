#include "UI/PadelMenuButtonRelay.h"

#include "UI/PadelMenuWidget.h"

void UPadelMenuButtonRelay::HandleClicked()
{
	if (Menu)
	{
		Menu->ChooseOption(Index);
	}
}
