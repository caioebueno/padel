#include "Presentation/PadelLookDevStage.h"

#include "Components/StaticMeshComponent.h"
#include "Court/PadelCourt.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const FString CourtGroup(TEXT("Court"));

	FVector ReadMetres(const FJsonObject& Object, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.TryGetArrayField(Field, Values) || Values->Num() != 3)
		{
			return FVector::ZeroVector;
		}
		return FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
	}

	TArray<FString> ReadStrings(const FJsonObject& Object, const TCHAR* Field)
	{
		TArray<FString> Out;
		Object.TryGetStringArrayField(Field, Out);
		return Out;
	}
}

APadelLookDevStage::APadelLookDevStage()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	Tags.Add(FName(TEXT("LookDevStage")));
}

bool APadelLookDevStage::LoadConfig(TArray<FPadelLookDevPart>& OutParts, TArray<FString>& OutCourtFallback, TArray<FPadelLookDevCamera>& OutCameras)
{
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("LookDev.json"));
	FString Json;
	TSharedPtr<FJsonObject> JsonRoot;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), JsonRoot) || !JsonRoot.IsValid())
	{
		UE_LOG(LogPadel, Warning, TEXT("Can't read %s: the look-dev stage shows only the placeholder court."), *Path);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* PartValues = nullptr;
	if (JsonRoot->TryGetArrayField(TEXT("Parts"), PartValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *PartValues)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}
			FPadelLookDevPart Part;
			Part.Part = Object->GetStringField(TEXT("Part"));
			Object->TryGetStringField(TEXT("Group"), Part.Group);
			Object->TryGetStringField(TEXT("Owner"), Part.Owner);
			if (!Object->TryGetStringField(TEXT("Venue"), Part.Venue))
			{
				Part.Venue = TEXT("Any");
			}
			Part.Meshes = ReadStrings(*Object, TEXT("Meshes"));
			OutParts.Add(MoveTemp(Part));
		}
	}
	OutCourtFallback = ReadStrings(*JsonRoot, TEXT("CourtFallback"));

	const TArray<TSharedPtr<FJsonValue>>* CameraValues = nullptr;
	if (JsonRoot->TryGetArrayField(TEXT("Cameras"), CameraValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *CameraValues)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}
			FPadelLookDevCamera Camera;
			Camera.Name = Object->GetStringField(TEXT("Name"));
			Camera.LocationM = ReadMetres(*Object, TEXT("LocationM"));
			Camera.LookAtM = ReadMetres(*Object, TEXT("LookAtM"));
			double Fov = Camera.FovDeg;
			Object->TryGetNumberField(TEXT("FovDeg"), Fov);
			Camera.FovDeg = static_cast<float>(Fov);
			OutCameras.Add(MoveTemp(Camera));
		}
	}
	return true;
}

bool APadelLookDevStage::PartFitsVenue(const FString& PartVenue, Padel::Presentation::Lighting::EVenue Venue)
{
	return PartVenue.IsEmpty() || PartVenue.Equals(TEXT("Any"), ESearchCase::IgnoreCase)
		|| PartVenue.Equals(UTF8_TO_TCHAR(Padel::Presentation::Lighting::VenueName(Venue)), ESearchCase::IgnoreCase);
}

UStaticMesh* APadelLookDevStage::LoadMesh(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}
	const FString ObjectPath = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPaths::GetBaseFilename(Path);
	return LoadObject<UStaticMesh>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void APadelLookDevStage::Clear()
{
	for (UStaticMeshComponent* Component : Placed)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	Placed.Reset();
	Placements.Reset();
	if (PlaceholderCourt)
	{
		PlaceholderCourt->Destroy();
		PlaceholderCourt = nullptr;
	}
}

UStaticMeshComponent* APadelLookDevStage::PlaceMesh(UStaticMesh* Mesh, const FString& Part)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	Component->SetupAttachment(Root);
	// Static, so virtual shadow maps cache these pages and only the players redraw.
	Component->SetMobility(EComponentMobility::Static);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->ComponentTags.Add(FName(*(TEXT("LookDev.") + Part)));
	Component->RegisterComponent();
	Placed.Add(Component);
	return Component;
}

void APadelLookDevStage::Assemble(Padel::Presentation::Lighting::EVenue Venue)
{
	if (!bLoaded)
	{
		bLoaded = true;
		LoadConfig(Parts, CourtFallback, Cameras);
	}
	Clear();

	int32 CourtMeshes = 0;
	for (const FPadelLookDevPart& Part : Parts)
	{
		if (!PartFitsVenue(Part.Venue, Venue))
		{
			continue;
		}
		for (const FString& Path : Part.Meshes)
		{
			UStaticMesh* Mesh = LoadMesh(Path);
			Placements.Add({ Part.Part, Path, Mesh != nullptr });
			if (Mesh)
			{
				PlaceMesh(Mesh, Part.Part);
				CourtMeshes += Part.Group == CourtGroup ? 1 : 0;
			}
		}
	}

	if (CourtMeshes == 0)
	{
		// None of the court parts imported yet: anything listed in CourtFallback (empty now that
		// the one-piece SM_CourtIndoor is gone), then the placeholder boxes.
		for (const FString& Path : CourtFallback)
		{
			UStaticMesh* Mesh = LoadMesh(Path);
			Placements.Add({ TEXT("CourtFallback"), Path, Mesh != nullptr });
			if (Mesh)
			{
				PlaceMesh(Mesh, TEXT("CourtFallback"));
				++CourtMeshes;
			}
		}
	}
	if (CourtMeshes == 0)
	{
		if (UWorld* World = GetWorld())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.Owner = this;
			PlaceholderCourt = World->SpawnActor<APadelCourt>(APadelCourt::StaticClass(), FTransform::Identity, Params);
		}
	}
	LogReport();
}

void APadelLookDevStage::LogReport() const
{
	int32 Missing = 0;
	for (const FPadelLookDevPlacement& Placement : Placements)
	{
		Missing += Placement.bPlaced ? 0 : 1;
	}
	UE_LOG(LogPadel, Log, TEXT("Look-dev stage: %d meshes placed, %d not imported yet%s."),
		Placements.Num() - Missing, Missing, PlaceholderCourt ? TEXT(", showing the placeholder court") : TEXT(""));
	for (const FPadelLookDevPart& Part : Parts)
	{
		for (const FPadelLookDevPlacement& Placement : Placements)
		{
			if (Placement.Part == Part.Part)
			{
				UE_LOG(LogPadel, Log, TEXT("  %-12s %-22s %s %s"), *Part.Part, *Part.Owner,
					Placement.bPlaced ? TEXT("placed ") : TEXT("missing"), *Placement.Mesh);
			}
		}
	}
}
