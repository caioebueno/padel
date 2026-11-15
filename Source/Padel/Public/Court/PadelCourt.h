#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourtLayout.h"
#include "PadelCourt.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class UMaterialInterface;

/**
 * Placeholder padel court at regulation size (GDD section 2). One engine cube per
 * box in PadelCore's BuildCourtLayout(), so it needs no binary assets and matches
 * the geometry the native tests check.
 *
 * Every collider is tagged "Surface.<Name>" and "Zone.<Name>". Glass and mesh
 * panels are invisible colliders with a visible frame, so the cameras see through
 * them until Agent 5 adds real materials.
 *
 * The ball does not use these colliders: it runs on PadelCore's own integrator,
 * which reads CourtSpec directly. They are for players, traces and debug tools.
 */
UCLASS()
class PADEL_API APadelCourt : public AActor
{
	GENERATED_BODY()

public:
	APadelCourt();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Surface a court component represents, from its tag; None if it is not a court surface. */
	static Padel::Court::ESurface GetSurface(const UPrimitiveComponent* Component);

	static FName SurfaceTag(Padel::Court::ESurface Surface);
	static FName ZoneTag(Padel::Court::EZone Zone);
	static FLinearColor ColorFor(Padel::Court::EColorRole Role);

	/** One component per layout box, in BuildCourtLayout() order. */
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetBoxComponents() const { return BoxComponents; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Court")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Court")
	TArray<TObjectPtr<UStaticMeshComponent>> BoxComponents;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;
};
