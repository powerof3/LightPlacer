#include "Hooks.h"

namespace Hooks
{
	void Install()
	{
		logger::info("{:*^30}", "HOOKS");

		Attach::Install();
		Detach::Install();
		Update ::Install();
	}
}
