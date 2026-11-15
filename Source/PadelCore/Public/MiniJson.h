// A minimal JSON reader so PadelCore can load its tuning from Data/*.json without
// Unreal's Json module (CLAUDE.md: tuning data is text, PadelCore is plain C++).
// Supports objects, arrays, strings (basic escapes), numbers, true/false/null.

#pragma once

#include "PadelCoreApi.h"

#include <string>
#include <vector>

namespace Padel::Json
{
	enum class EType : int
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object,
	};

	struct PADELCORE_API FValue
	{
		EType Type = EType::Null;
		bool Bool = false;
		double Number = 0.0;
		std::string String;
		std::vector<FValue> Items;
		// Object members: Keys[i] names Items[i].
		std::vector<std::string> Keys;

		// Member by key, or nullptr.
		const FValue* Find(const std::string& Key) const;
	};

	// Parses Text into Out. On failure returns false and describes the problem in Error.
	PADELCORE_API bool Parse(const std::string& Text, FValue& Out, std::string& Error);
}
