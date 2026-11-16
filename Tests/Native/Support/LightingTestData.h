// Loads the real Data/Lighting.json into the lighting model for the native tests, through
// the same Set* functions APadelVenueLighting uses, and collects every key the model
// doesn't know so a typo in the data fails a test instead of silently using a default.
#pragma once

#include "../JsonLite.h"
#include "Presentation/Model/PadelLightingModel.h"

#include <map>
#include <string>
#include <vector>

namespace LightingTestData
{
	using namespace Padel::Presentation::Lighting;
	using JsonLite::FValue;

	inline std::string DataPath(const char* File) { return std::string(PADEL_REPO_ROOT) + "/Data/" + File; }

	inline bool IsComment(const std::string& Key) { return !Key.empty() && Key[0] == '_'; }

	inline void LoadObject(FLightingPreset& Preset, const FValue& Object, const std::string& Prefix, std::vector<std::string>& Unknown)
	{
		for (const auto& [Key, Value] : Object.Object)
		{
			if (IsComment(Key))
			{
				continue;
			}
			const std::string Path = Prefix + Key;
			bool bKnown = false;
			switch (Value.Kind)
			{
			case FValue::EKind::Number: bKnown = SetPresetNumber(Preset, Path, Value.Number); break;
			case FValue::EKind::Bool: bKnown = SetPresetFlag(Preset, Path, Value.Bool); break;
			case FValue::EKind::String: bKnown = SetPresetText(Preset, Path, Value.String); break;
			case FValue::EKind::Object:
				LoadObject(Preset, Value, Path + ".", Unknown);
				bKnown = true;
				break;
			case FValue::EKind::Array:
				if (Path == "FixtureRows")
				{
					bKnown = true;
					for (const FValue& RowValue : Value.Array)
					{
						FFixtureRow Row;
						for (const auto& [RowKey, RowField] : RowValue.Object)
						{
							const bool bRowKnown = RowField.Kind == FValue::EKind::String
								? SetRowText(Row, RowKey, RowField.String)
								: SetRowNumber(Row, RowKey, RowField.Number);
							if (!IsComment(RowKey) && !bRowKnown)
							{
								Unknown.push_back("FixtureRows." + RowKey);
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
				Unknown.push_back(Path);
			}
		}
	}

	struct FLightingFile
	{
		std::string Default;
		std::map<std::string, FLightingPreset> Presets;
		std::vector<std::string> UnknownKeys;
	};

	inline FLightingFile Load()
	{
		const FValue Root = JsonLite::ParseFile(DataPath("Lighting.json"));
		FLightingFile File;
		File.Default = Root["Default"].String;
		for (const auto& [Name, Value] : Root["Presets"].Object)
		{
			FLightingPreset Preset;
			Preset.Name = Name;
			std::vector<std::string> Unknown;
			LoadObject(Preset, Value, "", Unknown);
			for (const std::string& Key : Unknown)
			{
				File.UnknownKeys.push_back(Name + "." + Key);
			}
			File.Presets[Name] = Preset;
		}
		return File;
	}
}
