#include "Water/LFLakeSurface.h"

#include "Water/LFWater.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

ALFLakeSurface::ALFLakeSurface()
{
	PrimaryActorTick.bCanEverTick = false;

	Surface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surface"));
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetCastShadow(false);
	Surface->bUseAsyncCooking = true;
	RootComponent = Surface;
}

void ALFLakeSurface::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildGrid();
}

void ALFLakeSurface::BeginPlay()
{
	Super::BeginPlay();
	if (!FMath::IsNearlyEqual(float(GetActorLocation().Z), LF::Water::BaseHeight, 1.f))
	{
		UE_LOG(LogLF, Warning, TEXT("ALFLakeSurface %s is at Z=%.1f but LF::Water::BaseHeight=%.1f - visuals won't match buoyancy."),
			*GetName(), GetActorLocation().Z, LF::Water::BaseHeight);
	}
	LF::Water::PushToMPC(this, WaterMPC);
}

void ALFLakeSurface::BuildGrid()
{
	const int32 NX = FMath::Max(1, FMath::CeilToInt(Size.X / CellSize));
	const int32 NY = FMath::Max(1, FMath::CeilToInt(Size.Y / CellSize));
	const float DX = Size.X / NX;
	const float DY = Size.Y / NY;

	TArray<FVector> Verts;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Tris;
	Verts.Reserve((NX + 1) * (NY + 1));
	Normals.Reserve(Verts.Max());
	UVs.Reserve(Verts.Max());
	Tris.Reserve(NX * NY * 6);

	for (int32 iy = 0; iy <= NY; ++iy)
	{
		for (int32 ix = 0; ix <= NX; ++ix)
		{
			Verts.Add(FVector(-0.5f * Size.X + ix * DX, -0.5f * Size.Y + iy * DY, 0.f));
			Normals.Add(FVector::UpVector);
			UVs.Add(FVector2D(ix * DX / 100.f, iy * DY / 100.f)); // 1 UV unit = 1 m
		}
	}
	for (int32 iy = 0; iy < NY; ++iy)
	{
		for (int32 ix = 0; ix < NX; ++ix)
		{
			const int32 A = iy * (NX + 1) + ix;
			const int32 B = A + 1;
			const int32 C = A + (NX + 1);
			const int32 D = C + 1;
			// Flipped in T01.2: the previous order faced down and was back-face culled from above.
			Tris.Append({ A, C, B, B, C, D });
		}
	}

	Surface->ClearAllMeshSections();
	Surface->CreateMeshSection(0, Verts, Tris, Normals, UVs, {}, {}, /*bCreateCollision*/ false);
	if (WaterMaterial)
	{
		Surface->SetMaterial(0, WaterMaterial);
	}
}
