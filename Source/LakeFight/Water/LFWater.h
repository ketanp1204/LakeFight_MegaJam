#pragma once

#include "CoreMinimal.h"

LAKEFIGHT_API DECLARE_LOG_CATEGORY_EXTERN(LogLF, Log, All);

class UMaterialParameterCollection;

/**
 * Lake Fight water: a flat lake plane + a few summed directional sine waves.
 * The SAME constants drive buoyancy (GetHeightAt) and the lake material (via MPC_LF_Water, pushed by PushToMPC).
 * Change waves ONLY in LFWater.cpp (WaveTable). Units: cm and seconds.
 */
namespace LF::Water
{
	struct FWave
	{
		FVector2f Direction;  // travel direction in XY (normalized at use)
		float Amplitude;      // cm
		float Wavelength;     // cm
		float Speed;          // phase speed, cm/s
	};

	/** World Z of the calm lake surface. */
	inline constexpr float BaseHeight = 0.f;
	inline constexpr int32 NumWaves = 3;

	/** MPC parameter names. Material reads Wave0..Wave2 = (DirX, DirY, Amplitude, Wavelength), WaveSpeeds = (S0, S1, S2, BaseHeight). */
	inline const FName MPCWaveParams[NumWaves] = { TEXT("Wave0"), TEXT("Wave1"), TEXT("Wave2") };
	inline const FName MPCSpeedParam = TEXT("WaveSpeeds");

	LAKEFIGHT_API TConstArrayView<FWave> GetWaves();

	/** Water surface height (world Z, cm) at a world XY position and time (UWorld::GetTimeSeconds matches the material Time node). */
	LAKEFIGHT_API float GetHeightAt(const FVector& WorldPos, float TimeSeconds);

	/** Writes the wave table into the given MPC for this world. Logs a warning if MPC is null. */
	LAKEFIGHT_API void PushToMPC(UObject* WorldContext, UMaterialParameterCollection* MPC);
}
