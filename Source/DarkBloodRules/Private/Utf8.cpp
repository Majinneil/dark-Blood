#include "DarkBloodRules/Utf8.h"

namespace DarkBlood::Rules::Utf8
{
	bool Decode(std::string_view Text, std::vector<char32_t>& OutCodePoints)
	{
		OutCodePoints.clear();
		OutCodePoints.reserve(Text.size());

		const auto* Bytes = reinterpret_cast<const unsigned char*>(Text.data());
		const size_t Size = Text.size();
		size_t Index = 0;

		while (Index < Size)
		{
			const unsigned char Lead = Bytes[Index];
			char32_t CodePoint = 0;
			size_t Extra = 0;
			char32_t MinValue = 0;

			if (Lead < 0x80)
			{
				CodePoint = Lead;
			}
			else if ((Lead & 0xE0) == 0xC0)
			{
				CodePoint = Lead & 0x1F;
				Extra = 1;
				MinValue = 0x80;
			}
			else if ((Lead & 0xF0) == 0xE0)
			{
				CodePoint = Lead & 0x0F;
				Extra = 2;
				MinValue = 0x800;
			}
			else if ((Lead & 0xF8) == 0xF0)
			{
				CodePoint = Lead & 0x07;
				Extra = 3;
				MinValue = 0x10000;
			}
			else
			{
				return false;
			}

			if (Index + Extra >= Size)
			{
				return false; // truncated sequence
			}

			for (size_t Continuation = 1; Continuation <= Extra; ++Continuation)
			{
				const unsigned char Byte = Bytes[Index + Continuation];
				if ((Byte & 0xC0) != 0x80)
				{
					return false;
				}
				CodePoint = (CodePoint << 6) | (Byte & 0x3F);
			}

			if (Extra > 0 && CodePoint < MinValue)
			{
				return false; // overlong encoding
			}
			if (CodePoint > 0x10FFFF || (CodePoint >= 0xD800 && CodePoint <= 0xDFFF))
			{
				return false;
			}

			OutCodePoints.push_back(CodePoint);
			Index += Extra + 1;
		}
		return true;
	}

	void Append(std::string& Out, char32_t CodePoint)
	{
		if (CodePoint < 0x80)
		{
			Out.push_back(static_cast<char>(CodePoint));
		}
		else if (CodePoint < 0x800)
		{
			Out.push_back(static_cast<char>(0xC0 | (CodePoint >> 6)));
			Out.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
		}
		else if (CodePoint < 0x10000)
		{
			Out.push_back(static_cast<char>(0xE0 | (CodePoint >> 12)));
			Out.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
			Out.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
		}
		else
		{
			Out.push_back(static_cast<char>(0xF0 | (CodePoint >> 18)));
			Out.push_back(static_cast<char>(0x80 | ((CodePoint >> 12) & 0x3F)));
			Out.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
			Out.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
		}
	}
}
