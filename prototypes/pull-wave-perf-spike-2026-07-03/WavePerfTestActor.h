// PROTOTYPE - NOT FOR PRODUCTION
// Question: Can Pull-Wave PEAK ISMC load sustain 60fps + 0.25ms per-tick on iPhone 17 across thermal states?
// Date: 2026-07-03

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stats/Stats.h"
#include "WavePerfTestActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

DECLARE_STATS_GROUP(TEXT("PullWaveSpike"), STATGROUP_PullWaveSpike, STATCAT_Advanced);

UCLASS()
class AWavePerfTestActor : public AActor
{
	GENERATED_BODY()

public:
	AWavePerfTestActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> WaveMassISMC;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> TrailCubeISMC;

	UPROPERTY(EditDefaultsOnly, Category = "PullWaveSpike")
	int32 WaveMassCount = 16;

	UPROPERTY(EditDefaultsOnly, Category = "PullWaveSpike")
	int32 TrailCubeCount = 48;

	UPROPERTY(EditDefaultsOnly, Category = "PullWaveSpike")
	float CullDistanceEndCm = 3500.f;

	UPROPERTY(EditDefaultsOnly, Category = "PullWaveSpike")
	float VelocityMinCmS = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "PullWaveSpike")
	float VelocityMaxCmS = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "PullWaveSpike")
	float WrapRangeCm = 3500.f;

private:
	void PopulateISMC(UInstancedStaticMeshComponent* Component, int32 Count, int32 NumCustomDataFloats);
	void AnimateInstances(float ElapsedSeconds);

	float ElapsedTime = 0.f;
	TArray<float> WaveMassVelocities;
	TArray<float> TrailCubeVelocities;
};
