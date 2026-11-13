#pragma once

#include "CoreMinimal.h"

class UInputAction;
class UInputMappingContext;

/**
 * Runtime-created Enhanced Input actions shared by ALFBrawler and ALFBoat (no assets to author):
 * Interact (toggle drive/stand: E, gamepad face-top) and Swing (paddle: LMB, right trigger).
 * Movement/look/jump reuse the template's IA_Move / IA_Look / IA_MouseLook / IA_Jump assets.
 */
namespace LF::Input
{
	struct FActions
	{
		UInputAction* Interact = nullptr;
		UInputAction* Swing = nullptr;
		UInputAction* Look = nullptr; // gamepad right stick (Axis2D) - boat free-look
		UInputMappingContext* Context = nullptr;
		UInputMappingContext* BoatContext = nullptr; // driving only: right-stick free-look
	};

	/** Created once (rooted, transient) on first use. */
	LAKEFIGHT_API const FActions& Get();
}
