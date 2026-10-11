#include "Player/DBLocalPlayer.h"

#include "DarkBlood.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString UDBLocalPlayer::GetGameLoginOptions() const
{
	return Super::GetGameLoginOptions() + TEXT("?DBKey=") + GetPlayerKey();
}

bool UDBLocalPlayer::IsValidPlayerKey(const FString& Key)
{
	if (Key.Len() != 32)
	{
		return false;
	}
	for (const TCHAR Char : Key)
	{
		if (!FChar::IsHexDigit(Char))
		{
			return false;
		}
	}
	return true;
}

FString UDBLocalPlayer::GetPlayerKey()
{
	static FString Key;
	if (!Key.IsEmpty())
	{
		return Key;
	}
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("DBPlayerKey="), Override))
	{
		// Several test clients on one machine: any text becomes a stable key of its own.
		Key = IsValidPlayerKey(Override) ? Override : FMD5::HashAnsiString(*Override);
		return Key;
	}
	const FString Path = FPaths::ProjectSavedDir() / TEXT("SaveGames") / TEXT("DB_PlayerKey.txt");
	FString Stored;
	if (FFileHelper::LoadFileToString(Stored, *Path) && IsValidPlayerKey(Stored.TrimStartAndEnd()))
	{
		Key = Stored.TrimStartAndEnd();
		return Key;
	}
	Key = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FFileHelper::SaveStringToFile(Key, *Path);
	UE_LOG(LogDBSave, Log, TEXT("Created this machine's player key"));
	return Key;
}
