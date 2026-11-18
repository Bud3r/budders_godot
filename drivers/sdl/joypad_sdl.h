/**************************************************************************/
/*  joypad_sdl.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/input/input.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"

typedef uint32_t SDL_JoystickID;
typedef struct SDL_Joystick SDL_Joystick;
typedef struct SDL_Gamepad SDL_Gamepad;
typedef struct SDL_Haptic SDL_Haptic;
union SDL_HapticEffect;

class JoypadSDL : public Input::JoypadDriver {
public:
	~JoypadSDL();

	Error initialize();
	void process_events();

	virtual bool set_hint(const String &p_name, const String &p_value) override;
	virtual String get_hint(const String &p_name) const override;

private:
	class Joypad : public Input::JoypadFeatures {
		struct ForceEffect {
			Input::JoyForceEffect type = Input::JOY_FORCE_EFFECT_CONSTANT;
			bool auto_destroy = false;
			uint64_t destroy_at_usec = 0; // 0 while not playing, or when it plays forever.
			uint64_t length_usec = 0; // 0 when it plays forever.
		};

		mutable bool haptic_opened = false;
		mutable SDL_Haptic *haptic = nullptr;
		HashMap<int, ForceEffect> force_effects;

		SDL_Haptic *_get_haptic() const;
		bool _make_haptic_effect(Input::JoyForceEffect p_type, const Dictionary &p_params, SDL_HapticEffect &r_effect, uint64_t &r_length_usec) const;

	public:
		// Guards the HD rumble and force feedback state, which game code and process_events() both touch.
		mutable BinaryMutex mutex;

		bool attached = false;
		StringName guid;

		SDL_JoystickID sdl_instance_idx;

		bool supports_force_feedback = false;
		bool supports_motion_sensors = false;
		uint64_t ff_effect_timestamp = 0;

		Vector3 last_accelerometer;
		uint64_t last_gyro_timestamp = 0;
		float sensor_rate = 0.0f;

		LocalVector<Vector2> ball_pending;
		LocalVector<Vector2> ball_frame;
		HashMap<uint32_t, Vector2> touch_positions;

		bool trigger_effect_set = false;

		bool hd_rumble_active = false;
		uint8_t hd_rumble_packet[10] = {};
		uint64_t hd_rumble_end_usec = 0;
		uint64_t hd_rumble_sent_usec = 0;

		virtual bool has_joy_light() const override;
		virtual bool has_joy_rgb_light() const override;
		virtual void set_joy_light(const Color &p_color) override;

		virtual bool has_joy_motion_sensors() const override;
		virtual void set_joy_motion_sensors_enabled(bool p_enable) override;

		virtual bool has_joy_vibration() const override;

		virtual bool has_joy_trigger_vibration() const override;
		virtual void start_joy_trigger_vibration(float p_left, float p_right, float p_duration) override;

		virtual bool send_joy_effect(const PackedByteArray &p_data) override;
		virtual bool start_joy_hd_vibration(const uint8_t *p_rumble, float p_duration) override;

		virtual bool has_joy_player_lights() const override;
		virtual void set_joy_player_index(int p_index) override;
		virtual int get_joy_player_index() const override;

		virtual Input::JoypadType get_joy_type(bool p_real) const override;
		virtual Input::JoyButtonLabel get_joy_button_label(JoyButton p_button) const override;
		virtual Input::JoyPowerState get_joy_power_state(int &r_percent) const override;
		virtual Input::JoyConnectionState get_joy_connection_state() const override;
		virtual TypedArray<Dictionary> get_joy_bindings() const override;

		virtual int get_joy_touchpad_count() const override;
		virtual int get_joy_touchpad_finger_count(int p_touchpad) const override;
		virtual bool get_joy_touchpad_finger(int p_touchpad, int p_finger, Vector2 &r_position, float &r_pressure) const override;

		virtual int get_joy_ball_count() const override;
		virtual Vector2 get_joy_ball_motion(int p_ball) const override;

		virtual bool has_joy_sensor(Input::JoySensor p_sensor) const override;
		virtual void set_joy_sensor_enabled(Input::JoySensor p_sensor, bool p_enable) override;
		virtual Vector3 get_joy_sensor_data(Input::JoySensor p_sensor) const override;

		virtual bool has_joy_force_feedback() const override;
		virtual bool is_joy_force_effect_supported(Input::JoyForceEffect p_effect) const override;
		virtual int create_joy_force_effect(Input::JoyForceEffect p_effect, const Dictionary &p_params) override;
		virtual bool update_joy_force_effect(int p_id, const Dictionary &p_params) override;
		virtual bool play_joy_force_effect(int p_id, int p_iterations) override;
		virtual void stop_joy_force_effect(int p_id) override;
		virtual void destroy_joy_force_effect(int p_id) override;
		virtual void stop_joy_force_effects() override;
		virtual void set_joy_force_feedback_gain(float p_gain) override;
		virtual void set_joy_force_feedback_autocenter(float p_autocenter) override;

		void update_hd_rumble(uint64_t p_now_usec);
		void destroy_finished_force_effects(uint64_t p_now_usec);
		void reset_output();

		SDL_Joystick *get_sdl_joystick() const;
		SDL_Gamepad *get_sdl_gamepad() const;
	};

	Joypad joypads[Input::JOYPADS_MAX];
	HashMap<SDL_JoystickID, int> sdl_instance_id_to_joypad_id;

	void close_joypad(int p_pad_idx);
};
