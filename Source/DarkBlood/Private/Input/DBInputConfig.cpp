#include "Input/DBInputConfig.h"

#include "Core/DBGameplayTags.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

const UInputAction* UDBInputConfig::FindNativeInputAction(const FGameplayTag& InputTag) const
{
	for (const FDBInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}
	return nullptr;
}

namespace
{
	FDBInputAction MakeBinding(const UInputAction* Action, const FGameplayTag& Tag)
	{
		FDBInputAction Binding;
		Binding.InputAction = Action;
		Binding.InputTag = Tag;
		return Binding;
	}
}

UDBInputConfig* UDBInputConfig::CreateDevelopmentDefaults(UObject* Outer)
{
	UDBInputConfig* Config = NewObject<UDBInputConfig>(Outer, TEXT("DevInputConfig"), RF_Transient);
	UInputMappingContext* Context = NewObject<UInputMappingContext>(Config, TEXT("IMC_Dev_Default"), RF_Transient);
	Config->DefaultMappingContext = Context;

	auto MakeAction = [Config](const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Config, Name, RF_Transient);
		Action->ValueType = ValueType;
		return Action;
	};

	auto AddNegate = [Context](FEnhancedActionKeyMapping& Mapping, bool bX, bool bY)
	{
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Context);
		Negate->bX = bX;
		Negate->bY = bY;
		Negate->bZ = false;
		Mapping.Modifiers.Add(Negate);
	};

	auto AddSwizzle = [Context](FEnhancedActionKeyMapping& Mapping)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	};

	// Movement: WASD produce a 2D vector (X = right, Y = forward).
	UInputAction* Move = MakeAction(TEXT("IA_Dev_Move"), EInputActionValueType::Axis2D);
	AddSwizzle(Context->MapKey(Move, EKeys::W));
	{
		FEnhancedActionKeyMapping& S = Context->MapKey(Move, EKeys::S);
		AddSwizzle(S);
		AddNegate(S, true, true);
	}
	AddNegate(Context->MapKey(Move, EKeys::A), true, false);
	Context->MapKey(Move, EKeys::D);
	Context->MapKey(Move, EKeys::Gamepad_Left2D);
	Config->NativeInputActions.Add(MakeBinding(Move, DBTags::Input_Move));

	UInputAction* Look = MakeAction(TEXT("IA_Dev_Look"), EInputActionValueType::Axis2D);
	AddNegate(Context->MapKey(Look, EKeys::Mouse2D), false, true);
	Context->MapKey(Look, EKeys::Gamepad_Right2D);
	Config->NativeInputActions.Add(MakeBinding(Look, DBTags::Input_Look));

	UInputAction* Jump = MakeAction(TEXT("IA_Dev_Jump"), EInputActionValueType::Boolean);
	Context->MapKey(Jump, EKeys::SpaceBar);
	Context->MapKey(Jump, EKeys::Gamepad_FaceButton_Bottom);
	Config->NativeInputActions.Add(MakeBinding(Jump, DBTags::Input_Jump));

	struct FAbilityBinding
	{
		const TCHAR* Name;
		FGameplayTag Tag;
		FKey Keyboard;
		FKey Gamepad;
	};
	const FAbilityBinding AbilityBindings[] = {
		{TEXT("IA_Dev_Sprint"), DBTags::Input_Sprint, EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick},
		{TEXT("IA_Dev_Dodge"), DBTags::Input_Dodge, EKeys::LeftAlt, EKeys::Gamepad_FaceButton_Right},
		{TEXT("IA_Dev_LightAttack"), DBTags::Input_LightAttack, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Left},
		{TEXT("IA_Dev_HeavyAttack"), DBTags::Input_HeavyAttack, EKeys::F, EKeys::Gamepad_FaceButton_Top},
		{TEXT("IA_Dev_Block"), DBTags::Input_Block, EKeys::RightMouseButton, EKeys::Gamepad_LeftShoulder},
		{TEXT("IA_Dev_Interact"), DBTags::Input_Interact, EKeys::E, EKeys::Gamepad_DPad_Up},
		{TEXT("IA_Dev_LockOn"), DBTags::Input_LockOn, EKeys::MiddleMouseButton, EKeys::Gamepad_RightThumbstick},
		{TEXT("IA_Dev_Ability1"), DBTags::Input_Ability1, EKeys::One, EKeys::Gamepad_RightShoulder},
		{TEXT("IA_Dev_Ability2"), DBTags::Input_Ability2, EKeys::Two, EKeys::Gamepad_RightTrigger},
		{TEXT("IA_Dev_Ability3"), DBTags::Input_Ability3, EKeys::Three, EKeys::Gamepad_LeftTrigger},
		{TEXT("IA_Dev_Ability4"), DBTags::Input_Ability4, EKeys::Four, EKeys::Gamepad_DPad_Down},
	};
	for (const FAbilityBinding& Binding : AbilityBindings)
	{
		UInputAction* Action = MakeAction(Binding.Name, EInputActionValueType::Boolean);
		Context->MapKey(Action, Binding.Keyboard);
		Context->MapKey(Action, Binding.Gamepad);
		Config->AbilityInputActions.Add(MakeBinding(Action, Binding.Tag));
	}
	return Config;
}
