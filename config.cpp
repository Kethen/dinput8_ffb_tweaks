// json
#include <json.hpp>

// unix-ish
#include <fcntl.h>

#include "logging.h"

#define __NOEXTERN
#include "config.h"
#undef __NOEXTERN

#include <pthread.h>

#define STR(s) #s

using json = nlohmann::json;

struct config current_config = {0};
pthread_mutex_t current_config_mutex;

static void log_config(struct config *c){
	#define PRINT_SETTING_BOOL(key) { \
		LOG("setting " STR(key) ": %s\n", c->key? "true" : "false");\
	}

	#undef PRINT_SETTING_BOOL

	#define PRINT_MODIFIER_INT32(key, subkey) { \
		LOG("modifier " STR(key) "." STR(subkey) ": %d\n", c->m.key.subkey); \
	}
	#define PRINT_MODIFIER_FLOAT(key, subkey) { \
		LOG("modifier " STR(key) "." STR(subkey) ": %f\n", c->m.key.subkey); \
	}
	#define PRINT_MODIFIERS(key) { \
		PRINT_MODIFIER_INT32(key, min_gain); \
		PRINT_MODIFIER_FLOAT(key, gain_multiplier); \
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
			LOG("warning: failed fetching modifier " STR(key) "." STR(subkey) " from json, adding default\n"); \
			updated = true; \
			incoming_config.m.key.subkey = d; \
			parsed_config["modifiers"][STR(key)][STR(subkey)] = d; \
		} \
	}
	#define FETCH_MODIFIERS(key) {\
		FETCH_MODIFIER(key, min_gain, 0); \
		FETCH_MODIFIER(key, gain_multiplier, 0.0); \
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

int init_config(){
	return pthread_mutex_init(&current_config_mutex, NULL);
}
