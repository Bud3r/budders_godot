#include "dither_noise.h"

#include "core/math/random_number_generator.h"
#include <random>


Ref<Noise> DitherNoise::get_dither_map_noise() const {
	return dither_map_noise;
}

void DitherNoise::set_dither_map_noise(const Ref<Noise> &value) {
	if (value.is_valid()) {
		value->connect_changed(callable_mp(this, &DitherNoise::_changed));
	}

	if (dither_map_noise.is_valid()) {
		dither_map_noise->disconnect_changed(callable_mp(this, &DitherNoise::_changed));
	}

	dither_map_noise = value;
	emit_changed();
}

void DitherNoise::set_seed(const int value) {
	seed = value;
	emit_changed();
}

int DitherNoise::get_seed() const {
	return seed;
}

real_t DitherNoise::get_range_max() const {
	return range_max;
}

void DitherNoise::set_range_max(const real_t p_range_max) {
	range_max = p_range_max;
	emit_changed();
}

real_t DitherNoise::get_range_min() const {
	return range_min;
}

void DitherNoise::set_range_min(const real_t p_range_min) {
	range_min = p_range_min;
	emit_changed();
}

real_t DitherNoise::get_noise_1d(real_t p_x) const {
	return get_noise_value(p_x, 0);
}

real_t DitherNoise::get_noise_2dv(Vector2 p_v) const {
	return get_noise_value(p_v.x, p_v.y);
}

real_t DitherNoise::get_noise_2d(real_t p_x, real_t p_y) const {
	return get_noise_value(p_x, p_y);
}

real_t DitherNoise::get_noise_3dv(Vector3 p_v) const {
	return get_noise_value(p_v.x, p_v.y);
}

real_t DitherNoise::get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const {
	return get_noise_value(p_x, p_y);
}

real_t DitherNoise::get_noise_value(real_t p_x, real_t p_y) const {
	if (dither_map_noise == nullptr) {
		return -1;
	}

	const real_t dither_threshold = dither_map_noise->get_noise_2d(p_x, p_y);
	auto hash_value = std::hash<int>::operator()(seed + static_cast<int>(p_x) + static_cast<int>(p_x) * static_cast<int>(p_y));
	double dither_value = static_cast<double>(hash_value) / static_cast<double>(std::numeric_limits<size_t>::max());
	dither_value -= range_min;
	dither_value *= range_max - range_min;
	if (dither_value < dither_threshold) {
		return 1;
	}

	return -1;
}

void DitherNoise::_changed() {
	emit_changed();
}

void DitherNoise::_bind_methods() {
	ADD_GODOT_PROPERTY(
		DitherNoise,
		dither_map_noise,
		PropertyInfo(
			Variant::OBJECT,
			"dither_map_noise",
			PROPERTY_HINT_RESOURCE_TYPE,
			"Noise"))
	ADD_GODOT_PROPERTY(
		int,
		seed,
		PropertyInfo(
			Variant::INT,
			"seed",
			PROPERTY_HINT_NONE
			))
	ADD_GODOT_PROPERTY(
		real_t,
		range_min,
		PropertyInfo(
			Variant::FLOAT,
			"range_min",
			PROPERTY_HINT_NONE
			)
		)
	ADD_GODOT_PROPERTY(
		real_t,
		range_max,
		PropertyInfo(
			Variant::FLOAT,
			"range_max",
			PROPERTY_HINT_NONE
			)
		)
}
