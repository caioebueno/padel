// GDD 8 shot choice: a weighted, seeded choice of shot and target from the
// situation, plus the timing the AI releases with (GDD 5.1). The shot system turns
// the decision into a real shot with the same contact model as a human's, so the
// AI's errors are emergent (GDD 5).

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiPlayerProfile.h"
#include "AI/AiRandom.h"

#include <vector>

namespace Padel::AI
{
	struct FShotContext
	{
		int HalfSign = 1;
		// Where the ball will be met, in metres.
		FAiVec Contact;
		EContactKind Kind = EContactKind::Groundstroke;
		bool bOffGlass = false;
		// Time to spare on the interception (FInterceptPlan::Slack).
		double Slack = 0.0;
		FAiPlayerProfile Profile;
		// Opponents' positions in metres.
		std::vector<FAiVec> Opponents;
	};

	struct FShotOption
	{
		EAiShot Shot = EAiShot::Drive;
		double Weight = 0.0;
	};

	// How the opponents stand, as GDD 8's shot choice reads it.
	enum class EOpponentShape : int
	{
		BothBack,
		Split,
		BothNet,
	};

	namespace ShotChooser
	{
		PADELCORE_API EOpponentShape ReadOpponents(const FShotContext& Context, const FAiTactics& Tactics);

		// The weighted options for a rally shot, before the pick. Exposed for tests.
		PADELCORE_API std::vector<FShotOption> Options(const FShotContext& Context, const FAiTactics& Tactics, const FAiDifficulty& Difficulty);

		// The intended landing point for a shot, before the aim scatter.
		PADELCORE_API FAiVec IntendedTarget(EAiShot Shot, const FShotContext& Context, const FAiTactics& Tactics, const FAiDifficulty& Difficulty,
			FPadelRng& Rng);

		PADELCORE_API FAiShotDecision ChooseRallyShot(const FShotContext& Context, const FAiTactics& Tactics, const FAiDifficulty& Difficulty,
			FPadelRng& Rng);

		// GDD 3.2: diagonally into the receiver's box. Server is where the server stands.
		PADELCORE_API FAiShotDecision ChooseServe(int HalfSign, const FAiVec& Server, const FAiTactics& Tactics, const FAiDifficulty& Difficulty,
			FPadelRng& Rng);

		// GDD 5.1 timing, from the difficulty's grade distribution.
		PADELCORE_API void SampleTiming(const FAiTactics& Tactics, const FAiDifficulty& Difficulty, FPadelRng& Rng, EAiTimingGrade& OutGrade,
			double& OutOffsetS);
	}
}
