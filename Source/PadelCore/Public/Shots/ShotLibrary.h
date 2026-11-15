// Every playable shot's params plus the shared contact model, as loaded from
// Data/Shots.json and Data/ShotModel.json.

#pragma once

#include "Shots/ShotModel.h"
#include "Shots/ShotParams.h"

#include <array>
#include <string>
#include <vector>

namespace Padel::Shots
{
	struct FShotLibrary
	{
		std::array<FShotParams, ShotTypeCount> Shots{};
		std::array<bool, ShotTypeCount> bLoaded{};
		FShotModel Model;

		void SetShot(const FShotParams& Params)
		{
			Shots[static_cast<int>(Params.Type)] = Params;
			bLoaded[static_cast<int>(Params.Type)] = true;
		}

		const FShotParams& Get(EShotType Type) const { return Shots[static_cast<int>(Type)]; }

		// Every shot but Mishit (shaped by the model, GDD 5.1) must be loaded and valid.
		std::vector<std::string> Validate() const
		{
			std::vector<std::string> Errors = Model.Validate();
			for (int Index = 0; Index < ShotTypeCount; ++Index)
			{
				const EShotType Type = static_cast<EShotType>(Index);
				if (Type == EShotType::Mishit)
				{
					continue;
				}
				if (!bLoaded[Index])
				{
					Errors.push_back(std::string("missing shot ") + ShotTypeName(Type));
					continue;
				}
				for (const std::string& Error : Shots[Index].Validate())
				{
					Errors.push_back(std::string(ShotTypeName(Type)) + ": " + Error);
				}
			}
			return Errors;
		}
	};
}
