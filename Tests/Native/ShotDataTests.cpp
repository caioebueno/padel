// The tuning files load and every slice shot has a valid row (GDD 4: no hard-coded values).
#include "doctest.h"
#include "ShotTestData.h"

using namespace ShotTestData;

TEST_CASE("Shots.Data: every slice shot has a valid row and the model is complete")
{
	const FShotLibrary Lib = LoadLibrary();
	const std::vector<std::string> Errors = Lib.Validate();
	for (const std::string& Error : Errors)
	{
		MESSAGE(Error);
	}
	CHECK(Errors.empty());
}

TEST_CASE("Shots.Data: speeds match the GDD 4 table")
{
	const FShotLibrary Lib = LoadLibrary();
	CHECK(Lib.Get(EShotType::Drive).SpeedMinKmh == 50);
	CHECK(Lib.Get(EShotType::Drive).SpeedMaxKmh == 100);
	CHECK(Lib.Get(EShotType::Smash).SpeedMaxKmh == 140);
	CHECK(Lib.Get(EShotType::Lob).ApexMinM == 6);
	CHECK(Lib.Get(EShotType::Lob).ApexMaxM == 8);
	CHECK(Lib.Get(EShotType::Chiquita).SpeedMaxKmh == 40);
	// GDD 5.1 base windows.
	CHECK(Lib.Get(EShotType::Drive).PerfectWindowMs == 40);
	CHECK(Lib.Get(EShotType::Drive).GoodWindowMs == 90);
	CHECK(Lib.Get(EShotType::Drive).EarlyLateWindowMs == 160);
	// GDD 7: full charge in 0.6 s.
	CHECK(Lib.Model.ChargeTimeS == doctest::Approx(0.6));
}

TEST_CASE("Shots.Data: movement, practice ball, player and feeds are valid")
{
	CHECK(LoadMovement().Validate().empty());
	CHECK(LoadPracticeBall().Validate().empty());
	const FPlayerRatings P = LoadPlayer();
	CHECK(P.Power >= 1);
	CHECK(P.Power <= 99);
	const std::vector<FBallMachineFeed> Feeds = LoadFeeds();
	CHECK(!Feeds.empty());
	for (const FBallMachineFeed& F : Feeds)
	{
		// Written for a player on +X: fed from the far half onto the near half.
		CHECK(F.OriginX < 0.0);
		CHECK(F.TargetX > 0.0);
	}
}

TEST_CASE("Shots.Data: rating names round-trip")
{
	for (int Index = 0; Index < static_cast<int>(ERating::Count); ++Index)
	{
		const ERating Rating = static_cast<ERating>(Index);
		CHECK(RatingFromName(RatingName(Rating)) == Rating);
	}
	CHECK(RatingFromName("Nope") == ERating::Count);
}
