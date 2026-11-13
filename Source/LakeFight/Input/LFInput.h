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
		UInputMappingContext* Context = nullptr;
	};

	/** Created once (rooted, transient) on first use. */
	LAKEFIGHT_API const FActions& Get();
}
