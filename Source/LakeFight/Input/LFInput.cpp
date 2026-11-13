#include "Input/LFInput.h"

#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UObject/Package.h"

namespace LF::Input
{
	const FActions& Get()
	{
		static FActions Actions;
		if (!Actions.Context)
		{
			UObject* Outer = GetTransientPackage();
			Actions.Interact = NewObject<UInputAction>(Outer, TEXT("IA_LF_Interact_Runtime"));
			Actions.Interact->ValueType = EInputActionValueType::Boolean;
			Actions.Interact->AddToRoot();

			Actions.Swing = NewObject<UInputAction>(Outer, TEXT("IA_LF_Swing_Runtime"));
			Actions.Swing->ValueType = EInputActionValueType::Boolean;
			Actions.Swing->AddToRoot();

			Actions.Context = NewObject<UInputMappingContext>(Outer, TEXT("IMC_LF_Shared_Runtime"));
			Actions.Context->MapKey(Actions.Interact, EKeys::E);
			Actions.Context->MapKey(Actions.Interact, EKeys::Gamepad_FaceButton_Top);
			Actions.Context->MapKey(Actions.Swing, EKeys::LeftMouseButton);
			Actions.Context->MapKey(Actions.Swing, EKeys::Gamepad_RightTrigger);
			Actions.Context->AddToRoot();
		}
		return Actions;
	}
}
