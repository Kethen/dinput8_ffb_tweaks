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

#define LOG_IF_LOG_EFFECTS(...){ \
	if(current_config.log_effects){ \
		LOG(__VA_ARGS__); \
	}else{ \
		LOG_VERBOSE(__VA_ARGS__); \
	} \
}

static void modify_effect(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items){
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
			LOG_IF_LOG_EFFECTS("0x%08x ", params->rgdwAxes[i]);
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
			LOG_IF_LOG_EFFECTS("%d ", params->rglDirection[i]);
		}
		LOG_IF_LOG_EFFECTS("\n");
	}
	if(*modified_items & DIEP_DURATION){
		LOG_IF_LOG_EFFECTS("duration: %d\n", params->dwDuration);
	}
	if(*modified_items & DIEP_ENVELOPE && params->lpEnvelope != NULL){
		LOG_IF_LOG_EFFECTS(
			"envelope:\n"
			"attack level: %d\n"
			"attack time: %d\n"
			"fade level: %d\n"
			"fade time: %d\n",
			params->lpEnvelope->dwAttackLevel,
			params->lpEnvelope->dwAttackTime,
			params->lpEnvelope->dwFadeLevel,
			params->lpEnvelope->dwFadeTime
		);
	}
	if(*modified_items & DIEP_GAIN){
		LOG_IF_LOG_EFFECTS("gain: %d\n", params->dwGain);
	}
	if(*modified_items & DIEP_SAMPLEPERIOD){
		LOG_IF_LOG_EFFECTS("sample period: %d\n", params->dwSamplePeriod);
	}
	if(*modified_items & DIEP_STARTDELAY){
		LOG_IF_LOG_EFFECTS("start delay: %d\n", params->dwStartDelay);
	}
	if(*modified_items & DIEP_TRIGGERBUTTON){
		LOG_IF_LOG_EFFECTS("trigger button format: %s\n", params->dwFlags & DIEFF_OBJECTIDS? "DIEFF_OBJECTIDS?": "DIEFF_OBJECTOFFSETS");
		if(params->dwTriggerButton == DIEB_NOTRIGGER){
			LOG_IF_LOG_EFFECTS("trigger button: none\n");
		}else{
			LOG_IF_LOG_EFFECTS("trigger button: 0x%08x\n", params->dwTriggerButton);
		}
	}
	// TODO type specific params
}

static void __attribute__((stdcall))set_param_cb(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items){
	LOG_IF_LOG_EFFECTS("modifying effect from set callback\n");
	modify_effect(effect_guid, params, modified_items);
}

static void __attribute__((stdcall))create_effect_cb(LPGUID effect_guid, LPDIEFFECT params){
	LOG_IF_LOG_EFFECTS("modifying effects from create callback\n");
	DWORD modified_items = DIEP_ALLPARAMS;
	modify_effect(effect_guid, params, &modified_items);
}

void bind_effect_modifier_to_hook(){
	set_set_param_cb(set_param_cb);
	set_create_effect_cb(create_effect_cb);
}
