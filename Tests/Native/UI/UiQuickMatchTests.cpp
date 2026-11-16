// The Quick Match setup and Versus screens' engine-free parts (docs/ui/MENU_SPEC.md 6.6,
// 7.3, 7.4, 7.12): player card stats and the overall rating from the real
// Data/Players.json, the setup focus map and control mapping, and the Versus clock.
#include "doctest.h"

#include "../ShotTestData.h"
#include "UI/Model/PadelUiPlayerCard.h"
#include "UI/Model/PadelUiQuickMatch.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/Model/PadelUiTheme.h"

#include <string>

using namespace Padel::Ui;

namespace
{
	FPlayerCardView QuickMatchCard(int Id)
	{
		const Padel::Shots::FPlayerRatings Ratings = ShotTestData::LoadPlayer(PlayerRowName(Id));
		return MakePlayerCard(Id, &Ratings, FUiTheme::Defaults().PlayerCardStats);
	}

	int QuickMatchStat(const FPlayerCardView& Card, const std::string& Label)
	{
		for (const FCardStat& Stat : Card.Stats)
		{
			if (Stat.Label == Label)
			{
				return Stat.Value;
			}
		}
		return UnknownCardStat;
	}
}

TEST_CASE("UI.PlayerCard: every slice player has a Data/Players.json row")
{
	for (int Id = 0; Id < 4; ++Id)
	{
		CAPTURE(Id);
		const std::string Row = PlayerRowName(Id);
		REQUIRE_FALSE(Row.empty());
		CHECK_NOTHROW(ShotTestData::LoadPlayer(Row));
	}
	CHECK(PlayerRowName(1) == "mikko_ventara");
	CHECK(PlayerRowName(7).empty());
}

TEST_CASE("UI.PlayerCard: the overall rating is the plain average until spec 13 Q1 (6.6)")
{
	// Spec 6.6 quotes these for the equal-weight average: Arunde 78, Ventara 77, Corvisa 78,
	// Halvaro 77, all inside GDD 6.4's 75-82.
	CHECK(QuickMatchCard(0).Overall == 78);
	CHECK(QuickMatchCard(1).Overall == 77);
	CHECK(QuickMatchCard(2).Overall == 78);
	CHECK(QuickMatchCard(3).Overall == 77);

	Padel::Shots::FPlayerRatings Flat;
	CHECK(OverallRating(Flat) == 50);
	Flat.Drive = 73;  // one attribute 23 higher moves the mean by exactly 1
	CHECK(OverallRating(Flat) == 51);
}

TEST_CASE("UI.PlayerCard: card stats average the theme's attribute groups, rounded half up")
{
	const FPlayerCardView Ventara = QuickMatchCard(1);
	REQUIRE(Ventara.Stats.size() == 4);
	CHECK(Ventara.Stats[0].Label == "Attack");
	CHECK(QuickMatchStat(Ventara, "Attack") == 83);    // (84 + 80 + 83 + 85) / 4 = 83
	CHECK(QuickMatchStat(Ventara, "Defence") == 75);   // 74.5 rounds up
	CHECK(QuickMatchStat(Ventara, "Net") == 75);
	CHECK(QuickMatchStat(Ventara, "Movement") == 76);

	const FPlayerCardView Arunde = QuickMatchCard(0);
	CHECK(QuickMatchStat(Arunde, "Attack") == 72);
	CHECK(QuickMatchStat(Arunde, "Defence") == 81);
	CHECK(QuickMatchStat(Arunde, "Net") == 79);        // 78.5 rounds up
	CHECK(QuickMatchStat(Arunde, "Movement") == 80);

	Padel::Shots::FPlayerRatings Ratings;
	CHECK(CardStat(Ratings, {}) == UnknownCardStat);
	CHECK(CardStat(Ratings, {"Drive", "NotARating"}) == UnknownCardStat);
}

TEST_CASE("UI.PlayerCard: profile line and names come from the roster and the ratings")
{
	const FPlayerCardView Ventara = QuickMatchCard(1);
	CHECK(Ventara.FirstName == "Mikko");
	CHECK(Ventara.Surname == "Ventara");
	CHECK(Ventara.Team == 0);
	CHECK(Ventara.bHasRatings);
	CHECK(Ventara.bLeftSide);
	CHECK_FALSE(Ventara.bLeftHanded);
	CHECK(Ventara.HeightM == doctest::Approx(1.86));
	CHECK(Ventara.Style == EPlayStyle::Power);

	const FPlayerCardView Halvaro = QuickMatchCard(3);
	CHECK(Halvaro.Team == 1);
	CHECK(Halvaro.bLeftHanded);
	CHECK(Halvaro.Style == EPlayStyle::Touch);
	CHECK(QuickMatchCard(0).Style == EPlayStyle::Wall);
	CHECK(QuickMatchCard(2).Style == EPlayStyle::AllRound);
	CHECK(PlayStyleFromTag(nullptr) == EPlayStyle::Unknown);
	CHECK(PlayStyleFromTag("Net") == EPlayStyle::Net);
}

TEST_CASE("UI.PlayerCard: a player without a row shows a fallback name and dashes (7.12)")
{
	const FPlayerCardView Missing = MakePlayerCard(5, nullptr, FUiTheme::Defaults().PlayerCardStats);
	CHECK(Missing.FirstName.empty());
	CHECK(Missing.Surname == "Player 5");
	CHECK_FALSE(Missing.bHasRatings);
	CHECK(Missing.Overall == UnknownCardStat);
	REQUIRE(Missing.Stats.size() == 4);
	for (const FCardStat& Stat : Missing.Stats)
	{
		CHECK(Stat.Value == UnknownCardStat);
	}
	// A roster player whose row is missing keeps their name.
	const FPlayerCardView NoRow = MakePlayerCard(1, nullptr, FUiTheme::Defaults().PlayerCardStats);
	CHECK(NoRow.Surname == "Ventara");
	CHECK(NoRow.Overall == UnknownCardStat);
}

TEST_CASE("UI.PlayerCard: meters fill to rating / 99")
{
	CHECK(MeterFill(99) == doctest::Approx(1.0));
	CHECK(MeterFill(77) == doctest::Approx(77.0 / 99.0));
	CHECK(MeterFill(UnknownCardStat) == 0.0f);
	CHECK(MeterFill(150) == 1.0f);
}

TEST_CASE("UI.Setup: the focus map is spec 7.3's table")
{
	const FFocusMap Map = SetupFocusMap();
	CHECK(Map.Validate().empty());
	CHECK(Map.FirstFocus() == SetupStartId);
	CHECK(Map.Move(SetupDifficultyId, EFocusDir::Up) == std::string(SetupStartId));
	CHECK(Map.Move(SetupDifficultyId, EFocusDir::Down) == std::string(SetupFormatId));
	CHECK(Map.Move(SetupFormatId, EFocusDir::Up) == std::string(SetupDifficultyId));
	CHECK(Map.Move(SetupFormatId, EFocusDir::Down) == std::string(SetupGoldenPointId));
	CHECK(Map.Move(SetupGoldenPointId, EFocusDir::Up) == std::string(SetupFormatId));
	CHECK(Map.Move(SetupGoldenPointId, EFocusDir::Down) == std::string(SetupStartId));
	CHECK(Map.Move(SetupStartId, EFocusDir::Up) == std::string(SetupGoldenPointId));
	CHECK(Map.Move(SetupStartId, EFocusDir::Down) == std::string(SetupDifficultyId));
	// Start has nothing left or right; on the rules rows the control takes them first.
	CHECK_FALSE(Map.Move(SetupStartId, EFocusDir::Left).has_value());
	CHECK_FALSE(Map.Move(SetupStartId, EFocusDir::Right).has_value());
}

TEST_CASE("UI.Setup: the controls map onto FMatchSetup")
{
	FMatchSetup Setup;
	CHECK(DifficultyIndex(Setup) == 1);
	SetDifficultyIndex(Setup, 2);
	CHECK(Setup.Difficulty == EDifficulty::Hard);
	SetDifficultyIndex(Setup, 3);
	CHECK(Setup.Difficulty == EDifficulty::Hard);

	CHECK(FormatIndex(Setup) == 1);  // best of 3 by default (GDD 13 decision 5)
	SetFormatIndex(Setup, 0);
	CHECK(Setup.SetsToWin == 1);
	CHECK(FormatIndex(Setup) == 0);
	SetFormatIndex(Setup, 1);
	CHECK(Setup.SetsToWin == 2);
}

TEST_CASE("UI.Setup: X swaps the human but the cards keep their places (7.3)")
{
	FMatchSetup Setup;
	const std::array<int, 2> Order = SetupPairOrder();
	CHECK(Order[0] == 1);  // Ventara, the default human (GDD 6.4)
	CHECK(Order[1] == 0);
	CHECK(Setup.HumanPlayerId == Order[0]);
	Setup.CyclePlayer();
	CHECK(Setup.HumanPlayerId == 0);
	CHECK(Setup.PartnerId() == 1);
	CHECK(SetupPairOrder() == Order);
	CHECK(SetupOpponentOrder() == std::array<int, 2>{2, 3});
}

TEST_CASE("UI.Versus: the human's name leads their pair (7.4)")
{
	FMatchSetup Setup;
	CHECK(VersusPairOrder(0, Setup) == std::array<int, 2>{1, 0});
	Setup.CyclePlayer();
	CHECK(VersusPairOrder(0, Setup) == std::array<int, 2>{0, 1});
	CHECK(VersusPairOrder(1, Setup) == std::array<int, 2>{2, 3});
}

TEST_CASE("UI.Versus: the card auto-advances once, and A skips only when the match is ready")
{
	FVersusClock Clock;
	Clock.CardSeconds = 2.5f;
	CHECK(Clock.CanSkip(true));
	CHECK_FALSE(Clock.CanSkip(false));
	Clock.Tick(2.4f);
	CHECK_FALSE(Clock.ShouldAutoStart(true));
	Clock.Tick(0.1f);
	CHECK(Clock.ShouldAutoStart(true));
	CHECK_FALSE(Clock.ShouldAutoStart(false));
	Clock.MarkStarted();
	CHECK_FALSE(Clock.ShouldAutoStart(true));
	CHECK_FALSE(Clock.CanSkip(true));
}

TEST_CASE("UI.Versus: a slow match shows the loading indicator, then offers the menu (7.12)")
{
	FVersusClock Clock;
	Clock.CardSeconds = 2.5f;
	Clock.LoadingAfter = 0.3f;
	Clock.StartTimeout = 20.0f;
	Clock.Tick(2.7f);
	CHECK_FALSE(Clock.ShowsLoading(false));
	Clock.Tick(0.2f);
	CHECK(Clock.ShowsLoading(false));
	CHECK_FALSE(Clock.ShowsLoading(true));
	CHECK_FALSE(Clock.TakeTimeout(false));
	Clock.Tick(17.5f);
	CHECK_FALSE(Clock.TakeTimeout(true));
	CHECK(Clock.TakeTimeout(false));
	CHECK_FALSE(Clock.TakeTimeout(false));
	// The match turning ready late still starts it.
	CHECK(Clock.ShouldAutoStart(true));
}
