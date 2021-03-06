// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Item.h"
#include "Ammo.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTER_API AAmmo : public AItem
{
	GENERATED_BODY()
public:
	AAmmo();

	virtual void Tick(float DeltaTime) override;

protected:

	virtual void BeginPlay() override;

	/** Override of SetItemProperties so we can set AmmoMesh properties */
	virtual void SetItemProperties(EItemState State) override;

private:
	/** Mesh for the ammo pickup */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Ammo, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* AmmoMesh;

public:
	FORCEINLINE UStaticMeshComponent* GetAmmoMesh() const { return AmmoMesh; }
	
};
