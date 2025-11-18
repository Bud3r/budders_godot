#include "scene/resources/texture.h"
#include "texture_noise.h"

#include <cassert>

Ref<Texture2D> TextureNoise::get_texture() const {
	return texture_;
}

void TextureNoise::set_texture(const Ref<Texture2D> &p_texture) {
	if (texture_ == p_texture) {
		return;
	}

	if (texture_.is_valid()) {
		texture_->disconnect_changed(callable_mp(this, &TextureNoise::_changed));
	}

	texture_ = p_texture;

	if (texture_.is_valid()) {
		texture_->connect_changed(callable_mp(this, &TextureNoise::_changed), 0);
	}

	// texture_->disconnect_changed()
	_changed();
}

real_t TextureNoise::get_noise_1d(real_t p_x) const {
	return get_noise_value(p_x, 0);
}

real_t TextureNoise::get_noise_2dv(Vector2 p_v) const {
	return get_noise_value(p_v.x, p_v.y);
}

real_t TextureNoise::get_noise_2d(real_t p_x, real_t p_y) const {
	return get_noise_value(p_x, p_y);
}

real_t TextureNoise::get_noise_3dv(Vector3 p_v) const {
	return get_noise_value(p_v.x, p_v.y);
}

real_t TextureNoise::get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const {
	return get_noise_value(p_x, p_y);
}

real_t TextureNoise::get_noise_value(real_t p_x, real_t p_y) const {
	if (image_ == nullptr)
		return 0;

	return image_->get_pixel(
		static_cast<int>(Math::floor(p_x)) % image_->get_width(),
		static_cast<int>(Math::floor(p_y)) % image_->get_height()
		).r;
}

void TextureNoise::_changed() {
	if (texture_ != nullptr) {
		image_ = texture_->get_image();
	}
	else {
		image_ = nullptr;
	}
	emit_changed();
}

void TextureNoise::_bind_methods() {
	// ClassDB::bind_method(D_METHOD("_changed"), &TextureNoise::_changed);
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &TextureNoise::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &TextureNoise::get_texture);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
}
