#ifndef __MODIFY_EFFECTS_H
#define __MODIFY_EFFECTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dinput.h>
#include <stdbool.h>
void modify_effects(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items, bool download_hook);

#ifdef __cplusplus
}
#endif

#endif
