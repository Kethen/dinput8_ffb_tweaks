#ifndef __MODIFY_EFFECTS_H
#define __MODIFY_EFFECTS_H
#include <dinput.h>
#include <stdbool.h>

extern "C" {
	void bind_effect_modifier_to_hook();
	void log_effect(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items, bool should_log);
	void log_device_prop(LPGUID prop_guid, LPDIPROPHEADER propheader, bool should_log);
}

#endif
