#include "register_types.h"

#include "voip_receiver.h"
#include "voip_sender.h"
#include "core/object/class_db.h"

void initialize_voip_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(VoipSender)
		GDREGISTER_CLASS(VoipReceiver)
	}

}

void uninitialize_voip_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
