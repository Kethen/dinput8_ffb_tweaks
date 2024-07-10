#include "hooking.h"
#include "logging.h"
#include "config.h"
#include <windows.h>
#include <pthread.h>
#include <unistd.h>
#include "modify_effects.h"

// XXX probably needs more functions implemented for this to be loaded correctly

void *config_parser_loop(void *args){
	while(true){
		sleep(2);
		parse_config(true);
	}
	return NULL;
}

extern "C" {

HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, REFIID variant, LPVOID *object, LPUNKNOWN com_stuff){
	static bool initialized = false;
	if(!initialized){
		initialized = true;
		init_logging();
		LOG("log initialized\n");

		init_config();
		parse_config(true);
		LOG("done parsing initial config\n");

		pthread_t config_reparse_thread;
		pthread_create(&config_reparse_thread, NULL, config_parser_loop, NULL);
	}

	bind_effect_modifier_to_hook();

	HMODULE dinput8_lib_handle = get_dinput8_handle();
	if(dinput8_lib_handle == NULL){
		LOG("cannot fetch dinput8.dll handle..?\n");
		return -1;
	}

	HRESULT (WINAPI *DirectInput8Create_ref)(HINSTANCE, DWORD, REFIID, LPVOID, LPUNKNOWN) = (HRESULT (WINAPI *)(HINSTANCE, DWORD, REFIID, LPVOID, LPUNKNOWN))GetProcAddress(dinput8_lib_handle, "DirectInput8Create");
	if(DirectInput8Create_ref == NULL){
		LOG("cannot fetch DirectInput8Create from handle..?\n");
		return -1;
	}
	HRESULT ret = DirectInput8Create_ref(instance, version, variant, object, com_stuff);
	static bool hooked = false;
	if(hooked){
		return ret;
	}
	if(ret == DI_OK){
		if(guid_equal(&variant, &IID_IDirectInput8A)){
			hooked = hook_create_device_A(*(LPDIRECTINPUT8A *)object) == 0;
		}else{
			hooked = hook_create_device_W(*(LPDIRECTINPUT8W *)object) == 0;
		}
	}
	return ret;
}

}
