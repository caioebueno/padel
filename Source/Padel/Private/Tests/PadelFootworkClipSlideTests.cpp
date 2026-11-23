// Why the video footwork slides (docs/decisions/0012-video-footwork.md). Picked in the probe's
// shuffle at a matched speed (play rate about 1.05), the footwork slid about 2 m/s, though its
// source clips slide about 3 cm/s after the pipeline's foot IK. This plays a few footwork clips
// alone, the pawn moved by each clip's own root motion, and measures planted-foot slide (the
// probe's plant rule) at each stage:
//   source   the clip on the SMPL-derived source skeleton, sampled directly (before the retarget)
//   raw      the retargeted clip, sampled directly, root motion applied
//   graph    the same clip pinned on the motion matcher, through the pose graph, warps off
//   orient / stride / lock / all   each M2 locomotion warp on alone, then all of them
// and logs the clip's travel direction against its root facing and the pelvis's.
// Skipped without the footwork database (git-ignored, built by footwork_setup.py).
#include "Misc/AutomationTest.h"

#include "Animation/AnimSequence.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Animation/PadelPlayerPoseNode.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Misc/ScopeExit.h"
#include "PadelMotionProbe.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "UObject/SoftObjectPath.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelFootworkClipSlide
{
	constexpr double Dt = 1.0 / 60.0;
	const TCHAR* const FootworkDatabase = TEXT("/Game/Padel/Animations/MotionMatching/Footwork/PSD_Padel_Footwork.PSD_Padel_Footwork");
	const TCHAR* const ClipsDir = TEXT("/Game/Padel/Animations/MotionMatching/Footwork/Clips");
	const TCHAR* const SourceDir = TEXT("/Game/Characters/Footwork");
	// The two split steps the probe's shuffle search picked, then the first shuffles found.
	const TCHAR* const NamedClips[] = {
		TEXT("split_step-01_24m03s_end_6D1QT-F4RoI_nearL_split_step_01"),
		TEXT("split_step-04_8m15s_close_AXquIGI8-E0_nearL_mirror_split_step_04"),
	};
	constexpr int32 Shuffles = 2;

	/** A bone's component-space transform at Time, straight from the sequence (root motion included). */
	FTransform SampleComponentSpace(const UAnimSequence* Clip, int32 SkeletonBone, double Time)
	{
		const FReferenceSkeleton& Ref = Clip->GetSkeleton()->GetReferenceSkeleton();
		FTransform Out = FTransform::Identity;
		const FAnimExtractContext Context(Time);
		for (int32 Bone = SkeletonBone; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
		{
			FTransform Local;
			Clip->GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), Context, true);
			Out = Out * Local;
		}
		return Out;
	}

	struct FSampled
	{
		TArray<FVector> Feet[2];
		/** Root travel direction against the root's forward and against the pelvis's, degrees (median of |angle|). */
		double TravelVsRootDeg = 0.0;
		double TravelVsPelvisDeg = 0.0;
		double SpeedCmps = 0.0;
		double PelvisHeightCm = 0.0;
		bool bValid = false;
	};

	/** Samples a sequence at 60 Hz with no anim graph: the contact point (lower of ankle and ball) per foot. */
	FSampled SampleDirect(const UAnimSequence* Clip)
	{
		FSampled Out;
		const USkeleton* Skeleton = Clip ? Clip->GetSkeleton() : nullptr;
		if (!Skeleton)
		{
			return Out;
		}
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		const int32 Root = 0;
		const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
		const int32 Ankles[2] = { Ref.FindBoneIndex(TEXT("foot_l")), Ref.FindBoneIndex(TEXT("foot_r")) };
		const int32 Balls[2] = { Ref.FindBoneIndex(TEXT("ball_l")), Ref.FindBoneIndex(TEXT("ball_r")) };
		if (Pelvis == INDEX_NONE || Ankles[0] == INDEX_NONE || Ankles[1] == INDEX_NONE)
		{
			return Out;
		}
		// The placeholder and the mannequin-named source face +Y in component space; the pelvis's
		// own forward is found from its rest pose against the root's.
		const FVector PelvisRestForward = SampleComponentSpace(Clip, Pelvis, 0.0).GetRotation().UnrotateVector(
			SampleComponentSpace(Clip, Root, 0.0).GetRotation().RotateVector(FVector(0.0, 1.0, 0.0)));
		TArray<double> VsRoot, VsPelvis, Speeds, Heights;
		const double Length = Clip->GetPlayLength();
		FVector LastRoot = SampleComponentSpace(Clip, Root, 0.0).GetLocation();
		for (double T = 0.0; T <= Length; T += Dt)
		{
			for (int32 F = 0; F < 2; ++F)
			{
				const FVector Ankle = SampleComponentSpace(Clip, Ankles[F], T).GetLocation();
				const FVector Ball = Balls[F] != INDEX_NONE ? SampleComponentSpace(Clip, Balls[F], T).GetLocation() : Ankle;
				Out.Feet[F].Add(Ball.Z < Ankle.Z ? Ball : Ankle);
			}
			const FTransform RootT = SampleComponentSpace(Clip, Root, T);
			const FTransform PelvisT = SampleComponentSpace(Clip, Pelvis, T);
			Heights.Add(PelvisT.GetLocation().Z - RootT.GetLocation().Z);
			const FVector Travel = (RootT.GetLocation() - LastRoot) * FVector(1.0, 1.0, 0.0);
			LastRoot = RootT.GetLocation();
			if (T > 0.0 && Travel.Size() / Dt > 30.0)
			{
				const FVector RootForward = RootT.GetRotation().RotateVector(FVector(0.0, 1.0, 0.0)) * FVector(1.0, 1.0, 0.0);
				const FVector PelvisForward = PelvisT.GetRotation().RotateVector(PelvisRestForward) * FVector(1.0, 1.0, 0.0);
				auto Angle = [&Travel](const FVector& Forward)
				{
					return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Travel.GetSafeNormal(), Forward.GetSafeNormal()), -1.0, 1.0)));
				};
				VsRoot.Add(Angle(RootForward));
				VsPelvis.Add(Angle(PelvisForward));
				Speeds.Add(Travel.Size() / Dt);
			}
		}
		auto Median = [](TArray<double> V) { V.Sort(); return V.IsEmpty() ? 0.0 : V[V.Num() / 2]; };
		Out.TravelVsRootDeg = Median(VsRoot);
		Out.TravelVsPelvisDeg = Median(VsPelvis);
		Out.SpeedCmps = Median(Speeds);
		Out.PelvisHeightCm = Median(Heights);
		Out.bValid = true;
		return Out;
	}

	/** Which M2 locomotion warps are on. */
	enum class EWarps : uint8 { Off, Orientation, Stride, FootLock, All };
	const TCHAR* WarpsName(EWarps W)
	{
		switch (W)
		{
		case EWarps::Off: return TEXT("graph, warps off");
		case EWarps::Orientation: return TEXT("orientation warp only");
		case EWarps::Stride: return TEXT("stride warp only");
		case EWarps::FootLock: return TEXT("foot lock only");
		default: return TEXT("all warps");
		}
	}

	/**
	 * Plays Clip pinned on a pawn's motion matcher, the pawn moved by the clip's own root motion;
	 * returns the planted slide (cm/s) of the placeholder's feet in world space, or -1 when it can't run.
	 */
	double RunPinned(FAutomationTestBase& Test, const UAnimSequence* Clip, const UPoseSearchDatabase* Database, EWarps Warps)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);
		ON_SCOPE_EXIT
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		};
		APadelMatchDirector* Director = World->SpawnActor<APadelMatchDirector>();
		Padel::Ui::FMatchSetup Setup;
		Setup.SetsToWin = 1;
		Setup.HumanPlayerId = -1;
		if (!Director || !Director->StartMatch(Setup))
		{
			Test.AddError(TEXT("the match didn't start"));
			return -1.0;
		}
		ON_SCOPE_EXIT { Director->CloseMatch(); };
		Director->Tick(static_cast<float>(Dt));
		APadelPlayerPawn* Pawn = nullptr;
		USkeletalMeshComponent* Driver = nullptr;
		UPadelPlayerAnimInstance* Anim = nullptr;
		for (int32 Id = 0; Id < Padel::NumPlayers && !Anim; ++Id)
		{
			Pawn = Director->GetPawn(Id);
			Driver = Pawn && Pawn->GetAnimation()->HasSkeletalBody() ? Pawn->GetAnimation()->GetDriverBody() : nullptr;
			Anim = Driver ? Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance()) : nullptr;
		}
		if (!Anim || Anim->LocomotionSet.MotionMatchingDatabases.IsEmpty() || Driver->GetSkeletalMeshAsset()->GetSkeleton() != Clip->GetSkeleton())
		{
			return -1.0;
		}
		Anim->RawSwingWeight = 0.0f;
		Anim->ContactWarpWeight = 0.0f;
		Anim->ArmCarryAlpha = 0.0f;
		Anim->Body = FPadelBodyDynamics();
		Anim->Stance = FPadelBodyDynamics();
		FPadelLocomotionWarpTuning& Warp = Anim->LocomotionSet.LocomotionWarp;
		const FPadelLocomotionWarpTuning AllOn = Warp;
		Warp.OrientationMaxDeg = Warps == EWarps::Orientation || Warps == EWarps::All ? AllOn.OrientationMaxDeg : 0.0;
		Warp.StrideMin = Warps == EWarps::Stride || Warps == EWarps::All ? AllOn.StrideMin : 1.0;
		Warp.StrideMax = Warps == EWarps::Stride || Warps == EWarps::All ? AllOn.StrideMax : 1.0;
		Warp.FootLockHeightUnits = Warps == EWarps::FootLock || Warps == EWarps::All ? AllOn.FootLockHeightUnits : 0.0;

		const double Length = Clip->GetPlayLength();
		FVector Position = Pawn->GetActorLocation();
		double Yaw = Pawn->GetActorRotation().Yaw;
		TArray<FVector> Past;
		TArray<FVector> Feet[2];
		bool bPinned = false;
		for (double T = 0.0; T + Dt <= Length; T += Dt)
		{
			// The clip's own root motion moves the pawn (the pose plays it in place, root locked).
			const FTransform Step = Clip->ExtractRootMotionFromRange(T, T + Dt, FAnimExtractContext());
			const FTransform Component = Driver->GetComponentTransform();
			const FVector Move = Component.GetRotation().RotateVector(Step.GetTranslation()) * FVector(1.0, 1.0, 0.0);
			Position += Move;
			Yaw += Step.GetRotation().Rotator().Yaw;
			Pawn->SetActorLocationAndRotation(Position, FRotator(0.0, Yaw, 0.0));
			World->TimeSeconds += Dt;
			Past.Add(Position);
			const FVector Velocity = Move / Dt;
			TArray<FVector> Trajectory;
			for (const float Time : Anim->TrajectoryTimes)
			{
				if (Time < 0.0f)
				{
					const int32 Back = FMath::Min(Past.Num() - 1, FMath::RoundToInt(-Time / Dt));
					Trajectory.Add(Past[Past.Num() - 1 - Back]);
				}
				else
				{
					// The clip's own future path, as the search would have seen it.
					const double To = FMath::Min(T + Dt + Time, Length);
					const FVector Ahead = Component.GetRotation().RotateVector(Clip->ExtractRootMotionFromRange(T + Dt, To, FAnimExtractContext()).GetTranslation());
					Trajectory.Add(Position + Ahead * FVector(1.0, 1.0, 0.0));
				}
			}
			Padel::Animation::FPlayerAnimFrame Frame;
			Frame.SpeedMps = Velocity.Size2D() / 100.0;
			Frame.Locomotion = Padel::Animation::ELocomotionState::Ready;
			Frame.FacingYawDeg = Yaw;
			Frame.DirectionDeg = FMath::FindDeltaAngleDegrees(Yaw, FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y, Velocity.X)));
			Anim->ApplyFrame(Frame, nullptr, nullptr, nullptr, Trajectory);
			Anim->Body = FPadelBodyDynamics();
			Anim->Stance = FPadelBodyDynamics();
			if (!bPinned)
			{
				// After the first update has built the matcher's history and configuration.
				Driver->TickAnimation(static_cast<float>(Dt), false);
				Anim->GetPoseNodeForTest().GetMotionMatcherForTest().PinForTest(Clip, Database, T + Dt);
				bPinned = true;
			}
			Driver->TickAnimation(static_cast<float>(Dt), false);
			Driver->RefreshBoneTransforms();
			for (int32 F = 0; F < 2; ++F)
			{
				const FName Ankle = F == 0 ? FName(TEXT("foot_l")) : FName(TEXT("foot_r"));
				const FName Ball = F == 0 ? FName(TEXT("ball_l")) : FName(TEXT("ball_r"));
				const FVector A = Driver->GetSocketLocation(Ankle);
				const FVector B = Driver->GetBoneIndex(Ball) != INDEX_NONE ? Driver->GetSocketLocation(Ball) : A;
				Feet[F].Add(B.Z < A.Z ? B : A);
			}
		}
		if (Anim->GetPoseNode().GetMotionMatcher().CurrentClip() != Clip)
		{
			Test.AddWarning(FString::Printf(TEXT("%s: the pinned clip didn't stay playing"), *Clip->GetName()));
		}
		return PadelMotionProbe::PlantedSlideCmps(Feet, Dt);
	}

	const UAnimSequence* FindSource(const FString& Name)
	{
		for (const FString& Candidate : { Name, Name + TEXT("_Anim") })
		{
			const FSoftObjectPath Path(FString::Printf(TEXT("%s/%s.%s"), SourceDir, *Candidate, *Candidate));
			if (const UAnimSequence* Found = Cast<UAnimSequence>(Path.TryLoad()))
			{
				return Found;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelFootworkClipSlideTest, "Padel.Animation.FootworkClipSlide",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelFootworkClipSlideTest::RunTest(const FString& Parameters)
{
	using namespace PadelFootworkClipSlide;
	const UPoseSearchDatabase* Database = Cast<UPoseSearchDatabase>(FSoftObjectPath(FootworkDatabase).TryLoad());
	if (!Database)
	{
		AddInfo(TEXT("Skipped: no PSD_Padel_Footwork (Tools/AssetPipeline/unreal/footwork_setup.py)"));
		return true;
	}
	TArray<const UAnimSequence*> Clips;
	for (const TCHAR* Name : NamedClips)
	{
		const FSoftObjectPath Path(FString::Printf(TEXT("%s/%s.%s"), ClipsDir, Name, Name));
		if (const UAnimSequence* Clip = Cast<UAnimSequence>(Path.TryLoad()))
		{
			Clips.Add(Clip);
		}
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ ClipsDir });
	TArray<FAssetData> Found;
	Registry.GetAssetsByPath(FName(ClipsDir), Found);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	int32 ShufflesFound = 0;
	for (const FAssetData& Asset : Found)
	{
		if (ShufflesFound < Shuffles && Asset.AssetName.ToString().StartsWith(TEXT("shuffle-")))
		{
			if (const UAnimSequence* Clip = Cast<UAnimSequence>(Asset.GetAsset()))
			{
				Clips.Add(Clip);
				++ShufflesFound;
			}
		}
	}
	if (Clips.IsEmpty())
	{
		AddInfo(TEXT("Skipped: none of the footwork clips this test reads is built"));
		return true;
	}

	// The probe's steady-slide limit (Padel.Animation.MotionMatching).
	constexpr double SteadyLimitCmps = 25.0;
	for (const UAnimSequence* Clip : Clips)
	{
		const FString Name = Clip->GetName();
		const FSampled Raw = SampleDirect(Clip);
		const UAnimSequence* SourceClip = FindSource(Name);
		const FSampled Source = SourceClip ? SampleDirect(SourceClip) : FSampled();
		const double RawSlide = PadelMotionProbe::PlantedSlideCmps(Raw.Feet, Dt);
		const double SourceSlide = Source.bValid ? PadelMotionProbe::PlantedSlideCmps(Source.Feet, Dt) : -1.0;
		AddInfo(FString::Printf(TEXT("%s: %.1f s; root speed %.0f cm/s; travel vs root forward %.0f deg, vs pelvis %.0f deg; pelvis %.0f cm up (source %.0f)"),
			*Name, Clip->GetPlayLength(), Raw.SpeedCmps, Raw.TravelVsRootDeg, Raw.TravelVsPelvisDeg, Raw.PelvisHeightCm, Source.PelvisHeightCm));
		AddInfo(FString::Printf(TEXT("%s: slide cm/s: source %.1f; raw retargeted %.1f"), *Name, SourceSlide, RawSlide));
		TArray<FString> Graph;
		double GraphOff = -1.0;
		for (const EWarps W : { EWarps::Off, EWarps::Orientation, EWarps::Stride, EWarps::FootLock, EWarps::All })
		{
			const double Slide = RunPinned(*this, Clip, Database, W);
			GraphOff = W == EWarps::Off ? Slide : GraphOff;
			Graph.Add(FString::Printf(TEXT("%s %.1f"), WarpsName(W), Slide));
		}
		AddInfo(FString::Printf(TEXT("%s: slide cm/s: %s"), *Name, *FString::Join(Graph, TEXT("; "))));
		TestTrue(FString::Printf(TEXT("%s: the retargeted clip's planted feet slide under %.0f cm/s (%.1f)"), *Name, SteadyLimitCmps, RawSlide), RawSlide < SteadyLimitCmps);
		if (GraphOff >= 0.0)
		{
			TestTrue(FString::Printf(TEXT("%s: through the pose graph, warps off, under %.0f cm/s (%.1f)"), *Name, SteadyLimitCmps, GraphOff), GraphOff < SteadyLimitCmps);
		}
	}
	return true;
}

#endif
