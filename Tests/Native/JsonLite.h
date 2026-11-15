// A minimal JSON reader for the native tests, so they check the real Data/*.json
// files rather than copies of the numbers. Test-only: the game reads the same files
// through Unreal DataTables.
#pragma once

#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace JsonLite
{
	struct FValue
	{
		enum class EKind { Null, Bool, Number, String, Array, Object } Kind = EKind::Null;
		bool Bool = false;
		double Number = 0.0;
		std::string String;
		std::vector<FValue> Array;
		std::map<std::string, FValue> Object;

		bool Has(const std::string& Key) const { return Kind == EKind::Object && Object.count(Key) > 0; }
		const FValue& operator[](const std::string& Key) const
		{
			const auto It = Object.find(Key);
			if (It == Object.end())
			{
				throw std::runtime_error("missing key " + Key);
			}
			return It->second;
		}
	};

	class FParser
	{
	public:
		explicit FParser(const std::string& InText) : Text(InText) {}

		FValue Parse()
		{
			FValue V = Value();
			Skip();
			if (Pos != Text.size())
			{
				Fail("trailing characters");
			}
			return V;
		}

	private:
		[[noreturn]] void Fail(const char* What) const
		{
			throw std::runtime_error(std::string("JSON: ") + What + " at " + std::to_string(Pos));
		}

		void Skip()
		{
			while (Pos < Text.size() && (Text[Pos] == ' ' || Text[Pos] == '\n' || Text[Pos] == '\r' || Text[Pos] == '\t'))
			{
				++Pos;
			}
		}

		bool Eat(char C)
		{
			Skip();
			if (Pos < Text.size() && Text[Pos] == C)
			{
				++Pos;
				return true;
			}
			return false;
		}

		std::string Str()
		{
			if (!Eat('"'))
			{
				Fail("expected string");
			}
			std::string Out;
			while (Pos < Text.size() && Text[Pos] != '"')
			{
				if (Text[Pos] == '\\' && Pos + 1 < Text.size())
				{
					++Pos;
				}
				Out += Text[Pos++];
			}
			if (Pos >= Text.size())
			{
				Fail("unterminated string");
			}
			++Pos;
			return Out;
		}

		FValue Value()
		{
			Skip();
			FValue V;
			if (Pos >= Text.size())
			{
				Fail("unexpected end");
			}
			const char C = Text[Pos];
			if (C == '{')
			{
				++Pos;
				V.Kind = FValue::EKind::Object;
				if (!Eat('}'))
				{
					do
					{
						Skip();
						const std::string Key = Str();
						if (!Eat(':'))
						{
							Fail("expected :");
						}
						V.Object[Key] = Value();
					} while (Eat(','));
					if (!Eat('}'))
					{
						Fail("expected }");
					}
				}
			}
			else if (C == '[')
			{
				++Pos;
				V.Kind = FValue::EKind::Array;
				if (!Eat(']'))
				{
					do
					{
						V.Array.push_back(Value());
					} while (Eat(','));
					if (!Eat(']'))
					{
						Fail("expected ]");
					}
				}
			}
			else if (C == '"')
			{
				V.Kind = FValue::EKind::String;
				V.String = Str();
			}
			else if (Text.compare(Pos, 4, "true") == 0 || Text.compare(Pos, 5, "false") == 0)
			{
				V.Kind = FValue::EKind::Bool;
				V.Bool = Text[Pos] == 't';
				Pos += V.Bool ? 4 : 5;
			}
			else if (Text.compare(Pos, 4, "null") == 0)
			{
				Pos += 4;
			}
			else
			{
				char* End = nullptr;
				V.Kind = FValue::EKind::Number;
				V.Number = std::strtod(Text.c_str() + Pos, &End);
				if (End == Text.c_str() + Pos)
				{
					Fail("bad value");
				}
				Pos = static_cast<std::size_t>(End - Text.c_str());
			}
			return V;
		}

		const std::string& Text;
		std::size_t Pos = 0;
	};

	inline FValue ParseFile(const std::string& Path)
	{
		std::ifstream File(Path);
		if (!File)
		{
			throw std::runtime_error("cannot open " + Path);
		}
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Text = Buffer.str();
		return FParser(Text).Parse();
	}
}
