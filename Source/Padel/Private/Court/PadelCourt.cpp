#include "Court/PadelCourt.h"

#include "Court/PadelCourtUnits.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APadelCourt::APadelCourt()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UStaticMesh* Cube = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;

	// The engine cube is 100 x 100 x 100 cm, so a scale equal to the size in metres gives the right box.
	for (const Padel::Court::FCourtBox& Box : Padel::Court::BuildCourtLayout())
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(FName(UTF8_TO_TCHAR(Box.Name.c_str())));
		Component->SetupAttachment(Root);
		Component->SetMobility(EComponentMobility::Static);
		Component->SetStaticMesh(Cube);
		Component->SetRelativeLocation(Padel::MetresToWorld(FVector(Box.Centre.X, Box.Centre.Y, Box.Centre.Z)));
		Component->SetRelativeScale3D(FVector(Box.Size.X, Box.Size.Y, Box.Size.Z));
		Component->SetCastShadow(Box.bVisible);
		Component->SetVisibility(Box.bVisible);
		Component->SetHiddenInGame(!Box.bVisible);

		if (Box.bCollides)
		{
			Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Component->ComponentTags.Add(SurfaceTag(Box.Surface));
			Component->ComponentTags.Add(ZoneTag(Box.Zone));
		}
		else
		{
			Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		}

		BoxComponents.Add(Component);
	}
}

void APadelCourt::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Dynamic materials can't be created in the constructor, so colours are applied here.
	const std::vector<Padel::Court::FCourtBox>& Layout = Padel::Court::BuildCourtLayout();
	if (!BaseMaterial || static_cast<size_t>(BoxComponents.Num()) != Layout.size())
	{
		return;
	}
	for (int32 Index = 0; Index < BoxComponents.Num(); ++Index)
	{
		UStaticMeshComponent* Component = BoxComponents[Index];
		if (!Component || !Layout[Index].bVisible)
		{
			continue;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Material->SetVectorParameterValue(TEXT("Color"), ColorFor(Layout[Index].Color));
		Component->SetMaterial(0, Material);
	}
}

FLinearColor APadelCourt::ColorFor(Padel::Court::EColorRole Role)
{
	using Padel::Court::EColorRole;
	// Placeholders (GDD 10: blue surface, white lines, dark frame and mesh).
	switch (Role)
	{
	case EColorRole::Turf: return FLinearColor(0.06f, 0.20f, 0.55f);
	case EColorRole::Runoff: return FLinearColor(0.05f, 0.06f, 0.08f);
	case EColorRole::Line: return FLinearColor(0.92f, 0.93f, 0.95f);
	case EColorRole::Net: return FLinearColor(0.02f, 0.02f, 0.025f);
	case EColorRole::Frame:
	case EColorRole::Hidden:
	default: return FLinearColor(0.03f, 0.03f, 0.035f);
	}
}

FName APadelCourt::SurfaceTag(Padel::Court::ESurface Surface)
{
	using Padel::Court::ESurface;
	switch (Surface)
	{
	case ESurface::Ground: return FName(TEXT("Surface.Ground"));
	case ESurface::Glass: return FName(TEXT("Surface.Glass"));
	case ESurface::Mesh: return FName(TEXT("Surface.Mesh"));
	case ESurface::Net: return FName(TEXT("Surface.Net"));
	case ESurface::Ceiling: return FName(TEXT("Surface.Ceiling"));
	default: return NAME_None;
	}
}

FName APadelCourt::ZoneTag(Padel::Court::EZone Zone)
{
	using Padel::Court::EZone;
	switch (Zone)
	{
	case EZone::Runoff: return FName(TEXT("Zone.Runoff"));
	case EZone::Ceiling: return FName(TEXT("Zone.Ceiling"));
	case EZone::Court:
	default: return FName(TEXT("Zone.Court"));
	}
}

Padel::Court::ESurface APadelCourt::GetSurface(const UPrimitiveComponent* Component)
{
	using Padel::Court::ESurface;
	if (!Component)
	{
		return ESurface::None;
	}
	for (const ESurface Surface : { ESurface::Ground, ESurface::Glass, ESurface::Mesh, ESurface::Net, ESurface::Ceiling })
	{
		if (Component->ComponentHasTag(SurfaceTag(Surface)))
		{
			return Surface;
		}
	}
	return ESurface::None;
}
