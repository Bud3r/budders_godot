#include "voip_sender.h"

#include "modules/multiplayer/scene_multiplayer.h"
#include "servers/audio/effects/audio_effect_capture.h"
#include "servers/audio/effects/audio_effect_record.h"


VoipSender::VoipSender() {
	// TODO: Check how to create new ref instance.
	audio_stream_microphone.instantiate();
	audio_effect_capture.instantiate();
}

void VoipSender::update() const {
	if (constexpr int buffer_size = 512; audio_effect_capture->can_get_buffer(buffer_size)) {
		// TODO: Finish implementation
		PackedVector2Array audio_buffer = audio_effect_capture->get_buffer(buffer_size);
		get_multiplayer()->get_multiplayer_peer()->set_target_peer(1);
		get_multiplayer()->get_multiplayer_peer()->put_packet(reinterpret_cast<const uint8_t *>(&audio_buffer), sizeof(Vector2) * buffer_size);
		get_multiplayer()->poll()
		audio_effect_capture->clear_buffer();
	}
}

void VoipSender::_notification(int p_what) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			AudioServer* audio_server = AudioServer::get_singleton();
			int bus_idx = audio_server->get_bus_count();
			audio_server->add_bus();
			audio_server->set_bus_name(bus_idx, "VoipSender");
			audio_server->add_bus_effect(bus_idx, audio_effect_capture);
			audio_server->get_bus_effect_instance(bus_idx, 0);
		} break;

		case NOTIFICATION_EXIT_TREE: {
		} break;

		case NOTIFICATION_INTERNAL_PROCESS:
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			update();
		} break;
	}
}
