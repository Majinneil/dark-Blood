#include "Core/DBGameSettings.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

UDBGameSettings::UDBGameSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Dark Blood");
}

EDBPersistenceMode UDBGameSettings::GetEffectivePersistenceMode() const
{
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("DBPersistence="), Override))
	{
		if (Override.Equals(TEXT("Server"), ESearchCase::IgnoreCase))
		{
			return EDBPersistenceMode::ServerAuthoritative;
		}
		if (Override.Equals(TEXT("Local"), ESearchCase::IgnoreCase))
		{
			return EDBPersistenceMode::LocalCharacters;
		}
	}
	return PersistenceMode;
}
