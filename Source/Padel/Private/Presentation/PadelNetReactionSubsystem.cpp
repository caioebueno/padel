#include "Presentation/PadelNetReactionSubsystem.h"

#include "Court/PadelCourtUnits.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "Presentation/PadelMatchEvents.h"

#include <string>

const TCHAR* UPadelNetReactionSubsystem::CollectionPath = TEXT("/Game/Padel/Materials/MPC_NetReaction.MPC_NetReaction");

namespace
{
	// MPC_NetReaction's vector parameters, in Tools/AssetPipeline/unreal/import_assets.py's order.
	const FName ImpactNames[] = { TEXT("Impact0"), TEXT("Impact1") };
	const FName ShapeName(TEXT("Shape"));
	const FName MotionName(TEXT("Motion"));
	const FName NetName(TEXT("Net"));
	const FName ExtraName(TEXT("Extra"));

	FLinearColor ToColor(const std::array<double, 4>& Values)
	{
		return FLinearColor(static_cast<float>(Values[0]), static_cast<float>(Values[1]), static_cast<float>(Values[2]),
			static_cast<float>(Values[3]));
	}
}

void UPadelNetReactionSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	Super::Initialize(InCollection);
	Model = Padel::Presentation::FNetReaction(LoadSettings());
	Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Collection)
	{
		UE_LOG(LogPadel, Log, TEXT("No %s yet (run Tools/AssetPipeline/pipeline.py unreal): the net won't move."), CollectionPath);
	}
	if (UPadelMatchEvents* Events = InCollection.InitializeDependency<UPadelMatchEvents>())
	{
		BindTo(*Events);
	}
}

void UPadelNetReactionSubsystem::Deinitialize()
{
	Unbind();
	Super::Deinitialize();
}

bool UPadelNetReactionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UPadelNetReactionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPadelNetReactionSubsystem, STATGROUP_Tickables);
}

void UPadelNetReactionSubsystem::BindTo(UPadelMatchEvents& Events)
{
	Unbind();
	BoundEvents = &Events;
	HitHandle = Events.OnBallHit.AddUObject(this, &UPadelNetReactionSubsystem::HandleBallHit);
	BouncedHandle = Events.OnBallBounced.AddUObject(this, &UPadelNetReactionSubsystem::HandleBallBounced);
}

void UPadelNetReactionSubsystem::Unbind()
{
	if (UPadelMatchEvents* Events = BoundEvents.Get())
	{
		Events->OnBallHit.Remove(HitHandle);
		Events->OnBallBounced.Remove(BouncedHandle);
	}
	BoundEvents.Reset();
	HitHandle.Reset();
	BouncedHandle.Reset();
}

void UPadelNetReactionSubsystem::HandleBallHit(int32, Padel::EShotType, Padel::ETimingGrade, double SpeedMps, const FVector&)
{
	Model.SetLastShotSpeed(SpeedMps);
}

void UPadelNetReactionSubsystem::HandleBallBounced(Padel::Court::ESurface Surface, Padel::Court::EHalf, const FVector& WorldPosition)
{
	if (Surface != Padel::Court::ESurface::Net)
	{
		return;
	}
	// The net is built in court space at the world origin (GDD 2), so world = court.
	const FVector Metres = Padel::WorldToMetres(WorldPosition);
	Model.OnNetContact(Metres.X, Metres.Y, Metres.Z);
}

void UPadelNetReactionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Advance(DeltaTime);
}

void UPadelNetReactionSubsystem::Advance(double Seconds)
{
	if (!Model.IsActive() && !bCollectionDirty)
	{
		return;
	}
	Model.Advance(Seconds);
	WriteCollection();
	bCollectionDirty = Model.IsActive();
}

void UPadelNetReactionSubsystem::WriteCollection()
{
	// Collection first: GetWorld() is only meaningful once the subsystem lives in a world.
	if (!Collection)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const Padel::Presentation::FNetShaderParams Params = Model.PackShaderParams();
	for (int32 Index = 0; Index < Padel::Presentation::FNetReaction::MaxImpacts; ++Index)
	{
		UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, ImpactNames[Index], ToColor(Params.Impacts[Index]));
	}
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, ShapeName, ToColor(Params.Shape));
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, MotionName, ToColor(Params.Motion));
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, NetName, ToColor(Params.Net));
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, ExtraName, ToColor(Params.Extra));
}

Padel::Presentation::FNetReactionSettings UPadelNetReactionSubsystem::LoadSettings()
{
	Padel::Presentation::FNetReactionSettings Settings;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("Net.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogPadel, Warning, TEXT("No %s: using the default net reaction."), *Path);
		return Settings;
	}
	std::string Error;
	if (!Padel::Presentation::ParseNetReactionSettings(TCHAR_TO_UTF8(*Text), Settings, Error))
	{
		UE_LOG(LogPadel, Warning, TEXT("%s: %s. Using the default net reaction."), *Path, UTF8_TO_TCHAR(Error.c_str()));
	}
	return Settings;
}
