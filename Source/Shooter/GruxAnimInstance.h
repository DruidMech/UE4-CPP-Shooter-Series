// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GruxAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTER_API UGruxAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement, meta = (AllowPrivateAccess = "true"))
	float Speed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class AEnemy* Enemy;
public:

	UFUNCTION(BlueprintCallable)
	void UpdateAnimationProperties(float DeltaTime);
};
