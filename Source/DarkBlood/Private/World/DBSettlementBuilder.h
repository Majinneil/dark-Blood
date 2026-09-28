// Builds one settlement of the open world from the art kit (DBArtBuilder): streets, houses by settlement type, walls,
// gates, lanterns, harbors with piers and ships, rice paddies, temples, particles. Local frame: origin on the leveled
// ground at the settlement center, +X towards the sea for coastal settlements. Deterministic (seeded by the site).
#pragma once

#include "CoreMinimal.h"

namespace DBArtBuild
{
	struct FArtBuilder;
}
struct FDBRealmSettlement;

namespace DBSettlements
{
	void Build(DBArtBuild::FArtBuilder& Builder, const FDBRealmSettlement& Site, int32 Seed);

	/** Yaw of the settlement frame: coastal settlements face the sea, others get a seeded orientation. */
	float FrameYaw(const FDBRealmSettlement& Site, int32 Seed);
}
