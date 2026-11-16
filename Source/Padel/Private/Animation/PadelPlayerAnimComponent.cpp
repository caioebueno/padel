#include "Animation/PadelPlayerAnimComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/PadelAnimData.h"
#include "Animation/PadelMetaHumanAnimInstance.h"
#include "Components/LODSyncComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/PadelShotUnits.h"
#include "Engine/World.h"
#include "PadelLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Retargeter/IKRetargeter.h"

namespace PadelAnimComponentDetail
{
	Padel::Animation::FRacketSettings LoadRacketSettings()
	{
		Padel::Animation::FRacketSettings Settings;
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("Racket.json"));
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogPadel, Warning, TEXT("No %s: using the default racket and grip."), *Path);
			return Settings;
		}
		std::string Error;
		if (!Padel::Animation::ParseRacketSettings(TCHAR_TO_UTF8(*Text), Settings, Error))
		{
			UE_LOG(LogPadel, Warning, TEXT("%s: %s. Using the default racket and grip."), *Path, UTF8_TO_TCHAR(Error.c_str()));
		}
		return Settings;
	}

	// A bone's transform in the skeleton's reference pose, in component space.
	FTransform RefPoseComponentSpace(const FReferenceSkeleton& Skeleton, int32 BoneIndex)
	{
		const TArray<FTransform>& Pose = Skeleton.GetRefBonePose();
		FTransform Result = FTransform::Identity;
		for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = Skeleton.GetParentIndex(Index))
		{
			Result = Result * Pose[Index];
		}
		return Result;
	}
}

UPadelPlayerAnimComponent::UPadelPlayerAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Out of line so TUniquePtr sees the complete FPadelAnimData.
UPadelPlayerAnimComponent::~UPadelPlayerAnimComponent() = default;

void UPadelPlayerAnimComponent::Init(const Padel::Shots::FShotLibrary* Shots, int32 TeamIndex, FName PlayerKey, USkeletalMeshComponent* InBody, USceneComponent* InRacket)
{
	RemoveMetaHuman();
	Body = InBody;
	Racket = InRacket;
	LoadRacket();
	Data = MakeUnique<FPadelAnimData>(FPadelAnimData::Load());
	if (!Data->bOk)
	{
		UE_LOG(LogPadel, Error, TEXT("Player animation disabled: the animation data did not load (see the errors above)."));
		Model.Reset();
		return;
	}
	Model = MakeUnique<Padel::Animation::FPlayerAnimModel>(&Data->Settings, &Data->Clips, Shots);

	// The skeletal body is used only when both the mesh and the AnimBP exist; until the
	// asset pipeline and the clips have been imported the placeholder shapes stay.
	bSkeletalBody = false;
	if (Body && Data->TeamMeshes.IsValidIndex(TeamIndex))
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Data->TeamMeshes[TeamIndex], nullptr, LOAD_Quiet | LOAD_NoWarn);
		UClass* AnimClass = LoadObject<UClass>(nullptr, *Data->AnimClass, nullptr, LOAD_Quiet | LOAD_NoWarn);
		if (Mesh && AnimClass && AnimClass->IsChildOf(UPadelPlayerAnimInstance::StaticClass()))
		{
			Body->SetSkeletalMeshAsset(Mesh);
			Body->SetAnimInstanceClass(AnimClass);
			Body->SetVisibility(true);
			bSkeletalBody = true;
			if (Racket && !Data->RacketBone.IsNone())
			{
				Racket->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Data->RacketBone);
			}
		}
		else
		{
			UE_LOG(LogPadel, Log, TEXT("Player animation: %s or %s not imported yet, using the placeholder body."),
				*Data->TeamMeshes[TeamIndex], *Data->AnimClass);
			Body->SetVisibility(false);
		}
	}
	if (UPadelPlayerAnimInstance* Anim = bSkeletalBody ? Cast<UPadelPlayerAnimInstance>(Body->GetAnimInstance()) : nullptr)
	{
		Anim->TrajectoryTimes.Reset();
		for (const double T : Data->Settings.TrajectoryTimesS)
		{
			Anim->TrajectoryTimes.Add(static_cast<float>(T));
		}
	}
	// In the palm, not on the wrist's pivot (0008). A MetaHuman shown below takes it over.
	if (bSkeletalBody)
	{
		AttachRacket(Body);
	}
	// The MetaHuman copies the placeholder skeleton's pose, so it needs that skeleton running.
	if (bSkeletalBody && !PlayerKey.IsNone())
	{
		ShowMetaHuman(PlayerKey);
	}
}

bool UPadelPlayerAnimComponent::ShowMetaHuman(FName PlayerKey)
{
	const FPadelPlayerBodyRow* Row = Data->PlayerBodies.Find(PlayerKey);
	if (!Row)
	{
		return false;
	}
	UClass* Class = LoadObject<UClass>(nullptr, *Row->MetaHumanClass, nullptr, LOAD_Quiet | LOAD_NoWarn);
	UIKRetargeter* Retargeter = LoadObject<UIKRetargeter>(nullptr, *Data->MetaHumanRetargeter, nullptr, LOAD_Quiet | LOAD_NoWarn);
	UWorld* World = GetWorld();
	if (!Class || !Class->IsChildOf(AActor::StaticClass()) || !Retargeter || !World)
	{
		// MetaHuman assets live outside git (0006), so a fresh clone lands here: keep the placeholder.
		UE_LOG(LogPadel, Log, TEXT("Player %s: MetaHuman %s or retargeter %s not on disk, using the placeholder body."),
			*PlayerKey.ToString(), *Row->MetaHumanClass, *Data->MetaHumanRetargeter);
		return false;
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MetaHuman = World->SpawnActor<AActor>(Class, Body->GetComponentTransform(), Params);
	if (!MetaHuman)
	{
		UE_LOG(LogPadel, Warning, TEXT("Player %s: could not spawn %s."), *PlayerKey.ToString(), *Row->MetaHumanClass);
		return false;
	}
	// Same origin and facing as the placeholder skeleton (both stand on their origin facing +Y).
	MetaHuman->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	USkeletalMeshComponent* MetaBody = nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(MetaHuman);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() == FName(*Row->BodyComponent))
		{
			MetaBody = Mesh;
		}
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// 60 fps on an M2 (0006): LOD 0 isn't visible from the broadcast camera.
		Mesh->OverrideMinLOD(Data->MetaHumanMinLod);
	}
	if (ULODSyncComponent* LodSync = MetaHuman->FindComponentByClass<ULODSyncComponent>())
	{
		LodSync->MinLOD = Data->MetaHumanMinLod;
	}
	if (!MetaBody)
	{
		UE_LOG(LogPadel, Warning, TEXT("Player %s: %s has no skeletal mesh component named %s, using the placeholder body."),
			*PlayerKey.ToString(), *Row->MetaHumanClass, *Row->BodyComponent);
		RemoveMetaHuman();
		return false;
	}

	MetaBody->SetAnimInstanceClass(UPadelMetaHumanAnimInstance::StaticClass());
	UPadelMetaHumanAnimInstance* Anim = Cast<UPadelMetaHumanAnimInstance>(MetaBody->GetAnimInstance());
	if (!Anim)
	{
		UE_LOG(LogPadel, Warning, TEXT("Player %s: the MetaHuman body did not take the retarget anim instance."), *PlayerKey.ToString());
		RemoveMetaHuman();
		return false;
	}
	Anim->Configure(Body, Retargeter, Data->MetaHumanIkLodThreshold);
	// The retarget reads the placeholder's pose of this frame, so that pose must be ready first.
	MetaBody->AddTickPrerequisiteComponent(Body);

	// The placeholder keeps animating while hidden: it is the pose the MetaHuman copies.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->SetVisibility(false);
	AttachRacket(MetaBody);
	UE_LOG(LogPadel, Log, TEXT("Player %s: showing MetaHuman %s."), *PlayerKey.ToString(), *Row->MetaHumanClass);
	return true;
}

void UPadelPlayerAnimComponent::LoadRacket()
{
	RacketSettings = PadelAnimComponentDetail::LoadRacketSettings();
	bImportedRacket = false;
	UStaticMeshComponent* RacketMesh = Cast<UStaticMeshComponent>(Racket);
	if (!RacketMesh)
	{
		return;
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, UTF8_TO_TCHAR(RacketSettings.Mesh.c_str()), nullptr, LOAD_Quiet | LOAD_NoWarn);
	if (!Mesh)
	{
		UE_LOG(LogPadel, Log, TEXT("%s isn't imported yet: keeping the placeholder racket. Run Tools/AssetPipeline/pipeline.py all."),
			UTF8_TO_TCHAR(RacketSettings.Mesh.c_str()));
		return;
	}
	// Built to size by the pipeline, the grip centre on its origin, the handle up +Z and a
	// face looking along +Y. Until it's in a hand, turn that face towards the net (+X here).
	RacketMesh->SetStaticMesh(Mesh);
	RacketMesh->SetRelativeScale3D(FVector::OneVector);
	RacketMesh->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
	bImportedRacket = true;
}

void UPadelPlayerAnimComponent::AttachRacket(USkeletalMeshComponent* To)
{
	if (!Racket || !To || !Data || Data->RacketBone.IsNone())
	{
		return;
	}
	Racket->AttachToComponent(To, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Data->RacketBone);
	// The placeholder skeleton is hidden under a MetaHuman; the racket must not inherit that.
	Racket->SetVisibility(true);
	Racket->SetHiddenInGame(false);
	const USkeletalMesh* Mesh = To->GetSkeletalMeshAsset();
	if (!bImportedRacket || !Mesh)
	{
		// The placeholder box stays centred on the bone, as before.
		return;
	}

	// Solve the grip once from the reference pose (RacketGrip.h); the hand bone then carries it.
	using namespace PadelAnimComponentDetail;
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const int32 HandIndex = Skeleton.FindBoneIndex(Data->RacketBone);
	const int32 ForearmIndex = Skeleton.FindBoneIndex(FName(UTF8_TO_TCHAR(RacketSettings.ForearmBone.c_str())));
	if (HandIndex == INDEX_NONE)
	{
		UE_LOG(LogPadel, Warning, TEXT("%s has no bone %s: the racket sits on the body's origin."), *Mesh->GetName(), *Data->RacketBone.ToString());
		return;
	}
	const int32 KnuckleIndex = Skeleton.FindBoneIndex(FName(UTF8_TO_TCHAR(RacketSettings.KnuckleBone.c_str())));
	const FTransform HandCS = RefPoseComponentSpace(Skeleton, HandIndex);
	const int32 ParentIndex = Skeleton.GetParentIndex(HandIndex);
	const int32 ArmIndex = ForearmIndex != INDEX_NONE ? ForearmIndex : ParentIndex;

	Padel::Animation::FRacketGripInput In;
	In.Hand = Padel::ToMetres(HandCS.GetLocation());
	In.Forearm = ArmIndex != INDEX_NONE ? Padel::ToMetres(RefPoseComponentSpace(Skeleton, ArmIndex).GetLocation()) : In.Hand;
	if (KnuckleIndex != INDEX_NONE)
	{
		In.Knuckle = Padel::ToMetres(RefPoseComponentSpace(Skeleton, KnuckleIndex).GetLocation());
		In.bHasKnuckle = true;
	}
	const Padel::Animation::FRacketGripFrame Grip = Padel::Animation::SolveRacketGrip(In, RacketSettings);
	if (!Grip.bValid)
	{
		UE_LOG(LogPadel, Warning, TEXT("%s: the hand bones are degenerate, the racket sits on %s."), *Mesh->GetName(), *Data->RacketBone.ToString());
		return;
	}
	const auto Axis = [](const Padel::FVec3& V) { return FVector(V.X, V.Y, V.Z); };
	const FTransform GripCS(Axis(Grip.AxisX), Axis(Grip.AxisY), Axis(Grip.AxisZ), Padel::ToWorld(Grip.Origin));
	FTransform Relative = GripCS.GetRelativeTransform(HandCS);
	Relative.SetScale3D(FVector::OneVector);
	Racket->SetRelativeTransform(Relative);
	UE_LOG(LogPadel, Log, TEXT("Racket in %s's %s: grip %.1f cm from the bone (%s finger bones)."), *Mesh->GetName(),
		*Data->RacketBone.ToString(), Relative.GetLocation().Size(), In.bHasKnuckle ? TEXT("with") : TEXT("no"));
}

void UPadelPlayerAnimComponent::RemoveMetaHuman()
{
	if (MetaHuman)
	{
		MetaHuman->Destroy();
		MetaHuman = nullptr;
	}
}

void UPadelPlayerAnimComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	RemoveMetaHuman();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UPadelPlayerAnimComponent::Step(const Padel::Shots::FPlayerShotController& Controller, const Padel::Shots::FSwingEvents& Events, double Dt, bool bRallyLive)
{
	if (Model)
	{
		Model->Step(Padel::Animation::FPlayerAnimInput::FromController(Controller, Events, bRallyLive), Dt);
		bStepped = true;
	}
}

void UPadelPlayerAnimComponent::NotifyPointEnded(bool bWon, uint32 PointIndex)
{
	if (Model)
	{
		Model->NotifyPointEnded(bWon, PointIndex);
	}
}

const Padel::Animation::FPlayerAnimFrame* UPadelPlayerAnimComponent::GetFrame() const
{
	return Model ? &Model->GetFrame() : nullptr;
}

UAnimSequenceBase* UPadelPlayerAnimComponent::Resolve(const std::string& AssetPath)
{
	if (AssetPath.empty())
	{
		return nullptr;
	}
	const FString Path = UTF8_TO_TCHAR(AssetPath.c_str());
	if (const TObjectPtr<UAnimSequenceBase>* Found = Sequences.Find(Path))
	{
		return Found->Get();
	}
	// A missing clip is normal while the placeholders are in: remember it as null.
	UAnimSequenceBase* Sequence = LoadObject<UAnimSequenceBase>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
	Sequences.Add(Path, Sequence);
	return Sequence;
}

void UPadelPlayerAnimComponent::Present()
{
	if (!Model || !bStepped)
	{
		return;
	}
	const Padel::Animation::FPlayerAnimFrame& Frame = Model->GetFrame();
	if (bSkeletalBody)
	{
		if (UPadelPlayerAnimInstance* Anim = Cast<UPadelPlayerAnimInstance>(Body->GetAnimInstance()))
		{
			TArray<FVector> Trajectory;
			Trajectory.Reserve(Frame.TrajectoryCount);
			for (int32 Index = 0; Index < Frame.TrajectoryCount; ++Index)
			{
				Trajectory.Add(Padel::ToWorld(Frame.Trajectory[Index]));
			}
			Anim->ApplyFrame(Frame, Frame.Clip ? Resolve(Frame.Clip->Asset) : nullptr, Frame.PrevClip ? Resolve(Frame.PrevClip->Asset) : nullptr,
				Resolve(Frame.CelebrationAsset), Trajectory);
		}
		return;
	}
	if (Racket)
	{
		// Placeholder swing: the racket face follows the arc and meets the ball at contact.
		// SM_Racket's origin is its grip, so lift the face, not the handle, onto the arc.
		const FVector FaceOffset = bImportedRacket ? Racket->GetUpVector() * Padel::ToWorld(Padel::FVec3{ 0.0, 0.0, RacketSettings.FaceCentreM }).Z : FVector::ZeroVector;
		Racket->SetWorldLocation(Padel::ToWorld(Frame.RacketPosition) - FaceOffset);
	}
}
