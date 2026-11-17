// Four players in a real match: the AI pairs (FAiTeamController) decide, and every
// player, AI or human, moves and swings through the shot system's own controller
// (FPlayerShotController, GDD 8: the AI plays through the same shot model as a human),
// against FMatchSimulation's ball and referee. Engine-free and deterministic, so the
// soak test runs it headless and the game runs the same code each fixed tick.
//
// Player ids are the match's: 0 and 1 are Team A, 2 and 3 Team B. A team's AI slot
// 0 (right) is the player the score has on the right side.
#pragma once

#include "PadelCoreApi.h"
#include "AI/AiEngineBallModel.h"
#include "AI/AiMatchBallPort.h"
#include "AI/AiTeamController.h"
#include "MatchSimulation.h"
#include "Shots/PlayerShotController.h"

#include <array>
#include <optional>

namespace Padel::AI
{
	struct FAiMatchSetup
	{
		// The player rows, by player id. The difficulty's attribute offset is applied here.
		std::array<Shots::FPlayerRatings, 4> Players;
		// Per team (A, B).
		std::array<const FAiDifficulty*, 2> Difficulties = { nullptr, nullptr };
		// A player the game drives with SetHumanInput, or -1 for four AI players.
		int HumanPlayerId = -1;
		uint64_t Seed = 0;
		// Start the next point by itself after PointGapS (the soak test; a game mode
		// may want to call BeginPoint itself, after a replay).
		bool bAutoBeginPoints = true;
	};

	class PADELCORE_API FAiMatchRunner : public IMatchEventListener
	{
	public:
		FAiMatchRunner(FMatchSimulation& InMatch, const Shots::FShotLibrary& InLibrary, const Shots::FMovementTuning& InMovement,
			const FAiTactics& InTactics, const FAiMatchSetup& InSetup);
		~FAiMatchRunner() override;
		FAiMatchRunner(const FAiMatchRunner&) = delete;
		FAiMatchRunner& operator=(const FAiMatchRunner&) = delete;

		// Starts the match and its first point if the match hasn't started.
		void Start();
		// One fixed 120 Hz tick: the AI's decisions, the four swing controllers, the
		// serve toss, then the match itself.
		void Tick();

		// The human's stick and buttons for the next tick.
		void SetHumanInput(const Shots::FPlayerInputFrame& Input) { HumanInput = Input; }
		// GDD 7 partner commands, from the human to their AI partner.
		bool GiveCommand(EPartnerCommand Command);
		// GDD 3.1 golden point with the human's pair receiving: the next point waits
		// until the game passes the player the human picked.
		bool IsWaitingForGoldenReceiver() const;
		bool ChooseGoldenPointReceiver(int PlayerId);

		const Shots::FPlayerShotController& Player(int PlayerId) const { return *Controllers[PlayerId]; }
		// The ball as the players see it: the serve toss while the match still holds the
		// ball, the match ball otherwise. What the game should draw.
		const Shots::IBallPort& Ball() const { return Port; }
		// What each player's swing did in the last tick.
		const Shots::FSwingEvents& LastEvents(int PlayerId) const { return Events[PlayerId]; }
		// True when that player's hit in the last tick was taken by the match.
		bool HitAccepted(int PlayerId) const { return bAccepted[PlayerId]; }
		// The AI has chosen this player's shot and waits for the swing controller to agree on the moment.
		bool HasPendingShot(int PlayerId) const { return Pending[PlayerId].has_value(); }
		const FAiTeamController& Team(ETeam Team) const { return *Teams[TeamIndex(Team)]; }
		int SlotOf(int PlayerId) const;
		// GDD 4: whether this AI player is set to play the ball off the glass, and which wall.
		FAiGlassRead GlassRead(int PlayerId) const;
		bool IsAi(int PlayerId) const { return PlayerId != Setup.HumanPlayerId; }

		// IMatchEventListener
		void OnServeStarted(int ServerId) override;
		void OnBallBounced(Court::ESurface Surface, Court::EHalf Half, const FVec3& Position) override;
		void OnPointEnded(ETeam WinningTeam, EPointReason Reason, int LastHitterId) override;
		void OnChangeOfEnds() override;
		void OnGoldenPointStarted(ETeam ReceivingTeam) override;

	private:
		int PlayerIdOf(int TeamIdx, int Slot) const { return Slot == RightSlot ? RightPlayer[TeamIdx] : PartnerOf(RightPlayer[TeamIdx]); }
		void BuildControllers(const std::array<FVec3, 4>& Positions);
		void SetUpServe();
		void ThinkAi(std::array<Shots::FPlayerInputFrame, 4>& Inputs);
		void ServeAi(std::array<Shots::FPlayerInputFrame, 4>& Inputs);
		void AfterHit(int HitterId);
		// The AI picks targets by tactics; the shot must still fly. A hard flat drive
		// from a low ball can't reach the net players' feet, so push the target deeper,
		// then take pace off, until the solved flight clears the tape.
		FAiShotDecision AimOverNet(const FAiVec& Contact, int HitterHalf, const FAiShotDecision& Decision) const;
		bool ClearsNet(const FVec3& Origin, const FVec3& Target, double Speed, const FVec3& SpinLocal) const;
		FAiWorldView ViewFor(int TeamIdx) const;

		FMatchSimulation& Match;
		const Shots::FShotLibrary& Library;
		const Shots::FMovementTuning& Movement;
		// A copy, with the stand points read from the shot library (GDD 5.2).
		FAiTactics Tactics;
		FAiMatchSetup Setup;
		FAiEngineBallModel Model;
		FAiMatchBallPort Port;
		// Ratings as played, after the difficulty offset.
		std::array<Shots::FPlayerRatings, 4> Ratings;
		std::array<std::optional<Shots::FPlayerShotController>, 4> Controllers;
		std::array<std::optional<FAiTeamController>, 2> Teams;
		std::array<int, 2> RightPlayer = { 0, 2 };
		std::array<Shots::FSwingEvents, 4> Events;
		// The AI's shot waits here until the swing controller has picked the same
		// contact moment as the AI's plan (it picks its own from the ball's path, and a
		// ball that passes the player twice, before and after the glass, has two).
		struct FPendingShot
		{
			FAiShotDecision Shot;
			double ContactTime = 0.0;
		};
		std::array<std::optional<FPendingShot>, 4> Pending;
		void ReleasePending();
		double BallDiesAt() const;
		// The AI picks a timing grade from its difficulty (GDD 8) as an offset in its own
		// GDD 5.1 base windows; the shot system grades in that shot's windows, widened or
		// narrowed by the player's rating. Map the offset band to band, so the grade the
		// difficulty asked for is the grade the player gets.
		double RealTimingOffset(int PlayerId, const FAiShotDecision& Shot) const;
		void ClearShots(int PlayerId);
		std::array<bool, 4> bAccepted = { false, false, false, false };
		Shots::FPlayerInputFrame HumanInput;

		// Match events are handled at the start of the next tick, outside the match's
		// own call stack (a serve fault arrives in the middle of a swing).
		bool bServePending = false;
		bool bEndsPending = false;
		int GoldenReceivingTeam = -1;
		// The AI server's toss is due at this match time; negative once tossed.
		double TossAt = -1.0;
		double PointEndedAt = -1.0;
		int EndsChanges = 0;
	};
}
