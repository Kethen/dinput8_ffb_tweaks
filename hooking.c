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


static bool guid_equal(const void *lhs, const void *rhs){
	return memcmp(lhs, rhs, sizeof(GUID)) == 0;
}

// step 1, hook DirectInput8Create to collect instances of LPDIRECTINPUT8A and LPDIRECTINPUT8W
// step 2, on DirectInput8Create, hook either variant of CreateDevice
// step 3, on CreateDevice, hook either variants of CreateEffect, likely merges here but you never know
// step 4, on CreateEffect, hook Download
// step 5, on Download, fire hook over at cpp bits with access to config

void common_download_hook(LPDIRECTINPUTEFFECT object){

}

HRESULT (__attribute__((stdcall)) *DownloadA_orig)(LPDIRECTINPUTEFFECT object);
HRESULT __attribute__((stdcall)) DownloadA_patched(LPDIRECTINPUTEFFECT object){
	LOG_VERBOSE("%s: object 0x%08x\n", __func__, object);
	common_download_hook(object);
	HRESULT ret = DownloadA_orig(object);
	return ret;
}

HRESULT (__attribute__((stdcall)) *CreateEffectA_orig)(LPDIRECTINPUTDEVICE8A object, REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter);
HRESULT __attribute__((stdcall)) CreateEffectA_patched(LPDIRECTINPUTDEVICE8A object, REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter){
	LOG_VERBOSE("%s: object 0x%08x, rguid 0x%08x\n", __func__, object, rguid);
	HRESULT ret = CreateEffectA_orig(object, rguid, lpeff, ppdeff, punkOuter);
	static bool hooked = false;
	if(hooked){
		return ret;
	}
	if(ret == DI_OK){
		hooked = true;
		int mhret = MH_CreateHook((*ppdeff)->lpVtbl->Download, (LPVOID)&DownloadA_patched, (LPVOID *)&DownloadA_orig);
		if(mhret != MH_OK){
			LOG("Failed creating hook for DownloadA, %d\n", ret);
			return ret;
		}
		mhret = MH_EnableHook((*ppdeff)->lpVtbl->Download);
		if(mhret != MH_OK){
			LOG("Failed enableing hook for DownloadA, %d\n", ret);
			return ret;
		}
		LOG_VERBOSE("hooked DownloadA at 0x%08x\n", (*ppdeff)->lpVtbl->Download);
	}
	return ret;
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
		hooked = true;
		int mhret = MH_CreateHook((*lplpDirectInputDevice)->lpVtbl->CreateEffect, (LPVOID)&CreateEffectA_patched, (LPVOID *)&CreateEffectA_orig);
		if(mhret != MH_OK){
			LOG("Failed creating hook for CreateEffectA, %d\n", ret);
			return ret;
		}
		mhret = MH_EnableHook((*lplpDirectInputDevice)->lpVtbl->CreateEffect);
		if(mhret != MH_OK){
			LOG("Failed enableing hook for CreateEffectA, %d\n", ret);
			return ret;
		}
		LOG_VERBOSE("hooked CreateEffectA at 0x%08x\n", (*lplpDirectInputDevice)->lpVtbl->CreateEffect);
	}
	return ret;
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
		hooked = true;
		if(guid_equal(variant, &IID_IDirectInput8A)){
			// A variant
			LPDIRECTINPUT8A dinput8_interface_A = *(LPDIRECTINPUT8A *)object;
			int mhret = MH_CreateHook(dinput8_interface_A->lpVtbl->CreateDevice, (LPVOID)&CreateDeviceA_patched, (LPVOID *)&CreateDeviceA_orig);
			if(mhret != MH_OK){
				LOG("Failed creating hook for CreateDeviceA, %d\n", ret);
				return ret;
			}
			mhret = MH_EnableHook(dinput8_interface_A->lpVtbl->CreateDevice);
			if(mhret != MH_OK){
				LOG("Failed enableing hook for CreateDeviceA, %d\n", ret);
				return ret;
			}
			LOG_VERBOSE("hooked CreateDeviceA at 0x%08x\n", dinput8_interface_A->lpVtbl->CreateDevice);
		}else{
			// W variant
			LOG_VERBOSE("CreateDeviceW wip\n");
		}
	}
	return ret;
}


int hook_functions(){
	int ret = MH_Initialize();
	if(ret != MH_OK && ret != MH_ERROR_ALREADY_INITIALIZED){
		LOG("Failed initializing MinHook, %d\n", ret);
		return -1;
	}

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
		return -1;
	}
	LOG_VERBOSE("fetched handle of %s\n", dll_path);

	void *DirectInput8Create_ref = GetProcAddress(dinput8_lib_handle, "DirectInput8Create");

	ret = MH_CreateHook(DirectInput8Create_ref, (LPVOID)&DirectInput8Create_patched, (LPVOID *)&DirectInput8Create_orig);
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
