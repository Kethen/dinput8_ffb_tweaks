#include <windows.h>

// pthread
#include <pthread.h>

// unix-ish
#include <fcntl.h>
#include <unistd.h>

// std
#include <stdio.h>
#include <stdint.h>

// windows
#include <memoryapi.h>

// dinput
#define CINTERFACE
#include <dinput.h>
#undef CINTERFACE

#include "hooking.h"
#include "logging.h"
#include "config.h"
#include "modify_effects.h"

void patch_memory(void *location, void *buffer, int len){
	DWORD orig_protect;
	VirtualProtect(location, len, PAGE_EXECUTE_READWRITE, &orig_protect);
	memcpy(location, buffer, len);
	DWORD dummy;
	VirtualProtect(location, len, orig_protect, &dummy);
}

void *config_parser_loop(void *args){
	while(true){
		sleep(2);
		parse_config(true);
	}
	return NULL;
}

// entrypoint
__attribute__((constructor))
int init(){
	init_logging();
	LOG("log initialized\n");

	init_config();
	parse_config(true);
	LOG("done parsing initial config\n");

	bind_effect_modifier_to_hook();
	LOG("done binding effect modifier to hooks\n");

	if(current_config.method_hooking){
		hook_create_device_methods();
		LOG("done hooking create device methods\n");
	}else{
		hook_dinput8create();
		LOG("done hooking DirectInput8Create\n");
	}

	pthread_t config_reparse_thread;
	pthread_create(&config_reparse_thread, NULL, config_parser_loop, NULL);

	return 0;
}
