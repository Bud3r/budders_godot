#include "voip_receiver.h"

#include "scene/2d/audio_stream_player_2d.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/audio/audio_stream_player.h"


VoipReceiver::VoipReceiver() {
	audio_stream_generator.instantiate();
}

void VoipReceiver::receive_buffer(const PackedVector2Array & frames) const {
	audio_stream_generator_playback->push_buffer(frames);
}

void VoipReceiver::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Node* node = get_node(audio_stream_player_path);
			AudioStreamPlayer* audio_stream_player = cast_to<AudioStreamPlayer>(node);
			AudioStreamPlayer2D* audio_stream_player_2d = cast_to<AudioStreamPlayer2D>(node);
			AudioStreamPlayer3D* audio_stream_player_3d = cast_to<AudioStreamPlayer3D>(node);

			if (audio_stream_player) {
				audio_stream_player->set_stream(audio_stream_generator);
				audio_stream_generator_playback = audio_stream_player->get_stream_playback();
			} else if (audio_stream_player_2d) {
				audio_stream_player_2d->set_stream(audio_stream_generator);
				audio_stream_generator_playback = audio_stream_player_2d->get_stream_playback();
			} else if (audio_stream_player_3d) {
				audio_stream_player_3d->set_stream(audio_stream_generator);
				audio_stream_generator_playback = audio_stream_player_3d->get_stream_playback();
			}
		}
	}
}
