#include "Hooks.h"

#include "Hooks/Attach.h"
#include "Hooks/Detach.h"
#include "Hooks/Misc.h"
#include "Hooks/Update.h"
#include "Settings.h"

namespace Hooks
{
	void Install()
	{
		Settings::GetSingleton()->LoadSettings();

		logger::info("{:*^50}", "HOOKS");

		Attach::Install();
		Detach::Install();
		Update ::Install();

		Misc::Install();
	}
}
