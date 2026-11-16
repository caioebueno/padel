#include "Presentation/PadelVenueLighting.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Court/PadelCourtUnits.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	using namespace Padel::Presentation::Lighting;

	FVector ToWorld(const Padel::Presentation::FVec3& Metres)
	{
		return Padel::MetresToWorld(FVector(Metres.X, Metres.Y, Metres.Z));
	}

	// The lens slot of SM_FloodlightHead (palette venue.FixtureLens, Tools/AssetPipeline).
	const FName LensSlot(TEXT("MI_Venue_FixtureLens"));
	// M_PadelEmissive's scalar (Tools/AssetPipeline/unreal/import_assets.py). With physical
	// exposure, emissive values read as cd/m^2, so the data's nits go straight in.
	const FName EmissiveParam(TEXT("EmissiveStrength"));

	std::string ToStd(const FString& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	void ReadPresetObject(FLightingPreset& Preset, const FJsonObject& Object, const FString& Prefix, const FString& PresetName)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object.Values)
		{
			if (Field.Key.StartsWith(TEXT("_")) || !Field.Value.IsValid())
			{
				continue;
			}
			const FString Key = Prefix + Field.Key;
			const std::string StdKey = ToStd(Key);
			bool bKnown = false;
			switch (Field.Value->Type)
			{
			case EJson::Number: bKnown = SetPresetNumber(Preset, StdKey, Field.Value->AsNumber()); break;
			case EJson::Boolean: bKnown = SetPresetFlag(Preset, StdKey, Field.Value->AsBool()); break;
			case EJson::String: bKnown = SetPresetText(Preset, StdKey, ToStd(Field.Value->AsString())); break;
			case EJson::Object:
				ReadPresetObject(Preset, *Field.Value->AsObject(), Key + TEXT("."), PresetName);
				bKnown = true;
				break;
			case EJson::Array:
				if (Key == TEXT("FixtureRows"))
				{
					bKnown = true;
					for (const TSharedPtr<FJsonValue>& RowValue : Field.Value->AsArray())
					{
						const TSharedPtr<FJsonObject> RowObject = RowValue.IsValid() ? RowValue->AsObject() : nullptr;
						if (!RowObject.IsValid())
						{
							continue;
						}
						FFixtureRow Row;
						for (const TPair<FString, TSharedPtr<FJsonValue>>& RowField : RowObject->Values)
						{
							if (RowField.Key.StartsWith(TEXT("_")) || !RowField.Value.IsValid())
							{
								continue;
							}
							const bool bRowKnown = RowField.Value->Type == EJson::String
								? SetRowText(Row, ToStd(RowField.Key), ToStd(RowField.Value->AsString()))
								: SetRowNumber(Row, ToStd(RowField.Key), RowField.Value->AsNumber());
							if (!bRowKnown)
							{
								UE_LOG(LogPadel, Warning, TEXT("Data/Lighting.json: %s has an unknown fixture row key %s."), *PresetName, *RowField.Key);
							}
						}
						Preset.Rows.push_back(Row);
					}
				}
				break;
			default: break;
			}
			if (!bKnown)
			{
				UE_LOG(LogPadel, Warning, TEXT("Data/Lighting.json: %s has an unknown key %s."), *PresetName, *Key);
			}
		}
	}
}

APadelVenueLighting::APadelVenueLighting()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	// The sun drives the sky atmosphere's colour and the aerial perspective.
	Sun->SetAtmosphereSunLight(true);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("BroadcastPost"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;

	Tags.Add(FName(TEXT("VenueLighting")));
}

void APadelVenueLighting::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// In the editor, show the preset while the level is edited; in a game, BeginPlay applies it.
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		ApplyPreset(StartPreset);
	}
}

void APadelVenueLighting::BeginPlay()
{
	Super::BeginPlay();
	ApplyPreset(StartPreset);
}

void APadelVenueLighting::EnsureLoaded() const
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;
	Presets = LoadPresets(DefaultPreset, FixtureMeshPath);
}

void APadelVenueLighting::EnsureFixtureMesh()
{
	if (bTriedFixtureMesh)
	{
		return;
	}
	bTriedFixtureMesh = true;
	EnsureLoaded();
	if (!FixtureMeshPath.IsEmpty())
	{
		// "/Game/X/SM_A" names the package; the object inside has the same name.
		const FString ObjectPath = FixtureMeshPath.Contains(TEXT("."))
			? FixtureMeshPath
			: FixtureMeshPath + TEXT(".") + FPaths::GetBaseFilename(FixtureMeshPath);
		FixtureMesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!FixtureMesh)
	{
		UE_LOG(LogPadel, Log, TEXT("No %s yet (run Tools/AssetPipeline): floodlights shine without fixture heads."), *FixtureMeshPath);
	}
}

std::vector<FLightingPreset> APadelVenueLighting::LoadPresets(FString& OutDefault, FString& OutFixtureMesh)
{
	std::vector<FLightingPreset> Out;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("Lighting.json"));
	FString Json;
	TSharedPtr<FJsonObject> JsonRoot;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), JsonRoot) || !JsonRoot.IsValid())
	{
		UE_LOG(LogPadel, Warning, TEXT("Can't read %s: using a built-in indoor preset."), *Path);
		// The row defaults are the indoor rig's shape; give it light so the court isn't black.
		FLightingPreset Fallback;
		Fallback.Name = "Fallback";
		FFixtureRow Row;
		Row.Count = 5;
		Row.StartX = -11.0;
		Row.EndX = 11.0;
		Row.OffsetY = 7.0;
		Row.HeightM = 8.05;
		Row.IntensityCd = 28500.0;
		Fallback.Rows.push_back(Row);
		Out.push_back(Fallback);
		OutDefault = TEXT("Fallback");
		return Out;
	}
	OutDefault = JsonRoot->GetStringField(TEXT("Default"));
	JsonRoot->TryGetStringField(TEXT("FixtureMesh"), OutFixtureMesh);
	const TSharedPtr<FJsonObject>* PresetsObject = nullptr;
	if (JsonRoot->TryGetObjectField(TEXT("Presets"), PresetsObject))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*PresetsObject)->Values)
		{
			const TSharedPtr<FJsonObject> Object = Entry.Value.IsValid() ? Entry.Value->AsObject() : nullptr;
			if (Entry.Key.StartsWith(TEXT("_")) || !Object.IsValid())
			{
				continue;
			}
			FLightingPreset Preset;
			Preset.Name = ToStd(Entry.Key);
			ReadPresetObject(Preset, *Object, FString(), Entry.Key);
			Out.push_back(MoveTemp(Preset));
		}
	}
	return Out;
}

const FLightingPreset* APadelVenueLighting::FindPreset(const FString& Name) const
{
	EnsureLoaded();
	const std::string Wanted = ToStd(Name);
	for (const FLightingPreset& Preset : Presets)
	{
		if (Preset.Name == Wanted)
		{
			return &Preset;
		}
	}
	return nullptr;
}

TArray<FString> APadelVenueLighting::GetPresetNames() const
{
	EnsureLoaded();
	TArray<FString> Names;
	for (const FLightingPreset& Preset : Presets)
	{
		Names.Add(UTF8_TO_TCHAR(Preset.Name.c_str()));
	}
	return Names;
}

EVenue APadelVenueLighting::GetActiveVenue() const
{
	const FLightingPreset* Preset = FindPreset(ActivePreset);
	return Preset ? Preset->Venue : EVenue::Indoor;
}

bool APadelVenueLighting::ApplyPreset(const FString& Name)
{
	EnsureLoaded();
	const FString Wanted = Name.IsEmpty() ? DefaultPreset : Name;
	const FLightingPreset* Preset = FindPreset(Wanted);
	if (!Preset)
	{
		UE_LOG(LogPadel, Warning, TEXT("No lighting preset %s in Data/Lighting.json (have: %s)."),
			*Wanted, *FString::Join(GetPresetNames(), TEXT(", ")));
		return false;
	}
	ApplySun(*Preset);
	ApplySky(*Preset);
	ApplyFog(*Preset);
	ApplyPost(*Preset);
	RebuildFixtures(*Preset);
	ActivePreset = Wanted;
	UE_LOG(LogPadel, Log, TEXT("Lighting preset %s (%s, %d floodlights)."),
		*Wanted, UTF8_TO_TCHAR(VenueName(Preset->Venue)), Preset->bFloodlightsOn ? Floodlights.Num() : 0);
	OnPresetApplied.Broadcast(ActivePreset);
	return true;
}

void APadelVenueLighting::ApplyNextPreset()
{
	EnsureLoaded();
	const TArray<FString> Names = GetPresetNames();
	if (Names.IsEmpty())
	{
		return;
	}
	const int32 Current = Names.IndexOfByKey(ActivePreset);
	ApplyPreset(Names[(Current + 1) % Names.Num()]);
}

void APadelVenueLighting::ApplySun(const FLightingPreset& Preset)
{
	const FSunSettings& S = Preset.Sun;
	Sun->SetVisibility(S.bEnabled);
	Sun->SetIntensity(S.bEnabled ? static_cast<float>(S.IlluminanceLux) : 0.0f);
	Sun->SetUseTemperature(true);
	Sun->SetTemperature(static_cast<float>(S.TemperatureK));
	Sun->SetLightSourceAngle(static_cast<float>(S.SourceAngleDeg));
	Sun->SetCastShadows(S.bEnabled);
	const Padel::Presentation::FVec3 Direction = SunDirection(S);
	Sun->SetWorldRotation(FVector(Direction.X, Direction.Y, Direction.Z).Rotation());
}

void APadelVenueLighting::ApplySky(const FLightingPreset& Preset)
{
	const FSkySettings& S = Preset.Sky;
	Atmosphere->SetVisibility(S.bAtmosphere);
	SkyLight->bRealTimeCapture = S.bRealTimeCapture;
	SkyLight->bLowerHemisphereIsBlack = S.bLowerHemisphereIsBlack;
	SkyLight->SetLowerHemisphereColor(FLinearColor::Black);
	SkyLight->SetIntensity(static_cast<float>(S.SkyLightIntensity));
	SkyLight->MarkRenderStateDirty();
	SkyLight->RecaptureSky();
}

void APadelVenueLighting::ApplyFog(const FLightingPreset& Preset)
{
	const FFogSettings& S = Preset.Fog;
	Fog->SetVisibility(S.Density > 0.0);
	Fog->SetFogDensity(static_cast<float>(S.Density));
	Fog->SetFogHeightFalloff(static_cast<float>(S.HeightFalloff));
	// Volumetric fog gives the floodlight beams their haze; the Mac profile can turn it off
	// globally with r.VolumetricFog (Config/DefaultDeviceProfiles.ini).
	Fog->SetVolumetricFog(S.bVolumetric);
	Fog->SetVolumetricFogScatteringDistribution(static_cast<float>(S.VolumetricScattering));
	const uint8 Albedo = static_cast<uint8>(FMath::Clamp(S.VolumetricAlbedo, 0.0, 1.0) * 255.0);
	Fog->SetVolumetricFogAlbedo(FColor(Albedo, Albedo, Albedo));
}

void APadelVenueLighting::ApplyPost(const FLightingPreset& Preset)
{
	const FPostSettings& P = Preset.Post;
	FPostProcessSettings& S = PostProcess->Settings;

	// A broadcast camera with a locked iris: manual, physically based exposure, so the
	// picture never pumps as the camera tracks or a player walks in front of a light.
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = 1;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = 0.0f;
	S.bOverride_CameraISO = true;
	S.CameraISO = static_cast<float>(P.ISO);
	S.bOverride_CameraShutterSpeed = true;
	S.CameraShutterSpeed = static_cast<float>(P.ShutterSpeed);
	S.bOverride_DepthOfFieldFstop = true;
	S.DepthOfFieldFstop = static_cast<float>(ApertureForEV100(P.ExposureEV100, P.ShutterSpeed, P.ISO));
	// The f-number is only for exposure: at broadcast distances everything is in focus, and
	// a focal distance of 0 keeps depth of field off.
	S.bOverride_DepthOfFieldFocalDistance = true;
	S.DepthOfFieldFocalDistance = 0.0f;

	S.bOverride_WhiteTemp = true;
	S.WhiteTemp = static_cast<float>(P.WhiteBalanceK);
	S.bOverride_WhiteTint = true;
	S.WhiteTint = static_cast<float>(P.WhiteTint);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(P.Contrast, P.Contrast, P.Contrast, 1.0);
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(P.Saturation, P.Saturation, P.Saturation, 1.0);

	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = static_cast<float>(P.BloomIntensity);
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = static_cast<float>(P.VignetteIntensity);
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = static_cast<float>(P.MotionBlurAmount);
	S.bOverride_MotionBlurMax = true;
	S.MotionBlurMax = static_cast<float>(P.MotionBlurMax);
	S.bOverride_LocalExposureHighlightContrastScale = true;
	S.LocalExposureHighlightContrastScale = static_cast<float>(P.LocalExposureHighlightContrast);
	S.bOverride_LocalExposureShadowContrastScale = true;
	S.LocalExposureShadowContrastScale = static_cast<float>(P.LocalExposureShadowContrast);
	// GDD 10 readability: no grain, fringing or flares between the viewer and the ball.
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = static_cast<float>(P.FilmGrain);
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = static_cast<float>(P.ChromaticAberration);
	S.bOverride_LensFlareIntensity = true;
	S.LensFlareIntensity = static_cast<float>(P.LensFlareIntensity);
}

void APadelVenueLighting::ClearFixtures()
{
	for (USpotLightComponent* Light : Floodlights)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	for (UStaticMeshComponent* Head : FixtureHeads)
	{
		if (Head)
		{
			Head->DestroyComponent();
		}
	}
	Floodlights.Reset();
	FixtureHeads.Reset();
}

void APadelVenueLighting::RebuildFixtures(const FLightingPreset& Preset)
{
	ClearFixtures();
	EnsureFixtureMesh();
	for (const FFixture& F : BuildFixtures(Preset))
	{
		const FVector Location = ToWorld(F.Position);
		// Spot lights and SM_FloodlightHead both shine along their local +X.
		const FRotator Rotation = FVector(F.Direction.X, F.Direction.Y, F.Direction.Z).Rotation();

		if (FixtureMesh)
		{
			UStaticMeshComponent* Head = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			Head->SetupAttachment(Root);
			Head->SetMobility(EComponentMobility::Movable);
			Head->SetStaticMesh(FixtureMesh);
			Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Head->SetRelativeLocationAndRotation(Location, Rotation);
			Head->RegisterComponent();
			const int32 Lens = Head->GetMaterialIndex(LensSlot);
			if (Lens != INDEX_NONE)
			{
				if (UMaterialInstanceDynamic* Material = Head->CreateDynamicMaterialInstance(Lens))
				{
					Material->SetScalarParameterValue(EmissiveParam, Preset.bFloodlightsOn ? static_cast<float>(F.LensLuminanceNits) : 0.0f);
				}
			}
			FixtureHeads.Add(Head);
		}

		if (!Preset.bFloodlightsOn)
		{
			continue;
		}
		USpotLightComponent* Light = NewObject<USpotLightComponent>(this, NAME_None, RF_Transient);
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		// Just in front of the lens, so the head's own housing never shadows its beam.
		Light->SetRelativeLocationAndRotation(Location + Rotation.Vector() * 5.0, Rotation);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(static_cast<float>(F.IntensityCd));
		Light->SetInnerConeAngle(static_cast<float>(F.InnerConeDeg));
		Light->SetOuterConeAngle(static_cast<float>(F.OuterConeDeg));
		Light->SetAttenuationRadius(static_cast<float>(F.AttenuationRadiusM * Padel::Court::UnrealUnitsPerMetre));
		Light->SetSourceRadius(static_cast<float>(F.SourceRadiusM * Padel::Court::UnrealUnitsPerMetre));
		Light->SetSoftSourceRadius(static_cast<float>(F.SourceRadiusM * Padel::Court::UnrealUnitsPerMetre));
		Light->SetUseTemperature(true);
		Light->SetTemperature(static_cast<float>(F.TemperatureK));
		// Only the central court fixtures cast shadows (decision 0009: the Metal budget);
		// house lights never do.
		Light->SetCastShadows(F.bCastShadows);
		Light->bCastVolumetricShadow = false;
		Light->RegisterComponent();
		Floodlights.Add(Light);
	}
}
