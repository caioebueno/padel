#include "Animation/PadelMotionMatchingBuilder.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Dom/JsonObject.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchFeatureChannel_Group.h"
#include "PoseSearch/PoseSearchFeatureChannel_Heading.h"
#include "PoseSearch/PoseSearchFeatureChannel_Pose.h"
#include "PoseSearch/PoseSearchFeatureChannel_Position.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/PoseSearchFeatureChannel_Velocity.h"
#include "PoseSearch/PoseSearchNormalizationSet.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#endif

namespace PadelMotionMatchingBuilderDetail
{
	const TCHAR* EditorOnly = TEXT("motion matching assets are built in the editor");

	TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, Object) ? Object : nullptr;
	}

	double Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double Default)
	{
		double Value = Default;
		return Object.IsValid() && Object->TryGetNumberField(Field, Value) ? Value : Default;
	}

	FString Text(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		FString Value;
		return Object.IsValid() && Object->TryGetStringField(Field, Value) ? Value : FString();
	}

	/** gasp_export.py writes Python's names for these ("None" for an empty name). */
	FName Group(const TSharedPtr<FJsonObject>& Object)
	{
		const FString Value = Text(Object, TEXT("normalization_group"));
		return Value.IsEmpty() || Value == TEXT("None") ? NAME_None : FName(*Value);
	}

	EInputQueryPose QueryPose(const TSharedPtr<FJsonObject>& Object)
	{
		return Text(Object, TEXT("input_query_pose")) == TEXT("USE_CHARACTER_POSE") ? EInputQueryPose::UseCharacterPose : EInputQueryPose::UseContinuingPose;
	}

	FBoneReference Bone(const TSharedPtr<FJsonObject>& Object)
	{
		FBoneReference Ref;
		Ref.BoneName = FName(*Text(Object, TEXT("bone")));
		return Ref;
	}

	/** Empties a private array property (a schema's Skeletons and Channels) so a rebuild starts clean. */
	void EmptyArray(UObject* Object, const TCHAR* Name)
	{
		if (FArrayProperty* Array = FindFProperty<FArrayProperty>(Object->GetClass(), Name))
		{
			FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Object));
			Helper.EmptyValues();
		}
	}

	/** One channel from the manifest, or null (with Error) for a kind this pipeline doesn't build. */
	UPoseSearchFeatureChannel* MakeChannel(UObject* Outer, const TSharedPtr<FJsonObject>& Json, FString& Error)
	{
		const FString Class = Text(Json, TEXT("class"));
		const float Weight = static_cast<float>(Number(Json, TEXT("weight"), 1.0));
		if (Class == TEXT("PoseSearchFeatureChannel_Trajectory"))
		{
			UPoseSearchFeatureChannel_Trajectory* Channel = NewObject<UPoseSearchFeatureChannel_Trajectory>(Outer, NAME_None, RF_Transactional);
#if WITH_EDITORONLY_DATA
			Channel->Weight = Weight;
#endif
			Channel->Samples.Reset();
			const TArray<TSharedPtr<FJsonValue>>* Samples = nullptr;
			if (Json->TryGetArrayField(TEXT("samples"), Samples))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Samples)
				{
					const TSharedPtr<FJsonObject> S = Value->AsObject();
					FPoseSearchTrajectorySample& Sample = Channel->Samples.AddDefaulted_GetRef();
					Sample.Offset = static_cast<float>(Number(S, TEXT("offset"), 0.0));
					Sample.Flags = static_cast<int32>(Number(S, TEXT("flags"), 0.0));
#if WITH_EDITORONLY_DATA
					Sample.Weight = static_cast<float>(Number(S, TEXT("weight"), 1.0));
#endif
				}
			}
			return Channel;
		}
		if (Class == TEXT("PoseSearchFeatureChannel_Group"))
		{
			UPoseSearchFeatureChannel_Group* Channel = NewObject<UPoseSearchFeatureChannel_Group>(Outer, NAME_None, RF_Transactional);
			const TArray<TSharedPtr<FJsonValue>>* Subs = nullptr;
			if (Json->TryGetArrayField(TEXT("sub_channels"), Subs))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Subs)
				{
					UPoseSearchFeatureChannel* Sub = MakeChannel(Channel, Value->AsObject(), Error);
					if (!Sub)
					{
						return nullptr;
					}
					Channel->SubChannels.Add(Sub);
				}
			}
			return Channel;
		}
		if (Class == TEXT("PoseSearchFeatureChannel_Position"))
		{
			UPoseSearchFeatureChannel_Position* Channel = NewObject<UPoseSearchFeatureChannel_Position>(Outer, NAME_None, RF_Transactional);
			Channel->Bone = Bone(Json);
			Channel->SampleTimeOffset = static_cast<float>(Number(Json, TEXT("sample_time_offset"), 0.0));
			Channel->InputQueryPose = QueryPose(Json);
#if WITH_EDITORONLY_DATA
			Channel->Weight = Weight;
			Channel->NormalizationGroup = Group(Json);
#endif
			return Channel;
		}
		if (Class == TEXT("PoseSearchFeatureChannel_Velocity"))
		{
			UPoseSearchFeatureChannel_Velocity* Channel = NewObject<UPoseSearchFeatureChannel_Velocity>(Outer, NAME_None, RF_Transactional);
			Channel->Bone = Bone(Json);
			Channel->SampleTimeOffset = static_cast<float>(Number(Json, TEXT("sample_time_offset"), 0.0));
			Channel->InputQueryPose = QueryPose(Json);
#if WITH_EDITORONLY_DATA
			Channel->Weight = Weight;
			Channel->NormalizationGroup = Group(Json);
#endif
			return Channel;
		}
		if (Class == TEXT("PoseSearchFeatureChannel_Heading"))
		{
			UPoseSearchFeatureChannel_Heading* Channel = NewObject<UPoseSearchFeatureChannel_Heading>(Outer, NAME_None, RF_Transactional);
			Channel->Bone = Bone(Json);
			Channel->SampleTimeOffset = static_cast<float>(Number(Json, TEXT("sample_time_offset"), 0.0));
			Channel->InputQueryPose = QueryPose(Json);
			const FString Axis = Text(Json, TEXT("heading_axis"));
			Channel->HeadingAxis = Axis == TEXT("Y") ? EHeadingAxis::Y : Axis == TEXT("Z") ? EHeadingAxis::Z : EHeadingAxis::X;
#if WITH_EDITORONLY_DATA
			Channel->Weight = Weight;
			Channel->NormalizationGroup = Group(Json);
#endif
			return Channel;
		}
		if (Class == TEXT("PoseSearchFeatureChannel_Pose"))
		{
			UPoseSearchFeatureChannel_Pose* Channel = NewObject<UPoseSearchFeatureChannel_Pose>(Outer, NAME_None, RF_Transactional);
			Channel->InputQueryPose = QueryPose(Json);
#if WITH_EDITORONLY_DATA
			Channel->Weight = Weight;
#endif
			Channel->SampledBones.Reset();
			const TArray<TSharedPtr<FJsonValue>>* Bones = nullptr;
			if (Json->TryGetArrayField(TEXT("sampled_bones"), Bones))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Bones)
				{
					const TSharedPtr<FJsonObject> B = Value->AsObject();
					FPoseSearchBone& Sampled = Channel->SampledBones.AddDefaulted_GetRef();
					Sampled.Reference = Bone(B);
					Sampled.Flags = static_cast<int32>(Number(B, TEXT("flags"), 2.0));
#if WITH_EDITORONLY_DATA
					Sampled.Weight = static_cast<float>(Number(B, TEXT("weight"), 1.0));
#endif
				}
			}
			return Channel;
		}
		Error = FString::Printf(TEXT("no builder for channel %s"), *Class);
		return nullptr;
	}
}

UObject* UPadelMotionMatchingBuilder::FindOrCreateAsset(UClass* AssetClass, const FString& PackagePath)
{
#if WITH_EDITOR
	if (!AssetClass || !PackagePath.StartsWith(TEXT("/Game/")))
	{
		return nullptr;
	}
	const FString Name = FPackageName::GetLongPackageAssetName(PackagePath);
	if (UObject* Existing = LoadObject<UObject>(nullptr, *(PackagePath + TEXT(".") + Name), nullptr, LOAD_Quiet | LOAD_NoWarn))
	{
		return Existing->IsA(AssetClass) ? Existing : nullptr;
	}
	UPackage* Package = CreatePackage(*PackagePath);
	UObject* Asset = NewObject<UObject>(Package, AssetClass, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Asset);
	Package->MarkPackageDirty();
	return Asset;
#else
	return nullptr;
#endif
}

FString UPadelMotionMatchingBuilder::BuildSchema(UPoseSearchSchema* Schema, USkeleton* Skeleton, const FString& SchemaJson)
{
	using namespace PadelMotionMatchingBuilderDetail;
#if WITH_EDITOR
	if (!Schema || !Skeleton)
	{
		return TEXT("BuildSchema needs a schema and a skeleton");
	}
	const TSharedPtr<FJsonObject> Json = Parse(SchemaJson);
	const TArray<TSharedPtr<FJsonValue>>* Channels = nullptr;
	if (!Json.IsValid() || !Json->TryGetArrayField(TEXT("channels"), Channels))
	{
		return TEXT("the schema JSON has no channels");
	}
	Schema->Modify();
	EmptyArray(Schema, TEXT("Skeletons"));
	EmptyArray(Schema, TEXT("Channels"));
	const TSharedPtr<FJsonObject>* Settings = nullptr;
	if (Json->TryGetObjectField(TEXT("settings"), Settings))
	{
		Schema->SampleRate = static_cast<int32>(Number(*Settings, TEXT("sample_rate"), 30.0));
		if (Text(*Settings, TEXT("data_preprocessor")) == TEXT("NORMALIZE_WITH_COMMON_SCHEMA"))
		{
			Schema->DataPreprocessor = EPoseSearchDataPreprocessor::NormalizeWithCommonSchema;
		}
	}
	// No mirror table: the sample's locomotion schemas use none.
	Schema->AddSkeleton(Skeleton);
	for (const TSharedPtr<FJsonValue>& Value : *Channels)
	{
		FString Error;
		UPoseSearchFeatureChannel* Channel = MakeChannel(Schema, Value->AsObject(), Error);
		if (!Channel)
		{
			return Error;
		}
		Schema->AddChannel(Channel);
	}
	// Finalize is private; the schema finalizes its channels on an edit.
	Schema->PostEditChange();
	Schema->MarkPackageDirty();
	return FString();
#else
	return EditorOnly;
#endif
}

FString UPadelMotionMatchingBuilder::BuildDatabase(UPoseSearchDatabase* Database, UPoseSearchSchema* Schema, UPoseSearchNormalizationSet* NormalizationSet,
	const TArray<UAnimSequence*>& Clips, const FString& SettingsJson)
{
	using namespace PadelMotionMatchingBuilderDetail;
#if WITH_EDITOR
	if (!Database || !Schema)
	{
		return TEXT("BuildDatabase needs a database and a schema");
	}
	const TSharedPtr<FJsonObject> Settings = Parse(SettingsJson);
	Database->Modify();
	Database->Schema = Schema;
	Database->NormalizationSet = NormalizationSet;
	Database->ContinuingPoseCostBias = static_cast<float>(Number(Settings, TEXT("continuing_pose_cost_bias"), Database->ContinuingPoseCostBias));
	Database->BaseCostBias = static_cast<float>(Number(Settings, TEXT("base_cost_bias"), Database->BaseCostBias));
	Database->LoopingCostBias = static_cast<float>(Number(Settings, TEXT("looping_cost_bias"), Database->LoopingCostBias));
	const TSharedPtr<FJsonObject>* Exclude = nullptr;
	if (Settings.IsValid() && Settings->TryGetObjectField(TEXT("exclude_from_database_parameters"), Exclude))
	{
		Database->ExcludeFromDatabaseParameters = FFloatInterval(static_cast<float>(Number(*Exclude, TEXT("min"), 0.0)), static_cast<float>(Number(*Exclude, TEXT("max"), -0.3)));
	}
	Database->Tags.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
	if (Settings.IsValid() && Settings->TryGetArrayField(TEXT("tags"), Tags))
	{
		for (const TSharedPtr<FJsonValue>& Tag : *Tags)
		{
			Database->Tags.Add(FName(*Tag->AsString()));
		}
	}
	while (Database->GetNumAnimationAssets() > 0)
	{
		Database->RemoveAnimationAssetAt(Database->GetNumAnimationAssets() - 1);
	}
	const USkeleton* Skeleton = Schema->GetSkeleton(Schema->GetDefaultRole());
	for (UAnimSequence* Clip : Clips)
	{
		if (!Clip || Clip->GetSkeleton() != Skeleton)
		{
			return FString::Printf(TEXT("%s isn't on the schema's skeleton"), Clip ? *Clip->GetPathName() : TEXT("a missing clip"));
		}
		FPoseSearchDatabaseAnimationAsset Entry;
		Entry.AnimAsset = Clip;
		Database->AddAnimationAsset(Entry);
	}
	Database->MarkPackageDirty();
	return FString();
#else
	return EditorOnly;
#endif
}

FString UPadelMotionMatchingBuilder::BuildNormalizationSet(UPoseSearchNormalizationSet* NormalizationSet, const TArray<UPoseSearchDatabase*>& Databases)
{
#if WITH_EDITOR
	if (!NormalizationSet)
	{
		return TEXT("BuildNormalizationSet needs a set");
	}
	NormalizationSet->Modify();
	NormalizationSet->Databases.Reset();
	for (const UPoseSearchDatabase* Database : Databases)
	{
		NormalizationSet->Databases.Add(Database);
	}
	NormalizationSet->MarkPackageDirty();
	return FString();
#else
	return PadelMotionMatchingBuilderDetail::EditorOnly;
#endif
}

FString UPadelMotionMatchingBuilder::BuildIndex(UPoseSearchDatabase* Database)
{
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	if (!Database)
	{
		return TEXT("BuildIndex needs a database");
	}
	const EAsyncBuildIndexResult Result = FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database,
		ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
	return Result == EAsyncBuildIndexResult::Success ? FString()
		: FString::Printf(TEXT("%s: the search index didn't build"), *Database->GetName());
#else
	return PadelMotionMatchingBuilderDetail::EditorOnly;
#endif
}
