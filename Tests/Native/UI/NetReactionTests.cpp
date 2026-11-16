// The net cloth's reaction to a ball (GDD 2 net, GDD 3.4 ball_bounced on "net").
#include "doctest.h"

#include "CourtSpec.h"
#include "Presentation/Model/PadelNetReaction.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

using namespace Padel::Presentation;
using Padel::Court::HalfWidth;
using Padel::Court::NetHeightAt;
using doctest::Approx;

namespace
{
	// Largest |offset| over the net's rest shape, on a 5 cm grid.
	double PeakOffset(const FNetReaction& Net, double* OutY = nullptr)
	{
		double Peak = 0.0;
		for (double Y = -HalfWidth; Y <= HalfWidth + 1e-9; Y += 0.05)
		{
			for (double Z = 0.0; Z <= NetHeightAt(Y) + 1e-9; Z += 0.05)
			{
				const double D = std::abs(Net.Displacement(Y, Z));
				if (D > Peak)
				{
					Peak = D;
					if (OutY)
					{
						*OutY = Y;
					}
				}
			}
		}
		return Peak;
	}

	// The net at the moment the pocket is deepest (a quarter period after the hit).
	FNetReaction HitAndWait(double Y, double Z, double Speed, double Direction, double Seconds)
	{
		FNetReaction Net;
		Net.AddImpact(Y, Z, Speed, Direction, false);
		Net.Advance(Seconds);
		return Net;
	}
}

TEST_CASE("Net.Reaction.RestsUntilHit")
{
	FNetReaction Net;
	CHECK_FALSE(Net.IsActive());
	CHECK(PeakOffset(Net) == 0.0);
	// A hit starts from the rest shape: it adds velocity, not a jump.
	Net.AddImpact(0.0, 0.5, 20.0, -1.0, false);
	CHECK(Net.IsActive());
	CHECK(PeakOffset(Net) == Approx(0.0).epsilon(1e-12));
}

TEST_CASE("Net.Reaction.GivesWayInTheBallsDirection")
{
	const double QuarterPeriod = 0.25 / FNetReactionSettings().FrequencyHz;
	const FNetReaction Pushed = HitAndWait(1.0, 0.5, 20.0, -1.0, QuarterPeriod);
	CHECK(Pushed.Displacement(1.0, 0.5) < -0.02);
	const FNetReaction Other = HitAndWait(1.0, 0.5, 20.0, +1.0, QuarterPeriod);
	CHECK(Other.Displacement(1.0, 0.5) == Approx(-Pushed.Displacement(1.0, 0.5)));
}

TEST_CASE("Net.Reaction.OnNetContactReadsTheSideFromTheSimulation")
{
	// BallSim puts a ball that goes into the net a ball radius in front of it, on the side it came from.
	const double QuarterPeriod = 0.25 / FNetReactionSettings().FrequencyHz;
	FNetReaction FromA;
	FromA.SetLastShotSpeed(20.0);
	CHECK(FromA.OnNetContact(+0.0325, 0.0, 0.4) == 0);
	FromA.Advance(QuarterPeriod);
	CHECK(FromA.Displacement(0.0, 0.4) < 0.0);

	FNetReaction FromB;
	FromB.SetLastShotSpeed(20.0);
	FromB.OnNetContact(-0.0325, 0.0, 0.4);
	FromB.Advance(QuarterPeriod);
	CHECK(FromB.Displacement(0.0, 0.4) > 0.0);
}

TEST_CASE("Net.Reaction.PocketIsAtTheHitAndScalesWithSpeed")
{
	const double QuarterPeriod = 0.25 / FNetReactionSettings().FrequencyHz;
	const FNetReaction Hard = HitAndWait(-2.0, 0.45, 25.0, -1.0, QuarterPeriod);
	double PeakY = 0.0;
	const double HardPeak = PeakOffset(Hard, &PeakY);
	CHECK(PeakY == Approx(-2.0).epsilon(0.05));
	CHECK(std::abs(Hard.Displacement(-2.0, 0.45)) > 3.0 * std::abs(Hard.Displacement(-2.0 + 1.2, 0.45)));

	const FNetReaction Soft = HitAndWait(-2.0, 0.45, 8.0, -1.0, QuarterPeriod);
	CHECK(PeakOffset(Soft) < HardPeak);
}

TEST_CASE("Net.Reaction.PostsAndGroundStayPut")
{
	FNetReaction Net;
	Net.AddImpact(HalfWidth - 0.1, 0.5, 30.0, -1.0, false);
	Net.AddImpact(0.0, 0.05, 30.0, 1.0, false);
	for (int Step = 0; Step < 120; ++Step)
	{
		Net.Advance(1.0 / 120.0);
		// The posts, caps, winch and lacing sit outside the inset and never move.
		const double Inset = HalfWidth - FNetReactionSettings().PostInsetM;
		// (Y is accumulated, so start a hair past the inset to keep rounding on the static side.)
		for (double Z = 0.0; Z <= Padel::Court::NetPostHeight; Z += 0.05)
		{
			for (double Y = Inset + 1e-9; Y <= HalfWidth + 0.2; Y += 0.02)
			{
				CHECK(Net.Displacement(Y, Z) == 0.0);
				CHECK(Net.Displacement(-Y, Z) == 0.0);
			}
		}
		CHECK(Net.Displacement(0.0, 0.0) == 0.0);
	}
}

TEST_CASE("Net.Reaction.TapeMovesLessThanTheMesh")
{
	// The tape sits on a tight steel cable; the mesh below it gives way more.
	const double QuarterPeriod = 0.25 / FNetReactionSettings().FrequencyHz;
	const double Top = NetHeightAt(0.0);
	FNetReaction Net;
	Net.AddImpact(0.0, Top - 0.25, 25.0, -1.0, false);
	Net.Advance(QuarterPeriod);
	const double Mesh = std::abs(Net.Displacement(0.0, Top - 0.25));
	const double Tape = std::abs(Net.Displacement(0.0, Top));
	CHECK(Tape < 0.5 * Mesh);
	CHECK(Tape > 0.0);
}

TEST_CASE("Net.Reaction.TapeClipIsSmallerThanAShotIntoTheNet")
{
	const double QuarterPeriod = 0.25 / FNetReactionSettings().FrequencyHz;
	FNetReaction Into;
	Into.SetLastShotSpeed(18.0);
	Into.OnNetContact(0.0325, 1.0, 0.7);
	Into.Advance(QuarterPeriod);
	FNetReaction Clip;
	Clip.SetLastShotSpeed(18.0);
	Clip.OnNetContact(0.0, 1.0, NetHeightAt(1.0));
	Clip.Advance(QuarterPeriod);
	CHECK(Clip.GetImpacts()[0].IsActive());
	CHECK(std::abs(Clip.GetImpacts()[0].Amplitude) == Approx(std::abs(Into.GetImpacts()[0].Amplitude) * FNetReactionSettings().TapeClipScale));
}

TEST_CASE("Net.Reaction.RippleRunsAlongTheNet")
{
	// Far from the hit, nothing moves at first; the shiver arrives later along the cable.
	FNetReaction Net;
	Net.AddImpact(-3.0, 0.5, 25.0, -1.0, false);
	Net.Advance(0.02);
	const double Early = std::abs(Net.Displacement(-0.5, 0.5));
	Net.Advance(0.34);
	double Later = 0.0;
	FNetReaction Probe = Net;
	for (int Step = 0; Step < 6; ++Step)
	{
		Later = std::max(Later, std::abs(Probe.Displacement(-0.5, 0.5)));
		Probe.Advance(0.01);
	}
	CHECK(Early < 1e-4);
	CHECK(Later > 10.0 * Early);
}

TEST_CASE("Net.Reaction.SettlesAndFreesItsSlot")
{
	const FNetReactionSettings Settings;
	FNetReaction Net;
	Net.AddImpact(0.5, 0.5, 30.0, 1.0, false);
	// Fades out smoothly: no pop on the last frame.
	Net.Advance(Settings.DurationSeconds - 0.01);
	CHECK(PeakOffset(Net) < 0.002);
	Net.Advance(0.02);
	CHECK_FALSE(Net.IsActive());
	CHECK(PeakOffset(Net) == 0.0);
}

TEST_CASE("Net.Reaction.IsCappedAndIgnoresSoftTouches")
{
	const FNetReactionSettings Settings;
	FNetReaction Net;
	CHECK(Net.AddImpact(0.0, 0.5, Settings.MinSpeedMps * 0.5, 1.0, false) == -1);
	CHECK_FALSE(Net.IsActive());
	Net.AddImpact(0.0, 0.5, 400.0, 1.0, false);
	Net.AddImpact(0.05, 0.5, 400.0, 1.0, false);
	CHECK(std::abs(Net.GetImpacts()[0].Amplitude) == Approx(Settings.MaxDisplacementM));
	for (int Step = 0; Step < 60; ++Step)
	{
		Net.Advance(1.0 / 120.0);
		CHECK(PeakOffset(Net) <= Settings.MaxDisplacementM + 1e-12);
	}
}

TEST_CASE("Net.Reaction.ThirdHitReplacesTheOldest")
{
	FNetReaction Net;
	CHECK(Net.AddImpact(-1.0, 0.5, 20.0, 1.0, false) == 0);
	Net.Advance(0.3);
	CHECK(Net.AddImpact(1.0, 0.5, 20.0, 1.0, false) == 1);
	Net.Advance(0.1);
	CHECK(Net.AddImpact(2.0, 0.5, 20.0, -1.0, false) == 0);
	CHECK(Net.GetImpacts()[0].Y == 2.0);
	CHECK(Net.GetImpacts()[0].Age == 0.0);
	CHECK(Net.GetImpacts()[1].Y == 1.0);
}

TEST_CASE("Net.Reaction.ShaderParamsAreInUnrealUnits")
{
	FNetReaction Net;
	Net.AddImpact(1.5, 0.4, 20.0, -1.0, false);
	Net.Advance(0.25);
	const FNetShaderParams Params = Net.PackShaderParams();
	CHECK(Params.Impacts[0][0] == Approx(150.0));
	CHECK(Params.Impacts[0][1] == Approx(40.0));
	CHECK(Params.Impacts[0][2] == Approx(-20.0 * FNetReactionSettings().DisplacementPerMps * 100.0));
	CHECK(Params.Impacts[0][3] == Approx(0.25));
	CHECK(Params.Impacts[1][2] == 0.0);
	CHECK(Params.Net[0] == Approx(500.0));
	CHECK(Params.Net[1] == Approx(88.0));
	CHECK(Params.Net[2] == Approx(92.0));
}

TEST_CASE("Net.Reaction.DataFileMatchesTheDefaults")
{
	std::ifstream File(PADEL_REPO_ROOT "/Data/Net.json");
	REQUIRE(File.good());
	std::stringstream Text;
	Text << File.rdbuf();
	FNetReactionSettings Loaded;
	Loaded.DurationSeconds = -1.0;
	std::string Error;
	REQUIRE_MESSAGE(ParseNetReactionSettings(Text.str(), Loaded, Error), Error);
	const FNetReactionSettings Defaults;
	CHECK(Loaded.DisplacementPerMps == Defaults.DisplacementPerMps);
	CHECK(Loaded.MaxDisplacementM == Defaults.MaxDisplacementM);
	CHECK(Loaded.MinSpeedMps == Defaults.MinSpeedMps);
	CHECK(Loaded.TapeClipScale == Defaults.TapeClipScale);
	CHECK(Loaded.BulgeRadiusM == Defaults.BulgeRadiusM);
	CHECK(Loaded.WaveSpeedMps == Defaults.WaveSpeedMps);
	CHECK(Loaded.WaveRadiusM == Defaults.WaveRadiusM);
	CHECK(Loaded.RippleShare == Defaults.RippleShare);
	CHECK(Loaded.FrequencyHz == Defaults.FrequencyHz);
	CHECK(Loaded.DampingPerSecond == Defaults.DampingPerSecond);
	CHECK(Loaded.DurationSeconds == Defaults.DurationSeconds);
	CHECK(Loaded.TapeShare == Defaults.TapeShare);
	CHECK(Loaded.PostInsetM == Defaults.PostInsetM);
	CHECK(Loaded.PostFadeM == Defaults.PostFadeM);
	CHECK(Loaded.GroundFadeM == Defaults.GroundFadeM);
	CHECK(Loaded.CableFadeM == Defaults.CableFadeM);
	CHECK(Loaded.DefaultSpeedMps == Defaults.DefaultSpeedMps);

	FNetReactionSettings Untouched;
	CHECK_FALSE(ParseNetReactionSettings("{\"Other\": {}}", Untouched, Error));
	CHECK(Untouched.DurationSeconds == Defaults.DurationSeconds);
}
