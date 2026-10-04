#include "DarkBloodRules/Archive.h"

#include <array>
#include <cstring>

namespace DarkBlood::Rules
{
	namespace
	{
		std::array<uint32, 256> BuildCrc32Table()
		{
			std::array<uint32, 256> Table{};
			for (uint32 Index = 0; Index < 256; ++Index)
			{
				uint32 Value = Index;
				for (int32 Bit = 0; Bit < 8; ++Bit)
				{
					Value = (Value & 1u) ? (0xEDB88320u ^ (Value >> 1)) : (Value >> 1);
				}
				Table[Index] = Value;
			}
			return Table;
		}
	}

	uint32 Crc32(const uint8* Data, size_t Size)
	{
		static const std::array<uint32, 256> Table = BuildCrc32Table();
		uint32 Crc = 0xFFFFFFFFu;
		for (size_t Index = 0; Index < Size; ++Index)
		{
			Crc = Table[(Crc ^ Data[Index]) & 0xFFu] ^ (Crc >> 8);
		}
		return Crc ^ 0xFFFFFFFFu;
	}

	void FBinaryWriter::WriteU8(uint8 Value)
	{
		Data.push_back(Value);
	}

	void FBinaryWriter::WriteU32(uint32 Value)
	{
		for (int32 Shift = 0; Shift < 32; Shift += 8)
		{
			Data.push_back(static_cast<uint8>((Value >> Shift) & 0xFFu));
		}
	}

	void FBinaryWriter::WriteU64(uint64 Value)
	{
		for (int32 Shift = 0; Shift < 64; Shift += 8)
		{
			Data.push_back(static_cast<uint8>((Value >> Shift) & 0xFFu));
		}
	}

	void FBinaryWriter::WriteF32(float Value)
	{
		uint32 Bits = 0;
		static_assert(sizeof(Bits) == sizeof(Value));
		std::memcpy(&Bits, &Value, sizeof(Bits));
		WriteU32(Bits);
	}

	void FBinaryWriter::WriteF64(double Value)
	{
		uint64 Bits = 0;
		static_assert(sizeof(Bits) == sizeof(Value));
		std::memcpy(&Bits, &Value, sizeof(Bits));
		WriteU64(Bits);
	}

	void FBinaryWriter::WriteString(std::string_view Value)
	{
		WriteU32(static_cast<uint32>(Value.size()));
		Data.insert(Data.end(), Value.begin(), Value.end());
	}

	bool FBinaryReader::Take(size_t Bytes, const uint8*& Out)
	{
		if (bError || Bytes > Size - Position)
		{
			bError = true;
			return false;
		}
		Out = Data + Position;
		Position += Bytes;
		return true;
	}

	bool FBinaryReader::ReadU8(uint8& Out)
	{
		const uint8* Bytes = nullptr;
		if (!Take(1, Bytes))
		{
			return false;
		}
		Out = Bytes[0];
		return true;
	}

	bool FBinaryReader::ReadU32(uint32& Out)
	{
		const uint8* Bytes = nullptr;
		if (!Take(4, Bytes))
		{
			return false;
		}
		Out = 0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Out |= static_cast<uint32>(Bytes[Index]) << (Index * 8);
		}
		return true;
	}

	bool FBinaryReader::ReadU64(uint64& Out)
	{
		const uint8* Bytes = nullptr;
		if (!Take(8, Bytes))
		{
			return false;
		}
		Out = 0;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Out |= static_cast<uint64>(Bytes[Index]) << (Index * 8);
		}
		return true;
	}

	bool FBinaryReader::ReadI32(int32& Out)
	{
		uint32 Value = 0;
		if (!ReadU32(Value))
		{
			return false;
		}
		Out = static_cast<int32>(Value);
		return true;
	}

	bool FBinaryReader::ReadI64(int64& Out)
	{
		uint64 Value = 0;
		if (!ReadU64(Value))
		{
			return false;
		}
		Out = static_cast<int64>(Value);
		return true;
	}

	bool FBinaryReader::ReadBool(bool& Out)
	{
		uint8 Value = 0;
		if (!ReadU8(Value) || Value > 1)
		{
			bError = true;
			return false;
		}
		Out = Value == 1;
		return true;
	}

	bool FBinaryReader::ReadF32(float& Out)
	{
		uint32 Bits = 0;
		if (!ReadU32(Bits))
		{
			return false;
		}
		std::memcpy(&Out, &Bits, sizeof(Out));
		return true;
	}

	bool FBinaryReader::ReadF64(double& Out)
	{
		uint64 Bits = 0;
		if (!ReadU64(Bits))
		{
			return false;
		}
		std::memcpy(&Out, &Bits, sizeof(Out));
		return true;
	}

	bool FBinaryReader::ReadString(std::string& Out)
	{
		uint32 Length = 0;
		if (!ReadU32(Length))
		{
			return false;
		}
		if (Length > MaxStringBytes)
		{
			bError = true;
			return false;
		}
		const uint8* Bytes = nullptr;
		if (!Take(Length, Bytes))
		{
			return false;
		}
		Out.assign(reinterpret_cast<const char*>(Bytes), Length);
		return true;
	}

	bool FBinaryReader::ReadCount(uint32& Out)
	{
		if (!ReadU32(Out))
		{
			return false;
		}
		// Every element needs at least one byte, so a count larger than the remaining data is corrupt.
		if (Out > MaxArrayElements || Out > Size - Position)
		{
			bError = true;
			return false;
		}
		return true;
	}
}
