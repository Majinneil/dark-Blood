// DARK BLOOD - Rules Core: portable binary archive (little-endian, bounds-checked).
// Used for save records so that the same bytes can be stored in a UE SaveGame,
// uploaded to a server or written to a database.
#pragma once

#include "DarkBloodRules/RulesCore.h"

#include <string>
#include <string_view>
#include <vector>

namespace DarkBlood::Rules
{
	DARKBLOODRULES_API uint32 Crc32(const uint8* Data, size_t Size);

	class FBinaryWriter
	{
	public:
		DARKBLOODRULES_API void WriteU8(uint8 Value);
		DARKBLOODRULES_API void WriteU32(uint32 Value);
		DARKBLOODRULES_API void WriteU64(uint64 Value);
		void WriteI32(int32 Value) { WriteU32(static_cast<uint32>(Value)); }
		void WriteI64(int64 Value) { WriteU64(static_cast<uint64>(Value)); }
		void WriteBool(bool bValue) { WriteU8(bValue ? 1 : 0); }
		DARKBLOODRULES_API void WriteF32(float Value);
		DARKBLOODRULES_API void WriteF64(double Value);
		DARKBLOODRULES_API void WriteString(std::string_view Value);

		const std::vector<uint8>& GetData() const { return Data; }
		std::vector<uint8>&& TakeData() { return std::move(Data); }

	private:
		std::vector<uint8> Data;
	};

	class FBinaryReader
	{
	public:
		static constexpr uint32 MaxStringBytes = 64 * 1024;
		static constexpr uint32 MaxArrayElements = 1u << 20;

		FBinaryReader(const uint8* InData, size_t InSize) : Data(InData), Size(InSize) {}

		DARKBLOODRULES_API bool ReadU8(uint8& Out);
		DARKBLOODRULES_API bool ReadU32(uint32& Out);
		DARKBLOODRULES_API bool ReadU64(uint64& Out);
		DARKBLOODRULES_API bool ReadI32(int32& Out);
		DARKBLOODRULES_API bool ReadI64(int64& Out);
		DARKBLOODRULES_API bool ReadBool(bool& Out);
		DARKBLOODRULES_API bool ReadF32(float& Out);
		DARKBLOODRULES_API bool ReadF64(double& Out);
		DARKBLOODRULES_API bool ReadString(std::string& Out);
		/** Reads an element count and rejects absurd values (corrupt or malicious data). */
		DARKBLOODRULES_API bool ReadCount(uint32& Out);

		bool HasError() const { return bError; }
		bool IsAtEnd() const { return Position == Size; }
		size_t GetPosition() const { return Position; }

	private:
		bool Take(size_t Bytes, const uint8*& Out);

		const uint8* Data = nullptr;
		size_t Size = 0;
		size_t Position = 0;
		bool bError = false;
	};
}
