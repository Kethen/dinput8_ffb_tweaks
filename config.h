#ifndef __CONFIG_H
#define __CONFIG_H

#include <pthread.h>
#include <stdint.h>

struct modifier {
	// not sure what else to modify yet
	int32_t min_gain;
	float gain_multiplier;
};

struct modifiers {
	// effects to modify, adding more based on time availability
	struct modifier spring;
	struct modifier friction;
};

struct config{
	struct modifiers m;
	bool log_effects;
	bool method_hooking;
};

void parse_config(bool update_if_needed);
int init_config();

#ifndef __NOEXTERN
extern struct config current_config;
extern pthread_mutex_t current_config_mutex;
#endif

#endif
