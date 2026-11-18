/**************************************************************************/
/*  joypad_sdl.cpp                                                        */
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

#include "joypad_sdl.h"

#ifdef SDL_ENABLED

#include "core/input/default_controller_mappings.h"
#include "core/os/os.h"
#include "core/variant/dictionary.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_haptic.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_power.h>

// Macro to skip the SDL joystick event handling if the device is an SDL gamepad, because
// there are separate events for SDL gamepads
#define SKIP_EVENT_FOR_GAMEPAD \
	if (SDL_IsGamepad(sdl_event.jdevice.which)) { \
		continue; \
	}

// Switch controllers drop rumble they haven't heard about for a while, so a held vibration is sent again this often.
static constexpr uint64_t HD_RUMBLE_RESEND_USEC = 30000;

// Size of DS5EffectsState_t, the body of a DualSense output report.
static constexpr int DUALSENSE_EFFECTS_SIZE = 47;

static Uint32 _rumble_duration_ms(float p_duration) {
	if (p_duration <= 0.0f) {
		return 0xFFFF; // SDL_MAX_RUMBLE_DURATION_MS
	}
	return (Uint32)MIN(p_duration * 1000.0f, 0xFFFF);
}

JoypadSDL::~JoypadSDL() {
	// Process any remaining input events
	process_events();
	for (int i = 0; i < Input::JOYPADS_MAX; i++) {
		if (joypads[i].attached) {
			close_joypad(i);
		}
	}
	if (Input::get_singleton()) {
		Input::get_singleton()->set_joypad_driver(nullptr);
	}
	SDL_Quit();
}

Error JoypadSDL::initialize() {
	SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
	SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
	for (const KeyValue<String, String> &E : Input::get_singleton()->get_pending_joy_driver_hints()) {
		SDL_SetHint(E.key.utf8().get_data(), E.value.utf8().get_data());
	}
	ERR_FAIL_COND_V_MSG(!SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD), FAILED, SDL_GetError());
	if (!SDL_InitSubSystem(SDL_INIT_HAPTIC)) {
		print_verbose(vformat("SDL: Force feedback is unavailable: %s", SDL_GetError()));
	}
	Input::get_singleton()->set_joypad_driver(this);

	// Add Godot's mapping database from memory
	int i = 0;
	while (DefaultControllerMappings::mappings[i]) {
		String mapping_string = DefaultControllerMappings::mappings[i++];
		CharString data = mapping_string.utf8();
		SDL_IOStream *rw = SDL_IOFromMem((void *)data.ptr(), data.size());
		SDL_AddGamepadMappingsFromIO(rw, true);
	}

	// Make sure that we handle already connected joypads when the driver is initialized.
	process_events();

	print_verbose("SDL: Init OK!");
	return OK;
}

bool JoypadSDL::set_hint(const String &p_name, const String &p_value) {
	return SDL_SetHint(p_name.utf8().get_data(), p_value.utf8().get_data());
}

String JoypadSDL::get_hint(const String &p_name) const {
	const char *value = SDL_GetHint(p_name.utf8().get_data());
	return value ? String::utf8(value) : String();
}

void JoypadSDL::process_events() {
	Input *input = Input::get_singleton();

	// Update rumble first for it to be applied when we handle SDL events
	for (int i = 0; i < Input::JOYPADS_MAX; i++) {
		Joypad &joy = joypads[i];
		if (joy.attached && joy.supports_force_feedback) {
			uint64_t timestamp = input->get_joy_vibration_timestamp(i);

			// Update the joypad rumble only if there was a new vibration request
			if (timestamp > joy.ff_effect_timestamp) {
				joy.ff_effect_timestamp = timestamp;

				SDL_Joystick *sdl_joy = SDL_GetJoystickFromID(joypads[i].sdl_instance_idx);
				Vector2 strength = input->get_joy_vibration_strength(i);
				// Invalid values for strength are filtered by Input::start_joy_vibration().

				float duration = input->get_joy_vibration_duration(i);
				if (duration < 0.0f) {
					continue; // Invalid duration.
				}

				{
					// A plain vibration replaces a held HD one.
					MutexLock lock(joy.mutex);
					joy.hd_rumble_active = false;
				}

				/*
					If the vibration was requested to start, SDL_RumbleJoystick will start it.
					If the vibration was requested to stop, strength and duration will be 0, so SDL will stop the rumble.

					Here strength.y goes first and then strength.x, because Input.get_joy_vibration_strength().x
					is vibration's weak magnitude (high frequency rumble), and .y is strong magnitude (low frequency rumble),
					SDL_RumbleJoystick takes low frequency rumble first and then high frequency rumble.

					Rumble strength goes from 0 to 0xFFFF.
				*/
				SDL_RumbleJoystick(sdl_joy, strength.y * UINT16_MAX, strength.x * UINT16_MAX, _rumble_duration_ms(duration));
			}
		}
	}

	SDL_Event sdl_event;
	while (SDL_PollEvent(&sdl_event)) {
		// A new joypad was attached
		if (sdl_event.type == SDL_EVENT_JOYSTICK_ADDED) {
			int joy_id = input->get_unused_joy_id();
			if (joy_id == -1) {
				// There is no space for more joypads...
				print_error("A new joypad was attached but couldn't allocate a new id for it because joypad limit was reached.");
			} else {
				SDL_Joystick *joy = nullptr;
				SDL_Gamepad *gamepad = nullptr;
				String device_name;

				// Gamepads must be opened with SDL_OpenGamepad to get their special remapped events
				if (SDL_IsGamepad(sdl_event.jdevice.which)) {
					gamepad = SDL_OpenGamepad(sdl_event.jdevice.which);

					ERR_CONTINUE_MSG(!gamepad,
							vformat("Error opening gamepad at index %d: %s", sdl_event.jdevice.which, SDL_GetError()));

					device_name = SDL_GetGamepadName(gamepad);
					joy = SDL_GetGamepadJoystick(gamepad);

					print_verbose(vformat("SDL: Gamepad %s connected", SDL_GetGamepadName(gamepad)));
				} else {
					joy = SDL_OpenJoystick(sdl_event.jdevice.which);
					ERR_CONTINUE_MSG(!joy,
							vformat("Error opening joystick at index %d: %s", sdl_event.jdevice.which, SDL_GetError()));

					device_name = SDL_GetJoystickName(joy);

					print_verbose(vformat("SDL: Joystick %s connected", SDL_GetJoystickName(joy)));
				}

				const int MAX_GUID_SIZE = 64;
				char guid[MAX_GUID_SIZE] = {};
				SDL_GUID joy_guid = SDL_GetJoystickGUID(joy);
				SDL_GUIDToString(joy_guid, guid, MAX_GUID_SIZE);
				SDL_PropertiesID propertiesID = SDL_GetJoystickProperties(joy);

				Joypad &joypad = joypads[joy_id];
				joypad.attached = true;
				joypad.sdl_instance_idx = sdl_event.jdevice.which;
				joypad.supports_force_feedback = SDL_GetBooleanProperty(propertiesID, SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, false);
				joypad.guid = StringName(String(guid));
				joypad.supports_motion_sensors = SDL_GamepadHasSensor(gamepad, SDL_SENSOR_ACCEL) || SDL_GamepadHasSensor(gamepad, SDL_SENSOR_GYRO);
				joypad.sensor_rate = joypad.supports_motion_sensors ? SDL_GetGamepadSensorDataRate(gamepad, SDL_SENSOR_GYRO) : 0.0f;
				joypad.last_accelerometer = Vector3();
				joypad.last_gyro_timestamp = 0;
				const int ball_count = MAX(SDL_GetNumJoystickBalls(joy), 0);
				joypad.ball_pending.clear();
				joypad.ball_pending.resize_initialized(ball_count);
				joypad.ball_frame.clear();
				joypad.ball_frame.resize_initialized(ball_count);
				joypad.touch_positions.clear();

				sdl_instance_id_to_joypad_id.insert(sdl_event.jdevice.which, joy_id);

				Dictionary joypad_info;
				// Skip Godot's mapping system if SDL already handles the joypad's mapping.
				joypad_info["mapping_handled"] = SDL_IsGamepad(sdl_event.jdevice.which);
				joypad_info["raw_name"] = String::utf8(SDL_GetJoystickName(joy));
				joypad_info["vendor_id"] = itos(SDL_GetJoystickVendor(joy));
				joypad_info["product_id"] = itos(SDL_GetJoystickProduct(joy));

				const String serial = String(SDL_GetJoystickSerial(joy));
				if (!serial.is_empty()) {
					joypad_info["serial_number"] = serial;
				}

				const Uint16 product_version = SDL_GetJoystickProductVersion(joy);
				if (product_version != 0) {
					joypad_info["product_version"] = itos(product_version);
				}
				const Uint16 firmware_version = SDL_GetJoystickFirmwareVersion(joy);
				if (firmware_version != 0) {
					joypad_info["firmware_version"] = itos(firmware_version);
				}
				const char *path = SDL_GetJoystickPath(joy);
				if (path) {
					joypad_info["path"] = String::utf8(path);
				}

#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED) || defined(MACOS_ENABLED)
				const uint64_t steam_handle = SDL_GetGamepadSteamHandle(gamepad);
				if (steam_handle != 0) {
					joypad_info["steam_input_index"] = itos(steam_handle);
				}
#endif

#ifdef WINDOWS_ENABLED
				const int player_index = SDL_GetJoystickPlayerIndex(joy);
				if (player_index >= 0 && joy_guid.data[14] == 'x') { // See also "SDL_IsJoystickXInput" in "thirdparty/sdl/joystick/SDL_joystick.c".
					// For XInput controllers SDL_GetJoystickPlayerIndex returns the XInput user index.
					joypad_info["xinput_index"] = itos(player_index);
				}
#endif

				input->joy_connection_changed(
						joy_id,
						true,
						device_name,
						joypad.guid,
						joypad_info);

				input->set_joy_features(joy_id, &joypad);

				if (joypad.supports_motion_sensors) {
					// Data rate for all sensors should be the same.
					input->set_joy_motion_sensors_rate(joy_id, SDL_GetGamepadSensorDataRate(gamepad, SDL_SENSOR_ACCEL));
				}
			}
			// An event for an attached joypad
		} else if (sdl_event.type >= SDL_EVENT_JOYSTICK_AXIS_MOTION && sdl_event.type < SDL_EVENT_FINGER_DOWN && sdl_instance_id_to_joypad_id.has(sdl_event.jdevice.which)) {
			int joy_id = sdl_instance_id_to_joypad_id.get(sdl_event.jdevice.which);
			Joypad &joypad = joypads[joy_id];

			switch (sdl_event.type) {
				case SDL_EVENT_JOYSTICK_REMOVED:
					input->joy_connection_changed(joy_id, false, "");
					close_joypad(joy_id);
					break;

				case SDL_EVENT_JOYSTICK_AXIS_MOTION:
					SKIP_EVENT_FOR_GAMEPAD;

					input->joy_axis(
							joy_id,
							static_cast<JoyAxis>(sdl_event.jaxis.axis), // Godot joy axis constants are already intentionally the same as SDL's
							((sdl_event.jaxis.value - SDL_JOYSTICK_AXIS_MIN) / (float)(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN) - 0.5f) * 2.0f);
					break;

				case SDL_EVENT_JOYSTICK_BALL_MOTION:
					if (sdl_event.jball.ball < joypad.ball_pending.size()) {
						joypad.ball_pending[sdl_event.jball.ball] += Vector2(sdl_event.jball.xrel, sdl_event.jball.yrel);
					}
					break;

				case SDL_EVENT_JOYSTICK_BUTTON_UP:
				case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
					SKIP_EVENT_FOR_GAMEPAD;

					// Some devices report pressing buttons with indices like 232+, 241+, etc. that are not valid,
					// so we ignore them here.
					if (sdl_event.jbutton.button >= (int)JoyButton::MAX) {
						continue;
					}

					input->joy_button(
							joy_id,
							static_cast<JoyButton>(sdl_event.jbutton.button), // Godot button constants are intentionally the same as SDL's, so we can just straight up use them
							sdl_event.jbutton.down);
					break;

				case SDL_EVENT_JOYSTICK_HAT_MOTION:
					SKIP_EVENT_FOR_GAMEPAD;

					input->joy_hat(
							joy_id,
							(HatMask)sdl_event.jhat.value // Godot hat masks are identical to SDL hat masks, so we can just use them as-is.
					);
					break;

				case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
					float axis_value;

					if (sdl_event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || sdl_event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
						// Gamepad triggers go from 0 to SDL_JOYSTICK_AXIS_MAX
						axis_value = sdl_event.gaxis.value / (float)SDL_JOYSTICK_AXIS_MAX;
					} else {
						// Other axis go from SDL_JOYSTICK_AXIS_MIN to SDL_JOYSTICK_AXIS_MAX
						axis_value =
								((sdl_event.gaxis.value - SDL_JOYSTICK_AXIS_MIN) / (float)(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN) - 0.5f) * 2.0f;
					}

					input->joy_axis(
							joy_id,
							static_cast<JoyAxis>(sdl_event.gaxis.axis), // Godot joy axis constants are already intentionally the same as SDL's
							axis_value);
				} break;

				// Do note SDL gamepads do not have separate events for the dpad
				case SDL_EVENT_GAMEPAD_BUTTON_UP:
				case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
					input->joy_button(
							joy_id,
							static_cast<JoyButton>(sdl_event.gbutton.button), // Godot button constants are intentionally the same as SDL's, so we can just straight up use them
							sdl_event.gbutton.down);
					break;

				case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
				case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
				case SDL_EVENT_GAMEPAD_TOUCHPAD_UP: {
					const SDL_GamepadTouchpadEvent &touch = sdl_event.gtouchpad;
					const uint32_t key = ((uint32_t)touch.touchpad << 16) | ((uint32_t)touch.finger & 0xFFFF);
					const Vector2 position(touch.x, touch.y);
					const Vector2 *last_position = joypad.touch_positions.getptr(key);

					Ref<InputEventJoypadTouchpad> event;
					event.instantiate();
					event->set_device(joy_id);
					event->set_touchpad(touch.touchpad);
					event->set_finger(touch.finger);
					event->set_position(position);
					event->set_relative(last_position && sdl_event.type != SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN ? position - *last_position : Vector2());
					event->set_pressure(touch.pressure);
					event->set_pressed(sdl_event.type != SDL_EVENT_GAMEPAD_TOUCHPAD_UP);

					if (sdl_event.type == SDL_EVENT_GAMEPAD_TOUCHPAD_UP) {
						joypad.touch_positions.erase(key);
					} else {
						joypad.touch_positions[key] = position;
					}

					input->parse_input_event(event);
				} break;

				case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: {
					const SDL_GamepadSensorEvent &sensor = sdl_event.gsensor;
					if (sensor.sensor == SDL_SENSOR_ACCEL) {
						joypad.last_accelerometer = Vector3(-sensor.data[0], -sensor.data[1], -sensor.data[2]);
					} else if (sensor.sensor == SDL_SENSOR_GYRO) {
						float delta = joypad.sensor_rate > 0.0f ? 1.0f / joypad.sensor_rate : 1.0f / 60.0f;
						if (joypad.last_gyro_timestamp != 0 && sensor.sensor_timestamp > joypad.last_gyro_timestamp) {
							const float measured = (sensor.sensor_timestamp - joypad.last_gyro_timestamp) / 1000000000.0f;
							if (measured < 0.1f) {
								delta = measured;
							}
						}
						joypad.last_gyro_timestamp = sensor.sensor_timestamp;
						input->joy_motion_sample(joy_id, joypad.last_accelerometer, Vector3(sensor.data[0], sensor.data[1], sensor.data[2]), delta);
					}
				} break;

#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED) || defined(MACOS_ENABLED)
				case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED: {
					const uint64_t steam_handle = SDL_GetGamepadSteamHandle(joypad.get_sdl_gamepad());
					input->set_joy_info_value(joy_id, "steam_input_index", steam_handle != 0 ? Variant(itos(steam_handle)) : Variant());
				} break;
#endif
			}
		}
	}

	const uint64_t now_usec = OS::get_singleton()->get_ticks_usec();
	for (int i = 0; i < Input::JOYPADS_MAX; i++) {
		Joypad &joy = joypads[i];
		if (!joy.attached) {
			continue;
		}
		if (joy.supports_motion_sensors) {
			input->joy_motion_frame(i);
		}
		for (uint32_t ball = 0; ball < joy.ball_pending.size(); ball++) {
			joy.ball_frame[ball] = joy.ball_pending[ball];
			joy.ball_pending[ball] = Vector2();
		}
		joy.update_hd_rumble(now_usec);
		joy.destroy_finished_force_effects(now_usec);
	}
}

void JoypadSDL::close_joypad(int p_pad_idx) {
	Joypad &joy = joypads[p_pad_idx];
	int sdl_instance_idx = joy.sdl_instance_idx;

	joy.reset_output();
	joy.attached = false;
	sdl_instance_id_to_joypad_id.erase(sdl_instance_idx);

	if (SDL_IsGamepad(sdl_instance_idx)) {
		SDL_Gamepad *gamepad = SDL_GetGamepadFromID(sdl_instance_idx);
		SDL_CloseGamepad(gamepad);
	} else {
		SDL_Joystick *sdl_joy = SDL_GetJoystickFromID(sdl_instance_idx);
		SDL_CloseJoystick(sdl_joy);
	}
}

bool JoypadSDL::Joypad::has_joy_light() const {
	SDL_Joystick *joystick = get_sdl_joystick();
	SDL_PropertiesID properties_id = SDL_GetJoystickProperties(joystick);
	if (properties_id == 0) {
		return false;
	}
	return SDL_GetBooleanProperty(properties_id, SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, false) || SDL_GetBooleanProperty(properties_id, SDL_PROP_JOYSTICK_CAP_MONO_LED_BOOLEAN, false);
}

bool JoypadSDL::Joypad::has_joy_rgb_light() const {
	SDL_PropertiesID properties_id = SDL_GetJoystickProperties(get_sdl_joystick());
	return properties_id != 0 && SDL_GetBooleanProperty(properties_id, SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, false);
}

void JoypadSDL::Joypad::set_joy_light(const Color &p_color) {
	SDL_SetJoystickLED(get_sdl_joystick(), p_color.get_r8(), p_color.get_g8(), p_color.get_b8());
}

bool JoypadSDL::Joypad::has_joy_motion_sensors() const {
	return supports_motion_sensors;
}

void JoypadSDL::Joypad::set_joy_motion_sensors_enabled(bool p_enable) {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_ACCEL, p_enable);
	SDL_SetGamepadSensorEnabled(gamepad, SDL_SENSOR_GYRO, p_enable);
	last_gyro_timestamp = 0;
}

bool JoypadSDL::Joypad::has_joy_vibration() const {
	return supports_force_feedback;
}

bool JoypadSDL::Joypad::has_joy_trigger_vibration() const {
	// Checked each time: some controllers only report it once SDL has talked to them.
	SDL_PropertiesID properties_id = SDL_GetJoystickProperties(get_sdl_joystick());
	return properties_id != 0 && SDL_GetBooleanProperty(properties_id, SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN, false);
}

void JoypadSDL::Joypad::start_joy_trigger_vibration(float p_left, float p_right, float p_duration) {
	SDL_RumbleJoystickTriggers(get_sdl_joystick(), p_left * UINT16_MAX, p_right * UINT16_MAX, _rumble_duration_ms(p_duration));
}

bool JoypadSDL::Joypad::send_joy_effect(const PackedByteArray &p_data) {
	if (!SDL_SendJoystickEffect(get_sdl_joystick(), p_data.ptr(), p_data.size())) {
		return false;
	}
	if (get_joy_type(true) == Input::JOYPAD_TYPE_PS5 && (p_data[0] & 0x0C) != 0) {
		trigger_effect_set = true;
	}
	return true;
}

static bool _is_neutral_switch_rumble(const uint8_t *p_rumble) {
	return p_rumble[0] == 0x00 && p_rumble[1] == 0x01 && p_rumble[2] == 0x40 && p_rumble[3] == 0x40;
}

bool JoypadSDL::Joypad::start_joy_hd_vibration(const uint8_t *p_rumble, float p_duration) {
	const Input::JoypadType type = get_joy_type(true);
	if (type < Input::JOYPAD_TYPE_SWITCH_PRO || type > Input::JOYPAD_TYPE_SWITCH_JOYCON_PAIR) {
		return false;
	}

	MutexLock lock(mutex);
	// A SwitchCommonOutputPacket_t: the rumble report ID, a packet number SDL fills in, then the rumble for each side.
	hd_rumble_packet[0] = 0x10;
	hd_rumble_packet[1] = 0;
	memcpy(hd_rumble_packet + 2, p_rumble, 8);

	const uint64_t now = OS::get_singleton()->get_ticks_usec();
	hd_rumble_active = !_is_neutral_switch_rumble(p_rumble) || !_is_neutral_switch_rumble(p_rumble + 4);
	hd_rumble_end_usec = p_duration > 0.0f ? now + (uint64_t)(p_duration * 1000000.0f) : 0;
	hd_rumble_sent_usec = now;
	return SDL_SendJoystickEffect(get_sdl_joystick(), hd_rumble_packet, sizeof(hd_rumble_packet));
}

void JoypadSDL::Joypad::update_hd_rumble(uint64_t p_now_usec) {
	MutexLock lock(mutex);
	if (!hd_rumble_active) {
		return;
	}
	if (hd_rumble_end_usec != 0 && p_now_usec >= hd_rumble_end_usec) {
		static const uint8_t neutral[4] = { 0x00, 0x01, 0x40, 0x40 };
		memcpy(hd_rumble_packet + 2, neutral, 4);
		memcpy(hd_rumble_packet + 6, neutral, 4);
		hd_rumble_active = false;
	} else if (p_now_usec - hd_rumble_sent_usec < HD_RUMBLE_RESEND_USEC) {
		return;
	}
	hd_rumble_sent_usec = p_now_usec;
	SDL_SendJoystickEffect(get_sdl_joystick(), hd_rumble_packet, sizeof(hd_rumble_packet));
}

void JoypadSDL::Joypad::reset_output() {
	SDL_Joystick *joystick = get_sdl_joystick();

	if (trigger_effect_set && joystick) {
		// Leave the DualSense triggers loose, or they stay stiff after the game is gone.
		uint8_t effects[DUALSENSE_EFFECTS_SIZE] = {};
		effects[0] = 0x0C;
		effects[10] = 0x05;
		effects[21] = 0x05;
		SDL_SendJoystickEffect(joystick, effects, sizeof(effects));
	}
	trigger_effect_set = false;

	MutexLock lock(mutex);
	if (hd_rumble_active && joystick) {
		static const uint8_t neutral[10] = { 0x10, 0x00, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };
		SDL_SendJoystickEffect(joystick, neutral, sizeof(neutral));
	}
	hd_rumble_active = false;

	force_effects.clear();
	if (haptic) {
		SDL_CloseHaptic(haptic);
	}
	haptic = nullptr;
	haptic_opened = false;
}

bool JoypadSDL::Joypad::has_joy_player_lights() const {
	SDL_PropertiesID properties_id = SDL_GetJoystickProperties(get_sdl_joystick());
	return properties_id != 0 && SDL_GetBooleanProperty(properties_id, SDL_PROP_JOYSTICK_CAP_PLAYER_LED_BOOLEAN, false);
}

void JoypadSDL::Joypad::set_joy_player_index(int p_index) {
	SDL_SetJoystickPlayerIndex(get_sdl_joystick(), p_index);
}

int JoypadSDL::Joypad::get_joy_player_index() const {
	return SDL_GetJoystickPlayerIndex(get_sdl_joystick());
}

Input::JoypadType JoypadSDL::Joypad::get_joy_type(bool p_real) const {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	if (!gamepad) {
		return Input::JOYPAD_TYPE_UNKNOWN;
	}
	const SDL_GamepadType type = p_real ? SDL_GetRealGamepadType(gamepad) : SDL_GetGamepadType(gamepad);
	if (type < SDL_GAMEPAD_TYPE_UNKNOWN || type >= SDL_GAMEPAD_TYPE_COUNT) {
		return Input::JOYPAD_TYPE_UNKNOWN;
	}
	return (Input::JoypadType)type;
}

Input::JoyButtonLabel JoypadSDL::Joypad::get_joy_button_label(JoyButton p_button) const {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	if (!gamepad || p_button < JoyButton::A || p_button >= JoyButton::SDL_MAX) {
		return Input::JOY_BUTTON_LABEL_UNKNOWN;
	}
	return (Input::JoyButtonLabel)SDL_GetGamepadButtonLabel(gamepad, (SDL_GamepadButton)p_button);
}

Input::JoyPowerState JoypadSDL::Joypad::get_joy_power_state(int &r_percent) const {
	return (Input::JoyPowerState)SDL_GetJoystickPowerInfo(get_sdl_joystick(), &r_percent);
}

Input::JoyConnectionState JoypadSDL::Joypad::get_joy_connection_state() const {
	return (Input::JoyConnectionState)SDL_GetJoystickConnectionState(get_sdl_joystick());
}

TypedArray<Dictionary> JoypadSDL::Joypad::get_joy_bindings() const {
	TypedArray<Dictionary> result;
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	if (!gamepad) {
		return result;
	}

	int count = 0;
	SDL_GamepadBinding **bindings = SDL_GetGamepadBindings(gamepad, &count);
	if (!bindings) {
		return result;
	}

	for (int i = 0; i < count; i++) {
		const SDL_GamepadBinding *binding = bindings[i];
		Dictionary entry;
		switch (binding->input_type) {
			case SDL_GAMEPAD_BINDTYPE_BUTTON:
				entry["input_type"] = "button";
				entry["input_button"] = binding->input.button;
				break;
			case SDL_GAMEPAD_BINDTYPE_AXIS:
				entry["input_type"] = "axis";
				entry["input_axis"] = binding->input.axis.axis;
				entry["input_axis_min"] = binding->input.axis.axis_min;
				entry["input_axis_max"] = binding->input.axis.axis_max;
				break;
			case SDL_GAMEPAD_BINDTYPE_HAT:
				entry["input_type"] = "hat";
				entry["input_hat"] = binding->input.hat.hat;
				entry["input_hat_mask"] = binding->input.hat.hat_mask;
				break;
			default:
				continue;
		}
		switch (binding->output_type) {
			case SDL_GAMEPAD_BINDTYPE_BUTTON:
				entry["output_type"] = "button";
				entry["output_button"] = (int)binding->output.button;
				break;
			case SDL_GAMEPAD_BINDTYPE_AXIS:
				entry["output_type"] = "axis";
				entry["output_axis"] = (int)binding->output.axis.axis;
				entry["output_axis_min"] = binding->output.axis.axis_min;
				entry["output_axis_max"] = binding->output.axis.axis_max;
				break;
			default:
				continue;
		}
		result.push_back(entry);
	}
	SDL_free(bindings);
	return result;
}

int JoypadSDL::Joypad::get_joy_touchpad_count() const {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	return gamepad ? SDL_GetNumGamepadTouchpads(gamepad) : 0;
}

int JoypadSDL::Joypad::get_joy_touchpad_finger_count(int p_touchpad) const {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	return gamepad ? MAX(SDL_GetNumGamepadTouchpadFingers(gamepad, p_touchpad), 0) : 0;
}

bool JoypadSDL::Joypad::get_joy_touchpad_finger(int p_touchpad, int p_finger, Vector2 &r_position, float &r_pressure) const {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	if (!gamepad) {
		return false;
	}
	bool down = false;
	float x = 0.0f;
	float y = 0.0f;
	float pressure = 0.0f;
	if (!SDL_GetGamepadTouchpadFinger(gamepad, p_touchpad, p_finger, &down, &x, &y, &pressure)) {
		return false;
	}
	r_position = Vector2(x, y);
	r_pressure = pressure;
	return down;
}

int JoypadSDL::Joypad::get_joy_ball_count() const {
	return ball_frame.size();
}

Vector2 JoypadSDL::Joypad::get_joy_ball_motion(int p_ball) const {
	return p_ball >= 0 && (uint32_t)p_ball < ball_frame.size() ? ball_frame[p_ball] : Vector2();
}

bool JoypadSDL::Joypad::has_joy_sensor(Input::JoySensor p_sensor) const {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	return gamepad && SDL_GamepadHasSensor(gamepad, (SDL_SensorType)p_sensor);
}

void JoypadSDL::Joypad::set_joy_sensor_enabled(Input::JoySensor p_sensor, bool p_enable) {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	if (gamepad) {
		SDL_SetGamepadSensorEnabled(gamepad, (SDL_SensorType)p_sensor, p_enable);
	}
}

Vector3 JoypadSDL::Joypad::get_joy_sensor_data(Input::JoySensor p_sensor) const {
	SDL_Gamepad *gamepad = get_sdl_gamepad();
	float data[3] = {};
	if (!gamepad || !SDL_GetGamepadSensorData(gamepad, (SDL_SensorType)p_sensor, data, 3)) {
		return Vector3();
	}
	return Vector3(data[0], data[1], data[2]);
}

// Force feedback, through SDL's haptic API. Mostly for wheels and flight sticks.

SDL_Haptic *JoypadSDL::Joypad::_get_haptic() const {
	if (!haptic_opened) {
		haptic_opened = true;
		SDL_Joystick *joystick = get_sdl_joystick();
		if (joystick && SDL_IsJoystickHaptic(joystick)) {
			haptic = SDL_OpenHapticFromJoystick(joystick);
		}
	}
	return haptic;
}

static Uint16 _haptic_level(const Dictionary &p_params, const String &p_key, float p_default) {
	return (Uint16)(CLAMP((float)p_params.get(p_key, p_default), 0.0f, 1.0f) * 65535.0f);
}

static Sint16 _haptic_signed_level(const Dictionary &p_params, const String &p_key, float p_default) {
	return (Sint16)(CLAMP((float)p_params.get(p_key, p_default), -1.0f, 1.0f) * 32767.0f);
}

static Uint16 _haptic_ms(const Dictionary &p_params, const String &p_key, float p_default) {
	return (Uint16)CLAMP((float)p_params.get(p_key, p_default) * 1000.0f, 0.0f, 65535.0f);
}

bool JoypadSDL::Joypad::_make_haptic_effect(Input::JoyForceEffect p_type, const Dictionary &p_params, SDL_HapticEffect &r_effect, uint64_t &r_length_usec) const {
	SDL_Haptic *device = _get_haptic();
	if (!device) {
		return false;
	}

	memset(&r_effect, 0, sizeof(r_effect));

	const float duration = p_params.get("duration", 0.0f);
	const Uint32 length = duration > 0.0f ? (Uint32)MIN(duration * 1000.0f, 4294967294.0f) : SDL_HAPTIC_INFINITY;
	const Uint16 delay = _haptic_ms(p_params, "delay", 0.0f);
	r_length_usec = duration > 0.0f ? (uint64_t)((duration + delay / 1000.0f) * 1000000.0f) : 0;

	SDL_HapticDirection direction = {};
	const bool steering = p_params.get("steering", SDL_GetNumHapticAxes(device) == 1);
	if (steering) {
		direction.type = SDL_HAPTIC_STEERING_AXIS;
	} else {
		direction.type = SDL_HAPTIC_POLAR;
		direction.dir[0] = (Sint32)(Math::fposmod((float)p_params.get("direction", 0.0f), 360.0f) * 100.0f);
	}

	const Uint16 type = (Uint16)(1u << p_type);
	switch (p_type) {
		case Input::JOY_FORCE_EFFECT_CONSTANT: {
			SDL_HapticConstant &effect = r_effect.constant;
			effect.type = type;
			effect.direction = direction;
			effect.length = length;
			effect.delay = delay;
			effect.level = _haptic_signed_level(p_params, "level", 1.0f);
			effect.attack_length = _haptic_ms(p_params, "attack_length", 0.0f);
			effect.attack_level = _haptic_level(p_params, "attack_level", 0.0f);
			effect.fade_length = _haptic_ms(p_params, "fade_length", 0.0f);
			effect.fade_level = _haptic_level(p_params, "fade_level", 0.0f);
		} break;
		case Input::JOY_FORCE_EFFECT_SINE:
		case Input::JOY_FORCE_EFFECT_SQUARE:
		case Input::JOY_FORCE_EFFECT_TRIANGLE:
		case Input::JOY_FORCE_EFFECT_SAWTOOTH_UP:
		case Input::JOY_FORCE_EFFECT_SAWTOOTH_DOWN: {
			SDL_HapticPeriodic &effect = r_effect.periodic;
			effect.type = type;
			effect.direction = direction;
			effect.length = length;
			effect.delay = delay;
			effect.period = _haptic_ms(p_params, "period", 0.1f);
			effect.magnitude = _haptic_signed_level(p_params, "magnitude", 1.0f);
			effect.offset = _haptic_signed_level(p_params, "offset", 0.0f);
			effect.phase = (Uint16)(Math::fposmod((float)p_params.get("phase", 0.0f), 360.0f) * 100.0f);
			effect.attack_length = _haptic_ms(p_params, "attack_length", 0.0f);
			effect.attack_level = _haptic_level(p_params, "attack_level", 0.0f);
			effect.fade_length = _haptic_ms(p_params, "fade_length", 0.0f);
			effect.fade_level = _haptic_level(p_params, "fade_level", 0.0f);
		} break;
		case Input::JOY_FORCE_EFFECT_RAMP: {
			SDL_HapticRamp &effect = r_effect.ramp;
			effect.type = type;
			effect.direction = direction;
			effect.length = length;
			effect.delay = delay;
			effect.start = _haptic_signed_level(p_params, "start", 1.0f);
			effect.end = _haptic_signed_level(p_params, "end", 0.0f);
			effect.attack_length = _haptic_ms(p_params, "attack_length", 0.0f);
			effect.attack_level = _haptic_level(p_params, "attack_level", 0.0f);
			effect.fade_length = _haptic_ms(p_params, "fade_length", 0.0f);
			effect.fade_level = _haptic_level(p_params, "fade_level", 0.0f);
		} break;
		case Input::JOY_FORCE_EFFECT_SPRING:
		case Input::JOY_FORCE_EFFECT_DAMPER:
		case Input::JOY_FORCE_EFFECT_INERTIA:
		case Input::JOY_FORCE_EFFECT_FRICTION: {
			SDL_HapticCondition &effect = r_effect.condition;
			effect.type = type;
			effect.direction = direction;
			effect.length = length;
			effect.delay = delay;
			for (int axis = 0; axis < 3; axis++) {
				effect.right_sat[axis] = _haptic_level(p_params, "right_saturation", 1.0f);
				effect.left_sat[axis] = _haptic_level(p_params, "left_saturation", 1.0f);
				effect.right_coeff[axis] = _haptic_signed_level(p_params, "right_coefficient", 1.0f);
				effect.left_coeff[axis] = _haptic_signed_level(p_params, "left_coefficient", 1.0f);
				effect.deadband[axis] = _haptic_level(p_params, "deadband", 0.0f);
				effect.center[axis] = _haptic_signed_level(p_params, "center", 0.0f);
			}
		} break;
		case Input::JOY_FORCE_EFFECT_LEFT_RIGHT: {
			SDL_HapticLeftRight &effect = r_effect.leftright;
			effect.type = type;
			effect.length = length;
			effect.large_magnitude = _haptic_level(p_params, "large_magnitude", 1.0f);
			effect.small_magnitude = _haptic_level(p_params, "small_magnitude", 1.0f);
		} break;
		default:
			return false;
	}
	return true;
}

bool JoypadSDL::Joypad::has_joy_force_feedback() const {
	SDL_Joystick *joystick = get_sdl_joystick();
	return joystick && SDL_IsJoystickHaptic(joystick);
}

bool JoypadSDL::Joypad::is_joy_force_effect_supported(Input::JoyForceEffect p_effect) const {
	MutexLock lock(mutex);
	SDL_Haptic *device = _get_haptic();
	return device && (SDL_GetHapticFeatures(device) & (1u << p_effect)) != 0;
}

int JoypadSDL::Joypad::create_joy_force_effect(Input::JoyForceEffect p_effect, const Dictionary &p_params) {
	MutexLock lock(mutex);
	SDL_HapticEffect effect;
	uint64_t length_usec = 0;
	if (!_make_haptic_effect(p_effect, p_params, effect, length_usec)) {
		return -1;
	}
	const int id = SDL_CreateHapticEffect(haptic, &effect);
	if (id < 0) {
		return -1;
	}
	ForceEffect &info = force_effects[id];
	info.type = p_effect;
	info.auto_destroy = p_params.get("_auto_destroy", false);
	info.length_usec = length_usec;
	info.destroy_at_usec = 0;
	return id;
}

bool JoypadSDL::Joypad::update_joy_force_effect(int p_id, const Dictionary &p_params) {
	MutexLock lock(mutex);
	ForceEffect *info = force_effects.getptr(p_id);
	if (!info) {
		return false;
	}
	SDL_HapticEffect effect;
	uint64_t length_usec = 0;
	if (!_make_haptic_effect(info->type, p_params, effect, length_usec)) {
		return false;
	}
	info->length_usec = length_usec;
	return SDL_UpdateHapticEffect(haptic, p_id, &effect);
}

bool JoypadSDL::Joypad::play_joy_force_effect(int p_id, int p_iterations) {
	MutexLock lock(mutex);
	ForceEffect *info = force_effects.getptr(p_id);
	if (!info || !haptic) {
		return false;
	}
	const bool forever = p_iterations <= 0;
	if (!SDL_RunHapticEffect(haptic, p_id, forever ? SDL_HAPTIC_INFINITY : (Uint32)p_iterations)) {
		return false;
	}
	if (info->auto_destroy && !forever && info->length_usec != 0) {
		// A little slack, so the device finishes before the effect goes.
		info->destroy_at_usec = OS::get_singleton()->get_ticks_usec() + info->length_usec * p_iterations + 100000;
	} else {
		info->destroy_at_usec = 0;
	}
	return true;
}

void JoypadSDL::Joypad::stop_joy_force_effect(int p_id) {
	MutexLock lock(mutex);
	if (haptic && force_effects.has(p_id)) {
		SDL_StopHapticEffect(haptic, p_id);
	}
}

void JoypadSDL::Joypad::destroy_joy_force_effect(int p_id) {
	MutexLock lock(mutex);
	if (haptic && force_effects.has(p_id)) {
		SDL_DestroyHapticEffect(haptic, p_id);
		force_effects.erase(p_id);
	}
}

void JoypadSDL::Joypad::stop_joy_force_effects() {
	MutexLock lock(mutex);
	if (haptic) {
		SDL_StopHapticEffects(haptic);
	}
}

void JoypadSDL::Joypad::set_joy_force_feedback_gain(float p_gain) {
	MutexLock lock(mutex);
	SDL_Haptic *device = _get_haptic();
	if (device && (SDL_GetHapticFeatures(device) & SDL_HAPTIC_GAIN)) {
		SDL_SetHapticGain(device, (int)Math::round(p_gain * 100.0f));
	}
}

void JoypadSDL::Joypad::set_joy_force_feedback_autocenter(float p_autocenter) {
	MutexLock lock(mutex);
	SDL_Haptic *device = _get_haptic();
	if (device && (SDL_GetHapticFeatures(device) & SDL_HAPTIC_AUTOCENTER)) {
		SDL_SetHapticAutocenter(device, (int)Math::round(p_autocenter * 100.0f));
	}
}

void JoypadSDL::Joypad::destroy_finished_force_effects(uint64_t p_now_usec) {
	MutexLock lock(mutex);
	if (!haptic || force_effects.is_empty()) {
		return;
	}
	LocalVector<int> finished;
	for (const KeyValue<int, ForceEffect> &E : force_effects) {
		if (E.value.auto_destroy && E.value.destroy_at_usec != 0 && p_now_usec >= E.value.destroy_at_usec) {
			finished.push_back(E.key);
		}
	}
	for (int id : finished) {
		SDL_DestroyHapticEffect(haptic, id);
		force_effects.erase(id);
	}
}

SDL_Joystick *JoypadSDL::Joypad::get_sdl_joystick() const {
	return SDL_GetJoystickFromID(sdl_instance_idx);
}

SDL_Gamepad *JoypadSDL::Joypad::get_sdl_gamepad() const {
	return SDL_GetGamepadFromID(sdl_instance_idx);
}

#endif // SDL_ENABLED
