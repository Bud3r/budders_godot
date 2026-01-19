#include "voip.h"

#include "core/config/project_settings.h"
#include "modules/multiplayer/scene_multiplayer.h"
#include "scene/2d/audio_stream_player_2d.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "servers/audio/effects/audio_effect_capture.h"
#include "servers/audio/effects/audio_effect_record.h"


Voip::Voip() {
	audio_stream_microphone.instantiate();
	audio_effect_capture.instantiate();
	audio_stream_generator.instantiate();

	Dictionary dictionary;
	dictionary["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	dictionary["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	dictionary["call_local"] = false;
	dictionary["channel"] = rpc_channel;
	rpc_config("receive_buffer", dictionary);
}

void Voip::update() {
	if (!is_multiplayer_authority()) {
		return;
	}

	if (audio_effect_capture->can_get_buffer(buffer_size)) {
		PackedVector2Array audio_buffer = audio_effect_capture->get_buffer(buffer_size);
		GDVIRTUAL_CALL(_send_buffer, audio_buffer);
		audio_effect_capture->clear_buffer();
	}
}

void Voip::_send_buffer(const PackedVector2Array &audio_buffer) {
	rpc("receive_buffer", audio_buffer);
}

void Voip::receive_buffer(const PackedVector2Array &audio_buffer) const {
	audio_stream_generator_playback->push_buffer(audio_buffer);
}

void Voip::_notification(int p_what) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#endif
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			AudioServer* audio_server = AudioServer::get_singleton();

			int bus_idx = audio_server->get_bus_index(bus_name);

			if (bus_idx == -1) {
				bus_idx = audio_server->get_bus_count();
				audio_server->add_bus();
				audio_server->set_bus_name(bus_idx, bus_name);
				audio_server->add_bus_effect(bus_idx, audio_effect_capture);
				audio_server->set_bus_mute(bus_idx, true);
			}

			Node* node = get_node(audio_stream_player_path);
			// AudioStreamPlayer* audio_stream_player = cast_to<AudioStreamPlayer>(node);
			// AudioStreamPlayer2D* audio_stream_player_2d = cast_to<AudioStreamPlayer2D>(node);
			// AudioStreamPlayer3D* audio_stream_player_3d = cast_to<AudioStreamPlayer3D>(node);

			if (AudioStreamPlayer* audio_stream_player = cast_to<AudioStreamPlayer>(node)) {
				if (is_multiplayer_authority()) {
					audio_stream_player->set_stream(audio_stream_microphone);
					audio_stream_player->set_bus(bus_name);
				}
				else {
					audio_stream_player->set_stream(audio_stream_generator);
					audio_stream_player->play();
					audio_stream_generator_playback = audio_stream_player->get_stream_playback();
				}
			} else if (AudioStreamPlayer2D* audio_stream_player_2d = cast_to<AudioStreamPlayer2D>(node)) {
				if (is_multiplayer_authority()) {
					audio_stream_player_2d->set_stream(audio_stream_microphone);
					audio_stream_player_2d->set_bus(bus_name);
				}
				else {
					audio_stream_player_2d->set_stream(audio_stream_generator);
					audio_stream_player_2d->play();
					audio_stream_generator_playback = audio_stream_player_2d->get_stream_playback();
				}
			} else if (AudioStreamPlayer3D* audio_stream_player_3d = cast_to<AudioStreamPlayer3D>(node)) {
				if (is_multiplayer_authority()) {
					audio_stream_player_3d->set_stream(audio_stream_microphone);
					audio_stream_player_3d->set_bus(bus_name);
				}
				else {
					audio_stream_player_3d->set_stream(audio_stream_generator);
					audio_stream_player_3d->play();
					audio_stream_generator_playback = audio_stream_player_3d->get_stream_playback();
				}
			} else {
				ERR_PRINT("audio_stream_player_path does not point to an AudioStreamPlayer");
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			AudioServer* audio_server = AudioServer::get_singleton();
			int bus_idx = audio_server->get_bus_index(bus_name);
			audio_server->remove_bus(bus_idx);
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			update();
		} break;
		default:
			break;
	}
}


NodePath Voip::get_audio_stream_player_path() const {
	return audio_stream_player_path;
}

void Voip::set_audio_stream_player_path(const NodePath &value) {
	audio_stream_player_path = value;
}

void Voip::_bind_methods() {
	// GLOBAL_DEF(PropertyInfo(Variant::INT, "voip/buffer_size", PROPERTY_HINT_RANGE, "16,192000,1,suffix:Hz"), buffer_size);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("get_bus_name"), &Voip::get_bus_name);
	ClassDB::bind_method(D_METHOD("receive_buffer", "audio_buffer"), &Voip::receive_buffer);
	ClassDB::bind_method(D_METHOD("set_audio_stream_player_path", "path"), &Voip::set_audio_stream_player_path);
	ClassDB::bind_method(D_METHOD("get_audio_stream_player_path"), &Voip::get_audio_stream_player_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "audio_stream_player_path"), "set_audio_stream_player_path", "get_audio_stream_player_path");
	GDVIRTUAL_BIND(_send_buffer, "audio_buffer")
}

StringName Voip::get_bus_name() {
	return bus_name;
}
