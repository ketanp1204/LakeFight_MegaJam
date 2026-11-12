#include "Water/LFWater.h"

#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

DEFINE_LOG_CATEGORY(LogLF);

namespace LF::Water
{
	// ---------------------------------------------------------------------------------------------
	// THE wave table - single source of truth. Sum of amplitudes must stay <= 15 cm (task T01).
	// MPC_LF_Water's default values are a copy for the editor viewport only; at BeginPlay ALFLakeSurface
	// pushes these into the MPC, so runtime visuals always match the physics.
	//                       Direction          Amp(cm) Wavelength(cm) Speed(cm/s)
	static const FWave WaveTable[NumWaves] = {
		{ FVector2f( 1.0f,  0.3f),  6.0f,  1800.f,  250.f },
		{ FVector2f(-0.4f,  1.0f),  4.0f,  1100.f,  180.f },
		{ FVector2f( 0.7f, -0.7f),  2.5f,   600.f,  140.f },
	};
	// ---------------------------------------------------------------------------------------------

	TConstArrayView<FWave> GetWaves()
	{
		return MakeArrayView(WaveTable, NumWaves);
	}

	float GetHeightAt(const FVector& WorldPos, float TimeSeconds)
	{
		float Height = BaseHeight;
		for (const FWave& W : WaveTable)
		{
			const FVector2f Dir = W.Direction.GetSafeNormal();
			const float K = UE_TWO_PI / W.Wavelength;
			const float Along = Dir.X * float(WorldPos.X) + Dir.Y * float(WorldPos.Y);
			Height += W.Amplitude * FMath::Sin(K * (Along - W.Speed * TimeSeconds));
		}
		return Height;
	}

	void PushToMPC(UObject* WorldContext, UMaterialParameterCollection* MPC)
	{
		if (!MPC)
		{
			UE_LOG(LogLF, Warning, TEXT("LF::Water::PushToMPC: no MPC assigned - lake material will use the MPC defaults."));
			return;
		}
		FLinearColor Speeds(0, 0, 0, BaseHeight);
		for (int32 i = 0; i < NumWaves; ++i)
		{
			const FWave& W = WaveTable[i];
			const FVector2f Dir = W.Direction.GetSafeNormal();
			UKismetMaterialLibrary::SetVectorParameterValue(WorldContext, MPC, MPCWaveParams[i],
				FLinearColor(Dir.X, Dir.Y, W.Amplitude, W.Wavelength));
			Speeds.Component(i) = W.Speed;
		}
		UKismetMaterialLibrary::SetVectorParameterValue(WorldContext, MPC, MPCSpeedParam, Speeds);
	}
}
