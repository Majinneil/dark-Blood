#include "DarkBloodRules/Boss.h"

#include <algorithm>
#include <cmath>

namespace DarkBlood::Rules
{
	FBossScaling GetBossScaling(int32 PlayerCount)
	{
		const int32 Players = std::clamp(PlayerCount, 1, 4);
		static const float Health[] = {1.f, 1.45f, 1.85f, 2.2f};
		static const float Cooldowns[] = {1.f, 0.9f, 0.85f, 0.8f};
		FBossScaling Scaling;
		Scaling.HealthMultiplier = Health[Players - 1];
		Scaling.ExtraAdds = Players - 1;
		Scaling.bSplitAttention = Players >= 2;
		Scaling.bAreaPressure = Players >= 3;
		Scaling.CooldownMultiplier = Cooldowns[Players - 1];
		return Scaling;
	}

	int32 EvaluateBossPhase(float HealthFraction, int32 CurrentPhase, const std::vector<float>& Thresholds)
	{
		int32 Phase = 0;
		for (const float Threshold : Thresholds)
		{
			if (HealthFraction <= Threshold)
			{
				++Phase;
			}
		}
		return std::max(Phase, CurrentPhase);
	}

	float GetEnrageMultiplier(double FightSeconds, double EnrageAfterSeconds)
	{
		if (EnrageAfterSeconds <= 0.0 || FightSeconds < EnrageAfterSeconds)
		{
			return 1.f;
		}
		const double Steps = std::floor((FightSeconds - EnrageAfterSeconds) / 30.0) + 1.0;
		return static_cast<float>(std::min(2.0, 1.0 + 0.25 * Steps));
	}

	float GetSignatureCooldown(int32 Phase, int32 PlayerCount)
	{
		const float Base = std::max(9.f, 18.f - 3.f * static_cast<float>(std::max(0, Phase)));
		return Base * GetBossScaling(PlayerCount).CooldownMultiplier;
	}
}
