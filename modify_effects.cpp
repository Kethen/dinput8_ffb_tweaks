#include "hooking.h"
#include "config.h"
#include <dinput.h>

extern "C" {
	void modify_effects(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items, bool download_hook){
	}
}
