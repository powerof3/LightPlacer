#include "LightControllers.h"

#include "LightData.h"
#include "Settings.h"

LightControllers::LightControllers(const LIGH::LightDefinition& a_lightDef)
{
	const bool randomAnimStart = a_lightDef.data.flags.any(LIGHT_FLAGS::RandomAnimStart);

#define INIT_CONTROLLER(controller)                                              \
	if (!a_lightDef.controller.empty()) {                                        \
		(controller) = LightController(&a_lightDef.controller, randomAnimStart); \
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
	const auto settings = Settings::GetSingleton();

	if (colorController) {
		a_light->diffuse = colorController.GetValue(a_delta);
	}
	if (radiusController) {
		const auto newRadius = radiusController.GetValue(a_delta) * a_scalingFactor * settings->GetGlobalLightRadiusMult();
		a_light->radius.x = newRadius;
		a_light->radius.y = newRadius;
		a_light->SetLightAttenuation(newRadius);
	}
	if (fadeController) {
		a_light->fade = fadeController.GetValue(a_delta) * a_scalingFactor * settings->GetGlobalLightFadeMult();
	}
	if (const auto parentNode = a_light->parent) {
		if (positionController) {
			parentNode->local.translate = positionController.GetValue(a_delta);
		}
		if (rotationController) {
			auto rotation = rotationController.GetValue(a_delta);
			RE::WrapRotation(rotation);
			parentNode->local.rotate.SetEulerAnglesXYZ(rotation.x, rotation.y, rotation.z);
		}
		if (positionController || rotationController) {
			UpdateNode(parentNode);
		}
	}
}
