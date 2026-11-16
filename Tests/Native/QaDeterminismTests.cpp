// QA (Agent 8), test plan P-12: the same seeded AI-vs-AI match produces the same ball,
// bit for bit, on every platform CI builds (Linux/GCC and macOS/Clang today, Windows/MSVC
// later). Same-process replay is covered by Ai.Soak; this one compares against a value
// recorded once, so a different compiler or maths library that changes even the last bit
// of a bounce shows up here. Replays, the soak and future online play depend on it.
//
// If this fails on one platform only, the simulation isn't portable: look for libm calls
// (std::sin, std::exp, std::pow...) or floating-point contraction (FMA) differing between
// compilers. If it fails everywhere after an intended change to the simulation, tuning or
// AI, re-record the checksum with the printed value and say so in the PR.
#include "doctest.h"

#include "AI/AiMatchRunner.h"
#include "ShotTestData.h"
#include "Support/AiTestData.h"
#include "TestSupport.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
	const std::array<std::string, Padel::NumPlayers> SliceRows = { "selva_arunde", "mikko_ventara", "ilan_corvisa", "ruben_halvaro" };

	// FNV-1a over the exact bit patterns, so any difference in any bit changes the hash.
	struct FHasher
	{
		std::uint64_t Hash = 1469598103934665603ULL;

		void Bytes(const void* Data, std::size_t Size)
		{
			const auto* P = static_cast<const unsigned char*>(Data);
			for (std::size_t I = 0; I < Size; ++I)
			{
				Hash ^= P[I];
				Hash *= 1099511628211ULL;
			}
		}
		void Int(long long Value) { Bytes(&Value, sizeof(Value)); }
		void Double(double Value)
		{
			// +0.0 and -0.0 compare equal but hash differently; fold them together.
			if (Value == 0.0)
			{
				Value = 0.0;
			}
			std::uint64_t Bits = 0;
			std::memcpy(&Bits, &Value, sizeof(Bits));
			Int(static_cast<long long>(Bits));
		}
		void Vec(const Padel::FVec3& V)
		{
			Double(V.X);
			Double(V.Y);
			Double(V.Z);
		}
	};

	struct FRecorder : Padel::IMatchEventListener
	{
		FHasher Hasher;
		int Points = 0;
		int Bounces = 0;

		void OnBallHit(int PlayerId, Padel::EShotType Shot, Padel::ETimingGrade Grade, double Speed, const Padel::FVec3& Spin) override
		{
			Hasher.Int(PlayerId);
			Hasher.Int(static_cast<long long>(Shot));
			Hasher.Int(static_cast<long long>(Grade));
			Hasher.Double(Speed);
			Hasher.Vec(Spin);
		}
		void OnBallBounced(Padel::Court::ESurface Surface, Padel::Court::EHalf Half, const Padel::FVec3& Position) override
		{
			++Bounces;
			Hasher.Int(static_cast<long long>(Surface));
			Hasher.Int(static_cast<long long>(Half));
			Hasher.Vec(Position);
		}
		void OnPointEnded(Padel::ETeam Winner, Padel::EPointReason Reason, int LastHitter) override
		{
			++Points;
			Hasher.Int(static_cast<long long>(Winner));
			Hasher.Int(static_cast<long long>(Reason));
			Hasher.Int(LastHitter);
		}
	};

	constexpr int PointsToPlay = 12;
	constexpr std::uint64_t Seed = 20261116;
	// Recorded on Linux/GCC 13 from main at 8d98356 plus PadelMath. See the comment at the top before changing it.
	constexpr std::uint64_t ExpectedChecksum = 0xb4cd9a662dfaf193ULL;
}

TEST_CASE("QA.Determinism: a seeded AI match gives the same checksum on every platform (TP P-12)")
{
	using namespace Padel;
	using namespace Padel::AI;

	const Shots::FShotLibrary Library = ShotTestData::LoadLibrary();
	const Shots::FMovementTuning Movement = ShotTestData::LoadMovement();
	FAiMatchSetup Setup;
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		Setup.Players[static_cast<std::size_t>(Id)] = ShotTestData::LoadPlayer(SliceRows[static_cast<std::size_t>(Id)]);
	}
	const FAiDifficulty& Medium = TestData::Difficulty("Medium");
	Setup.Difficulties = { &Medium, &Medium };
	Setup.Seed = Seed;

	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), Setup.Seed);
	FRecorder Recorder;
	Match.AddListener(&Recorder);
	FAiMatchRunner Runner(Match, Library, Movement, TestData::Tactics(), Setup);
	Runner.Start();

	const long long MaxTicks = static_cast<long long>(SimTickRate) * 60 * 30;
	for (long long Tick = 0; Tick < MaxTicks && Recorder.Points < PointsToPlay; ++Tick)
	{
		Runner.Tick();
	}
	Match.RemoveListener(&Recorder);

	REQUIRE(Recorder.Points == PointsToPlay);
	CHECK(Recorder.Bounces > PointsToPlay);
	std::printf("[qa] P-12 checksum 0x%016llxULL after %d points, %d bounces\n",
		static_cast<unsigned long long>(Recorder.Hasher.Hash), Recorder.Points, Recorder.Bounces);
	CHECK(Recorder.Hasher.Hash == ExpectedChecksum);
}
