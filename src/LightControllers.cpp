#include "LightControllers.h"

namespace Animation
{
	LightData LightData::operator+(const LightData& a_rhs) const
	{
		LightData result;
		if (IsValid(color) && IsValid(a_rhs.color)) {
			result.color = color + a_rhs.color;
		}
		if (IsValid(radius) && IsValid(a_rhs.radius)) {
			result.radius = radius + a_rhs.radius;
		}
		if (IsValid(fade) && IsValid(a_rhs.fade)) {
			result.fade = fade + a_rhs.fade;
		}
		if (IsValid(translation) && IsValid(a_rhs.translation)) {
			result.translation = translation + a_rhs.translation;
		}
		if (IsValid(rotation) && IsValid(a_rhs.rotation)) {
			result.rotation = rotation + a_rhs.rotation;
		}
		return result;
	}

	LightData LightData::operator-(const LightData& a_rhs) const
	{
		LightData result;
		if (IsValid(color) && IsValid(a_rhs.color)) {
			result.color = color - a_rhs.color;
		}
		if (IsValid(radius) && IsValid(a_rhs.radius)) {
			result.radius = radius - a_rhs.radius;
		}
		if (IsValid(fade) && IsValid(a_rhs.fade)) {
			result.fade = fade - a_rhs.fade;
		}
		if (IsValid(translation) && IsValid(a_rhs.translation)) {
			result.translation = translation - a_rhs.translation;
		}
		if (IsValid(rotation) && IsValid(a_rhs.rotation)) {
			result.rotation = rotation - a_rhs.rotation;
		}
		return result;
	}

	LightData LightData::operator*(float a_scalar) const
	{
		LightData result;
		if (IsValid(color)) {
			result.color = color * a_scalar;
		}
		if (IsValid(radius)) {
			result.radius = radius * a_scalar;
		}
		if (IsValid(fade)) {
			result.fade = fade * a_scalar;
		}
		if (IsValid(translation)) {
			result.translation = translation * a_scalar;
		}
		if (IsValid(rotation)) {
			result.rotation = rotation * a_scalar;
		}
		return result;
	}

	bool LightData::GetValidColor() const { return IsValid(color); }

	bool LightData::GetValidFade() const { return IsValid(fade); }

	bool LightData::GetValidRadius() const { return IsValid(radius); }

	bool LightData::GetValidTranslation() const { return IsValid(translation); }

	bool LightData::GetValidRotation() const { return IsValid(rotation); }

	void LightData::Update(const RE::NiPointer<RE::NiPointLight>& a_niLight, float a_scale) const
	{
		if (IsValid(color)) {
			a_niLight->diffuse = color;
		}
		if (IsValid(radius)) {
			a_niLight->radius = { radius, radius, radius } * a_scale;
			a_niLight->SetLightAttenuation(radius);
		}
		if (IsValid(fade)) {
			a_niLight->fade = fade;
		}

		const bool validTranslation = IsValid(translation);
		const bool validRotation = IsValid(rotation);

		if (validTranslation || validRotation) {
			if (auto parentNode = a_niLight->parent) {
				if (validTranslation) {
					parentNode->local.translate = translation;
				}
				if (validRotation) {
					parentNode->local.rotate.SetEulerAnglesXYZ(RE::deg_to_rad(rotation[0]), RE::deg_to_rad(rotation[1]), RE::deg_to_rad(rotation[2]));
				}
				RE::UpdateNode(parentNode);
			}
		}
	}

	LightData operator*(float a_scalar, const LightData& data)
	{
		return data * a_scalar;
	}
}
