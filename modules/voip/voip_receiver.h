#pragma once
#include "scene/main/node.h"
#include "servers/audio/effects/audio_stream_generator.h"

class VoipReceiver : Node {
	GDCLASS(VoipReceiver, Node)
protected:
	void _notification(int p_what) override;
private:
	Ref<AudioStreamGenerator> audio_stream_generator;
	Ref<AudioStreamGeneratorPlayback> audio_stream_generator_playback;
	NodePath audio_stream_player_path;
	VoipReceiver();
	void receive_buffer(const PackedVector2Array& frames) const;
};
