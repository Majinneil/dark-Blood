#include "World/DBTeleport.h"

#include "DarkBlood.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

bool DBTeleport::MovePawn(APawn* Pawn, const FVector& Location, float Yaw)
{
	if (!Pawn)
	{
		return false;
	}
	const FRotator Facing(0.f, Yaw, 0.f);
	bool bMoved = Pawn->TeleportTo(Location, Facing);
	// Occupied (another player just arrived): the nearest free place on a ring around the target.
	for (int32 Ring = 1; Ring <= 2 && !bMoved; ++Ring)
	{
		for (int32 Step = 0; Step < 8 && !bMoved; ++Step)
		{
			const FVector Offset = FRotator(0.f, Yaw + 180.f + Step * 45.f, 0.f).Vector() * (130.f * Ring);
			bMoved = Pawn->TeleportTo(Location + Offset, Facing);
		}
	}
	if (!bMoved)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("Teleport of %s: no free place near (%.0f, %.0f, %.0f) - placed without check"), *GetNameSafe(Pawn), Location.X,
			Location.Y, Location.Z);
		Pawn->TeleportTo(Location, Facing, false, true);
	}
	if (AController* Controller = Pawn->GetController())
	{
		Controller->SetControlRotation(FRotator(-8.f, Yaw, 0.f));
	}
	return true;
}
