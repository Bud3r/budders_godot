/**************************************************************************/
/*  hid_device_sdl.h                                                      */
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

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"

typedef struct SDL_hid_device SDL_hid_device;

// Raw access to USB and Bluetooth HID devices, for controllers and features SDL does not wrap.
class HIDDevice : public RefCounted {
	GDCLASS(HIDDevice, RefCounted);

	SDL_hid_device *device = nullptr;

protected:
	static void _bind_methods();

public:
	static TypedArray<Dictionary> enumerate(int p_vendor_id = 0, int p_product_id = 0);

	Error open(const String &p_path);
	Error open_by_id(int p_vendor_id, int p_product_id, const String &p_serial_number = String());
	void close();
	bool is_open() const;

	int write(const PackedByteArray &p_data);
	PackedByteArray read(int p_max_size, int p_timeout_msec = 0);
	int send_feature_report(const PackedByteArray &p_data);
	PackedByteArray get_feature_report(int p_report_id, int p_max_size);
	PackedByteArray get_input_report(int p_report_id, int p_max_size);
	PackedByteArray get_report_descriptor();
	Dictionary get_info() const;
	String get_error() const;

	~HIDDevice();
};
