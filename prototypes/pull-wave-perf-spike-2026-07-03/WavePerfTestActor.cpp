// PROTOTYPE - NOT FOR PRODUCTION
// Question: Can Pull-Wave PEAK ISMC load sustain 60fps + 0.25ms per-tick on iPhone 17 across thermal states?
// Date: 2026-07-03

#include "WavePerfTestActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

DECLARE_CYCLE_STAT(TEXT("PullWavePerTick"), STAT_PullWavePerTick, STATGROUP_PullWaveSpike);

AWavePerfTestActor::AWavePerfTestActor()
{
	PrimaryActorTick.bCanEverTick = true;

	WaveMassISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WaveMassISMC"));
	RootComponent = WaveMassISMC;
	WaveMassISMC->SetMobility(EComponentMobility::Movable);
	WaveMassISMC->SetGenerateOverlapEvents(false);
	WaveMassISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TrailCubeISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TrailCubeISMC"));
	TrailCubeISMC->SetupAttachment(RootComponent);
	TrailCubeISMC->SetMobility(EComponentMobility::Movable);
	TrailCubeISMC->SetGenerateOverlapEvents(false);
	TrailCubeISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultCube.Succeeded())
	{
		WaveMassISMC->SetStaticMesh(DefaultCube.Object);
		TrailCubeISMC->SetStaticMesh(DefaultCube.Object);
	}
}

void AWavePerfTestActor::BeginPlay()
{
	Super::BeginPlay();

	WaveMassISMC->SetNumCustomDataFloats(3);
	WaveMassISMC->SetCullDistances(0, static_cast<int32>(CullDistanceEndCm));

	TrailCubeISMC->SetNumCustomDataFloats(1);
	TrailCubeISMC->SetCullDistances(0, static_cast<int32>(CullDistanceEndCm));

	PopulateISMC(WaveMassISMC, WaveMassCount, 3);
	PopulateISMC(TrailCubeISMC, TrailCubeCount, 1);

	WaveMassVelocities.SetNum(WaveMassCount);
	TrailCubeVelocities.SetNum(TrailCubeCount);
	for (int32 i = 0; i < WaveMassCount; ++i)
	{
		const float T = static_cast<float>(i) / FMath::Max(1, WaveMassCount - 1);
		WaveMassVelocities[i] = FMath::Lerp(VelocityMinCmS, VelocityMaxCmS, T);
	}
	for (int32 i = 0; i < TrailCubeCount; ++i)
	{
		const float T = static_cast<float>(i) / FMath::Max(1, TrailCubeCount - 1);
		TrailCubeVelocities[i] = FMath::Lerp(VelocityMinCmS, VelocityMaxCmS, T);
	}
}

void AWavePerfTestActor::PopulateISMC(UInstancedStaticMeshComponent* Component, int32 Count, int32 NumCustomDataFloats)
{
	Component->ClearInstances();
	for (int32 i = 0; i < Count; ++i)
	{
		const float T = static_cast<float>(i) / FMath::Max(1, Count - 1);
		const float StartX = FMath::Lerp(-WrapRangeCm, WrapRangeCm, T);
		const float StartY = FMath::Sin(static_cast<float>(i)) * 100.f;
		const FTransform Xform(FRotator::ZeroRotator, FVector(StartX, StartY, 100.f), FVector(1.f));
		const int32 InstanceIdx = Component->AddInstance(Xform);
		for (int32 D = 0; D < NumCustomDataFloats; ++D)
		{
			Component->SetCustomDataValue(InstanceIdx, D, 0.f, false);
		}
	}
}

void AWavePerfTestActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ElapsedTime += DeltaSeconds;
	AnimateInstances(ElapsedTime);
}

void AWavePerfTestActor::AnimateInstances(float ElapsedSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_PullWavePerTick);

	const float WrapSpan = WrapRangeCm * 2.f;

	for (int32 i = 0; i < WaveMassCount; ++i)
	{
		const float Velocity = WaveMassVelocities[i];
		const float RawX = ElapsedSeconds * Velocity + static_cast<float>(i) * 200.f;
		const float X = FMath::Fmod(RawX, WrapSpan) - WrapRangeCm;
		const float Y = FMath::Sin(ElapsedSeconds * 2.f + static_cast<float>(i)) * 100.f;
		const float Z = 100.f + FMath::Sin(ElapsedSeconds * 3.f + static_cast<float>(i)) * 20.f;
		const FTransform Xform(FRotator::ZeroRotator, FVector(X, Y, Z), FVector(1.f));
		WaveMassISMC->UpdateInstanceTransform(i, Xform, /*bWorldSpace=*/false, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);

		WaveMassISMC->SetCustomDataValue(i, 0, FMath::Sin(ElapsedSeconds + static_cast<float>(i)), false);
		WaveMassISMC->SetCustomDataValue(i, 1, FMath::Cos(ElapsedSeconds + static_cast<float>(i)), false);
		WaveMassISMC->SetCustomDataValue(i, 2, ElapsedSeconds, false);
	}
	WaveMassISMC->MarkRenderStateDirty();

	for (int32 i = 0; i < TrailCubeCount; ++i)
	{
		const float Velocity = TrailCubeVelocities[i];
		const float RawX = ElapsedSeconds * Velocity + static_cast<float>(i) * 100.f;
		const float X = FMath::Fmod(RawX, WrapSpan) - WrapRangeCm;
		const float Y = FMath::Cos(ElapsedSeconds * 2.f + static_cast<float>(i)) * 100.f;
		const float Z = 80.f + FMath::Cos(ElapsedSeconds * 3.f + static_cast<float>(i)) * 15.f;
		const FTransform Xform(FRotator::ZeroRotator, FVector(X, Y, Z), FVector(0.5f));
		TrailCubeISMC->UpdateInstanceTransform(i, Xform, /*bWorldSpace=*/false, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);

		TrailCubeISMC->SetCustomDataValue(i, 0, ElapsedSeconds + static_cast<float>(i) * 0.1f, false);
	}
	TrailCubeISMC->MarkRenderStateDirty();
}
