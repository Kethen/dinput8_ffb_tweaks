#include "hooking.h"
#include "config.h"
#include "logging.h"

#include <dinput.h>

#define STR(s) #s

static const char *get_effect_string(LPGUID effect_guid){
	#define EFFECT_STRING(guid) { \
		if(guid == *effect_guid){ \
			return STR(guid); \
		} \
	}
	EFFECT_STRING(GUID_ConstantForce);
	EFFECT_STRING(GUID_RampForce);
	EFFECT_STRING(GUID_Square);
	EFFECT_STRING(GUID_Sine);
	EFFECT_STRING(GUID_Triangle);
	EFFECT_STRING(GUID_SawtoothUp);
	EFFECT_STRING(GUID_SawtoothDown);
	EFFECT_STRING(GUID_Spring);
	EFFECT_STRING(GUID_Damper);
	EFFECT_STRING(GUID_Inertia);
	EFFECT_STRING(GUID_Friction);
	EFFECT_STRING(GUID_CustomForce);
	#undef EFFECT_STRING

	return "UNKNOWN_EFFECT";
}

static const char *get_device_prop_string(LPGUID prop_guid){
	#define PROP_STRING(guid) { \
		if(&DIPROP_##guid == prop_guid){ \
			return STR(guid); \
		} \
	}
	PROP_STRING(APPDATA);
	PROP_STRING(AUTOCENTER);
	PROP_STRING(AXISMODE);
	PROP_STRING(BUFFERSIZE);
	PROP_STRING(CALIBRATION);
	PROP_STRING(CALIBRATIONMODE);
	PROP_STRING(CPOINTS);
	PROP_STRING(DEADZONE);
	PROP_STRING(FFGAIN);
	PROP_STRING(INSTANCENAME);
	PROP_STRING(PRODUCTNAME);
	PROP_STRING(RANGE);
	PROP_STRING(SATURATION);
	#undef PROP_STRING

	return "UNKNOWN_DEVICE_PROP";
}

#define LOG_IF_LOG_EFFECTS(...){ \
	if(should_log){ \
		LOG(__VA_ARGS__); \
	}else{ \
		LOG_VERBOSE(__VA_ARGS__); \
	} \
}

extern "C" {

void log_device_prop(LPGUID prop_guid, LPDIPROPHEADER propheader, bool should_log){
	// prop_guid is an enum on a pointer :(
	LOG_IF_LOG_EFFECTS(
		"device prop %s:\n",
		get_device_prop_string(prop_guid)
	);

	// TODO log more props
	if(prop_guid == &DIPROP_FFGAIN && propheader != NULL){
		DIPROPDWORD *prop = (DIPROPDWORD *)propheader;
		LOG_IF_LOG_EFFECTS("gain: %ld\n", prop->dwData);
		return;
	}

	if(prop_guid == &DIPROP_AUTOCENTER && propheader != NULL){
		DIPROPDWORD *prop = (DIPROPDWORD *)propheader;
		LOG_IF_LOG_EFFECTS("auto center: %ld\n", prop->dwData);
		return;
	}

	if(prop_guid == &DIPROP_RANGE && propheader != NULL){
		DIPROPRANGE *prop = (DIPROPRANGE *)propheader;
		LOG_IF_LOG_EFFECTS("range min, max: %ld, %ld\n", prop->lMin, prop->lMax);
		return;
	}
}

void log_effect(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items, bool should_log){
	LOG_IF_LOG_EFFECTS(
		"effect %s, %s, %s, %s\n",
		get_effect_string(effect_guid),
		*modified_items & DIEP_NODOWNLOAD? "no download flag set" : "no download flag not set",
		*modified_items & DIEP_NORESTART? "no restart flag set" : "no restart flag not set",
		*modified_items & DIEP_START? "start flag set" : "start flag not set"
	);
	if(*modified_items & DIEP_AXES && params->rgdwAxes != NULL){
		LOG_IF_LOG_EFFECTS("axes ids: ");
		for(int i = 0;i < params->cAxes; i++){
			LOG_IF_LOG_EFFECTS("0x%08lx ", params->rgdwAxes[i]);
		}
		LOG_IF_LOG_EFFECTS("\n");
	}
	if(*modified_items & DIEP_DIRECTION && params->rglDirection != NULL){
		LOG_IF_LOG_EFFECTS("direction format: %s %s %s\n",
			params->dwFlags & DIEFF_CARTESIAN? "DIEFF_CARTESIAN": "",
			params->dwFlags & DIEFF_POLAR? "DIEFF_POLAR": "",
			params->dwFlags & DIEFF_SPHERICAL? "DIEFF_SPHERICAL": "");
		LOG_IF_LOG_EFFECTS("directions: ");
		for(int i = 0;i < params->cAxes; i++){
			LOG_IF_LOG_EFFECTS("%ld ", params->rglDirection[i]);
		}
		LOG_IF_LOG_EFFECTS("\n");
	}
	if(*modified_items & DIEP_DURATION){
		LOG_IF_LOG_EFFECTS("duration: %lu\n", params->dwDuration);
	}
	if(*modified_items & DIEP_ENVELOPE && params->lpEnvelope != NULL){
		LOG_IF_LOG_EFFECTS(
			"envelope:\n"
			"attack level: %lu\n"
			"attack time: %lu\n"
			"fade level: %lu\n"
			"fade time: %lu\n"
			"end envelope\n",
			params->lpEnvelope->dwAttackLevel,
			params->lpEnvelope->dwAttackTime,
			params->lpEnvelope->dwFadeLevel,
			params->lpEnvelope->dwFadeTime
		);
	}
	if(*modified_items & DIEP_GAIN){
		LOG_IF_LOG_EFFECTS("gain: %lu\n", params->dwGain);
	}
	if(*modified_items & DIEP_SAMPLEPERIOD){
		LOG_IF_LOG_EFFECTS("sample period: %lu\n", params->dwSamplePeriod);
	}
	if(*modified_items & DIEP_STARTDELAY){
		LOG_IF_LOG_EFFECTS("start delay: %lu\n", params->dwStartDelay);
	}
	if(*modified_items & DIEP_TRIGGERBUTTON){
		LOG_IF_LOG_EFFECTS("trigger button format: %s\n", params->dwFlags & DIEFF_OBJECTIDS? "DIEFF_OBJECTIDS?": "DIEFF_OBJECTOFFSETS");
		if(params->dwTriggerButton == DIEB_NOTRIGGER){
			LOG_IF_LOG_EFFECTS("trigger button: none\n");
		}else{
			LOG_IF_LOG_EFFECTS("trigger button: 0x%08lx\n", params->dwTriggerButton);
		}
	}

	if(*modified_items & DIEP_TYPESPECIFICPARAMS && params->lpvTypeSpecificParams != NULL){
		LOG_IF_LOG_EFFECTS("type specific params:\n");
		if(
			*effect_guid == GUID_Spring ||
			*effect_guid == GUID_Damper ||
			*effect_guid == GUID_Inertia ||
			*effect_guid == GUID_Friction
		){
			DICONDITION *base_pointer = (DICONDITION *)params->lpvTypeSpecificParams;
			int num_item = params->cbTypeSpecificParams / sizeof(DICONDITION);
			for(int i = 0;i < num_item; i++){
				LOG_IF_LOG_EFFECTS(
					"item #%d:\n"
					"offset: %ld\n"
					"positive coefficient: %ld\n"
					"negative coefficient: %ld\n"
					"positive saturation: %lu\n"
					"negative saturation: %lu\n"
					"dead band: %ld\n",
					i,
					base_pointer[i].lOffset,
					base_pointer[i].lPositiveCoefficient,
					base_pointer[i].lNegativeCoefficient,
					base_pointer[i].dwPositiveSaturation,
					base_pointer[i].dwNegativeSaturation,
					base_pointer[i].lDeadBand
				)
			}
		}

		// periodic effects
		if(
			*effect_guid == GUID_Square ||
			*effect_guid == GUID_Sine ||
			*effect_guid == GUID_Triangle ||
			*effect_guid == GUID_SawtoothUp ||
			*effect_guid == GUID_SawtoothDown
		){
			DIPERIODIC *base_pointer = (DIPERIODIC *)params->lpvTypeSpecificParams;
			int num_item = params->cbTypeSpecificParams / sizeof(DIPERIODIC);
			for(int i = 0;i < num_item; i++){
				LOG_IF_LOG_EFFECTS(
					"item #%d:\n"
					"magnitude: %lu\n"
					"offset: %ld\n"
					"phase: %lu\n"
					"period: %lu\n",
					i,
					base_pointer[i].dwMagnitude,
					base_pointer[i].lOffset,
					base_pointer[i].dwPhase,
					base_pointer[i].dwPeriod
				);
			}
		}

		if(*effect_guid == GUID_CustomForce){
			DICUSTOMFORCE *base_pointer = (DICUSTOMFORCE *)params->lpvTypeSpecificParams;
			int num_item = params->cbTypeSpecificParams / sizeof(DICUSTOMFORCE);
			for(int i = 0;i < num_item; i++){
				LOG_IF_LOG_EFFECTS(
					"item #%d\n"
					"sample period: %lu\n",
					i,
					base_pointer[i].dwSamplePeriod
				);
				LOG_IF_LOG_EFFECTS("force:\n")
				for(int j = 0;j < base_pointer[i].cSamples;j++){
					LOG_IF_LOG_EFFECTS("sample #%d: ", j)
					for(int k = 0;k < base_pointer[i].cChannels;k++){
						LOG_IF_LOG_EFFECTS("%ld ", base_pointer[i].rglForceData[j * base_pointer[i].cChannels + k]);
					}
					LOG_IF_LOG_EFFECTS("\n");
				}
			}
		}

		if(*effect_guid == GUID_ConstantForce){
			DICONSTANTFORCE *base_pointer = (DICONSTANTFORCE *)params->lpvTypeSpecificParams;
			int num_item = params->cbTypeSpecificParams / sizeof(DICONSTANTFORCE);
			for(int i = 0;i < num_item;i++){
				LOG_IF_LOG_EFFECTS(
					"item #%d\n"
					"magnitude: %ld\n",
					i,
					base_pointer[i].lMagnitude
				);
			}
		}

		if(*effect_guid == GUID_RampForce){
			DIRAMPFORCE *base_pointer = (DIRAMPFORCE *)params->lpvTypeSpecificParams;
			int num_item = params->cbTypeSpecificParams / sizeof(DIRAMPFORCE);
			for(int i = 0;i < num_item;i++){
				LOG_IF_LOG_EFFECTS(
					"item #%d\n"
					"start: %ld\n"
					"end: %ld\n",
					i,
					base_pointer[i].lStart,
					base_pointer[i].lEnd
				);
			}
		}
	}
}

static void modify_effect(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items){
	log_effect(effect_guid, params, modified_items, current_config.log_effects);
}

static void modify_device_prop(LPGUID prop_guid, LPDIPROPHEADER propheader){
	log_device_prop(prop_guid, propheader, current_config.log_effects);
}

static void __attribute__((stdcall))set_param_cb(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items){
	bool should_log = current_config.log_effects;
	LOG_IF_LOG_EFFECTS("modifying effect from set callback\n");
	modify_effect(effect_guid, params, modified_items);
}

static void __attribute__((stdcall))create_effect_cb(LPGUID effect_guid, LPDIEFFECT params){
	bool should_log = current_config.log_effects;
	LOG_IF_LOG_EFFECTS("modifying effects from create callback\n");
	DWORD modified_items = DIEP_ALLPARAMS;
	modify_effect(effect_guid, params, &modified_items);
}

static void __attribute__((stdcall))set_device_property_cb(LPGUID prop_guid, LPDIPROPHEADER propheader){
	bool should_log = current_config.log_effects;
	LOG_IF_LOG_EFFECTS("modifying device prop from set prop callback\n");
	modify_device_prop(prop_guid, propheader);
}

// TODO maybe callback to expose objects, or add that exposure to the above callbacks

void bind_effect_modifier_to_hook(){
	set_set_param_cb(set_param_cb);
	set_create_effect_cb(create_effect_cb);
	set_set_device_property_cb(set_device_property_cb);
}

}
