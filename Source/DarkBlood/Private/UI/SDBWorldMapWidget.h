// World map window ([M]): the painted world map (T_WorldMap, imported by Tools/UE58/db_import_world_map.py) cropped to
// the continent, with the local player as an arrow, the other players and the settlements. Positions use the same
// projection the realm layout was built from (DBRealm::ToMapPixel), so markers sit where the map shows them.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;

class SDBWorldMapWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBWorldMapWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetRegionText() const;

	TWeakObjectPtr<APlayerController> Owner;
};
