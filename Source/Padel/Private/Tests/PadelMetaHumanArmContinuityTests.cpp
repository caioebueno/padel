// How continuously the racket arm moves through a swing (the MetaHuman arm-jump fix): each of
// the game's swing arcs (as Padel.Animation.SwingClearance), from every stance that carries the
// racket, at the clip's own timing at 60 Hz, records the largest turn of any arm bone of the
// shown body in a frame. The running half is in Padel.Animation.MotionMatching (gated there).
//
// Recorded, not gated yet. Before the fix the worst swing turned an arm bone 179.5 deg in a
// frame; after it, 48 to 62 deg (right-handers) and 166 deg (the left-hander's backhand). What's
// left is a design gap, not a flip in the IK: with no swing clips imported, the warp plans every
// swing around the locomotion clip's hand angle (an idle hand hanging the racket down), so mid-
// windup it must swing the hand across the shoulder or turn the racket half over in a few frames.
// The follow-up gives the warp its own wrist and racket angle through a swing
// (docs/animation/MOVEMENT_FLUENCY.md); then this gates at 20 deg.
#include "Misc/AutomationTest.h"

#include "PadelContactWarpTestSupport.h"

#include "Animation/Model/PlayerAnimModel.h"
#include "Animation/PadelAnimData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Misc/ScopeExit.h"
#include "UI/Model/PadelUiMatchSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMetaHumanArmContinuityTest, "Padel.Animation.MetaHumanArmContinuity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMetaHumanArmContinuityTest::RunTest(const FString& Parameters)
{
	using namespace PadelContactWarpTests;
	using namespace Padel::Animation;
	using Padel::EShotType;
	constexpr double GoalTurnDeg = 20.0; // the gate once the wrist/racket follow-up lands
	constexpr double Dt = 1.0 / 60.0;
	const FPadelAnimData Data = FPadelAnimData::Load();
	if (!TestTrue(TEXT("Animation data loads"), Data.bOk))
	{
		return false;
	}
	struct FSwing { EShotType Shot; ESwingSide Side; FTarget Contact; };
	const FSwing Swings[] = {
		{ EShotType::Drive, ESwingSide::Fore, { 0.6, 0.4, 1.0 } },
		{ EShotType::Drive, ESwingSide::Back, { 0.55, 0.4, 1.0 } },
		{ EShotType::Chiquita, ESwingSide::Fore, { 0.55, 0.45, 0.45 } },
		{ EShotType::Chiquita, ESwingSide::Back, { 0.5, 0.45, 0.45 } },
		{ EShotType::Smash, ESwingSide::Fore, { 0.25, 0.45, 2.45 } },
		{ EShotType::Bandeja, ESwingSide::Fore, { 0.3, 0.4, 2.2 } },
	};
	const TCHAR* const ArmBones[] = { TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("upperarm_l"), TEXT("upperarm_r"),
		TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("hand_l"), TEXT("hand_r") };

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	APadelMatchDirector* Director = World->SpawnActor<APadelMatchDirector>();
	Padel::Ui::FMatchSetup Setup;
	Setup.SetsToWin = 1;
	Setup.HumanPlayerId = -1;
	if (!TestNotNull(TEXT("Director"), Director) || !TestTrue(TEXT("Match started"), Director->StartMatch(Setup)))
	{
		return false;
	}
	ON_SCOPE_EXIT { Director->CloseMatch(); };
	for (int32 Tick = 0; Tick < 10; ++Tick)
	{
		Director->Tick(1.0f / 60.0f);
	}

	int32 Checked = 0;
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		UStaticMeshComponent* Racket = Pawn && Pawn->GetAnimation()->HasSkeletalBody() ? FindRacket(Pawn) : nullptr;
		USkeletalMeshComponent* Shown = Racket ? Cast<USkeletalMeshComponent>(Racket->GetAttachParent()) : nullptr;
		if (!Shown)
		{
			continue;
		}
		++Checked;
		const TCHAR* BodyName = Pawn->GetAnimation()->GetMetaHuman() ? TEXT("MetaHuman") : TEXT("placeholder");
		TArray<int32> Bones;
		for (const TCHAR* Name : ArmBones)
		{
			const int32 Index = Shown->GetBoneIndex(FName(Name));
			if (Index != INDEX_NONE)
			{
				Bones.Add(Index);
			}
		}
		double Worst = 0.0;
		FString WorstAt;
		for (int32 StanceIndex = 0; StanceIndex < NumStances; ++StanceIndex)
		{
			const FStancePose& Stance = Data.Settings.Stances[static_cast<std::size_t>(StanceIndex)];
			if (Stance.ArmAlpha <= 0.0)
			{
				continue;
			}
			for (const FSwing& Swing : Swings)
			{
				const FAnimClip* Clip = Data.Clips.Find(Swing.Shot, Swing.Side, ESwingVariant::Perfect);
				if (!TestNotNull(TEXT("Clip"), Clip) || Clip->EndS <= Clip->ContactS || Clip->ContactS <= Clip->WindupS || Clip->WindupS <= 0.0)
				{
					continue;
				}
				// The model's keys (PlayerAnimModel StepRacket): ready, backswing, contact, follow-through.
				const double RightSign = Swing.Side == ESwingSide::Fore ? 1.0 : -1.0;
				const FPlayerFrameVec Ready = Stance.Racket;
				const FPlayerFrameVec Back{ Clip->Backswing.RightM * RightSign, Clip->Backswing.ForwardM, Clip->Backswing.UpM };
				const FPlayerFrameVec Contact{ Swing.Contact.RightM * RightSign, Swing.Contact.ForwardM, Swing.Contact.UpM };
				const FPlayerFrameVec Follow{ Clip->Follow.RightM * RightSign, Clip->Follow.ForwardM, Clip->Follow.UpM };
				auto LerpKey = [](const FPlayerFrameVec& A, const FPlayerFrameVec& B, double T)
				{
					return FPlayerFrameVec{ A.RightM + (B.RightM - A.RightM) * T, A.ForwardM + (B.ForwardM - A.ForwardM) * T, A.UpM + (B.UpM - A.UpM) * T };
				};
				TArray<FQuat> Last;
				const FString Name = FString::Printf(TEXT("%s from %s"), UTF8_TO_TCHAR(Clip->Name.c_str()), UTF8_TO_TCHAR(StanceName(static_cast<EStance>(StanceIndex))));
				for (double T = 0.0; T <= Clip->EndS + 1e-6; T += Dt)
				{
					// Swing progress at the clip's timing: 0..1 windup, 1..2 forward swing, 2..3 follow-through.
					const double P = T <= Clip->WindupS ? T / Clip->WindupS
						: T <= Clip->ContactS ? 1.0 + (T - Clip->WindupS) / (Clip->ContactS - Clip->WindupS)
						: 2.0 + (T - Clip->ContactS) / (Clip->EndS - Clip->ContactS);
					const FPlayerFrameVec Key = P <= 1.0 ? LerpKey(Ready, Back, P) : (P <= 2.0 ? LerpKey(Back, Contact, P - 1.0) : LerpKey(Contact, Follow, P - 2.0));
					PoseAt(Pawn, Around(Pawn, Key.RightM, Key.ForwardM, Key.UpM));
					const TArray<FTransform>& CS = Shown->GetComponentSpaceTransforms();
					TArray<FQuat> Now;
					for (const int32 Bone : Bones)
					{
						Now.Add(CS.IsValidIndex(Bone) ? CS[Bone].GetRotation() : FQuat::Identity);
					}
					for (int32 B = 0; B < Last.Num(); ++B)
					{
						const double Deg = FMath::RadiansToDegrees(Now[B].AngularDistance(Last[B]));
						if (Deg > Worst)
						{
							Worst = Deg;
							WorstAt = FString::Printf(TEXT("%s, %s at progress %.2f"), *Shown->GetBoneName(Bones[B]).ToString(), *Name, P);
						}
					}
					Last = MoveTemp(Now);
				}
			}
		}
		AddInfo(FString::Printf(TEXT("Pawn %d (%s): largest arm bone turn in a frame through the swings %.1f deg (%s)"), Id, BodyName, Worst, *WorstAt));
		if (Worst >= GoalTurnDeg)
		{
			AddInfo(FString::Printf(TEXT("Pawn %d (%s): over the %.0f deg goal (not gated until the wrist/racket follow-up)"), Id, BodyName, GoalTurnDeg));
		}
	}
	if (Checked == 0)
	{
		AddInfo(TEXT("No skeletal bodies on this machine; skipped."));
	}
	return true;
}

#endif
