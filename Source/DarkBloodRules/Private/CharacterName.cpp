#include "DarkBloodRules/CharacterName.h"

#include "DarkBloodRules/Utf8.h"

#include <vector>

namespace DarkBlood::Rules
{
	namespace
	{
		bool IsNameWhitespace(char C)
		{
			return C == ' ' || C == '\t' || C == '\n' || C == '\r' || C == '\f' || C == '\v';
		}

		bool IsNameSeparator(char32_t C)
		{
			return C == U' ' || C == U'-' || C == U'\'' || C == U'’';
		}

		bool IsNameLetter(char32_t C)
		{
			if ((C >= U'A' && C <= U'Z') || (C >= U'a' && C <= U'z'))
			{
				return true;
			}
			// Latin-1 Supplement letters (excluding multiplication and division signs).
			if (C >= 0x00C0 && C <= 0x00FF && C != 0x00D7 && C != 0x00F7)
			{
				return true;
			}
			// Latin Extended-A.
			if (C >= 0x0100 && C <= 0x017F)
			{
				return true;
			}
			// Hiragana and Katakana (including the prolonged sound mark U+30FC).
			if ((C >= 0x3041 && C <= 0x3096) || (C >= 0x30A1 && C <= 0x30FA) || C == 0x30FC)
			{
				return true;
			}
			// CJK Unified Ideographs.
			if (C >= 0x4E00 && C <= 0x9FFF)
			{
				return true;
			}
			return false;
		}
	}

	std::string NormalizeCharacterName(std::string_view Raw)
	{
		std::string Result;
		Result.reserve(Raw.size());
		bool bPendingSpace = false;
		for (const char C : Raw)
		{
			if (IsNameWhitespace(C))
			{
				bPendingSpace = !Result.empty();
				continue;
			}
			if (bPendingSpace)
			{
				Result.push_back(' ');
				bPendingSpace = false;
			}
			Result.push_back(C);
		}
		return Result;
	}

	ENameValidation ValidateCharacterName(std::string_view Name, const FNameRules& Rules)
	{
		if (Name.empty())
		{
			return ENameValidation::Empty;
		}

		std::vector<char32_t> CodePoints;
		if (!Utf8::Decode(Name, CodePoints))
		{
			return ENameValidation::InvalidUtf8;
		}

		const int32 Length = static_cast<int32>(CodePoints.size());
		if (Length < Rules.MinLength)
		{
			return ENameValidation::TooShort;
		}
		if (Length > Rules.MaxLength)
		{
			return ENameValidation::TooLong;
		}

		int32 Words = 1;
		bool bPreviousWasSeparator = false;
		for (int32 Index = 0; Index < Length; ++Index)
		{
			const char32_t C = CodePoints[Index];
			if (IsNameSeparator(C))
			{
				if (Index == 0 || Index == Length - 1)
				{
					return ENameValidation::SeparatorAtEdge;
				}
				if (bPreviousWasSeparator)
				{
					return ENameValidation::ConsecutiveSeparators;
				}
				if (C == U' ')
				{
					++Words;
				}
				bPreviousWasSeparator = true;
				continue;
			}
			if (!IsNameLetter(C))
			{
				return ENameValidation::InvalidCharacter;
			}
			bPreviousWasSeparator = false;
		}

		if (Words > Rules.MaxWords)
		{
			return ENameValidation::TooManyWords;
		}
		return ENameValidation::Ok;
	}

	std::string GetCallName(std::string_view FullName)
	{
		const size_t Space = FullName.find(' ');
		return std::string(Space == std::string_view::npos ? FullName : FullName.substr(0, Space));
	}

	const char* ToString(ENameValidation Result)
	{
		switch (Result)
		{
		case ENameValidation::Ok: return "Ok";
		case ENameValidation::Empty: return "Empty";
		case ENameValidation::InvalidUtf8: return "InvalidUtf8";
		case ENameValidation::TooShort: return "TooShort";
		case ENameValidation::TooLong: return "TooLong";
		case ENameValidation::InvalidCharacter: return "InvalidCharacter";
		case ENameValidation::SeparatorAtEdge: return "SeparatorAtEdge";
		case ENameValidation::ConsecutiveSeparators: return "ConsecutiveSeparators";
		case ENameValidation::TooManyWords: return "TooManyWords";
		}
		return "Unknown";
	}
}
