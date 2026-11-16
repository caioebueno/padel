#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Presentation/Model/PadelLightingModel.h"
#include "PadelLookDevStage.generated.h"

class APadelCourt;
class UStaticMesh;
class UStaticMeshComponent;

/** One entry of Data/LookDev.json "Parts": a court part or venue piece and who owns it. */
struct FPadelLookDevPart
{
	FString Part;
	FString Group;
	FString Owner;
	// "Indoor", "Outdoor" or "Any", matched against the lighting preset's venue.
	FString Venue;
	TArray<FString> Meshes;
};

/** A look-dev camera bookmark, in metres on the court axes. */
struct FPadelLookDevCamera
{
	FString Name;
	FVector LocationM = FVector::ZeroVector;
	FVector LookAtM = FVector::ZeroVector;
	float FovDeg = 50.0f;
};

/** What the stage did with one mesh path, for the report and the tests. */
struct FPadelLookDevPlacement
{
	FString Part;
	FString Mesh;
	bool bPlaced = false;
};

/**
 * The shared look-dev stage: assembles every court part and venue mesh from
 * Data/LookDev.json at the origin (the asset pipeline builds them in court coordinates),
 * so the five court-art parts are judged together, under the real lighting, from the
 * real broadcast camera. Parts that aren't imported yet are skipped and reported; with no
 * court part at all it falls back to the pipeline's one-piece court, then to APadelCourt.
 *
 * Built from text, not a .umap, so it can be reviewed and never goes stale:
 * APadelLookDevGameMode spawns it into any map.
 */
UCLASS()
class PADEL_API APadelLookDevStage : public AActor
{
	GENERATED_BODY()

public:
	APadelLookDevStage();

	/** Places the parts that belong in this venue, replacing whatever was placed before. */
	void Assemble(Padel::Presentation::Lighting::EVenue Venue);

	/** Logs each part, its owner and which of its meshes are in place. */
	void LogReport() const;

	const TArray<FPadelLookDevPart>& GetParts() const { return Parts; }
	const TArray<FPadelLookDevCamera>& GetCameras() const { return Cameras; }
	const TArray<FPadelLookDevPlacement>& GetPlacements() const { return Placements; }
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetPlacedComponents() const { return Placed; }
	bool IsShowingPlaceholderCourt() const { return PlaceholderCourt != nullptr; }

	/** Reads Data/LookDev.json. Returns false if the file is missing or malformed. */
	static bool LoadConfig(TArray<FPadelLookDevPart>& OutParts, TArray<FString>& OutCourtFallback, TArray<FPadelLookDevCamera>& OutCameras);

	/** Whether a part tagged with Venue ("Indoor", "Outdoor", "Any") belongs in this venue. */
	static bool PartFitsVenue(const FString& PartVenue, Padel::Presentation::Lighting::EVenue Venue);

	/** Loads "/Game/X/SM_A" (or "/Game/X/SM_A.SM_A") quietly; null if it isn't imported. */
	static UStaticMesh* LoadMesh(const FString& Path);

private:
	UStaticMeshComponent* PlaceMesh(UStaticMesh* Mesh, const FString& Part);
	void Clear();

	UPROPERTY(VisibleAnywhere, Category = "LookDev")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Placed;

	UPROPERTY(Transient)
	TObjectPtr<APadelCourt> PlaceholderCourt;

	TArray<FPadelLookDevPart> Parts;
	TArray<FString> CourtFallback;
	TArray<FPadelLookDevCamera> Cameras;
	TArray<FPadelLookDevPlacement> Placements;
	bool bLoaded = false;
};
