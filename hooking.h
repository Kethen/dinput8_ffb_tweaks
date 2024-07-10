#ifndef __HOOKING_H
#define __HOOKING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <dinput.h>
#include <stdbool.h>

void set_set_param_cb(void (__attribute__((stdcall)) *cb)(LPGUID effect_guid, LPDIEFFECT params, DWORD *dwFlags));
void set_create_effect_cb(void (__attribute__((stdcall)) *cb)(LPGUID effect_guid, LPDIEFFECT params));
bool guid_equal(const void *lhs, const void *rhs);
int hook_dinput8create();
int hook_create_device_A(LPDIRECTINPUT8A dinput8_interface_A);
int hook_create_device_W(LPDIRECTINPUT8W dinput8_interface_W);
int hook_create_effect_A(LPDIRECTINPUTDEVICE8A lplpDirectInputDevice);
int hook_create_effect_W(LPDIRECTINPUTDEVICE8W lplpDirectInputDevice);
int hook_download_set_A(LPDIRECTINPUTEFFECT ppdeff);
int hook_download_set_W(LPDIRECTINPUTEFFECT ppdeff);
HMODULE get_dinput8_handle();

#ifdef __cplusplus
};
#endif

#endif
