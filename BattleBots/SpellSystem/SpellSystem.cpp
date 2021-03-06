// Copyright 2015 VMR Games, Inc. All Rights Reserved.

#include "BattleBots.h"
#include "BattleBotsGameMode.h"
#include "Character/BBotCharacter.h"
#include "SpellSystem.h"


// Sets default values
ASpellSystem::ASpellSystem()
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;

  //Must be true for an Actor to replicate anything
  bReplicates = true;
  bReplicateMovement = true;
  bAlwaysRelevant = true;

  collisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisonComp"));
  collisionComp->OnComponentBeginOverlap.AddDynamic(this, &ASpellSystem::OnCollisionOverlapBegin);
  collisionComp->OnComponentEndOverlap.AddDynamic(this, &ASpellSystem::OnCollisionOverlapEnd);
  RootComponent = collisionComp;

  spellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpellMesh"));
  spellMesh->AttachParent = RootComponent;

  particleComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Particle Comp"));
  particleComp->AttachTo(RootComponent);
  particleComp->bAutoActivate = false;
  particleComp->bAutoDestroy = false;

  audioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComp"));
  audioComp->bAutoActivate = false;
  audioComp->bAutoDestroy = false;
  audioComp->AttachParent = RootComponent;

  projectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
  projectileMovementComp->ProjectileGravityScale = 0;
}

// Called after all components have been initialized with default values
void ASpellSystem::PostInitializeComponents()
{
  Super::PostInitializeComponents();

  // @TODO: Does not replicate ever tick when set on the server, set initial speed under constructor
  // Initialize the spell speed, by changing velocity to direction. Directly changing PMC->initialSpeed is a known engine bug.
  //projectileMovementComp->Velocity = projectileMovementComp->Velocity.GetSafeNormal() * spellDataInfo.spellSpeed;

  if (HasAuthority())
  {
    // Sets the default damage event type
    defaultDamageEvent.DamageTypeClass = UDamageType::StaticClass();

    SetDamageToDeal(spellDataInfo.spellDamage);

    // Sets the spell dps (Used for AOETicks) - Possible bug if DerivedClasses call Super::PostInitializeComponents() last
    damagePerSecond = GetDamageToDeal() / spellDataInfo.spellDuration;
    GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow, FString::FromInt(damagePerSecond * 100));
  }
}


// Called when the game starts or when spawned
void ASpellSystem::BeginPlay()
{
  Super::BeginPlay();

  if (HasAuthority())
  {
    if (!GetSpellCaster()) {
      // Check if spell caster is set under server
      GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Yellow, TEXT("Spell caster is NULL"));
    }
  }
}

// Called when a spell collides with a player
void ASpellSystem::OnCollisionOverlapBegin(class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
  ABBotCharacter* enemyPlayer = Cast<ABBotCharacter>(OtherActor);
  //@todo: Set spells to ignore each other in editor
  ASpellSystem* otherSpell = Cast<ASpellSystem>(OtherActor);

  if (IsEnemy(enemyPlayer)) {
    if (!OverlappedActors.Contains(enemyPlayer))
    {
      DealDamage(enemyPlayer);
      // The actor is removed on overlap end
      OverlappedActors.AddUnique(enemyPlayer);
    }
  }
  else if (!enemyPlayer && !otherSpell)
  {
    // If the spell collides with the wall destroy it
    DestroySpell();
  }
}

void ASpellSystem::OnCollisionOverlapEnd(class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
  ABBotCharacter* enemyPlayer = Cast<ABBotCharacter>(OtherActor);

  if (enemyPlayer)
  {
    OverlappedActors.Remove(enemyPlayer);
  }
}

bool ASpellSystem::IsEnemy(ABBotCharacter* possibleEnemy)
{
  if (Role < ROLE_Authority)
  {
    ServerIsEnemy(possibleEnemy);
  }
  else
  {
    return (possibleEnemy
         && possibleEnemy != GetSpellCaster()
         && possibleEnemy->CanRecieveDamage(Instigator->Controller, GetDamageType()));
  }
  return false;
}

void ASpellSystem::ServerIsEnemy_Implementation(ABBotCharacter* possibleEnemy)
{
  IsEnemy(possibleEnemy);
}

bool ASpellSystem::ServerIsEnemy_Validate(ABBotCharacter* possibleEnemy)
{
  return true;
}

// Deal basic projectile functionality and damage
void ASpellSystem::DealDamage(ABBotCharacter* enemyPlayer)
{
  if (HasAuthority())
  {
    if (spellDataInfo.bKnockBack)
    {
      enemyPlayer->KnockbackPlayer(GetActorLocation());
    }

    UGameplayStatics::ApplyDamage(enemyPlayer, GetDamageToDeal(), GetInstigatorController(), this, GetDamageEvent().DamageTypeClass);
    DealUniqueSpellFunctionality(enemyPlayer);
    DestroySpell();
  }
}

void ASpellSystem::DealUniqueSpellFunctionality(ABBotCharacter* enemyPlayer)
{
  // Must be overriden -  Ignite , Slow, Heal, KnockBack,etc
}

float ASpellSystem::GetSpellCost() const
{
  return spellDataInfo.spellCost;
}

float ASpellSystem::GetCastTime() const
{
  return spellDataInfo.castTime;
}

// No processing required for default damage types
float ASpellSystem::ProcessElementalDmg(float initialDamage)
{
  return initialDamage;
}

FVector ASpellSystem::GetSpellSpawnLocation()
{
  return GetSpellCaster()->GetActorLocation();
}

void ASpellSystem::SpawnSpell(TSubclassOf<ASpellSystem> tempSpell)
{
  // Check to see if our spell object is valid and not deleted by GC
  if (this->IsValidLowLevel()) {

    if (HasAuthority()) {

      UWorld* const World = GetWorld();
      float currentTime = World ? World->GetTimeSeconds() : 0.1f;
      if (CDHelper < currentTime) {
        // Check if the cool down timer is up before casting
        SpawnSpell_Internal(tempSpell);
        CDHelper = currentTime + spellDataInfo.coolDown;
      }
    }
  }
}

void ASpellSystem::SpawnSpell_Internal(TSubclassOf<ASpellSystem> tempSpell)
{
  if (HasAuthority())
  {
    if (GetSpellCaster()) {
      FActorSpawnParameters spawnInfo;
      spawnInfo.Owner = GetOwner();
      spawnInfo.Instigator = Instigator;
      spawnInfo.bNoCollisionFail = true;

      // Spawn the spell into the world
      spellSpawner = GetWorld()->SpawnActor<ASpellSystem>(tempSpell,
                                                          GetSpellSpawnLocation(),
                                                          GetSpellCaster()->GetActorRotation(),
                                                          spawnInfo);

      // Process spell destruction timers
      ProcessSpellTimers();
    }
  }
}


void ASpellSystem::ProcessSpellTimers()
{
  /*A Handle to manage the FX/Destruction timer.
  * A new handle must be created every time to prevent
  * endless timer reset with new spell spawns */
  FTimerHandle SpellDestructionHandle;

  // Prevents double calls of Simulate explosion from the initial timer
  if (spellDataInfo.bIsPiercing)
  {
    // If piercing then simulate explosion after its duration is up.
    GetWorldTimerManager().SetTimer(SpellDestructionHandle, spellSpawner, &ASpellSystem::SimulateExplosion, spellDataInfo.spellDuration, false);
  }
  // Destroy the spell after its duration is up
  spellSpawner->SetLifeSpan(GetFunctionalityDuration() + spellDataInfo.spellDuration);
}

FDamageEvent& ASpellSystem::GetDamageEvent()
{
  return defaultDamageEvent;
}

float ASpellSystem::GetFunctionalityDuration()
{
  return 0.1;
}

void ASpellSystem::DestroySpell()
{
  if (HasAuthority())
  {
    if (!spellDataInfo.bIsPiercing) {
      // If the spell is not piercing then destroy spell at contact
      SimulateExplosion();
    }
  }
}

void ASpellSystem::SimulateExplosion_Implementation()
{
  // Multicast function that runs on both the client and the server
  SetActorEnableCollision(false);
  SetActorHiddenInGame(true);

  // Play sound and particle effect on contact
  if (explosionSound) {
    UGameplayStatics::PlaySoundAtLocation(this, explosionSound, GetActorLocation());
  }
  if (spellFX) {
    UGameplayStatics::SpawnEmitterAtLocation(this, spellFX, GetActorLocation(), GetActorRotation());
  }
}

float ASpellSystem::GetPreProcessedDotDamage()
{
  return spellDataInfo.spellDamage;
}

float ASpellSystem::GetDamageToDeal()
{
  return damageToDeal;
}

void ASpellSystem::SetDamageToDeal(float newDmg)
{
  if (HasAuthority()){
    damageToDeal = newDmg;
  }
}

void ASpellSystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);

  // Value is already updated locally, so we may skip it in replication step for the owner only
  DOREPLIFETIME_CONDITION(ASpellSystem, damageToDeal, COND_OwnerOnly);
  DOREPLIFETIME_CONDITION(ASpellSystem, CDHelper, COND_OwnerOnly);
}

void ASpellSystem::AOETick()
{
  if (HasAuthority())
  {
    // No point in getting all overlapping actors if our initial collision has not picked up anything
    if (OverlappedActors.Num() == 0)
      return;

    // Deal damage to the overlapped actors
    for (int i = 0; i < OverlappedActors.Num(); i++)
    {
      //The enemy will never be the spellcaster, because we already did that check on collision overlap.
      ABBotCharacter* enemy = Cast<ABBotCharacter>(OverlappedActors[i]);

      // Only apply damage if the actors capsule component is overlapping
      if (enemy && collisionComp->IsOverlappingComponent(enemy->GetCapsuleComponent()))
      {
        /* Because we are checking collision and dealing damage in tick, we
        have to call DealDamage to ensure the enemy players get ignited. */
        DealDamage(enemy);

        //@TODO: ApplyRadialDamage is an alternative method, but would have to rewire dealdamage
      }
    }
  }
}

void ASpellSystem::Reset()
{
  Super::Reset();

  SetActorEnableCollision(false);
  SetActorHiddenInGame(true);
}

void ASpellSystem::Reset_Implementation()
{
  Reset();
  Destroy();
}

void ASpellSystem::ClearUniqueTimers()
{
  // Must be overriden
}








