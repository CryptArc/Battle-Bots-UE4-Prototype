// // Copyright 2015 VMR Games, Inc. All Rights Reserved.

#include "BattleBots.h"
#include "PoisonSpell.h"



void APoisonSpell::PostInitializeComponents()
{
  Super::PostInitializeComponents();

  if (HasAuthority())
  {
    // Set the damage type of the spell to poison
    defaultDamageEvent.DamageTypeClass = UBBotDmgType_Poison::StaticClass();
    // Set the dot damage per poison tick
    poisonDotDamage = ProcessElementalDmg(GetPreProcessedDotDamage());
    // Acts as an offset to prevent edge cases with odd tick durations
    poisonDotDelay = poisonTick / 2;
  }
}

float APoisonSpell::ProcessElementalDmg(float initialDamage)
{
  if (GetSpellCaster()) {
    float dmgMod = 1 + FMath::Clamp(GetSpellCaster()->GetDamageModifier_Poison(), -1.f, 1.f);
    return FMath::Abs(initialDamage * dmgMod);
  }
  else {
    GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Caster is null - poison"));
    return initialDamage;
  }
}

float APoisonSpell::GetPreProcessedDotDamage()
{
  return spellDataInfo.spellDamage / poisonDuration;
}

FDamageEvent& APoisonSpell::GetDamageEvent()
{
  return defaultDamageEvent;
}

void APoisonSpell::DealDamage(ABBotCharacter* enemyPlayer)
{
  if (HasAuthority())
  {
    // Deal damage only on the server

    // Deal poison damage per poisonTick on the enemy player
    DealUniqueSpellFunctionality(enemyPlayer);

    DestroySpell();
  }
}

void APoisonSpell::DealUniqueSpellFunctionality(ABBotCharacter* enemyPlayer)
{
  if (HasAuthority())
  {
	  poisonDotDelegate.BindUObject(this, &APoisonSpell::PoisonEnemy, (ABBotCharacter*)enemyPlayer);
	
	  poisonDuration = GetFunctionalityDuration() + GetWorld()->GetTimeSeconds();
	  GetWorldTimerManager().SetTimer(poisonDotHandler, poisonDotDelegate, poisonTick, true, poisonDotDelay);
  }
}

void APoisonSpell::PoisonEnemy(ABBotCharacter* enemyPlayer)
{
  if (HasAuthority())
  {
	  if (poisonDuration <= GetWorld()->GetTimeSeconds())
	  {
	    GetWorldTimerManager().ClearTimer(poisonDotHandler);
	  }
    else if (enemyPlayer)
	  {
	    UGameplayStatics::ApplyDamage(enemyPlayer, poisonDotDamage, GetInstigatorController(), this, GetDamageType());
	  }
  }
}

float APoisonSpell::GetFunctionalityDuration()
{
  return GetClass()->GetDefaultObject<APoisonSpell>()->poisonDuration;
}