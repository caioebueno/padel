#include "MiniJson.h"

#include <cstdlib>

namespace Padel::Json
{
	const FValue* FValue::Find(const std::string& Key) const
	{
		if (Type != EType::Object)
		{
			return nullptr;
		}
		for (size_t Index = 0; Index < Keys.size(); ++Index)
		{
			if (Keys[Index] == Key)
			{
				return &Items[Index];
			}
		}
		return nullptr;
	}

	namespace
	{
		struct FParser
		{
			const std::string& Text;
			size_t Pos = 0;
			std::string Error;

			void SkipSpace()
			{
				while (Pos < Text.size() && (Text[Pos] == ' ' || Text[Pos] == '\t' || Text[Pos] == '\n' || Text[Pos] == '\r'))
				{
					++Pos;
				}
			}

			bool Fail(const char* Message)
			{
				if (Error.empty())
				{
					Error = std::string(Message) + " at offset " + std::to_string(Pos);
				}
				return false;
			}

			bool Literal(const char* Word)
			{
				size_t Index = 0;
				while (Word[Index] != '\0')
				{
					if (Pos + Index >= Text.size() || Text[Pos + Index] != Word[Index])
					{
						return false;
					}
					++Index;
				}
				Pos += Index;
				return true;
			}

			bool ParseString(std::string& Out)
			{
				if (Pos >= Text.size() || Text[Pos] != '"')
				{
					return Fail("expected a string");
				}
				++Pos;
				while (Pos < Text.size() && Text[Pos] != '"')
				{
					char C = Text[Pos++];
					if (C == '\\')
					{
						if (Pos >= Text.size())
						{
							return Fail("unfinished escape");
						}
						const char E = Text[Pos++];
						switch (E)
						{
						case 'n': C = '\n'; break;
						case 't': C = '\t'; break;
						case 'r': C = '\r'; break;
						case 'b': C = '\b'; break;
						case 'f': C = '\f'; break;
						case 'u':
							// Tuning files are ASCII; keep the escape as-is rather than decode it.
							Out += "\\u";
							continue;
						default: C = E; break;
						}
					}
					Out += C;
				}
				if (Pos >= Text.size())
				{
					return Fail("unterminated string");
				}
				++Pos;
				return true;
			}

			bool ParseValue(FValue& Out, int Depth)
			{
				if (Depth > 64)
				{
					return Fail("nested too deeply");
				}
				SkipSpace();
				if (Pos >= Text.size())
				{
					return Fail("unexpected end");
				}
				const char C = Text[Pos];
				if (C == '{')
				{
					Out.Type = EType::Object;
					++Pos;
					SkipSpace();
					if (Pos < Text.size() && Text[Pos] == '}')
					{
						++Pos;
						return true;
					}
					while (true)
					{
						SkipSpace();
						std::string Key;
						if (!ParseString(Key))
						{
							return false;
						}
						SkipSpace();
						if (Pos >= Text.size() || Text[Pos] != ':')
						{
							return Fail("expected ':'");
						}
						++Pos;
						FValue Member;
						if (!ParseValue(Member, Depth + 1))
						{
							return false;
						}
						Out.Keys.push_back(std::move(Key));
						Out.Items.push_back(std::move(Member));
						SkipSpace();
						if (Pos < Text.size() && Text[Pos] == ',')
						{
							++Pos;
							continue;
						}
						if (Pos < Text.size() && Text[Pos] == '}')
						{
							++Pos;
							return true;
						}
						return Fail("expected ',' or '}'");
					}
				}
				if (C == '[')
				{
					Out.Type = EType::Array;
					++Pos;
					SkipSpace();
					if (Pos < Text.size() && Text[Pos] == ']')
					{
						++Pos;
						return true;
					}
					while (true)
					{
						FValue Item;
						if (!ParseValue(Item, Depth + 1))
						{
							return false;
						}
						Out.Items.push_back(std::move(Item));
						SkipSpace();
						if (Pos < Text.size() && Text[Pos] == ',')
						{
							++Pos;
							continue;
						}
						if (Pos < Text.size() && Text[Pos] == ']')
						{
							++Pos;
							return true;
						}
						return Fail("expected ',' or ']'");
					}
				}
				if (C == '"')
				{
					Out.Type = EType::String;
					return ParseString(Out.String);
				}
				if (Literal("true"))
				{
					Out.Type = EType::Bool;
					Out.Bool = true;
					return true;
				}
				if (Literal("false"))
				{
					Out.Type = EType::Bool;
					return true;
				}
				if (Literal("null"))
				{
					Out.Type = EType::Null;
					return true;
				}
				const char* Begin = Text.c_str() + Pos;
				char* End = nullptr;
				const double Number = std::strtod(Begin, &End);
				if (End == Begin)
				{
					return Fail("unexpected character");
				}
				Out.Type = EType::Number;
				Out.Number = Number;
				Pos += static_cast<size_t>(End - Begin);
				return true;
			}
		};
	}

	bool Parse(const std::string& Text, FValue& Out, std::string& Error)
	{
		FParser Parser{ Text, 0, std::string() };
		Out = FValue();
		if (!Parser.ParseValue(Out, 0))
		{
			Error = Parser.Error;
			return false;
		}
		Parser.SkipSpace();
		if (Parser.Pos != Text.size())
		{
			Parser.Fail("trailing characters");
			Error = Parser.Error;
			return false;
		}
		return true;
	}
}
