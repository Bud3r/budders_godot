#pragma once
#include "modules/noise/noise.h"
#include "scene/resources/texture.h"
class TextureNoise : public Noise {
	GDCLASS(TextureNoise, Noise)
	OBJ_SAVE_TYPE(TextureNoise)

private:
	Ref<Texture2D> texture_;
	Ref<Image> image_;

protected:
	static void _bind_methods();

public:
	Ref<Texture2D> get_texture() const;
	void set_texture(const Ref<Texture2D> &texture);
	real_t get_noise_1d(real_t p_x) const override;
	real_t get_noise_2dv(Vector2 p_v) const override;
	real_t get_noise_2d(real_t p_x, real_t p_y) const override;
	real_t get_noise_3dv(Vector3 p_v) const override;
	real_t get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const override;
	real_t get_noise_value(real_t p_x, real_t p_y) const;

	void _changed();
};
