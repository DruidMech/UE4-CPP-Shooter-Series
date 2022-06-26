// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "EnemyController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SphereComponent.h"
#include "ShooterCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkeletalMeshSocket.h"

// Sets default values
AEnemy::AEnemy() :
	Health(100.f),
	MaxHealth(100.f),
	HealthBarDisplayTime(4.f),
	bCanHitReact(true),
	HitReactTimeMin(.5f),
	HitReactTimeMax(3.f),
	HitNumberDestroyTime(1.5f),
	bStunned(false),
	StunChance(0.5f),
	AttackLNormal(TEXT("AttackLNormal")),
	AttackLFast(TEXT("AttackLFast")),
	AttackRNormal(TEXT("AttackRNormal")),
	AttackRFast(TEXT("AttackRFast")),
	BaseDamage(20.f),
	LeftWeaponSocket(TEXT("FX_Trail_L_01")),
	RightWeaponSocket(TEXT("FX_Trail_R_01")),
	bCanAttack(true),
	AttackWaitTime(1.5f),
	bDying(false)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AgroSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AgroSphere"));
	AgroSphere->SetupAttachment(GetRootComponent());

	CombatRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CombatSphere"));
	CombatRangeSphere->SetupAttachment(GetRootComponent());

	LeftWeaponCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("Left Weapon Collision"));
	LeftWeaponCollision->SetupAttachment(GetMesh(), FName("LeftWeaponBone"));

	RightWeaponCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("Right Weapon Collision"));
	RightWeaponCollision->SetupAttachment(GetMesh(), FName("RightWeaponBone"));
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	AgroSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::AgroSphereOverlap);

	CombatRangeSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::CombatRangeSphereOverlap);
	CombatRangeSphere->OnComponentEndOverlap.AddDynamic(this, &AEnemy::CombatRangeSphereEndOverlap);

	LeftWeaponCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::OnLeftWeaponOverlap);
	RightWeaponCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::AEnemy::OnRightWeaponOverlap);

	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftWeaponCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	LeftWeaponCollision->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	LeftWeaponCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightWeaponCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	RightWeaponCollision->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	RightWeaponCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);


	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);


	FVector WorldPatrolPoint = UKismetMathLibrary::TransformLocation(GetActorTransform(), PatrolPoint);
	FVector WorldPatrolPoint2 = UKismetMathLibrary::TransformLocation(GetActorTransform(), PatrolPoint2);

	EnemyController = Cast<AEnemyController>(GetController());

	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(FName("CanAttack"), true);
		
		EnemyController->GetBlackboardComponent()->SetValueAsVector(TEXT("PatrolPoint"), WorldPatrolPoint);
		DrawDebugSphere(GetWorld(), WorldPatrolPoint, 25, 100, FColor::Red, true);

		EnemyController->GetBlackboardComponent()->SetValueAsVector(TEXT("PatrolPoint2"), WorldPatrolPoint2);
		DrawDebugSphere(GetWorld(), WorldPatrolPoint2, 25, 100, FColor::Red, true);


		EnemyController->RunBehaviorTree(BehaviorTree);
	}
}

void AEnemy::ShowHealthBar_Implementation()
{
	GetWorldTimerManager().ClearTimer(HealthBarTimer);
	GetWorldTimerManager().SetTimer(
		HealthBarTimer,
		this,
		&AEnemy::HideHealthBar,
		HealthBarDisplayTime);
}

void AEnemy::Die()
{
	if (bDying) return;
	
	bDying = true;
	HideHealthBar();
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && DeathMontage)
	{
		AnimInstance->Montage_Play(DeathMontage);
	}

	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(FName("Dead"), true);
		EnemyController->StopMovement();
	}
}

void AEnemy::PlayHitMontage(FName Section, float PlayRate)
{
	if (bCanHitReact)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Play(HitMontage, PlayRate);
			AnimInstance->Montage_JumpToSection(Section, HitMontage);
		}

		bCanHitReact = false;
		const float HitReactTime{FMath::FRandRange(HitReactTimeMin, HitReactTimeMax)};
		GetWorldTimerManager().SetTimer(
			HitReactTimer,
			this,
			&AEnemy::ResetHitReactTimer,
			HitReactTime);
	}
}

void AEnemy::ResetHitReactTimer()
{
	bCanHitReact = true;
}

void AEnemy::StoreHitNumber(UUserWidget* HitNumber, FVector Location)
{
	HitNumbers.Add(HitNumber, Location);

	FTimerHandle HitNumberTimer;
	FTimerDelegate HitNumberDelegate;
	HitNumberDelegate.BindUFunction(this, FName("DestroyHitNumber"), HitNumber);
	GetWorld()->GetTimerManager().SetTimer(
		HitNumberTimer,
		HitNumberDelegate,
		HitNumberDestroyTime,
		false);
}

void AEnemy::DestroyHitNumber(UUserWidget* HitNumber)
{
	HitNumbers.Remove(HitNumber);
	HitNumber->RemoveFromParent();
}

void AEnemy::UpdateHitNumbers()
{
	for (auto& HitPair : HitNumbers)
	{
		UUserWidget* HitNumber{HitPair.Key};
		const FVector Location{HitPair.Value};
		FVector2D ScreenPosition;
		UGameplayStatics::ProjectWorldToScreen(
			GetWorld()->GetFirstPlayerController(),
			Location,
			ScreenPosition);
		HitNumber->SetPositionInViewport(ScreenPosition);
	}
}

void AEnemy::SetStunned(bool Stunned)
{
	bStunned = Stunned;
	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool("Stunned", Stunned);
	}
}

void AEnemy::AgroSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                               const FHitResult& SweepResult)
{
	if (OtherActor == nullptr)
	{
		return;
	}

	AShooterCharacter* Character = Cast<AShooterCharacter>(OtherActor);
	if (Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("Here"));
		EnemyController->GetBlackboardComponent()->SetValueAsObject(TEXT("Target"), Character);
	}
}

void AEnemy::CombatRangeSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                      const FHitResult& SweepResult)
{
	if (OtherActor == nullptr)
	{
		return;
	}

	auto ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter)
	{
		bAttackRange = true;

		if (EnemyController)
		{
			EnemyController->GetBlackboardComponent()->SetValueAsBool(TEXT("Is In Attack Range"), true);
		}
	}
}

void AEnemy::CombatRangeSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == nullptr)
	{
		return;
	}

	auto ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter)
	{
		bAttackRange = false;

		if (EnemyController)
		{
			EnemyController->GetBlackboardComponent()->SetValueAsBool(TEXT("Is In Attack Range"), false);
		}
	}
}

void AEnemy::PlayAttackMontage(FName Section, float PlayRate)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && AttackMontage)
	{
		AnimInstance->Montage_Play(AttackMontage);
		AnimInstance->Montage_JumpToSection(Section, AttackMontage);
	}
	bCanAttack = false;
	GetWorldTimerManager().SetTimer(
		AttackWaitTimer,
		this,
		&AEnemy::ResetCanAttack,
		AttackWaitTime);
	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(FName("CanAttack"), false);
	}
}

FName AEnemy::GetAttackSectionName()
{
	FName SectionName;
	const int32 Section{FMath::RandRange(1, 4)};
	switch (Section)
	{
	case 1:
		SectionName = AttackLFast;
		break;
	case 2:
		SectionName = AttackLNormal;
		break;
	case 3:
		SectionName = AttackRFast;
		break;
	case 4:
		SectionName = AttackRNormal;
	}
	return SectionName;
}

void AEnemy::DoDamage(AShooterCharacter* Victim)
{
	if (Victim == nullptr)
	{
		return;
	}
	
	UGameplayStatics::ApplyDamage(Victim, BaseDamage, EnemyController, this, UDamageType::StaticClass());
}

void AEnemy::SpawnBloodParticle(const AShooterCharacter* ShooterCharacter, FName SocketName)
{
	const USkeletalMeshSocket* TipSocket{GetMesh()->GetSocketByName(SocketName)};
	if (TipSocket)
	{
		const FTransform SocketTransform{TipSocket->GetSocketTransform(GetMesh())};
		if (ShooterCharacter->GetBloodParticle())
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				ShooterCharacter->GetBloodParticle(),
				SocketTransform
				);
		}
	}
}

void AEnemy::StunCharacter(AShooterCharacter* Victim)
{
	if (Victim)
	{
		const float Stun = FMath::FRandRange(0.f, 1.f);
		if (Stun <= Victim->GetStunChance())
		{
			Victim->Stun();
		}
	}
}

void AEnemy::ResetCanAttack()
{
	bCanAttack = true;
	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsBool(FName("CanAttack"), true);
	}
}

void AEnemy::DestroyEnemy()
{
	Destroy();
}

void AEnemy::FinishDeath()
{
	GetMesh()->bPauseAnims = true;
	GetWorldTimerManager().SetTimer(DeathTimer, this, &AEnemy::DestroyEnemy, DeathTime);
}

void AEnemy::OnLeftWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                 UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                 const FHitResult& SweepResult)
{
	auto ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if(ShooterCharacter)
	{
		DoDamage(ShooterCharacter);
		SpawnBloodParticle(ShooterCharacter, LeftWeaponSocket);
		StunCharacter(ShooterCharacter);
	}
}

void AEnemy::OnRightWeaponOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                  const FHitResult& SweepResult)
{
	auto ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if(ShooterCharacter)
	{
		DoDamage(ShooterCharacter);
		SpawnBloodParticle(ShooterCharacter, RightWeaponSocket);
		StunCharacter(ShooterCharacter);
	}
}

void AEnemy::ActivateLeftWeaponCollision()
{
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AEnemy::DeactivateLeftWeaponCollision()
{
	LeftWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::ActivateRightWeaponCollision()
{
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AEnemy::DeactivateRightWeaponCollision()
{
	RightWeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateHitNumbers();
}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AEnemy::BulletHit_Implementation(FHitResult HitResult)
{
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
	if (ImpactParticles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, HitResult.Location, FRotator(0.f), true);
	}
	ShowHealthBar();

	float Stunned = FMath::FRandRange(0.f, 1.f);
	if (Stunned < StunChance && !bDying)
	{
		SetStunned(true);
		PlayHitMontage(FName("HitReactFront"));
	}
}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
                         AActor* DamageCauser)
{
	if (EnemyController)
	{
		EnemyController->GetBlackboardComponent()->SetValueAsObject(FName("Target"), DamageCauser);
	}
	
	if (Health - DamageAmount <= 0.f)
	{
		Health = 0.f;
		Die();
	}
	else
	{
		Health -= DamageAmount;
	}
	
	return DamageAmount;
}
