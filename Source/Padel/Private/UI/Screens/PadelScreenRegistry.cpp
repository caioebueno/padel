#include "UI/Screens/PadelScreenRegistry.h"

#include "UI/Screens/PadelPlaceholderScreen.h"
#include "UI/Screens/PadelSetupScreen.h"
#include "UI/Screens/PadelVersusScreen.h"

TSubclassOf<UPadelScreenWidget> PadelScreenRegistry::ScreenClass(EPadelScreen Screen)
{
	// Each workstream replaces its screens' placeholder with the real class here.
	switch (Screen)
	{
	case EPadelScreen::Title:          return UPadelPlaceholderScreen::StaticClass(); // 7.1, workstream A
	case EPadelScreen::MainMenu:       return UPadelPlaceholderScreen::StaticClass(); // 7.2, workstream A
	case EPadelScreen::MatchSetup:     return UPadelSetupScreen::StaticClass(); // 7.3, workstream B
	case EPadelScreen::Versus:         return UPadelVersusScreen::StaticClass(); // 7.4, workstream B
	case EPadelScreen::Paused:         return UPadelPlaceholderScreen::StaticClass(); // 7.5, workstream C
	case EPadelScreen::Controls:       return UPadelPlaceholderScreen::StaticClass(); // 7.6, workstream D
	case EPadelScreen::Settings:       return UPadelPlaceholderScreen::StaticClass(); // 7.7, workstream D
	case EPadelScreen::Stats:          return UPadelPlaceholderScreen::StaticClass(); // 7.8, workstream C
	case EPadelScreen::Practice:       return UPadelPlaceholderScreen::StaticClass(); // 7.9, workstream A
	case EPadelScreen::PracticePaused: return UPadelPlaceholderScreen::StaticClass(); // 7.9, workstream A
	case EPadelScreen::Match:          return nullptr; // the game itself: no menu
	}
	return UPadelPlaceholderScreen::StaticClass();
}
