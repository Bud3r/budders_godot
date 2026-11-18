/**************************************************************************/
/*  hid_device_sdl.cpp                                                    */
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


#include "hid_device_sdl.h"

#ifdef SDL_ENABLED

#include "core/object/class_db.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hidapi.h>
#include <SDL3/SDL_stdinc.h>

static String _wide_to_string(const wchar_t *p_text) {
	String result;
	if (p_text) {
		result += p_text;
	}
	return result;
}

static Dictionary _device_info_to_dictionary(const SDL_hid_device_info *p_info) {
	Dictionary info;
	info["path"] = p_info->path ? String::utf8(p_info->path) : String();
	info["vendor_id"] = p_info->vendor_id;
	info["product_id"] = p_info->product_id;
	info["serial_number"] = _wide_to_string(p_info->serial_number);
	info["release_number"] = p_info->release_number;
	info["manufacturer"] = _wide_to_string(p_info->manufacturer_string);
	info["product"] = _wide_to_string(p_info->product_string);
	info["usage_page"] = p_info->usage_page;
	info["usage"] = p_info->usage;
	info["interface_number"] = p_info->interface_number;
	info["bluetooth"] = p_info->bus_type == SDL_HID_API_BUS_BLUETOOTH;
	return info;
}

TypedArray<Dictionary> HIDDevice::enumerate(int p_vendor_id, int p_product_id) {
	TypedArray<Dictionary> result;
	if (SDL_hid_init() != 0) {
		return result;
	}
	SDL_hid_device_info *devices = SDL_hid_enumerate((unsigned short)p_vendor_id, (unsigned short)p_product_id);
	for (SDL_hid_device_info *info = devices; info; info = info->next) {
		result.push_back(_device_info_to_dictionary(info));
	}
	SDL_hid_free_enumeration(devices);
	return result;
}

Error HIDDevice::open(const String &p_path) {
	close();
	ERR_FAIL_COND_V(SDL_hid_init() != 0, ERR_UNAVAILABLE);
	device = SDL_hid_open_path(p_path.utf8().get_data());
	return device ? OK : ERR_CANT_OPEN;
}

Error HIDDevice::open_by_id(int p_vendor_id, int p_product_id, const String &p_serial_number) {
	close();
	ERR_FAIL_COND_V(SDL_hid_init() != 0, ERR_UNAVAILABLE);
	if (p_serial_number.is_empty()) {
		device = SDL_hid_open((unsigned short)p_vendor_id, (unsigned short)p_product_id, nullptr);
	} else {
		const Char16String serial16 = p_serial_number.utf16();
		if constexpr (sizeof(wchar_t) == 2) {
			device = SDL_hid_open((unsigned short)p_vendor_id, (unsigned short)p_product_id, (const wchar_t *)serial16.get_data());
		} else {
			Vector<wchar_t> wide;
			for (int i = 0; i < p_serial_number.length(); i++) {
				wide.push_back((wchar_t)p_serial_number[i]);
			}
			wide.push_back(0);
			device = SDL_hid_open((unsigned short)p_vendor_id, (unsigned short)p_product_id, wide.ptr());
		}
	}
	return device ? OK : ERR_CANT_OPEN;
}

void HIDDevice::close() {
	if (device) {
		SDL_hid_close(device);
		device = nullptr;
	}
}

bool HIDDevice::is_open() const {
	return device != nullptr;
}

int HIDDevice::write(const PackedByteArray &p_data) {
	ERR_FAIL_NULL_V_MSG(device, -1, "The HID device is not open.");
	return SDL_hid_write(device, p_data.ptr(), p_data.size());
}

PackedByteArray HIDDevice::read(int p_max_size, int p_timeout_msec) {
	ERR_FAIL_NULL_V_MSG(device, PackedByteArray(), "The HID device is not open.");
	ERR_FAIL_COND_V(p_max_size <= 0, PackedByteArray());
	PackedByteArray data;
	data.resize(p_max_size);
	const int size = SDL_hid_read_timeout(device, data.ptrw(), p_max_size, p_timeout_msec);
	data.resize(MAX(size, 0));
	return data;
}

int HIDDevice::send_feature_report(const PackedByteArray &p_data) {
	ERR_FAIL_NULL_V_MSG(device, -1, "The HID device is not open.");
	return SDL_hid_send_feature_report(device, p_data.ptr(), p_data.size());
}

PackedByteArray HIDDevice::get_feature_report(int p_report_id, int p_max_size) {
	ERR_FAIL_NULL_V_MSG(device, PackedByteArray(), "The HID device is not open.");
	ERR_FAIL_COND_V(p_max_size <= 0, PackedByteArray());
	PackedByteArray data;
	data.resize(p_max_size + 1);
	data.ptrw()[0] = (uint8_t)p_report_id;
	const int size = SDL_hid_get_feature_report(device, data.ptrw(), data.size());
	data.resize(MAX(size, 0));
	return data;
}

PackedByteArray HIDDevice::get_input_report(int p_report_id, int p_max_size) {
	ERR_FAIL_NULL_V_MSG(device, PackedByteArray(), "The HID device is not open.");
	ERR_FAIL_COND_V(p_max_size <= 0, PackedByteArray());
	PackedByteArray data;
	data.resize(p_max_size + 1);
	data.ptrw()[0] = (uint8_t)p_report_id;
	const int size = SDL_hid_get_input_report(device, data.ptrw(), data.size());
	data.resize(MAX(size, 0));
	return data;
}

PackedByteArray HIDDevice::get_report_descriptor() {
	ERR_FAIL_NULL_V_MSG(device, PackedByteArray(), "The HID device is not open.");
	PackedByteArray data;
	data.resize(4096); // HID_API_MAX_REPORT_DESCRIPTOR_SIZE
	const int size = SDL_hid_get_report_descriptor(device, data.ptrw(), data.size());
	data.resize(MAX(size, 0));
	return data;
}

Dictionary HIDDevice::get_info() const {
	ERR_FAIL_NULL_V_MSG(device, Dictionary(), "The HID device is not open.");
	const SDL_hid_device_info *info = SDL_hid_get_device_info(device);
	return info ? _device_info_to_dictionary(info) : Dictionary();
}

String HIDDevice::get_error() const {
	return String::utf8(SDL_GetError());
}

HIDDevice::~HIDDevice() {
	close();
}

void HIDDevice::_bind_methods() {
	ClassDB::bind_static_method("HIDDevice", D_METHOD("enumerate", "vendor_id", "product_id"), &HIDDevice::enumerate, DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("open", "path"), &HIDDevice::open);
	ClassDB::bind_method(D_METHOD("open_by_id", "vendor_id", "product_id", "serial_number"), &HIDDevice::open_by_id, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("close"), &HIDDevice::close);
	ClassDB::bind_method(D_METHOD("is_open"), &HIDDevice::is_open);
	ClassDB::bind_method(D_METHOD("write", "data"), &HIDDevice::write);
	ClassDB::bind_method(D_METHOD("read", "max_size", "timeout_msec"), &HIDDevice::read, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("send_feature_report", "data"), &HIDDevice::send_feature_report);
	ClassDB::bind_method(D_METHOD("get_feature_report", "report_id", "max_size"), &HIDDevice::get_feature_report);
	ClassDB::bind_method(D_METHOD("get_input_report", "report_id", "max_size"), &HIDDevice::get_input_report);
	ClassDB::bind_method(D_METHOD("get_report_descriptor"), &HIDDevice::get_report_descriptor);
	ClassDB::bind_method(D_METHOD("get_info"), &HIDDevice::get_info);
	ClassDB::bind_method(D_METHOD("get_error"), &HIDDevice::get_error);
}

#endif // SDL_ENABLED
