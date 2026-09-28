// Builds the open world map /Game/DarkBlood/Maps/L_Realm from the realm layout (DBRealmLayout): a 16 x 16 km landscape
// (4064 quads, 4 m) with height and paint layers, the sea plane, sky/light/fog, the player start on the capital plateau
// and the realm director. Editor only:
//   UnrealEditor-Cmd DarkBlood.uproject -run=DBBuildRealm [-Preview]
// -Preview only writes Saved/Realm/RealmPreview.bmp (map view of height and paint) without creating the map.
#pragma once

#include "Commandlets/Commandlet.h"

#include "DBBuildRealmCommandlet.generated.h"

UCLASS()
class DARKBLOOD_API UDBBuildRealmCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDBBuildRealmCommandlet();

	virtual int32 Main(const FString& Params) override;
};
