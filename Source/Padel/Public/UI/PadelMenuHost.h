#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "UI/Model/PadelUiMatchStats.h"
#include "UI/Model/PadelUiScore.h"
#include "PadelMenuHost.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPadelMenuHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * What the menu screens ask of the game (APadelHUD implements it). Screens never reach
 * the match, the world or the player controller themselves: they read the setup, score
 * and stats from here and ask for game actions here. Screen-to-screen moves go through
 * UPadelMenuRoot instead.
 */
class PADEL_API IPadelMenuHost
{
	GENERATED_BODY()

public:
	/** The Quick Match setup the setup screen edits (spec 7.3), kept between matches. */
	virtual Padel::Ui::FMatchSetup& GetMenuMatchSetup() = 0;
	/** The live score, for the Pause header (spec 7.5). */
	virtual const Padel::Ui::FScoreView& GetMenuScore() const = 0;
	/** The finished match's stats (spec 7.8). */
	virtual const Padel::Ui::FMatchStats& GetMenuMatchStats() const = 0;

	/** Versus card done (spec 7.4): start the match with the setup. Shows the Match screen. */
	virtual void MenuStartMatch() = 0;
	/** Pause: Resume, or B (spec 7.5). */
	virtual void MenuResumeMatch() = 0;
	/** Pause: Restart, after its dialog (spec 7.10): drops the match and shows Versus, which starts it again. */
	virtual void MenuRestartMatch() = 0;
	/** Pause: Quit to main menu, after its dialog: the match is dropped, no stats (spec 7.10). */
	virtual void MenuQuitMatch() = 0;
	/** Main menu: Quit game, after its dialog. */
	virtual void MenuQuitGame() = 0;

	/**
	 * Practice intro: Start practice (spec 7.9). Hides the menus and hands play to the
	 * ball machine drills, which W9 owns; the screen stays Practice while drilling, and
	 * Start opens Practice pause.
	 */
	virtual void MenuStartPractice() = 0;
	/** Practice pause: Quit practice. No confirm, nothing is lost (spec 7.9). Main menu. */
	virtual void MenuQuitPractice() = 0;
	/** True between Start practice and Quit practice. */
	virtual bool IsMenuPracticeRunning() const = 0;
	/** The current drill's name for the Practice pause header (spec 7.9), or empty. */
	virtual FText GetMenuPracticeDrill() const = 0;
};
