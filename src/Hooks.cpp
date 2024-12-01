#include "Hooks.h"

namespace Hooks
{
	void Install()
	{
		logger::info("{:*^50}", "HOOKS");

		Attach::Install();
		Detach::Install();
		Update ::Install();
	}
}
