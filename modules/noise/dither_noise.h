#pragma once
#include "noise.h"
#include "scene/2d/node_2d.h"

#include <random>

#define ADD_GODOT_PROPERTY(class, name, property_info)\
	ClassDB::bind_method(D_METHOD("set_" #name, #name), &set_##name##);\
	ClassDB::bind_method(D_METHOD("get_" #name), &get_##name##);\
	ADD_PROPERTY(property_info, "set_" #name, +"get_" #name);\

class DitherNoise : public Noise {
	GDCLASS(DitherNoise, Noise)
	OBJ_SAVE_TYPE(DitherNoise)

private:
	Ref<Noise> dither_map_noise;
	int seed = 0;
	real_t range_max;
	real_t range_min;

public:
	real_t get_range_max() const;

	void set_range_max(real_t range_max);

	real_t get_range_min() const;

	void set_range_min(real_t p_range_min);

protected:
	static void _bind_methods();

public:
	Ref<Noise> get_dither_map_noise() const;
	void set_dither_map_noise(const Ref<Noise> &value);

	void set_seed(int value);
	int get_seed() const;

	real_t get_noise_1d(real_t p_x) const override;
	real_t get_noise_2dv(Vector2 p_v) const override;
	real_t get_noise_2d(real_t p_x, real_t p_y) const override;
	real_t get_noise_3dv(Vector3 p_v) const override;
	real_t get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const override;
	real_t get_noise_value(real_t p_x, real_t p_y) const;

	void _changed();
};
