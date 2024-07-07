#include <windows.h>

// https://github.com/TsudaKageyu/minhook
#include "MinHook.h"

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

// json
#include <json.hpp>

#define STR(s) #s

using json = nlohmann::json;

pthread_mutex_t log_mutex;
int log_fd = -1;

void init_logging(){
	pthread_mutex_init(&log_mutex, NULL);
	const char *log_path = "./dinput8_ffb_tweaks.txt";
	unlink(log_path);
	log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC);
}

int read_data_from_fd(int fd, char *buffer, int len){
	int bytes_read = 0;
	while(bytes_read < len){
		int bytes_read_this_cycle = read(fd, &buffer[bytes_read], len - bytes_read);
		if(bytes_read_this_cycle < 0){
			return bytes_read_this_cycle;
		}
		bytes_read += bytes_read_this_cycle;
	}
	return bytes_read;
}

int write_data_to_fd(int fd, const char *buffer, int len){
	int bytes_written = 0;
	while(bytes_written < len){
		int bytes_written_this_cycle = write(fd, &buffer[bytes_written], len - bytes_written);
		if(bytes_written_this_cycle < 0){
			return bytes_written_this_cycle;
		}
		bytes_written += bytes_written_this_cycle;
	}
	return bytes_written;
}

#define LOG(...){ \
	pthread_mutex_lock(&log_mutex); \
	if(log_fd >= 0){ \
		char _log_buffer[1024]; \
		int _log_len = sprintf(_log_buffer, __VA_ARGS__); \
		write_data_to_fd(log_fd, _log_buffer, _log_len); \
	} \
	pthread_mutex_unlock(&log_mutex); \
}

#if 0
#define LOG_VERBOSE(...) LOG(__VA_ARGS__)
#else
#define LOG_VERBOSE(...)
#endif

struct modifier {
	// not sure what else to modify yet
	int32_t min_gain;
};

struct modifiers {
	// effects to modify, adding more based on time availability
	struct modifier spring;
	struct modifier friction;
};

struct config{
	struct modifiers m;
};

struct config current_config = {0};
pthread_mutex_t current_config_mutex;

void log_config(struct config *c){
	#define PRINT_SETTING_BOOL(key) { \
		LOG("setting " STR(key) ": %s\n", c->key? "true" : "false");\
	}

	#undef PRINT_SETTING_BOOL

	#define PRINT_MODIFIER_INT32(key, subkey) { \
		LOG("modifier " STR(key) "." STR(subkey) ": %d\n", c->m.key.subkey); \
	}
	#define PRINT_MODIFIERS(key) { \
		PRINT_MODIFIER_INT32(key, min_gain); \
	}
	PRINT_MODIFIERS(spring);
	PRINT_MODIFIERS(friction);
	#undef PRINT_MODIFIER_INT32
	#undef PRINT_MODIFIERS
}

void parse_config(){
	const char *config_path = "./dinput8_ffb_tweaks_config.json";
	bool updated = false;
	json parsed_config;

	int config_fd = open(config_path, O_BINARY | O_RDONLY);
	if(config_fd < 0){
		LOG("warning, failed opening %s for reading, using defaults\n", config_path);
	}

	while(config_fd >= 0){
		int file_size = lseek(config_fd, 0, SEEK_END);
		if(file_size < 0){
			LOG("failed fetching config file size, using defaults\n");
			break;
		}

		int seek_result = lseek(config_fd, 0, SEEK_SET);
		if(seek_result < 0){
			LOG("failed rewinding config file, using defaults\n");
			break;
		}

		char *buffer = (char *)malloc(file_size);
		if(buffer == NULL){
			LOG("failed allocating buffer for reading the config file, using defaults\n");
			free(buffer);
			break;
		}

		int bytes_read = read_data_from_fd(config_fd, buffer, file_size);
		if(bytes_read < 0){
			LOG("failed reading %s, using defaults\n", config_path);
			free(buffer);
			break;
		}

		try{
			parsed_config = json::parse(std::string(buffer, bytes_read));
		}catch(...){
			LOG("failed parsing %s, using defaults\n", config_path);
			parsed_config = {};
			free(buffer);
			break;
		}
		free(buffer);
		break;
	}
	if(config_fd >= 0){
		close(config_fd);
	}

	struct config incoming_config = {0};

	#define FETCH_SETTING(key, d) { \
		try{ \
			incoming_config.key = parsed_config.at(STR(key)); \
		}catch(...){ \
			LOG("warning: failed fetching config " STR(key) " from json, adding default\n") \
			updated = true; \
			incoming_config.key = d; \
			parsed_config[STR(key)] = d; \
		} \
	}
	#undef FETCH_SETTING

	#define FETCH_MODIFIER(key, subkey, d) { \
		try{ \
			incoming_config.m.key.subkey = parsed_config.at("modifiers").at(STR(key)).at(STR(subkey)); \
		}catch(...){ \
			LOG("warning: failed fetching modifier " STR(key) "." STR(subkey) " from json, adding default"); \
			updated = true; \
			incoming_config.m.key.subkey = d; \
			parsed_config["modifiers"][STR(key)][STR(subkey)] = d; \
		} \
	}
	#define FETCH_MODIFIERS(key) {\
		FETCH_MODIFIER(key, min_gain, 0); \
	}
	FETCH_MODIFIERS(spring);
	FETCH_MODIFIERS(friction);
	#undef FETCH_MODIFIERS
	#undef FETCH_MODIFIER

	if(memcmp(&current_config, &incoming_config, sizeof(struct config)) != 0){
		pthread_mutex_lock(&current_config_mutex);
		memcpy(&current_config, &incoming_config, sizeof(struct config));
		pthread_mutex_unlock(&current_config_mutex);
		log_config(&current_config);
	}

	if(!updated){
		return;
	}
	config_fd = open(config_path, O_BINARY | O_WRONLY | O_TRUNC | O_CREAT);

	if(config_fd < 0){
		LOG("warning: failed opening %s for updating, please make sure the file is not readonly\n", config_path);
		return;
	}

	std::string out_json = parsed_config.dump(4);
	int write_result = write_data_to_fd(config_fd, out_json.c_str(), out_json.size());
	if(write_result != out_json.size()){
		LOG("warning: failed writing updated config to %s\n", config_path);
	}
	close(config_fd);
}


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
		parse_config();
	}
	return NULL;
}

int hook_functions(){
	int ret = MH_Initialize();
	if(ret != MH_OK && ret != MH_ERROR_ALREADY_INITIALIZED){
		LOG("Failed initializing MinHook, %d\n", ret);
		return -1;
	}

	/*
	ret = MH_CreateHook(f00bcfbb0_ref, (LPVOID)&f00bcfbb0_patched, (LPVOID *)&f00bcfbb0_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for 0x00bcfbb0, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(f00bcfbb0_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for 0x00bcfbb0, %d\n", ret);
		return -1;
	}
	*/

	return 0;
}

// entrypoint
__attribute__((constructor))
int init(){
	init_logging();
	pthread_mutex_init(&current_config_mutex, NULL);
	LOG("log initialized\n");

	parse_config();
	LOG("done parsing initial config\n");

	hook_functions();
	LOG("done hooking functions\n");

	pthread_t config_reparse_thread;
	pthread_create(&config_reparse_thread, NULL, config_parser_loop, NULL);

	return 0;
}
