/**************************************************************************/
/*  input.h                                                               */
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

#include "core/input/input_event.h"
#include "core/object/object.h"
#include "core/os/keyboard.h"
#include "core/os/thread_safe.h"
#include "core/templates/rb_map.h"
#include "core/templates/rb_set.h"
#include "core/variant/typed_array.h"

class GamepadMotion;

namespace InputClassEnums {
// Keep synced with DisplayServerEnums::MouseMode enum.
enum MouseMode : int {
	MOUSE_MODE_VISIBLE,
	MOUSE_MODE_HIDDEN,
	MOUSE_MODE_CAPTURED,
	MOUSE_MODE_CONFINED,
	MOUSE_MODE_CONFINED_HIDDEN,
	MOUSE_MODE_MAX,
};

// Keep synced with DisplayServerEnums and Control enums.
#undef CursorShape
enum CursorShape : int {
	CURSOR_ARROW,
	CURSOR_IBEAM,
	CURSOR_POINTING_HAND,
	CURSOR_CROSS,
	CURSOR_WAIT,
	CURSOR_BUSY,
	CURSOR_DRAG,
	CURSOR_CAN_DROP,
	CURSOR_FORBIDDEN,
	CURSOR_VSIZE,
	CURSOR_HSIZE,
	CURSOR_BDIAGSIZE,
	CURSOR_FDIAGSIZE,
	CURSOR_MOVE,
	CURSOR_VSPLIT,
	CURSOR_HSPLIT,
	CURSOR_HELP,
	CURSOR_MAX
};
} //namespace InputClassEnums

class Input : public Object {
	GDCLASS(Input, Object);
	_THREAD_SAFE_CLASS_

	static inline Input *singleton = nullptr;

	static constexpr uint64_t MAX_EVENT = 32;

public:
	// TODO: When we migrate to C++20, replace these with "using enum" and skip prefixing MouseMode and CursorShape in other files.
	using MouseMode = InputClassEnums::MouseMode;
	using CursorShape = InputClassEnums::CursorShape;

	// Values match SDL_GamepadType.
	enum JoypadType {
		JOYPAD_TYPE_UNKNOWN,
		JOYPAD_TYPE_STANDARD,
		JOYPAD_TYPE_XBOX_360,
		JOYPAD_TYPE_XBOX_ONE,
		JOYPAD_TYPE_PS3,
		JOYPAD_TYPE_PS4,
		JOYPAD_TYPE_PS5,
		JOYPAD_TYPE_SWITCH_PRO,
		JOYPAD_TYPE_SWITCH_JOYCON_LEFT,
		JOYPAD_TYPE_SWITCH_JOYCON_RIGHT,
		JOYPAD_TYPE_SWITCH_JOYCON_PAIR,
		JOYPAD_TYPE_MAX,
	};

	// Values match SDL_GamepadButtonLabel.
	enum JoyButtonLabel {
		JOY_BUTTON_LABEL_UNKNOWN,
		JOY_BUTTON_LABEL_A,
		JOY_BUTTON_LABEL_B,
		JOY_BUTTON_LABEL_X,
		JOY_BUTTON_LABEL_Y,
		JOY_BUTTON_LABEL_CROSS,
		JOY_BUTTON_LABEL_CIRCLE,
		JOY_BUTTON_LABEL_SQUARE,
		JOY_BUTTON_LABEL_TRIANGLE,
	};

	// Values match SDL_PowerState.
	enum JoyPowerState {
		JOY_POWER_STATE_ERROR = -1,
		JOY_POWER_STATE_UNKNOWN,
		JOY_POWER_STATE_ON_BATTERY,
		JOY_POWER_STATE_NO_BATTERY,
		JOY_POWER_STATE_CHARGING,
		JOY_POWER_STATE_CHARGED,
	};

	// Values match SDL_JoystickConnectionState.
	enum JoyConnectionState {
		JOY_CONNECTION_INVALID = -1,
		JOY_CONNECTION_UNKNOWN,
		JOY_CONNECTION_WIRED,
		JOY_CONNECTION_WIRELESS,
	};

	// Values match SDL_SensorType.
	enum JoySensor {
		JOY_SENSOR_ACCELEROMETER = 1,
		JOY_SENSOR_GYROSCOPE,
		JOY_SENSOR_ACCELEROMETER_LEFT,
		JOY_SENSOR_GYROSCOPE_LEFT,
		JOY_SENSOR_ACCELEROMETER_RIGHT,
		JOY_SENSOR_GYROSCOPE_RIGHT,
	};

	enum JoyTriggerEffect {
		JOY_TRIGGER_EFFECT_OFF,
		JOY_TRIGGER_EFFECT_FEEDBACK,
		JOY_TRIGGER_EFFECT_WEAPON,
		JOY_TRIGGER_EFFECT_VIBRATION,
	};

	enum JoyForceEffect {
		JOY_FORCE_EFFECT_CONSTANT,
		JOY_FORCE_EFFECT_SINE,
		JOY_FORCE_EFFECT_SQUARE,
		JOY_FORCE_EFFECT_TRIANGLE,
		JOY_FORCE_EFFECT_SAWTOOTH_UP,
		JOY_FORCE_EFFECT_SAWTOOTH_DOWN,
		JOY_FORCE_EFFECT_RAMP,
		JOY_FORCE_EFFECT_SPRING,
		JOY_FORCE_EFFECT_DAMPER,
		JOY_FORCE_EFFECT_INERTIA,
		JOY_FORCE_EFFECT_FRICTION,
		JOY_FORCE_EFFECT_LEFT_RIGHT,
		JOY_FORCE_EFFECT_MAX,
	};

	class JoypadFeatures {
	public:
		virtual ~JoypadFeatures() {}

		virtual bool has_joy_vibration() const { return false; }

		virtual bool has_joy_light() const { return false; }
		virtual bool has_joy_rgb_light() const { return false; }
		virtual void set_joy_light(const Color &p_color) {}

		virtual bool has_joy_motion_sensors() const { return false; }
		virtual void set_joy_motion_sensors_enabled(bool p_enable) {}

		virtual bool has_joy_trigger_vibration() const { return false; }
		virtual void start_joy_trigger_vibration(float p_left, float p_right, float p_duration) {}

		virtual bool send_joy_effect(const PackedByteArray &p_data) { return false; }
		// Eight bytes of Switch rumble data: four for the left motor, four for the right.
		virtual bool start_joy_hd_vibration(const uint8_t *p_rumble, float p_duration) { return false; }

		virtual bool has_joy_player_lights() const { return false; }
		virtual void set_joy_player_index(int p_index) {}
		virtual int get_joy_player_index() const { return -1; }

		virtual JoypadType get_joy_type(bool p_real) const { return JOYPAD_TYPE_UNKNOWN; }
		virtual JoyButtonLabel get_joy_button_label(JoyButton p_button) const { return JOY_BUTTON_LABEL_UNKNOWN; }
		virtual JoyPowerState get_joy_power_state(int &r_percent) const {
			r_percent = -1;
			return JOY_POWER_STATE_UNKNOWN;
		}
		virtual JoyConnectionState get_joy_connection_state() const { return JOY_CONNECTION_UNKNOWN; }
		virtual TypedArray<Dictionary> get_joy_bindings() const { return TypedArray<Dictionary>(); }

		virtual int get_joy_touchpad_count() const { return 0; }
		virtual int get_joy_touchpad_finger_count(int p_touchpad) const { return 0; }
		virtual bool get_joy_touchpad_finger(int p_touchpad, int p_finger, Vector2 &r_position, float &r_pressure) const { return false; }

		virtual int get_joy_ball_count() const { return 0; }
		virtual Vector2 get_joy_ball_motion(int p_ball) const { return Vector2(); }

		virtual bool has_joy_sensor(JoySensor p_sensor) const { return false; }
		virtual void set_joy_sensor_enabled(JoySensor p_sensor, bool p_enable) {}
		virtual Vector3 get_joy_sensor_data(JoySensor p_sensor) const { return Vector3(); }

		virtual bool has_joy_force_feedback() const { return false; }
		virtual bool is_joy_force_effect_supported(JoyForceEffect p_effect) const { return false; }
		virtual int create_joy_force_effect(JoyForceEffect p_effect, const Dictionary &p_params) { return -1; }
		virtual bool update_joy_force_effect(int p_id, const Dictionary &p_params) { return false; }
		virtual bool play_joy_force_effect(int p_id, int p_iterations) { return false; }
		virtual void stop_joy_force_effect(int p_id) {}
		virtual void destroy_joy_force_effect(int p_id) {}
		virtual void stop_joy_force_effects() {}
		virtual void set_joy_force_feedback_gain(float p_gain) {}
		virtual void set_joy_force_feedback_autocenter(float p_autocenter) {}
	};

	// The joypad backend as a whole, for settings that are not per device.
	class JoypadDriver {
	public:
		virtual ~JoypadDriver() {}

		virtual bool set_hint(const String &p_name, const String &p_value) { return false; }
		virtual String get_hint(const String &p_name) const { return String(); }
	};

	static constexpr int32_t JOYPADS_MAX = 16;

	typedef void (*EventDispatchFunc)(const Ref<InputEvent> &p_event);

private:
	BitField<MouseButtonMask> mouse_button_mask = MouseButtonMask::NONE;

	RBSet<Key> key_label_pressed;
	RBSet<Key> physical_keys_pressed;
	RBSet<Key> keys_pressed;
	RBSet<JoyButton> joy_buttons_pressed;
	RBMap<JoyAxis, float> _joy_axis;
	//RBMap<StringName,int> custom_action_press;
	bool gravity_enabled = false;
	Vector3 gravity;
	bool accelerometer_enabled = false;
	Vector3 accelerometer;
	bool magnetometer_enabled = false;
	Vector3 magnetometer;
	bool gyroscope_enabled = false;
	Vector3 gyroscope;
	Vector2 mouse_pos;
	int64_t mouse_window = 0;
	bool legacy_just_pressed_behavior = false;
	bool disable_input = false;
	bool ignore_joypad_on_unfocused_application = false;
	bool application_focused = true;
	bool embedder_focused = false;

	struct ActionState {
		uint64_t pressed_physics_frame = UINT64_MAX;
		uint64_t pressed_process_frame = UINT64_MAX;
		uint64_t released_physics_frame = UINT64_MAX;
		uint64_t released_process_frame = UINT64_MAX;
		ObjectID pressed_event_id;
		ObjectID released_event_id;
		bool exact = true;

		struct DeviceState {
			bool pressed[MAX_EVENT] = { false };
			float strength[MAX_EVENT] = { 0.0 };
			float raw_strength[MAX_EVENT] = { 0.0 };
			InputEventType event_type[MAX_EVENT] = { InputEventType::INVALID };
		};
		bool api_pressed = false;
		float api_strength = 0.0;
		HashMap<int, DeviceState> device_states;

		// Cache.
		struct ActionStateCache {
			bool pressed = false;
			float strength = false;
			float raw_strength = false;
		} cache;
	};

	HashMap<StringName, ActionState> action_states;

	bool emulate_touch_from_mouse = false;
	bool emulate_mouse_from_touch = false;
	bool agile_input_event_flushing = false;
	bool use_accumulated_input = true;

	int mouse_from_touch_index = -1;

	struct VibrationInfo {
		float weak_magnitude;
		float strong_magnitude;
		float duration; // Duration in seconds
		uint64_t timestamp;
	};

	HashMap<int, VibrationInfo> joy_vibration;

	struct MotionInfo {
		bool sensors_enabled : 1;
		bool calibrating : 1;
		bool calibrated : 1;
		float sensor_data_rate = 0.0f;
		uint64_t last_timestamp = 0;
		GamepadMotion *gamepad_motion = nullptr;
		// Gyroscope rotation summed over every sample, in radians: pending since the last frame, and over the last frame.
		Vector3 pending_rotation;
		Vector3 frame_rotation;

		MotionInfo() {
			sensors_enabled = false;
			calibrating = false;
			calibrated = false;
		}
	};

	HashMap<int, MotionInfo> joy_motion;

	struct VelocityTrack {
		uint64_t last_tick = 0;
		Vector2 velocity;
		Vector2 screen_velocity;
		Vector2 accum;
		Vector2 screen_accum;
		float accum_t = 0.0f;
		float min_ref_frame;
		float max_ref_frame;

		void update(const Vector2 &p_delta_p, const Vector2 &p_screen_delta_p);
		void reset();
		VelocityTrack();
	};

	struct Joypad {
		StringName name;
		StringName uid;
		bool connected = false;
		bool is_known = false;
		bool last_buttons[(size_t)JoyButton::MAX] = { false };
		float last_axis[(size_t)JoyAxis::MAX] = { 0.0f };
		HatMask last_hat = HatMask::CENTER;
		int mapping = -1;
		int hat_current = 0;
		Dictionary info;
		bool has_light = false;
		bool has_vibration = false;
		Input::JoypadFeatures *features = nullptr;
	};

	JoypadDriver *joypad_driver = nullptr;
	HashMap<String, String> pending_joy_driver_hints;

	JoypadFeatures *_get_joy_features(int p_device) const;

	VelocityTrack mouse_velocity_track;
	HashMap<int, VelocityTrack> touch_velocity_track;
	HashMap<int, Joypad> joy_names;

	HashSet<uint32_t> ignored_device_ids;

	int fallback_mapping = -1; // Index of the guid in map_db.

	CursorShape default_shape = CursorShape::CURSOR_ARROW;

	enum JoyType {
		TYPE_BUTTON,
		TYPE_AXIS,
		TYPE_HAT,
		TYPE_MAX,
	};

	enum JoyAxisRange {
		NEGATIVE_HALF_AXIS = -1,
		FULL_AXIS = 0,
		POSITIVE_HALF_AXIS = 1
	};

	struct JoyEvent {
		int type = TYPE_MAX;
		int index = -1; // Can be either JoyAxis or JoyButton.
		float value = 0.f;
	};

	struct JoyBinding {
		JoyType inputType;
		union {
			JoyButton button;

			struct {
				JoyAxis axis;
				JoyAxisRange range;
				bool invert;
			} axis;

			struct {
				HatDir hat;
				HatMask hat_mask;
			} hat;

		} input;

		JoyType outputType;
		union {
			JoyButton button;

			struct {
				JoyAxis axis;
				JoyAxisRange range;
			} axis;

		} output;
	};

	struct JoyDeviceMapping {
		String uid;
		String name;
		Vector<JoyBinding> bindings;
	};

	Vector<JoyDeviceMapping> map_db;

	void _set_joypad_mapping(Joypad &p_js, int p_map_index);

	JoyEvent _get_mapped_button_event(const JoyDeviceMapping &mapping, JoyButton p_button);
	JoyEvent _get_mapped_axis_event(const JoyDeviceMapping &mapping, JoyAxis p_axis, float p_value, JoyAxisRange &r_range);
	void _get_mapped_hat_events(const JoyDeviceMapping &mapping, HatDir p_hat, JoyEvent r_events[(size_t)HatDir::MAX]);
	JoyButton _get_output_button(const String &output);
	JoyAxis _get_output_axis(const String &output);
	void _button_event(int p_device, JoyButton p_index, bool p_pressed);
	void _axis_event(int p_device, JoyAxis p_axis, float p_value);
	void _update_action_cache(const StringName &p_action_name, ActionState &r_action_state);
	void _update_joypad_features(int p_device);

	void _parse_input_event_impl(const Ref<InputEvent> &p_event, bool p_is_emulated);

	List<Ref<InputEvent>> buffered_events;
#ifdef DEBUG_ENABLED
	HashSet<Ref<InputEvent>> frame_parsed_events;
	uint64_t last_parsed_frame = UINT64_MAX;
#endif

	friend class DisplayServer;
	friend class SceneTree;
	friend class SceneDebugger;

	static void (*set_mouse_mode_func)(MouseMode);
	static MouseMode (*get_mouse_mode_func)();
	static void (*set_mouse_mode_override_func)(MouseMode);
	static MouseMode (*get_mouse_mode_override_func)();
	static void (*set_mouse_mode_override_enabled_func)(bool);
	static bool (*is_mouse_mode_override_enabled_func)();
	static void (*warp_mouse_func)(const Vector2 &p_position);

	static CursorShape (*get_current_cursor_shape_func)();
	static void (*set_custom_mouse_cursor_func)(const Ref<Resource> &, CursorShape, const Vector2 &);

	EventDispatchFunc event_dispatch_function = nullptr;

	bool _should_ignore_joypad_events() const;

#ifndef DISABLE_DEPRECATED
	void _vibrate_handheld_bind_compat_91143(int p_duration_ms = 500);
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

protected:
	static void _bind_methods();

public:
	void set_mouse_mode(MouseMode p_mode);
	MouseMode get_mouse_mode() const;
	void set_mouse_mode_override(MouseMode p_mode);
	MouseMode get_mouse_mode_override() const;
	void set_mouse_mode_override_enabled(bool p_override_enabled);
	bool is_mouse_mode_override_enabled();

#ifdef TOOLS_ENABLED
	void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	static Input *get_singleton();

	bool is_anything_pressed() const;
	bool is_any_key_pressed() const;
	bool is_key_pressed(Key p_keycode) const;
	bool is_physical_key_pressed(Key p_keycode) const;
	bool is_key_label_pressed(Key p_keycode) const;
	bool is_mouse_button_pressed(MouseButton p_button) const;
	bool is_joy_button_pressed(int p_device, JoyButton p_button) const;
	bool is_action_pressed(const StringName &p_action, bool p_exact = false) const;
	bool is_action_just_pressed(const StringName &p_action, bool p_exact = false) const;
	bool is_action_just_released(const StringName &p_action, bool p_exact = false) const;
	bool is_action_just_pressed_by_event(const StringName &p_action, RequiredParam<InputEvent> rp_event, bool p_exact = false) const;
	bool is_action_just_released_by_event(const StringName &p_action, RequiredParam<InputEvent> rp_event, bool p_exact = false) const;
	float get_action_strength(const StringName &p_action, bool p_exact = false) const;
	float get_action_raw_strength(const StringName &p_action, bool p_exact = false) const;

	float get_axis(const StringName &p_negative_action, const StringName &p_positive_action) const;
	Vector2 get_vector(const StringName &p_negative_x, const StringName &p_positive_x, const StringName &p_negative_y, const StringName &p_positive_y, float p_deadzone = -1.0f) const;

	float get_joy_axis(int p_device, JoyAxis p_axis) const;
	String get_joy_name(int p_idx);
	TypedArray<int> get_connected_joypads();
	Vector2 get_joy_vibration_strength(int p_device);
	float get_joy_vibration_duration(int p_device);
	float get_joy_vibration_remaining_duration(int p_device);
	uint64_t get_joy_vibration_timestamp(int p_device);
	bool is_joy_vibrating(int p_device);
	bool has_joy_vibration(int p_device) const;
	void joy_connection_changed(int p_idx, bool p_connected, const String &p_name, const String &p_guid = "", const Dictionary &p_joypad_info = Dictionary());

	Vector3 get_gravity() const;
	Vector3 get_accelerometer() const;
	Vector3 get_magnetometer() const;
	Vector3 get_gyroscope() const;

	Point2 get_mouse_position() const;
	Vector2 get_last_mouse_velocity();
	Vector2 get_last_mouse_screen_velocity();
	BitField<MouseButtonMask> get_mouse_button_mask() const;

	void warp_mouse(const Vector2 &p_position);
	Point2 warp_mouse_motion(const Ref<InputEventMouseMotion> &p_motion, const Rect2 &p_rect);

	void parse_input_event(RequiredParam<InputEvent> rp_event);

	void set_gravity(const Vector3 &p_gravity);
	void set_accelerometer(const Vector3 &p_accel);
	void set_magnetometer(const Vector3 &p_magnetometer);
	void set_gyroscope(const Vector3 &p_gyroscope);
	void set_joy_axis(int p_device, JoyAxis p_axis, float p_value);

	void set_joy_features(int p_device, JoypadFeatures *p_features);

	void set_joy_light(int p_device, const Color &p_color);
	bool has_joy_light(int p_device) const;

	Vector3 get_joy_accelerometer(int p_device) const;
	Vector3 get_joy_gravity(int p_device) const;
	Vector3 get_joy_gyroscope(int p_device) const;

	void set_joy_motion_sensors_enabled(int p_device, bool p_enable);
	bool is_joy_motion_sensors_enabled(int p_device) const;

	bool has_joy_motion_sensors(int p_device) const;
	float get_joy_motion_sensors_rate(int p_device) const;

	void start_joy_motion_sensors_calibration(int p_device);
	void stop_joy_motion_sensors_calibration(int p_device);
	void clear_joy_motion_sensors_calibration(int p_device);

	Dictionary get_joy_motion_sensors_calibration(int p_device) const;
	void set_joy_motion_sensors_calibration(int p_device, const Dictionary &p_calibration_info);

	bool is_joy_motion_sensors_calibrating(int p_device) const;
	bool is_joy_motion_sensors_calibrated(int p_device) const;

	void set_joy_motion_sensors_rate(int p_device, float p_rate);

	void start_joy_vibration(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration = 0);
	void stop_joy_vibration(int p_device);

	bool has_joy_trigger_vibration(int p_device) const;
	void start_joy_trigger_vibration(int p_device, float p_left_magnitude, float p_right_magnitude, float p_duration = 0);
	void stop_joy_trigger_vibration(int p_device);

	bool send_joy_effect(int p_device, const PackedByteArray &p_data);
	bool has_joy_adaptive_triggers(int p_device) const;
	bool set_joy_trigger_effect(int p_device, JoyAxis p_trigger, JoyTriggerEffect p_effect, float p_start = 0.0f, float p_end = 1.0f, float p_strength = 1.0f, float p_frequency = 0.0f);
	bool has_joy_hd_vibration(int p_device) const;
	bool start_joy_hd_vibration(int p_device, float p_low_frequency, float p_low_amplitude, float p_high_frequency, float p_high_amplitude, float p_duration = 0, bool p_left = true, bool p_right = true);

	bool has_joy_rgb_light(int p_device) const;
	bool has_joy_player_lights(int p_device) const;
	void set_joy_player_index(int p_device, int p_index);
	int get_joy_player_index(int p_device) const;

	JoypadType get_joy_type(int p_device) const;
	JoypadType get_joy_real_type(int p_device) const;
	JoyButtonLabel get_joy_button_label(int p_device, JoyButton p_button) const;
	JoyPowerState get_joy_power_state(int p_device) const;
	int get_joy_battery_percent(int p_device) const;
	JoyConnectionState get_joy_connection_state(int p_device) const;
	TypedArray<Dictionary> get_joy_bindings(int p_device) const;

	int get_joy_touchpad_count(int p_device) const;
	int get_joy_touchpad_finger_count(int p_device, int p_touchpad) const;
	bool is_joy_touchpad_finger_down(int p_device, int p_touchpad, int p_finger) const;
	Vector2 get_joy_touchpad_finger_position(int p_device, int p_touchpad, int p_finger) const;
	float get_joy_touchpad_finger_pressure(int p_device, int p_touchpad, int p_finger) const;

	int get_joy_ball_count(int p_device) const;
	Vector2 get_joy_ball_motion(int p_device, int p_ball) const;

	Vector3 get_joy_gyroscope_rotation(int p_device) const;
	bool has_joy_sensor(int p_device, JoySensor p_sensor) const;
	void set_joy_sensor_enabled(int p_device, JoySensor p_sensor, bool p_enable);
	Vector3 get_joy_sensor_data(int p_device, JoySensor p_sensor) const;

	bool has_joy_force_feedback(int p_device) const;
	bool is_joy_force_effect_supported(int p_device, JoyForceEffect p_effect) const;
	int create_joy_force_effect(int p_device, JoyForceEffect p_effect, const Dictionary &p_params = Dictionary());
	bool update_joy_force_effect(int p_device, int p_effect_id, const Dictionary &p_params);
	bool play_joy_force_effect(int p_device, int p_effect_id, int p_iterations = 1);
	int start_joy_force_effect(int p_device, JoyForceEffect p_effect, const Dictionary &p_params = Dictionary());
	void stop_joy_force_effect(int p_device, int p_effect_id);
	void destroy_joy_force_effect(int p_device, int p_effect_id);
	void stop_joy_force_effects(int p_device);
	void set_joy_force_feedback_gain(int p_device, float p_gain);
	void set_joy_force_feedback_autocenter(int p_device, float p_autocenter);

	bool set_joy_driver_hint(const String &p_name, const String &p_value);
	String get_joy_driver_hint(const String &p_name) const;
	void vibrate_handheld(int p_duration_ms = 500, float p_amplitude = -1.0);

	void set_ignore_joypad_on_unfocused_application(bool p_ignore);
	bool is_ignoring_joypad_on_unfocused_application() const;

	void set_mouse_position(const Point2 &p_posf);

	void action_press(const StringName &p_action, float p_strength = 1.f);
	void action_release(const StringName &p_action);

	void set_emulate_touch_from_mouse(bool p_emulate);
	bool is_emulating_touch_from_mouse() const;
	void ensure_touch_mouse_raised();

	void set_emulate_mouse_from_touch(bool p_emulate);
	bool is_emulating_mouse_from_touch() const;

	CursorShape get_default_cursor_shape() const;
	void set_default_cursor_shape(CursorShape p_shape);
	CursorShape get_current_cursor_shape() const;
	void set_custom_mouse_cursor(const Ref<Resource> &p_cursor, CursorShape p_shape = Input::CursorShape::CURSOR_ARROW, const Vector2 &p_hotspot = Vector2());

	void parse_mapping(const String &p_mapping);
	void joy_button(int p_device, JoyButton p_button, bool p_pressed);
	void joy_axis(int p_device, JoyAxis p_axis, float p_value);
	void joy_hat(int p_device, BitField<HatMask> p_val);
	void joy_motion_sensors(int p_device, const Vector3 &p_accelerometer, const Vector3 &p_gyroscope);
	// One sensor sample taken p_delta seconds after the previous one, for drivers that see every sample.
	void joy_motion_sample(int p_device, const Vector3 &p_accelerometer, const Vector3 &p_gyroscope, float p_delta);
	void joy_motion_frame(int p_device);
	void set_joy_info_value(int p_device, const String &p_key, const Variant &p_value);
	void set_joypad_driver(JoypadDriver *p_driver);
	const HashMap<String, String> &get_pending_joy_driver_hints() const { return pending_joy_driver_hints; }

	void add_joy_mapping(const String &p_mapping, bool p_update_existing = false);
	void remove_joy_mapping(const String &p_guid);

	int get_unused_joy_id();

	bool is_joy_known(int p_device);
	String get_joy_guid(int p_device) const;
	bool should_ignore_device(int p_vendor_id, int p_product_id) const;
	Dictionary get_joy_info(int p_device) const;
	void set_fallback_mapping(const String &p_guid);

#ifdef DEBUG_ENABLED
	void flush_frame_parsed_events();
#endif
	void flush_buffered_events();
	bool is_agile_input_event_flushing();
	void set_agile_input_event_flushing(bool p_enable);
	void set_use_accumulated_input(bool p_enable);
	bool is_using_accumulated_input();

	void release_pressed_events();

	void set_event_dispatch_function(EventDispatchFunc p_function);

	void set_disable_input(bool p_disable);
	bool is_input_disabled() const;

	Input();
	~Input();
};

VARIANT_ENUM_CAST(Input::MouseMode);
VARIANT_ENUM_CAST(Input::CursorShape);
VARIANT_ENUM_CAST(Input::JoypadType);
VARIANT_ENUM_CAST(Input::JoyButtonLabel);
VARIANT_ENUM_CAST(Input::JoyPowerState);
VARIANT_ENUM_CAST(Input::JoyConnectionState);
VARIANT_ENUM_CAST(Input::JoySensor);
VARIANT_ENUM_CAST(Input::JoyTriggerEffect);
VARIANT_ENUM_CAST(Input::JoyForceEffect);
