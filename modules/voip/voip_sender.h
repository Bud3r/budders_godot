#pragma once
#include "scene/audio/audio_stream_player.h"
#include "servers/audio/audio_stream.h"
#include "servers/audio/effects/audio_effect_capture.h"
#include "servers/audio/effects/audio_effect_record.h"

class VoipSender : AudioStreamPlayer {
	GDCLASS(VoipSender, Node)
public:
	VoipSender();
protected:
	void update() const;
	void _notification(int p_what) override;
private:
	Ref<AudioStreamMicrophone> audio_stream_microphone;
	Ref<AudioEffectCapture> audio_effect_capture;
	Ref<AudioEffectRecordInstance> audio_effect_record_instance;
};
