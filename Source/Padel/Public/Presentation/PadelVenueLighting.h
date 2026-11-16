#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Presentation/Model/PadelLightingModel.h"
#include "PadelVenueLighting.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelLightingPresetApplied, const FString& /*PresetName*/);

/**
 * All of a venue's light (GDD 10): floodlights, sun, sky, fog and the broadcast
 * post-process, from one preset in Data/Lighting.json. Everything is movable and
 * physically based (candela, lux, kelvin, a fixed EV100 exposure), so Lumen lights the
 * court the way the native tests measure it. Padel::Presentation::Lighting builds the
 * fixtures; this actor only turns them into components.
 *
 * Presets switch at runtime (ApplyPreset, or the Padel.LookDev.Preset console command
 * in the look-dev level).
 */
UCLASS()
class PADEL_API APadelVenueLighting : public AActor
{
	GENERATED_BODY()

public:
	APadelVenueLighting();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Applies a preset by name. Returns false (and keeps the current one) if it isn't in Data/Lighting.json. */
	bool ApplyPreset(const FString& Name);

	/** Applies the next preset in Data/Lighting.json order, wrapping round. */
	void ApplyNextPreset();

	TArray<FString> GetPresetNames() const;
	const FString& GetActivePresetName() const { return ActivePreset; }
	Padel::Presentation::Lighting::EVenue GetActiveVenue() const;
	const Padel::Presentation::Lighting::FLightingPreset* FindPreset(const FString& Name) const;

	const TArray<TObjectPtr<USpotLightComponent>>& GetFloodlights() const { return Floodlights; }
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetFixtureHeads() const { return FixtureHeads; }
	UDirectionalLightComponent* GetSun() const { return Sun; }
	UPostProcessComponent* GetPostProcess() const { return PostProcess; }

	/** Fired after a preset is applied, so the stage can swap the venue meshes. */
	FOnPadelLightingPresetApplied OnPresetApplied;

	/** Preset to start with; empty uses "Default" from Data/Lighting.json. */
	UPROPERTY(EditAnywhere, Category = "Lighting")
	FString StartPreset;

	/**
	 * Reads Data/Lighting.json. Unknown keys are logged as warnings so a typo is visible.
	 * Returns the presets in file order and sets OutDefault and OutFixtureMesh.
	 */
	static std::vector<Padel::Presentation::Lighting::FLightingPreset> LoadPresets(FString& OutDefault, FString& OutFixtureMesh);

private:
	void EnsureLoaded() const;
	void EnsureFixtureMesh();
	void ApplySun(const Padel::Presentation::Lighting::FLightingPreset& Preset);
	void ApplySky(const Padel::Presentation::Lighting::FLightingPreset& Preset);
	void ApplyFog(const Padel::Presentation::Lighting::FLightingPreset& Preset);
	void ApplyPost(const Padel::Presentation::Lighting::FLightingPreset& Preset);
	void RebuildFixtures(const Padel::Presentation::Lighting::FLightingPreset& Preset);
	void ClearFixtures();

	UPROPERTY(VisibleAnywhere, Category = "Lighting")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Lighting")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Lighting")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Lighting")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Lighting")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Lighting")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USpotLightComponent>> Floodlights;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> FixtureHeads;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> FixtureMesh;

	// Data/Lighting.json, read on first use (mutable so the const getters can read it too).
	mutable std::vector<Padel::Presentation::Lighting::FLightingPreset> Presets;
	mutable FString DefaultPreset;
	mutable FString FixtureMeshPath;
	mutable bool bLoaded = false;
	FString ActivePreset;
	bool bTriedFixtureMesh = false;
};
