#include "Core/DBGameplayTags.h"

namespace DBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Move, "Input.Move", "Movement input (native binding).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Look, "Input.Look", "Camera input (native binding).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Jump, "Input.Jump", "Jump / double jump.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Sprint, "Input.Sprint", "Sprint ability input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Dodge, "Input.Dodge", "Dodge / roll / dash ability input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_LightAttack, "Input.LightAttack", "Light attack / combo.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_HeavyAttack, "Input.HeavyAttack", "Heavy / charged attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Block, "Input.Block", "Block and parry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Interact, "Input.Interact", "Interact with NPCs and objects.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_LockOn, "Input.LockOn", "Toggle target lock.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability1, "Input.Ability.1", "Class ability slot 1.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability2, "Input.Ability.2", "Class ability slot 2.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability3, "Input.Ability.3", "Class ability slot 3.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Ability4, "Input.Ability.4", "Class ability slot 4.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Character is dead.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Blocking, "State.Blocking", "Character is blocking.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ParryWindow, "State.ParryWindow", "Inside the perfect-parry window.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Invulnerability frames (dodge, cinematic).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "Performing an attack (movement input is ignored).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "Performing a dodge.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sprinting, "State.Sprinting", "Sprinting (stamina does not regenerate).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Staggered, "State.Staggered", "Poise broken / hit reaction, cannot act.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_KnockedDown, "State.KnockedDown", "Knocked to the ground, getting up.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CounterWindow, "State.CounterWindow", "After a perfect parry: the next attack is a counter.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_StaminaRegenDelay, "State.StaminaRegenDelay", "Stamina was just spent; regeneration paused.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_PoiseRecoverDelay, "State.PoiseRecoverDelay", "Poise was just damaged; recovery paused.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_DodgeRecovery, "State.DodgeRecovery", "Just finished a dodge: an attack now is a dash attack.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_DoubleJump, "Movement.DoubleJump", "Double jump unlocked.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Hit, "GameplayCue.Combat.Hit", "A hit dealt damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Blocked, "GameplayCue.Combat.Blocked", "A hit was blocked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Parried, "GameplayCue.Combat.Parried", "A hit was perfectly parried.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Stagger, "GameplayCue.Combat.Stagger", "Poise broken / hit reaction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Combat_Dodge, "GameplayCue.Combat.Dodge", "Dodge started.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack, "Ability.Attack", "Any attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Light, "Ability.Attack.Light", "Light attack / combo.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Heavy, "Ability.Attack.Heavy", "Heavy / charged attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Block, "Ability.Block", "Block / parry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dodge, "Ability.Dodge", "Dodge / roll / dash.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Sprint, "Ability.Sprint", "Sprint.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_HitReact, "Ability.HitReact", "Hit reaction (stagger, knockdown, parried).");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_HitReact, "Event.Combat.HitReact", "Victim must react; magnitude = EHitReaction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ParrySuccess, "Event.Combat.ParrySuccess", "Defender parried an attack perfectly.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "Base damage passed into the damage execution.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_PoiseDamage, "SetByCaller.PoiseDamage", "Poise damage passed into the damage execution.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_StaminaCost, "SetByCaller.StaminaCost", "Stamina spent by an action.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Knockdown, "Damage.Knockdown", "Hit knocks the target down regardless of poise.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Physical, "Damage.Type.Physical", "Physical damage (reduced by armor).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Fire, "Damage.Type.Fire", "Fire / ash damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Frost, "Damage.Type.Frost", "Frost damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Lightning, "Damage.Type.Lightning", "Thunder / lightning damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Shadow, "Damage.Type.Shadow", "Shadow damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Poison, "Damage.Type.Poison", "Poison / plague damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Spirit, "Damage.Type.Spirit", "Spiritual damage (monk, shrines).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_DarkBlood, "Damage.Type.DarkBlood", "Corruption of the Dark Blood.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Unblockable, "Damage.Unblockable", "Attack cannot be blocked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Unparryable, "Damage.Unparryable", "Attack cannot be parried.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Class_Warrior, "Class.Warrior", "Krieger (Warrior)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Class_Shadowrunner, "Class.Shadowrunner", "Schattenlaeufer (Shadowrunner)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Class_Mage, "Class.Mage", "Magier (Mage)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Class_Monk, "Class.Monk", "Moench (Monk)");
}
