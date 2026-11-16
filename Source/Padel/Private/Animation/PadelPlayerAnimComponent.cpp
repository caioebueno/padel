#include "Animation/PadelPlayerAnimComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/PadelAnimData.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Gameplay/PadelShotUnits.h"
#include "PadelLog.h"

UPadelPlayerAnimComponent::UPadelPlayerAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Out of line so TUniquePtr sees the complete FPadelAnimData.
UPadelPlayerAnimComponent::~UPadelPlayerAnimComponent() = default;

void UPadelPlayerAnimComponent::Init(const Padel::Shots::FShotLibrary* Shots, int32 TeamIndex, USkeletalMeshComponent* InBody, USceneComponent* InRacket)
{
	Body = InBody;
	Racket = InRacket;
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
		Racket->SetWorldLocation(Padel::ToWorld(Frame.RacketPosition));
	}
}
