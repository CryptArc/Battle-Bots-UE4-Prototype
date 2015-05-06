// Copyright 2015 VMR Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "SpellSystem.generated.h"

class ABBotCharacter;

USTRUCT()
struct FSpellData{
  GENERATED_USTRUCT_BODY()

  UPROPERTY(EditDefaultsOnly, Category = SpellInfo)
    FName spellName;
  UPROPERTY(EditDefaultsOnly, Category = SpellInfo)
    FString toolTip;
  UPROPERTY(EditDefaultsOnly, Category = SpellInfo)
    UTexture2D* icon;
  UPROPERTY(EditDefaultsOnly, Category = Config)
    int32 spellDamage;
  UPROPERTY(EditDefaultsOnly, Category = Config)
    float spellCost;
  UPROPERTY(EditDefaultsOnly, Category = Config)
    float spellDuration;
  UPROPERTY(EditDefaultsOnly, Category = Config)
    float spellSpeed;
  UPROPERTY(EditDefaultsOnly, Category = Config)
    float coolDown;
  UPROPERTY(EditDefaultsOnly, Category = Config)
    float castTime;
  UPROPERTY(EditDefaultsOnly, Category = Config)
    bool isPiercing;
};

UCLASS()
class BATTLEBOTS_API ASpellSystem : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASpellSystem();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

  /** An object that holds our spell configurations*/
  UPROPERTY(EditDefaultsOnly, Category = Config)
  FSpellData spellDataInfo;
  
  /** A simple collision primitive to use as the root component*/
  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Collision")
  USphereComponent* collisionComp;

  /** UStaticMeshComponent to represent the spell in the world.*/
  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Config")
  UStaticMeshComponent* spellMesh;

  UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PhysicsConfig")
  UProjectileMovementComponent* projectileMovementComp;

  UPROPERTY(EditDefaultsOnly, Category = "Effects")
  UParticleSystemComponent* spellFX;

  UPROPERTY(VisibleDefaultsOnly)
  UAudioComponent* audioComp;

  UPROPERTY(EditDefaultsOnly, Category = "Effects")
  USoundCue* explosionSound;

  // Is called when a spell collides with a player
  UFUNCTION()
  void OnCollisionOverlapBegin(class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

  UFUNCTION(BlueprintCallable, Category = "SpellSystem")
  float GetSpellCost() const;

  // Parents this spell to the caster actor
  void SetCaster(ABBotCharacter* caster);

  /** A wrapper function that determines what type of spell to cast AOE, Instant...*/
  UFUNCTION(BlueprintCallable, Category = "SpellSystem")
    void CastSpell();

protected:
  // A reference to the caster of the spell
  ABBotCharacter* Caster;

private:

  // Server side RPC for cast spell
  UFUNCTION(Reliable, Server, WithValidation)
  void ServerCastSpell();
  virtual void ServerCastSpell_Implementation();
  virtual bool ServerCastSpell_Validate();
  
  // Destroys spell after reaching a certain range or if it collides
  void DestroySpell();
	
};
