// TEST-ONLY AI-vs-AI match harness for the soak run (GDD 12, item 2). It stands in for
// the match engine (Agent 2) and the shot system (Agent 3) with the smallest rules
// and shot model that let four AI players play real points: GDD 3.3 point endings,
// the serve box and faults (3.2), golden-point games, sets and tiebreaks (3.1).
// When the real engine lands, the soak test switches to it and this file goes.
#pragma once

#include "AI/AiTeamController.h"
#include "AI/AiTeamFrame.h"
#include "StandInBallModel.h"

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <cstdio>
#include <cstdlib>

namespace Padel::AI::Test
{
	struct FSoakStats
	{
		int Matches = 0;
		int Points = 0;
		int StuckPoints = 0;
		int InvariantFailures = 0;
		int Shots = 0;
		int Whiffs = 0;
		int PointsWithGlassShot = 0;
		std::map<std::string, int> Reasons;
		std::map<std::string, int> ShotCounts;

		void Add(const FSoakStats& O)
		{
			Matches += O.Matches;
			Points += O.Points;
			StuckPoints += O.StuckPoints;
			InvariantFailures += O.InvariantFailures;
			Shots += O.Shots;
			Whiffs += O.Whiffs;
			PointsWithGlassShot += O.PointsWithGlassShot;
			for (const auto& [K, V] : O.Reasons) Reasons[K] += V;
			for (const auto& [K, V] : O.ShotCounts) ShotCounts[K] += V;
		}
		double ShotsPerPoint() const { return Points > 0 ? static_cast<double>(Shots) / Points : 0.0; }
		double GlassShare() const { return Points > 0 ? static_cast<double>(PointsWithGlassShot) / Points : 0.0; }
	};

	struct FMatchOptions
	{
		int SetsToWin = 1;
		int GamesPerSet = 6;
		// A point is stuck when nobody has hit the ball for this long (a live rally
		// always has a shot every couple of seconds), or when it runs absurdly long.
		double MaxSecondsWithoutHit = 20.0;
		double MaxPointSeconds = 600.0;
	};

	class FStandInMatch
	{
	public:
		static constexpr double Dt = 1.0 / 120.0;  // CLAUDE.md: the sim steps at 120 Hz

		FStandInMatch(const FAiTactics& InTactics, const FAiDifficulty& DifficultyA, const FAiDifficulty& DifficultyB,
			uint64_t Seed, FMatchOptions InOptions = {})
			: Tactics(InTactics)
			, Options(InOptions)
			, Rng(Seed ^ 0xba11ba11ba11ba11ull)
		{
			const std::array<const FAiDifficulty*, 2> Difficulties = { &DifficultyA, &DifficultyB };
			for (int Team = 0; Team < 2; ++Team)
			{
				FAiTeamSetup Setup;
				Setup.HalfSign = Team == 0 ? 1 : -1;
				Setup.Profiles = { TestDataPlayer(ESide::Right), TestDataPlayer(ESide::Left) };
				Setup.Seed = Seed * 4 + static_cast<uint64_t>(Team) + 1;
				Teams[Team].emplace(Setup, Tactics, *Difficulties[Team], Model);
			}
		}

		int Winner = -1;

		FSoakStats Play()
		{
			Stats = {};
			Stats.Matches = 1;
			int ServingTeam = 0;
			std::array<int, 2> NextServerSlot = { RightSlot, RightSlot };
			while (Sets[0] < Options.SetsToWin && Sets[1] < Options.SetsToWin)
			{
				const bool bTiebreak = Games[0] == Options.GamesPerSet && Games[1] == Options.GamesPerSet;
				const int GameServer = ServingTeam;
				const int ServerSlot = NextServerSlot[GameServer];
				std::array<int, 2> Points = { 0, 0 };
				int PointIndex = 0;
				while (true)
				{
					int Server = GameServer;
					int Slot = ServerSlot;
					if (bTiebreak)
					{
						// GDD 3.2: first point by the next server, then two each in turn.
						const int Turn = (PointIndex + 1) / 2;
						Server = (GameServer + Turn) % 2;
						Slot = (ServerSlot + Turn / 2) % 2;
					}
					bool bFromRight = PointIndex % 2 == 0;
					const bool bGolden = !bTiebreak && Points[0] == 3 && Points[1] == 3;
					if (bGolden)
					{
						// GDD 3.1: the receiving pair picks its receiver, and so the side.
						bFromRight = Teams[1 - Server]->ChooseGoldenPointReceiver() == RightSlot;
					}
					const int PointWinner = PlayPoint(Server, Slot, bFromRight);
					if (PointWinner < 0)
					{
						Winner = -1;
						return Stats;  // stuck: abandon the match, the soak reports it
					}
					++Points[PointWinner];
					++PointIndex;
					// GDD 3.1: with golden point on, 40-40 is one deciding point, so the first
					// pair to 4 points wins the game. A tiebreak is first to 7, by 2.
					const int W = Points[0] > Points[1] ? 0 : 1;
					const bool bGameOver = bTiebreak ? Points[W] >= 7 && Points[W] - Points[1 - W] >= 2 : Points[W] >= 4;
					if (bGameOver)
					{
						++Games[W];
						break;
					}
				}
				NextServerSlot[GameServer] = 1 - NextServerSlot[GameServer];
				ServingTeam = 1 - ServingTeam;
				const int Lead = Games[0] > Games[1] ? 0 : 1;
				const bool bSetOver = (Games[Lead] >= Options.GamesPerSet && Games[Lead] - Games[1 - Lead] >= 2)
					|| Games[Lead] == Options.GamesPerSet + 1;
				if (bSetOver)
				{
					++Sets[Lead];
					Games = { 0, 0 };
				}
			}
			Winner = Sets[0] > Sets[1] ? 0 : 1;
			return Stats;
		}

	private:
		struct FPlayer
		{
			FAiVec Pos;
			FAiVec Vel;
		};

		struct FSwing
		{
			int Team = 0;
			int Slot = 0;
			FAiShotDecision Decision;
			double ContactTime = 0.0;
		};

		// --- Point state (the stand-in referee) ---
		int LastHitter = 0;
		int Bounces = 0;
		bool bServe = false;
		bool bNetTouched = false;
		bool bWallAfterBounce = false;
		int ServeFaults = 0;
		bool bServeLet = false;
		bool bServeFault = false;
		int PointWinnerTeam = -1;
		std::string Reason;
		bool bGlassShotThisPoint = false;
		int ReceiverSlot = 0;

		static FAiPlayerProfile TestDataPlayer(ESide Side)
		{
			FAiPlayerProfile P;
			P.Speed = 76; P.Acceleration = 78; P.Reach = 75; P.Jump = 70; P.WallPlay = 78;
			P.Anticipation = 77; P.Positioning = 79; P.Teamwork = 78; P.ReturnSkill = Side == ESide::Right ? 76 : 72;
			P.PreferredSide = Side;
			return P;
		}

		int HalfOfTeam(int Team) const { return Teams[Team]->GetHalfSign(); }

		void End(int WinnerTeam, const char* Why)
		{
			if (PointWinnerTeam >= 0)
			{
				++Stats.InvariantFailures;  // a point must end exactly once
				return;
			}
			PointWinnerTeam = WinnerTeam;
			Reason = Why;
		}

		// GDD 3.3 point endings, from the ball's contacts.
		void OnContact(const FAiContact& C)
		{
			const int Receiver = 1 - LastHitter;
			const bool bOnReceiverHalf = static_cast<int>(Court::HalfOf(C.Position.X)) == HalfOfTeam(Receiver);
			switch (C.Surface)
			{
			case Court::ESurface::Net:
				bNetTouched = true;
				break;
			case Court::ESurface::Ground:
				if (!bOnReceiverHalf)
				{
					if (Bounces >= 1)
					{
						End(LastHitter, "BALL_CAME_BACK");  // GDD 3.3 special case, slice: hitter wins
					}
					else
					{
						End(Receiver, bNetTouched ? "INTO_NET" : "OWN_SIDE");
					}
					return;
				}
				if (!Court::IsInsideCourt(C.Position.X, C.Position.Y))
				{
					End(Bounces >= 1 ? LastHitter : Receiver, Bounces >= 1 ? "OUT_OF_COURT_WINNER" : "OUT_DIRECT");
					return;
				}
				++Bounces;
				if (bServe && Bounces == 1)
				{
					// GDD 3.2: diagonal, inside the receiver's service box.
					const FAiVec2 Local = TeamFrame::ToLocal(HalfOfTeam(Receiver), C.Position);
					const bool bRightBox = ReceiverSlot == RightSlot ? Local.X >= -Court::LineWidth : Local.X <= Court::LineWidth;
					const bool bInBox = Local.Y <= Court::ServiceLineFromNet + Court::LineWidth && bRightBox;
					if (!bInBox)
					{
						bServeFault = true;
					}
					else if (bNetTouched)
					{
						bServeLet = true;
					}
				}
				if (Bounces >= 2)
				{
					End(LastHitter, "DOUBLE_BOUNCE");
				}
				break;
			case Court::ESurface::Glass:
			case Court::ESurface::Mesh:
				if (!bOnReceiverHalf)
				{
					break;  // own wall: allowed, it must still come over (GDD 3.3 contrapared)
				}
				if (Bounces == 0)
				{
					if (bServe)
					{
						bServeFault = true;
					}
					else
					{
						End(Receiver, "WALL_BEFORE_BOUNCE");
					}
					return;
				}
				if (bServe && C.Surface == Court::ESurface::Mesh && !bWallAfterBounce)
				{
					bServeFault = true;  // GDD 3.2: mesh first after the bounce
				}
				bWallAfterBounce = true;
				break;
			case Court::ESurface::Ceiling:
				End(Bounces >= 1 ? LastHitter : Receiver, Bounces >= 1 ? "OUT_OF_COURT_WINNER" : "OUT_DIRECT");
				break;
			default:
				break;
			}
		}

		// Stand-in shot model: GDD 4 speeds and a GDD 5 style scatter by timing grade.
		void Launch(const FAiVec& From, const FAiShotDecision& D)
		{
			double Speed = 20.0;
			double Error = 0.6;
			double Apex = 0.0;
			switch (D.Shot)
			{
			case EAiShot::Drive: Speed = 21.0; Error = 0.7; break;
			case EAiShot::Volley: Speed = 15.0; Error = 0.6; break;
			case EAiShot::Lob: Speed = 12.0; Error = 0.9; Apex = 6.5; break;
			case EAiShot::Bandeja: Speed = 17.0; Error = 0.7; break;
			case EAiShot::Vibora: Speed = 22.0; Error = 0.9; break;
			case EAiShot::Smash: Speed = 30.0; Error = 1.0; break;
			case EAiShot::Chiquita: Speed = 8.5; Error = 0.5; break;
			case EAiShot::ServeFlat:
			case EAiShot::ServeKick: Speed = 15.0; Error = 0.5; break;
			}
			double Mult = 1.0;
			switch (D.Grade)
			{
			case EAiTimingGrade::Perfect: Mult = 0.5; Speed *= 1.05; break;
			case EAiTimingGrade::Good: Mult = 1.0; break;
			case EAiTimingGrade::Early:
			case EAiTimingGrade::Late: Mult = 1.8; break;
			case EAiTimingGrade::Miss: Mult = 3.5; Speed *= 0.5; Apex = 0.0; break;
			}
			Speed *= 0.75 + 0.35 * D.Charge;
			const double Radius = Error * Mult * (1.0 - 0.7 * 0.78) * std::sqrt(Rng.NextDouble());
			const double Angle = Rng.Range(0.0, 6.283185307179586);
			FAiVec Target = D.Target + FAiVec{ std::cos(Angle) * Radius, std::sin(Angle) * Radius, 0.0 };
			Target.Z = 0.0;

			FAiVec Delta = Target - From;
			const double Distance = std::max(0.5, Delta.Length2D());
			const double G = FStandInBallModel::Gravity;
			double Flight = 0.0;
			double Vz = 0.0;
			if (Apex > From.Z)
			{
				Vz = std::sqrt(2.0 * G * (Apex - From.Z));
				Flight = (Vz + std::sqrt(Vz * Vz + 2.0 * G * From.Z)) / G;
			}
			else
			{
				// Flatter shots: slow down until the arc clears the net.
				double Horizontal = Speed;
				for (int Try = 0; Try < 20; ++Try)
				{
					Flight = Distance / Horizontal;
					Vz = (0.5 * G * Flight * Flight - From.Z) / Flight;
					const double CrossT = std::abs(From.X) / (std::abs(Delta.X) / Flight + 1e-9);
					const double CrossZ = From.Z + Vz * CrossT - 0.5 * G * CrossT * CrossT;
					const double CrossY = From.Y + Delta.Y * (CrossT / Flight);
					if (CrossT > Flight || CrossZ > Court::NetHeightAt(CrossY) + 0.15)
					{
						break;
					}
					Horizontal *= 0.9;
				}
			}
			Ball.Velocity = { Delta.X / Flight, Delta.Y / Flight, Vz };
		}

		int PlayPoint(int ServerTeam, int ServerSlot, bool bFromRight)
		{
			const int Receiver = 1 - ServerTeam;
			ReceiverSlot = bFromRight ? RightSlot : LeftSlot;
			PointWinnerTeam = -1;
			Reason.clear();
			bGlassShotThisPoint = false;
			ServeFaults = 0;
			int ShotsThisPoint = 0;

			while (true)  // serves, until a point ends (lets and first faults replay)
			{
				bServeLet = false;
				bServeFault = false;
				for (int Team = 0; Team < 2; ++Team)
				{
					Teams[Team]->OnPointStarted(Team == ServerTeam);
					const int H = HalfOfTeam(Team);
					for (int Slot = 0; Slot < 2; ++Slot)
					{
						const double Side = Slot == RightSlot ? 1.0 : -1.0;
						FAiVec2 Local = { Side * 2.2, Tactics.BackDepthM };
						if (Team == ServerTeam && Slot == ServerSlot)
						{
							Local = { (bFromRight ? 1.0 : -1.0) * 1.5, 7.6 };
						}
						else if (Team == ServerTeam)
						{
							Local = { Side * 2.2, Tactics.NetDepthM + 0.5 };
						}
						else if (Slot == ReceiverSlot)
						{
							Local = { Side * 2.5, 9.0 };
						}
						Players[Team][Slot] = { TeamFrame::ToCourt(H, Local), {} };
					}
				}
				Time += 1.0;  // between points
				const FAiVec ServerPos = Players[ServerTeam][ServerSlot].Pos;
				const FAiShotDecision Serve = Teams[ServerTeam]->ChooseServe(ServerPos);
				Ball = {};
				Ball.Position = ServerPos + FAiVec{ -HalfOfTeam(ServerTeam) * 0.3, 0.0, 0.8 };
				Launch(Ball.Position, Serve);
				LastHitter = ServerTeam;
				Bounces = 0;
				bServe = true;
				bNetTouched = false;
				bWallAfterBounce = false;
				Swing.reset();
				++ShotsThisPoint;
				++Stats.ShotCounts[AiShotName(Serve.Shot)];
				for (int Team = 0; Team < 2; ++Team)
				{
					Teams[Team]->OnBallHit(Time, HalfOfTeam(ServerTeam), Serve.Shot, Ball.Position, Serve.Target);
				}

				const double Start = Time;
				LastHitTime = Time;
				while (PointWinnerTeam < 0 && !bServeLet && !bServeFault)
				{
					if (Time - Start > Options.MaxPointSeconds || Time - LastHitTime > Options.MaxSecondsWithoutHit)
					{
						++Stats.StuckPoints;
						if (std::getenv("PADEL_SOAK_DEBUG"))
						{
							std::printf("stuck ball=(%.2f,%.2f,%.2f) v=(%.2f,%.2f,%.2f) last=%d bounces=%d serve=%d shots=%d swing=%d\n",
								Ball.Position.X, Ball.Position.Y, Ball.Position.Z, Ball.Velocity.X, Ball.Velocity.Y, Ball.Velocity.Z,
								LastHitter, Bounces, bServe, ShotsThisPoint, Swing.has_value());
						}
						Stats.Shots += ShotsThisPoint;
						++Stats.Points;
						return -1;
					}
					Tick(Receiver, ShotsThisPoint);
				}
				if (bServeLet && PointWinnerTeam < 0)
				{
					continue;
				}
				if (bServeFault && PointWinnerTeam < 0)
				{
					if (++ServeFaults >= 2)
					{
						End(Receiver, "DOUBLE_FAULT");
					}
					else
					{
						continue;
					}
				}
				break;
			}
			for (int Team = 0; Team < 2; ++Team)
			{
				Teams[Team]->OnPointEnded();
			}
			++Stats.Points;
			Stats.Shots += ShotsThisPoint;
			Stats.PointsWithGlassShot += bGlassShotThisPoint ? 1 : 0;
			if (PointWinnerTeam < 0 || Reason.empty())
			{
				++Stats.InvariantFailures;
				return -1;
			}
			++Stats.Reasons[Reason];
			return PointWinnerTeam;
		}

		void Tick(int ServeReceiver, int& ShotsThisPoint)
		{
			Time += Dt;
			for (const FAiContact& C : Model.StepWith(Ball, Dt, &Rng))
			{
				OnContact(C);
				for (int Team = 0; Team < 2; ++Team)
				{
					Teams[Team]->OnBallContact(Time, C.Surface);
				}
				if (PointWinnerTeam >= 0 || bServeFault || bServeLet)
				{
					return;
				}
			}
			const int Receiver = 1 - LastHitter;
			if (std::abs(Ball.Position.X) > Court::HalfLength + 0.2 || std::abs(Ball.Position.Y) > Court::HalfWidth + 0.2)
			{
				if (bServe && Bounces >= 1)
				{
					End(LastHitter, "OUT_OF_COURT_WINNER");  // a serve can be a winner too
				}
				else
				{
					End(Bounces >= 1 ? LastHitter : Receiver, Bounces >= 1 ? "OUT_OF_COURT_WINNER" : "OUT_DIRECT");
				}
				return;
			}
			if (Model.IsDead(Ball))
			{
				const bool bOnReceiver = static_cast<int>(Court::HalfOf(Ball.Position.X)) == HalfOfTeam(Receiver);
				End(bOnReceiver && Bounces >= 1 ? LastHitter : Receiver, bOnReceiver && Bounces >= 1 ? "DOUBLE_BOUNCE" : "OWN_SIDE");
				return;
			}
			for (int Team = 0; Team < 2; ++Team)
			{
				FAiWorldView View;
				View.Time = Time;
				View.Ball = Ball;
				View.bMustBounce = bServe && Team == ServeReceiver;
				for (int Slot = 0; Slot < 2; ++Slot)
				{
					View.OwnPositions[Slot] = Players[Team][Slot].Pos;
					View.OwnVelocities[Slot] = Players[Team][Slot].Vel;
					View.OpponentPositions[Slot] = Players[1 - Team][Slot].Pos;
				}
				const std::array<std::optional<FAiIntent>, 2> Intents = Teams[Team]->Tick(View);
				for (int Slot = 0; Slot < 2; ++Slot)
				{
					if (!Intents[Slot])
					{
						continue;
					}
					MovePlayer(Team, Slot, *Intents[Slot]);
					if (Intents[Slot]->Shot)
					{
						Swing = FSwing{ Team, Slot, *Intents[Slot]->Shot, Intents[Slot]->ShotContactTime };
					}
				}
			}

			if (Swing && Time >= Swing->ContactTime)
			{
				TrySwing(*Swing, ShotsThisPoint);
				Swing.reset();
			}
		}

		void MovePlayer(int Team, int Slot, const FAiIntent& Intent)
		{
			FPlayer& P = Players[Team][Slot];
			const FAiPlayerProfile& Profile = Teams[Team]->GetProfile(Slot);
			const double Top = Profile.TopSpeed(Tactics) * (Intent.bSprint ? 1.15 : 1.0);  // GDD 6.3 sprint
			const FAiVec Desired = TeamFrame::CourtDirection(HalfOfTeam(Team), Intent.Move) * Top;
			const double MaxDelta = Profile.TopSpeed(Tactics) / Profile.AccelTime(Tactics) * Dt;
			FAiVec Change = Desired - P.Vel;
			const double Len = Change.Length2D();
			if (Len > MaxDelta)
			{
				Change = Change * (MaxDelta / Len);
			}
			P.Vel += Change;
			P.Pos += P.Vel * Dt;
			const int H = HalfOfTeam(Team);
			const double Depth = ClampD(TeamFrame::Depth(H, P.Pos), 0.3, Court::HalfLength - 0.2);
			P.Pos.X = Depth * H;
			P.Pos.Y = ClampD(P.Pos.Y, -Court::HalfWidth + 0.2, Court::HalfWidth - 0.2);
		}

		void TrySwing(const FSwing& S, int& ShotsThisPoint)
		{
			const FPlayer& P = Players[S.Team][S.Slot];
			const FAiPlayerProfile& Profile = Teams[S.Team]->GetProfile(S.Slot);
			const bool bOwnHalf = static_cast<int>(Court::HalfOf(Ball.Position.X)) == HalfOfTeam(S.Team);
			const bool bTheirBall = S.Team != LastHitter && bOwnHalf;
			const double Reach = Profile.SideReach(Tactics) + 0.35;
			const double Gap = (Ball.Position - P.Pos).Length2D();
			const bool bInReach = Gap <= Reach && Ball.Position.Z >= 0.1 && Ball.Position.Z <= Profile.MaxContactHeight(Tactics) + 0.1;
			if (!bTheirBall || !bInReach)
			{
				if (std::getenv("PADEL_SOAK_DEBUG"))
				{
					const auto& Plan = Teams[S.Team]->GetPlan(S.Slot);
					std::printf("whiff team=%d slot=%d their=%d gap=%.2f z=%.2f reach=%.2f slack=%.2f planZ=%.2f planPos=(%.2f,%.2f) ball=(%.2f,%.2f) player=(%.2f,%.2f) shot=%s\n",
						S.Team, S.Slot, bTheirBall, Gap, Ball.Position.Z, Reach, Plan ? Plan->Slack : -99.0, Plan ? Plan->Sample.Position.Z : -1.0,
						Plan ? Plan->Sample.Position.X : 0.0, Plan ? Plan->Sample.Position.Y : 0.0, Ball.Position.X, Ball.Position.Y, P.Pos.X, P.Pos.Y, AiShotName(S.Decision.Shot));
				}
				++Stats.Whiffs;
				return;
			}
			if (bServe && Bounces == 0)
			{
				End(LastHitter, "VOLLEYED_SERVE");
				return;
			}
			if (bWallAfterBounce)
			{
				bGlassShotThisPoint = true;
			}
			Launch(Ball.Position, S.Decision);
			LastHitter = S.Team;
			LastHitTime = Time;
			Bounces = 0;
			bServe = false;
			bNetTouched = false;
			bWallAfterBounce = false;
			++ShotsThisPoint;
			++Stats.ShotCounts[AiShotName(S.Decision.Shot)];
			for (int Team = 0; Team < 2; ++Team)
			{
				Teams[Team]->OnBallHit(Time, HalfOfTeam(S.Team), S.Decision.Shot, Ball.Position, S.Decision.Target);
			}
		}

		const FAiTactics& Tactics;
		FMatchOptions Options;
		FPadelRng Rng;
		FStandInBallModel Model;
		std::array<std::optional<FAiTeamController>, 2> Teams;
		std::array<std::array<FPlayer, 2>, 2> Players;
		FAiBall Ball;
		std::optional<FSwing> Swing;
		double Time = 0.0;
		double LastHitTime = 0.0;
		std::array<int, 2> Games = { 0, 0 };
		std::array<int, 2> Sets = { 0, 0 };
		FSoakStats Stats;
	};
}
