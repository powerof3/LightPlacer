#include "LightControllers.h"

#include "LightData.h"

bool LightAnimData::GetValidColor() const { return IsValid(color); }

bool LightAnimData::GetValidFade() const { return IsValid(fade); }

bool LightAnimData::GetValidRadius() const { return IsValid(radius); }

bool LightAnimData::GetValidTranslation() const { return IsValid(translation); }

bool LightAnimData::GetValidRotation() const { return IsValid(rotation); }

LightControllers::LightControllers(const LIGH::LightSourceData& a_src)
{
	const bool randomAnimStart = a_src.data.flags.any(LIGHT_FLAGS::RandomAnimStart);

#define INIT_CONTROLLER(controller)                                        \
	if (!a_src.controller.empty()) {                                       \
		(controller) = LightController(a_src.controller, randomAnimStart); \
	}

	INIT_CONTROLLER(colorController)
	INIT_CONTROLLER(radiusController)
	INIT_CONTROLLER(fadeController)
	INIT_CONTROLLER(positionController)
	INIT_CONTROLLER(rotationController)

#undef INIT_CONTROLLER
}

void LightControllers::UpdateAnimation(const RE::NiPointer<RE::NiPointLight>& a_light, float a_delta, float a_scalingFactor)
{
	if (colorController) {
		a_light->diffuse = colorController->GetValue(a_delta);
	}
	if (radiusController) {
		const auto newRadius = radiusController->GetValue(a_delta) * a_scalingFactor;
		a_light->radius = { newRadius, newRadius, newRadius };
		a_light->SetLightAttenuation(newRadius);
	}
	if (fadeController) {
		a_light->fade = fadeController->GetValue(a_delta);
	}
	if (const auto parentNode = a_light->parent) {
		if (positionController) {
			parentNode->local.translate = positionController->GetValue(a_delta);
		}
		if (rotationController) {
			auto rotation = rotationController->GetValue(a_delta);
			RE::WrapRotation(rotation);
			parentNode->local.rotate.SetEulerAnglesXYZ(rotation.x, rotation.y, rotation.z);
		}
		if (positionController || rotationController) {
			UpdateNode(parentNode);
		}
	}
}
