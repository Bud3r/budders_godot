#pragma once
#include "scene/audio/audio_stream_player.h"
#include "servers/audio/audio_stream.h"
#include "servers/audio/effects/audio_effect_capture.h"
#include "servers/audio/effects/audio_effect_record.h"
#include "servers/audio/effects/audio_stream_generator.h"

const String bus_name = "VoipSender";
constexpr int rpc_channel = 88;
constexpr int buffer_size = 512;

class Voip : public Node {
	GDCLASS(Voip, Node)
public:
	Voip();
protected:
	void update();
	void _send_buffer(const PackedVector2Array &audio_buffer);
	void receive_buffer(const PackedVector2Array &audio_buffer) const;
	void _notification(int p_what);
	NodePath get_audio_stream_player_path() const;
	void set_audio_stream_player_path(const NodePath &value);
	static void _bind_methods();
	static StringName get_bus_name();
	GDVIRTUAL1(_send_buffer, PackedVector2Array)

private:
	Ref<AudioStreamGenerator> audio_stream_generator;
	Ref<AudioStreamGeneratorPlayback> audio_stream_generator_playback;
	NodePath audio_stream_player_path;
	Ref<AudioStreamMicrophone> audio_stream_microphone;
	Ref<AudioEffectCapture> audio_effect_capture;
	Ref<AudioEffectRecordInstance> audio_effect_record_instance;
};
