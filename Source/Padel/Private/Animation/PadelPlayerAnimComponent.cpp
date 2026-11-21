#include "Animation/PadelPlayerAnimComponent.h"

#include "Animation/PadelContactWarp.h"

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
#include "AnimationRuntime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PadelLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
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

	// The skeletal body is used only when the mesh and something to animate it exist: the
	// AnimBP of 0005, or (0006) the C++ pose with its clips. Until then the shapes stay.
	// A MetaHuman player's hidden driver is always the first placeholder: its skeleton is
	// the one the clips and the MetaHuman retargeter are made for.
	bSkeletalBody = false;
	const bool bWantsMetaHuman = !PlayerKey.IsNone() && Data->PlayerBodies.Contains(PlayerKey);
	const int32 MeshIndex = bWantsMetaHuman ? 0 : TeamIndex;
	if (Body && Data->TeamMeshes.IsValidIndex(MeshIndex))
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Data->TeamMeshes[MeshIndex], nullptr, LOAD_Quiet | LOAD_NoWarn);
		UClass* AnimClass = LoadObject<UClass>(nullptr, *Data->AnimClass, nullptr, LOAD_Quiet | LOAD_NoWarn);
		const FPadelLocomotionSet Set = LoadLocomotionSet();
		if (Mesh && (!AnimClass || !AnimClass->IsChildOf(UPadelPlayerAnimInstance::StaticClass()))
			&& Set.IsValid() && Set.Idle->GetSkeleton() == Mesh->GetSkeleton())
		{
			AnimClass = UPadelPlayerAnimInstance::StaticClass();
		}
		if (Mesh && AnimClass && AnimClass->IsChildOf(UPadelPlayerAnimInstance::StaticClass()))
		{
			Body->SetSkeletalMeshAsset(Mesh);
			Body->SetAnimInstanceClass(AnimClass);
			Body->SetVisibility(true);
			bSkeletalBody = true;
			if (UPadelPlayerAnimInstance* Anim = Cast<UPadelPlayerAnimInstance>(Body->GetAnimInstance()))
			{
				Anim->LocomotionSet = Set;
				Anim->WarpTuning = Data->Warp;
			}
			if (Racket && !Data->RacketBone.IsNone())
			{
				Racket->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Data->RacketBone);
			}
		}
		else
		{
			UE_LOG(LogPadel, Log, TEXT("Player animation: %s, or %s and the locomotion clips, not imported yet: using the placeholder body."),
				*Data->TeamMeshes[MeshIndex], *Data->AnimClass);
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

FPadelLocomotionSet UPadelPlayerAnimComponent::LoadLocomotionSet() const
{
	auto Load = [](const FString& Path) -> UAnimSequenceBase*
	{
		return Path.IsEmpty() ? nullptr : LoadObject<UAnimSequenceBase>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
	};
	FPadelLocomotionSet Set;
	Set.Idle = Load(Data->IdleAsset);
	for (const FString& Path : Data->WalkAssets)
	{
		Set.Walk.Add(Load(Path));
	}
	for (const FString& Path : Data->JogAssets)
	{
		Set.Jog.Add(Load(Path));
	}
	Set.Ready = Load(Data->ReadyAsset);
	for (const FString& Path : Data->WalkUprightAssets)
	{
		Set.WalkUpright.Add(Load(Path));
	}
	Set.WalkClipSpeedMps = Data->WalkClipSpeedMps;
	Set.JogClipSpeedMps = Data->Settings.RunClipSpeedMps;
	Set.WalkMinMps = Data->Settings.WalkMinMps;
	Set.StrideRateMin = Data->Settings.StrideRateMin;
	Set.StrideRateMax = Data->Settings.StrideRateMax;
	// M1: motion matching where its databases are on disk and on the clips' skeleton (all or
	// none: a partial set would leave holes in the search); otherwise the loop mix above.
	const auto LoadDatabase = [&Set](const FString& Path) -> const UPoseSearchDatabase*
	{
		const UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
		const UPoseSearchSchema* Schema = Database ? Database->Schema.Get() : nullptr;
		return Schema && Set.Idle && Schema->GetSkeleton(Schema->GetDefaultRole()) == Set.Idle->GetSkeleton() ? Database : nullptr;
	};
	const auto RoleOf = [this](const FString& Path)
	{
		return Data->MotionMatchingReadyDatabases.Contains(Path) ? EPadelMotionMatchingRole::Ready
			: Data->MotionMatchingStillDatabases.Contains(Path) ? EPadelMotionMatchingRole::Still
			: Data->MotionMatchingStopDatabases.Contains(Path) ? EPadelMotionMatchingRole::Stop
			: Data->MotionMatchingTurnInPlaceDatabases.Contains(Path) ? EPadelMotionMatchingRole::TurnInPlace
			: Data->MotionMatchingWalkDatabases.Contains(Path) ? EPadelMotionMatchingRole::MovingWalk
			: Data->MotionMatchingSprintDatabases.Contains(Path) ? EPadelMotionMatchingRole::MovingSprint : EPadelMotionMatchingRole::Moving;
	};
	for (const FString& Path : Data->MotionMatchingDatabases)
	{
		const UPoseSearchDatabase* Database = LoadDatabase(Path);
		if (!Database)
		{
			Set.MotionMatchingDatabases.Reset();
			Set.MotionMatchingRoles.Reset();
			UE_LOG(LogPadel, Log, TEXT("Player animation: %s isn't built (Tools/AssetPipeline/unreal/animation_setup.py): locomotion uses the loop mix."), *Path);
			break;
		}
		Set.MotionMatchingDatabases.Add(Database);
		Set.MotionMatchingRoles.Add(RoleOf(Path));
	}
	// The extras (the video footwork, 0012) join a working set, one by one: each is optional.
	for (const FString& Path : Data->MotionMatchingExtraDatabases)
	{
		const UPoseSearchDatabase* Database = Set.MotionMatchingDatabases.IsEmpty() ? nullptr : LoadDatabase(Path);
		if (!Database)
		{
			UE_LOG(LogPadel, Log, TEXT("Player animation: extra database %s isn't built (Tools/AssetPipeline/unreal/footwork_setup.py): searching without it."), *Path);
			continue;
		}
		Set.MotionMatchingDatabases.Add(Database);
		Set.MotionMatchingRoles.Add(RoleOf(Path));
	}
	if (Set.MotionMatchingDatabases.IsEmpty())
	{
		Set.MotionMatchingRoles.Reset();
	}
	Set.MotionMatching = Data->MotionMatching;
	Set.MocapLegFadeFromMps = Data->MocapLegFadeFromMps;
	Set.MocapLegFadeToMps = Data->MocapLegFadeToMps;
	Set.MocapLegKeep = Data->MocapLegKeep;
	Set.MocapLeanKeep = Data->MocapLeanKeep;
	Set.MocapChestTwistKeep = Data->MocapChestTwistKeep;
	Set.MocapHeadMaxTwistDeg = Data->MocapHeadMaxTwistDeg;
	Set.LocomotionWarp = Data->LocomotionWarp;
	return Set;
}

bool UPadelPlayerAnimComponent::ShowMetaHuman(FName PlayerKey)
{
	const FPadelPlayerBodyRow* Row = Data->PlayerBodies.Find(PlayerKey);
	if (!Row)
	{
		return false;
	}
	UClass* Class = LoadObject<UClass>(nullptr, *Row->MetaHumanClass, nullptr, LOAD_Quiet | LOAD_NoWarn);
	FString ClassPath = Row->MetaHumanClass;
	if (!Class && !Data->MetaHumanFallbackClass.IsEmpty())
	{
		// Until every player has their own MetaHuman, one stands in for the rest, in their kit.
		Class = LoadObject<UClass>(nullptr, *Data->MetaHumanFallbackClass, nullptr, LOAD_Quiet | LOAD_NoWarn);
		ClassPath = Data->MetaHumanFallbackClass;
	}
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
	}
	// 60 fps on an M2 (0006): LOD 0 isn't visible from the broadcast camera. Only through the
	// MetaHuman's LOD Sync, which maps each part's LODs to the body's: forcing a min LOD on each
	// mesh on its own let the outfit pick a LOD whose bones the body (its leader pose) no longer
	// updates, and the shirt stretched into a long drape.
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
	// The MetaHuman warps its own arm after the retarget; the hidden driver doesn't.
	if (UPadelPlayerAnimInstance* DriverAnim = Cast<UPadelPlayerAnimInstance>(Body->GetAnimInstance()))
	{
		DriverAnim->bWarpThisBody = false;
	}
	ApplyKit(*Row);
	AttachShoes(MetaBody);
	UE_LOG(LogPadel, Log, TEXT("Player %s: showing MetaHuman %s."), *PlayerKey.ToString(), *ClassPath);
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
	const FName RacketBone = OnRacketSide(Data->RacketBone);
	Racket->AttachToComponent(To, FAttachmentTransformRules::SnapToTargetNotIncludingScale, RacketBone);
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
	const int32 HandIndex = Skeleton.FindBoneIndex(RacketBone);
	const int32 ForearmIndex = Skeleton.FindBoneIndex(OnRacketSide(FName(UTF8_TO_TCHAR(RacketSettings.ForearmBone.c_str()))));
	if (HandIndex == INDEX_NONE)
	{
		UE_LOG(LogPadel, Warning, TEXT("%s has no bone %s: the racket sits on the body's origin."), *Mesh->GetName(), *RacketBone.ToString());
		return;
	}
	const int32 KnuckleIndex = Skeleton.FindBoneIndex(OnRacketSide(FName(UTF8_TO_TCHAR(RacketSettings.KnuckleBone.c_str()))));
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
		UE_LOG(LogPadel, Warning, TEXT("%s: the hand bones are degenerate, the racket sits on %s."), *Mesh->GetName(), *RacketBone.ToString());
		return;
	}
	const auto Axis = [](const Padel::FVec3& V) { return FVector(V.X, V.Y, V.Z); };
	const FTransform GripCS(Axis(Grip.AxisX), Axis(Grip.AxisY), Axis(Grip.AxisZ), Padel::ToWorld(Grip.Origin));
	FTransform Relative = GripCS.GetRelativeTransform(HandCS);
	Relative.SetScale3D(FVector::OneVector);
	Racket->SetRelativeTransform(Relative);
	UE_LOG(LogPadel, Log, TEXT("Racket in %s's %s: grip %.1f cm from the bone (%s finger bones)."), *Mesh->GetName(),
		*RacketBone.ToString(), Relative.GetLocation().Size(), In.bHasKnuckle ? TEXT("with") : TEXT("no"));

	// The contact warp aims this face centre, on this body's arm, at the ball (PadelContactWarp.h).
	const FVector FaceInHand = PadelContactWarp::FaceInHand(Relative, Padel::ToWorld(Padel::FVec3{ 0.0, 0.0, RacketSettings.FaceCentreM }).Z);
	if (To == Body)
	{
		if (UPadelPlayerAnimInstance* Anim = Cast<UPadelPlayerAnimInstance>(Body->GetAnimInstance()))
		{
			Anim->RacketFaceInHand = FaceInHand;
		}
	}
	else if (UPadelMetaHumanAnimInstance* MetaAnim = Cast<UPadelMetaHumanAnimInstance>(To->GetAnimInstance()))
	{
		MetaAnim->SetRacketFace(FaceInHand, bLeftHanded);
	}
}

void UPadelPlayerAnimComponent::ApplyKit(const FPadelPlayerBodyRow& Row)
{
	// MetaHuman garments colour through these two vector parameters (the Creator's default
	// garment: slots ending in _Shirt and _Short). A shared MetaHuman gets each player's kit
	// through its own material instances; the asset itself is left alone.
	static const FName ColourParams[] = { FName(TEXT("diffuse_color_1")), FName(TEXT("diffuse_color_2")) };
	auto Colour = [](const FString& Hex, FLinearColor& Out)
	{
		if (Hex.IsEmpty())
		{
			return false;
		}
		Out = FLinearColor(FColor::FromHex(Hex));
		return true;
	};
	FLinearColor Shirt, Shorts;
	const bool bShirt = Colour(Row.ShirtColour, Shirt);
	const bool bShorts = Colour(Row.ShortsColour, Shorts);
	if (!MetaHuman || (!bShirt && !bShorts))
	{
		return;
	}
	TInlineComponentArray<USkinnedMeshComponent*> Meshes(MetaHuman);
	for (USkinnedMeshComponent* Mesh : Meshes)
	{
		const TArray<FName> Slots = Mesh->GetMaterialSlotNames();
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const FString Slot = Slots[Index].ToString();
			const bool bIsShirt = bShirt && Slot.EndsWith(TEXT("_Shirt"));
			const bool bIsShorts = bShorts && (Slot.EndsWith(TEXT("_Short")) || Slot.EndsWith(TEXT("_Shorts")));
			if (!bIsShirt && !bIsShorts)
			{
				continue;
			}
			if (UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(Index))
			{
				for (const FName& Param : ColourParams)
				{
					Mid->SetVectorParameterValue(Param, bIsShirt ? Shirt : Shorts);
				}
			}
		}
	}
}

void UPadelPlayerAnimComponent::AttachShoes(USkeletalMeshComponent* MetaBody)
{
	UStaticMesh* Shoe = Data->MetaHumanShoeMesh.IsEmpty() ? nullptr
		: LoadObject<UStaticMesh>(nullptr, *Data->MetaHumanShoeMesh, nullptr, LOAD_Quiet | LOAD_NoWarn);
	if (!Shoe || !MetaBody || !MetaBody->GetSkeletalMeshAsset())
	{
		return;
	}
	const FReferenceSkeleton& Ref = MetaBody->GetSkeletalMeshAsset()->GetRefSkeleton();
	for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
	{
		const FName FootBone(*FString::Printf(TEXT("foot_%s"), Side));
		const FName BallBone(*FString::Printf(TEXT("ball_%s"), Side));
		const int32 Foot = Ref.FindBoneIndex(FootBone);
		const int32 Ball = Ref.FindBoneIndex(BallBone);
		if (Foot == INDEX_NONE || Ball == INDEX_NONE)
		{
			continue;
		}
		// Place the shoe in the reference pose, where the MetaHuman stands on its origin: on the
		// ground under the ankle, pointing at the ball of the foot, sized to the foot. Then keep
		// that placement relative to the foot bone, whatever the bone's own axes are.
		const FTransform FootCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Foot);
		const FTransform BallCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Ball);
		FVector Forward = BallCS.GetLocation() - FootCS.GetLocation();
		Forward.Z = 0.0;
		const double Length = Forward.Size();
		if (Length < 1.0)
		{
			continue;
		}
		const double Scale = Length / (Data->ShoeAnkleToBallM * 100.0);
		const FTransform ShoeCS(FRotationMatrix::MakeFromXZ(Forward / Length, FVector::UpVector).ToQuat(),
			FVector(FootCS.GetLocation().X, FootCS.GetLocation().Y, 0.0), FVector(Scale));

		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(MetaHuman);
		Mesh->SetStaticMesh(Shoe);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetupAttachment(MetaBody, FootBone);
		Mesh->SetRelativeTransform(ShoeCS.GetRelativeTransform(FootCS));
		Mesh->RegisterComponent();
		// The bare toes would bend through the shoe on every push-off.
		MetaBody->HideBoneByName(BallBone, EPhysBodyOp::PBO_None);
	}
}

void UPadelPlayerAnimComponent::RemoveMetaHuman()
{
	if (UPadelPlayerAnimInstance* DriverAnim = Body ? Cast<UPadelPlayerAnimInstance>(Body->GetAnimInstance()) : nullptr)
	{
		DriverAnim->bWarpThisBody = true;
	}
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

void UPadelPlayerAnimComponent::Step(const Padel::Shots::FPlayerShotController& Controller, const Padel::Shots::FSwingEvents& Events, double Dt,
	const Padel::Animation::FPlayerAnimContext& Context)
{
	if (Model)
	{
		// Left-handers (Players.json Handedness) hold the racket and reach with the left arm.
		const bool bLeft = Controller.GetRatings().ForehandSign() < 0.0;
		if (bLeft != bLeftHanded)
		{
			SetLeftHanded(bLeft);
		}
		const Padel::Animation::FPlayerAnimInput In = Padel::Animation::FPlayerAnimInput::FromController(Controller, Events, Context);
		Model->Step(In, Dt);
		bStepped = true;
	}
}

FName UPadelPlayerAnimComponent::OnRacketSide(FName RightSideBone) const
{
	FString Name = RightSideBone.ToString();
	if (bLeftHanded && Name.EndsWith(TEXT("_r")))
	{
		Name.LeftChopInline(2);
		Name += TEXT("_l");
	}
	return FName(*Name);
}

void UPadelPlayerAnimComponent::SetLeftHanded(bool bLeft)
{
	bLeftHanded = bLeft;
	if (UPadelPlayerAnimInstance* Anim = Body ? Cast<UPadelPlayerAnimInstance>(Body->GetAnimInstance()) : nullptr)
	{
		Anim->bLeftHanded = bLeft;
	}
	// Re-grip on the racket hand of whichever body holds it now (the MetaHuman, or the placeholder).
	if (USkeletalMeshComponent* Holder = Racket ? Cast<USkeletalMeshComponent>(Racket->GetAttachParent()) : nullptr)
	{
		AttachRacket(Holder);
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
		// N1: the body turns from square-to-the-net to its facing (the pawn itself always
		// faces the net). The MetaHuman and the racket ride the body.
		if (!bBodyBaseYawSet)
		{
			BodyBaseYaw = Body->GetRelativeRotation().Yaw;
			bBodyBaseYawSet = true;
		}
		const double Turn = FRotator::NormalizeAxis(Frame.FacingYawDeg - Frame.NetYawDeg);
		FRotator Rotation = Body->GetRelativeRotation();
		Rotation.Yaw = BodyBaseYaw + Turn;
		Body->SetRelativeRotation(Rotation);
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
