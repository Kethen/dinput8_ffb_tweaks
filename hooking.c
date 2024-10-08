// https://github.com/TsudaKageyu/minhook
#include "MinHook.h"

// windows
#include <windows.h>

// dinput
#include <dinput.h>

// std
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// unix ish
#include <unistd.h>

#include "logging.h"

static int init_mh(){
	int ret = MH_Initialize();
	if(ret != MH_OK && ret != MH_ERROR_ALREADY_INITIALIZED){
		LOG("Failed initializing MinHook, %d\n", ret);
		return -1;
	}
	return 0;
}

bool guid_equal(const void *lhs, const void *rhs){
	return memcmp(lhs, rhs, sizeof(GUID)) == 0;
}

// step 1, hook DirectInput8Create to collect instances of LPDIRECTINPUT8A and LPDIRECTINPUT8W
// step 2, on DirectInput8Create, hook either variant of CreateDevice
// step 3, on CreateDevice, hook either variants of CreateEffect, likely merges here but you never know
// step 4, on CreateEffect, hook Download
// step 5, on Download, fire hook over at cpp bits with access to config

static void (__attribute__((stdcall)) *set_param_cb)(LPGUID effect_guid, LPDIEFFECT params, DWORD *dwFlags) = NULL;
void set_set_param_cb(void (__attribute__((stdcall)) *cb)(LPGUID effect_guid, LPDIEFFECT params, DWORD *dwFlags)){
	set_param_cb = cb;
}

void common_set_hook(LPDIRECTINPUTEFFECT object, LPDIEFFECT peff, DWORD *dwFlags){
	GUID effect;
	object->lpVtbl->GetEffectGuid(object, &effect);
	if(set_param_cb != NULL){
		set_param_cb(&effect, peff, dwFlags);
	}
}

// only the SetParametersW hook is used now, there should not be a difference
HRESULT (__attribute__((stdcall)) *SetParametersA_orig)(LPDIRECTINPUTEFFECT object, LPDIEFFECT peff, DWORD dwFlags) = NULL;
HRESULT __attribute__((stdcall)) SetParametersA_patched(LPDIRECTINPUTEFFECT object, LPDIEFFECT peff, DWORD dwFlags){
	LOG_VERBOSE("%s: object 0x%08x\n", __func__, object);
	common_set_hook(object, peff, &dwFlags);
	return SetParametersA_orig(object, peff, dwFlags);
}

HRESULT (__attribute__((stdcall)) *SetParametersW_orig)(LPDIRECTINPUTEFFECT object, LPDIEFFECT peff, DWORD dwFlags) = NULL;
HRESULT __attribute__((stdcall)) SetParametersW_patched(LPDIRECTINPUTEFFECT object, LPDIEFFECT peff, DWORD dwFlags){
	LOG_VERBOSE("%s: object 0x%08x\n", __func__, object);
	common_set_hook(object, peff, &dwFlags);
	return SetParametersW_orig(object, peff, dwFlags);
}

void common_download_hook(LPDIRECTINPUTEFFECT object){
	return;
	// undecided, some flags could make doing it here funky, might remove the hook
}

// only the DownloadW hook is used now, there should not be a difference
HRESULT (__attribute__((stdcall)) *DownloadA_orig)(LPDIRECTINPUTEFFECT object);
HRESULT __attribute__((stdcall)) DownloadA_patched(LPDIRECTINPUTEFFECT object){
	//LOG_VERBOSE("%s: object 0x%08x\n", __func__, object);
	common_download_hook(object);
	HRESULT ret = DownloadA_orig(object);
	return ret;
}

HRESULT (__attribute__((stdcall)) *DownloadW_orig)(LPDIRECTINPUTEFFECT object);
HRESULT __attribute__((stdcall)) DownloadW_patched(LPDIRECTINPUTEFFECT object){
	//LOG_VERBOSE("%s: object 0x%08x\n", __func__, object);
	common_download_hook(object);
	HRESULT ret = DownloadW_orig(object);
	return ret;
}

int hook_download_set_A(LPDIRECTINPUTEFFECT ppdeff){
	if(init_mh() != 0){
		return -1;
	}

	int mhret = MH_CreateHook(ppdeff->lpVtbl->Download, (LPVOID)&DownloadA_patched, (LPVOID *)&DownloadA_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for DownloadA, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(ppdeff->lpVtbl->Download);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for DownloadA, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked DownloadA at 0x%08x\n", ppdeff->lpVtbl->Download);

	mhret = MH_CreateHook(ppdeff->lpVtbl->SetParameters, (LPVOID)&SetParametersA_patched, (LPVOID *)&SetParametersA_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for SetParametersA, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(ppdeff->lpVtbl->SetParameters);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for SetParametersA, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked SetParametersA at 0x%08x\n", ppdeff->lpVtbl->SetParameters);

	return 0;
}

int hook_download_set_W(LPDIRECTINPUTEFFECT ppdeff){
	if(init_mh() != 0){
		return -1;
	}

	int mhret = MH_CreateHook(ppdeff->lpVtbl->Download, (LPVOID)&DownloadW_patched, (LPVOID *)&DownloadW_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for DownloadW, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(ppdeff->lpVtbl->Download);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for DownloadW, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked DownloadW at 0x%08x\n", ppdeff->lpVtbl->Download);

	mhret = MH_CreateHook(ppdeff->lpVtbl->SetParameters, (LPVOID)&SetParametersW_patched, (LPVOID *)&SetParametersW_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for SetParametersW, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(ppdeff->lpVtbl->SetParameters);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for SetParametersW, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked SetParametersW at 0x%08x\n", ppdeff->lpVtbl->SetParameters);

	return 0;
}

static void (__attribute__((stdcall)) *set_device_property_cb)(LPGUID prop_guid, LPDIPROPHEADER propheader) = NULL;
void set_set_device_property_cb(void (__attribute__((stdcall)) *cb)(LPGUID prop_guid, LPDIPROPHEADER propheader)){
	set_device_property_cb = cb;
}

HRESULT (__attribute__((stdcall)) *SetPropertyA_orig)(LPDIRECTINPUTDEVICE8A object, LPGUID rguidProp, LPDIPROPHEADER pdiph);
HRESULT __attribute__((stdcall)) SetPropertyA_patched(LPDIRECTINPUTDEVICE8A object, LPGUID rguidProp, LPDIPROPHEADER pdiph){
	LOG_VERBOSE("%s: object 0x%08x, rguidProp 0x%08x, pdiph 0x%08x\n", __func__, object, rguidProp, pdiph);
	if(set_device_property_cb != NULL){
		set_device_property_cb(rguidProp, pdiph);
	}

	return SetPropertyA_orig(object, rguidProp, pdiph);
}

HRESULT (__attribute__((stdcall)) *SetPropertyW_orig)(LPDIRECTINPUTDEVICE8W object, LPGUID rguidProp, LPDIPROPHEADER pdiph);
HRESULT __attribute__((stdcall)) SetPropertyW_patched(LPDIRECTINPUTDEVICE8W object, LPGUID rguidProp, LPDIPROPHEADER pdiph){
	LOG_VERBOSE("%s: object 0x%08x, rguidProp 0x%08x, pdiph 0x%08x\n", __func__, object, rguidProp, pdiph);
	if(set_device_property_cb != NULL){
		set_device_property_cb(rguidProp, pdiph);
	}

	return SetPropertyW_orig(object, rguidProp, pdiph);
}

static void (__attribute__((stdcall)) *create_effect_cb)(LPGUID effect_guid, LPDIEFFECT params) = NULL;
void set_create_effect_cb(void (__attribute__((stdcall)) *cb)(LPGUID effect_guid, LPDIEFFECT params)){
	create_effect_cb = cb;
}

bool download_hooked = false;

HRESULT (__attribute__((stdcall)) *CreateEffectA_orig)(LPDIRECTINPUTDEVICE8A object, LPGUID rguid, LPDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter);
HRESULT __attribute__((stdcall)) CreateEffectA_patched(LPDIRECTINPUTDEVICE8A object, LPGUID rguid, LPDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter){
	LOG_VERBOSE("%s: object 0x%08x, rguid 0x%08x\n", __func__, object, rguid);

	if(create_effect_cb != NULL && lpeff != NULL){
		create_effect_cb(rguid, lpeff);
	}

	HRESULT ret = CreateEffectA_orig(object, rguid, lpeff, ppdeff, punkOuter);
	if(download_hooked){
		return ret;
	}
	if(ret == DI_OK){
		download_hooked = hook_download_set_W(*ppdeff) == 0;
	}
	return ret;
}

HRESULT (__attribute__((stdcall)) *CreateEffectW_orig)(LPDIRECTINPUTDEVICE8W object, LPGUID rguid, LPDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter);
HRESULT __attribute__((stdcall)) CreateEffectW_patched(LPDIRECTINPUTDEVICE8W object, LPGUID rguid, LPDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter){
	LOG_VERBOSE("%s: object 0x%08x, rguid 0x%08x\n", __func__, object, rguid);

	if(create_effect_cb != NULL && lpeff != NULL){
		create_effect_cb(rguid, lpeff);
	}

	HRESULT ret = CreateEffectW_orig(object, rguid, lpeff, ppdeff, punkOuter);
	if(download_hooked){
		return ret;
	}
	if(ret == DI_OK){
		download_hooked = hook_download_set_W(*ppdeff) == 0;
	}
	return ret;
}

int hook_create_effect_A(LPDIRECTINPUTDEVICE8A lplpDirectInputDevice){
	if(init_mh() != 0){
		return -1;
	}

	int mhret = MH_CreateHook(lplpDirectInputDevice->lpVtbl->CreateEffect, (LPVOID)&CreateEffectA_patched, (LPVOID *)&CreateEffectA_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for CreateEffectA, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(lplpDirectInputDevice->lpVtbl->CreateEffect);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for CreateEffectA, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked CreateEffectA at 0x%08x\n", lplpDirectInputDevice->lpVtbl->CreateEffect);

	mhret = MH_CreateHook(lplpDirectInputDevice->lpVtbl->SetProperty, (LPVOID)&SetPropertyA_patched, (LPVOID *)&SetPropertyA_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for SetPropertyA, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(lplpDirectInputDevice->lpVtbl->SetProperty);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for SetPropertyA, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked SetPropertyA at 0x%08x\n", lplpDirectInputDevice->lpVtbl->SetProperty);

	return 0;
}

int hook_create_effect_W(LPDIRECTINPUTDEVICE8W lplpDirectInputDevice){
	if(init_mh() != 0){
		return -1;
	}

	int mhret = MH_CreateHook(lplpDirectInputDevice->lpVtbl->CreateEffect, (LPVOID)&CreateEffectW_patched, (LPVOID *)&CreateEffectW_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for CreateEffectW, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(lplpDirectInputDevice->lpVtbl->CreateEffect);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for CreateEffectW, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked CreateEffectW at 0x%08x\n", lplpDirectInputDevice->lpVtbl->CreateEffect);

	mhret = MH_CreateHook(lplpDirectInputDevice->lpVtbl->SetProperty, (LPVOID)&SetPropertyW_patched, (LPVOID *)&SetPropertyW_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for SetPropertyW, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(lplpDirectInputDevice->lpVtbl->SetProperty);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for SetPropertyW, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked SetPropertyW at 0x%08x\n", lplpDirectInputDevice->lpVtbl->SetProperty);

	return 0;
}

HRESULT (__attribute__((stdcall)) *CreateDeviceA_orig)(LPDIRECTINPUT8A object, REFGUID rguid, LPDIRECTINPUTDEVICE8A *lplpDirectInputDevice, LPUNKNOWN pUnkOuter);
HRESULT __attribute__((stdcall)) CreateDeviceA_patched(LPDIRECTINPUT8A object, REFGUID rguid, LPDIRECTINPUTDEVICE8A *lplpDirectInputDevice, LPUNKNOWN pUnkOuter){
	LOG_VERBOSE("%s: object 0x%08x, rguid 0x%08x\n", __func__, object, rguid);
	HRESULT ret = CreateDeviceA_orig(object, rguid, lplpDirectInputDevice, pUnkOuter);
	static bool hooked = false;
	if(hooked){
		return ret;
	}
	if(ret == DI_OK){
		hooked = hook_create_effect_A(*lplpDirectInputDevice) == 0;
	}
	return ret;
}

HRESULT (__attribute__((stdcall)) *CreateDeviceW_orig)(LPDIRECTINPUT8W object, REFGUID rguid, LPDIRECTINPUTDEVICE8W *lplpDirectInputDevice, LPUNKNOWN pUnkOuter);
HRESULT __attribute__((stdcall)) CreateDeviceW_patched(LPDIRECTINPUT8W object, REFGUID rguid, LPDIRECTINPUTDEVICE8W *lplpDirectInputDevice, LPUNKNOWN pUnkOuter){
	LOG_VERBOSE("%s: object 0x%08x, rguid 0x%08x\n", __func__, object, rguid);
	HRESULT ret = CreateDeviceW_orig(object, rguid, lplpDirectInputDevice, pUnkOuter);
	static bool hooked = false;
	if(hooked){
		return ret;
	}
	if(ret == DI_OK){
		hooked = hook_create_effect_W(*lplpDirectInputDevice) == 0;
	}
	return ret;
}

int hook_create_device_A(LPDIRECTINPUT8A dinput8_interface_A){
	if(init_mh() != 0){
		return -1;
	}

	int mhret = MH_CreateHook(dinput8_interface_A->lpVtbl->CreateDevice, (LPVOID)&CreateDeviceA_patched, (LPVOID *)&CreateDeviceA_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for CreateDeviceA, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(dinput8_interface_A->lpVtbl->CreateDevice);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for CreateDeviceA, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked CreateDeviceA at 0x%08x\n", dinput8_interface_A->lpVtbl->CreateDevice);
	return 0;
}

int hook_create_device_W(LPDIRECTINPUT8W dinput8_interface_W){
	if(init_mh() != 0){
		return -1;
	}

	int mhret = MH_CreateHook(dinput8_interface_W->lpVtbl->CreateDevice, (LPVOID)&CreateDeviceW_patched, (LPVOID *)&CreateDeviceW_orig);
	if(mhret != MH_OK){
		LOG("Failed creating hook for CreateDeviceW, %d\n", mhret);
		return -1;
	}
	mhret = MH_EnableHook(dinput8_interface_W->lpVtbl->CreateDevice);
	if(mhret != MH_OK){
		LOG("Failed enableing hook for CreateDeviceW, %d\n", mhret);
		return -1;
	}
	LOG_VERBOSE("hooked CreateDeviceW at 0x%08x\n", dinput8_interface_W->lpVtbl->CreateDevice);
	return 0;
}

HRESULT (WINAPI *DirectInput8Create_orig)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
HRESULT WINAPI DirectInput8Create_patched(HINSTANCE instance, DWORD version, REFIID variant, LPVOID *object, LPUNKNOWN com_stuff){
	LOG_VERBOSE("%s: instance 0x%08x, version 0x%08x\n", __func__, instance, version);
	HRESULT ret = DirectInput8Create_orig(instance, version, variant, object, com_stuff);
	static bool hooked = false;
	if(hooked){
		return ret;
	}
	if(ret == DI_OK){
		if(guid_equal(variant, &IID_IDirectInput8A)){
			hooked = hook_create_device_A(*(LPDIRECTINPUT8A *)object) == 0;
		}else{
			hooked = hook_create_device_W(*(LPDIRECTINPUT8W *)object) == 0;
		}
	}
	return ret;
}

HMODULE get_dinput8_handle(){
	char windows_install_path[512];
	GetWindowsDirectoryA(windows_install_path, sizeof(windows_install_path));
	char dll_path[512];
	sprintf(dll_path, "%s\\syswow64\\dinput8.dll", windows_install_path);

	HMODULE dinput8_lib_handle = LoadLibraryA(dll_path);
	if(dinput8_lib_handle == NULL){
		sprintf(dll_path, "%s\\system32\\dinput8.dll", windows_install_path);
		dinput8_lib_handle = LoadLibraryA(dll_path);
	}
	if(dinput8_lib_handle == NULL){
		LOG("Failed fetching dinput8.dll handle\n");
		return NULL;
	}
	LOG_VERBOSE("fetched handle of %s\n", dll_path);
	return dinput8_lib_handle;
}

int hook_dinput8create(){
	if(init_mh() != 0){
		return -1;
	}

	HMODULE dinput8_lib_handle = get_dinput8_handle();
	if(dinput8_lib_handle == NULL){
		return -1;
	}

	void *DirectInput8Create_ref = GetProcAddress(dinput8_lib_handle, "DirectInput8Create");

	int ret = MH_CreateHook(DirectInput8Create_ref, (LPVOID)&DirectInput8Create_patched, (LPVOID *)&DirectInput8Create_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for DirectInput8Create, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(DirectInput8Create_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for DirectInput8Create, %d\n", ret);
		return -1;
	}
	LOG_VERBOSE("hooked DirectInput8Create at 0x%08x\n", DirectInput8Create_ref);
	//sleep(10);
	return 0;
}

int hook_create_device_methods(){
	HMODULE dinput8 = get_dinput8_handle();
	if(dinput8 == NULL){
		LOG("Failed fetching dinput8 handle\n");
		return -1;
	}
	HRESULT (WINAPI *DirectInput8Create_fetched)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN) = (HRESULT (WINAPI *)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN))GetProcAddress(dinput8, "DirectInput8Create");

	// make a dll soup to hook methods becausing hooking DirectInput8Create itself triggers hooking detection on tdu2's physics engine

	// this instance will be a memory leak? how the hek are you supposed to free these
	LPDIRECTINPUT8A direct_input_8_interface_A;

	// while wine dinput8 doesn't seem to care if I send garbo as instance handle here, the win10 implementation cares
	HINSTANCE instance_handle = GetModuleHandleA(NULL);

	HRESULT res = DirectInput8Create_fetched((HINSTANCE)instance_handle, 0x0800, &IID_IDirectInput8A, (LPVOID *)&direct_input_8_interface_A, NULL);
	if(res != DI_OK){
		LOG("Failed creating dinput8 A interface, 0x%08lx\n", res);
		return -1;
	}
	if(hook_create_device_A(direct_input_8_interface_A) != 0){
		LOG("Failed hooking dinput8 A interface\n");
		return -1;
	}

	LPDIRECTINPUT8W direct_input_8_interface_W;
	res = DirectInput8Create_fetched((HINSTANCE)instance_handle, 0x0800, &IID_IDirectInput8W, (LPVOID *)&direct_input_8_interface_W, NULL);
	if(res != DI_OK){
		LOG("Failed creating dinput8 W interface, 0x%08lx\n", res);
		return -1;
	}
	if(hook_create_device_W(direct_input_8_interface_W) != 0){
		LOG("Failed hooking dinput8 W interface\n");
		return -1;
	}

	return 0;
}
