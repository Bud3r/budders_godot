#include "register_types.h"

#include "voip_receiver.h"
#include "voip.h"
#include "core/object/class_db.h"
#include "scene/register_scene_types.h"

void initialize_voip_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(Voip)
	}

}

void uninitialize_voip_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
