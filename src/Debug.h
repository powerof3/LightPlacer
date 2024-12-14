#pragma once

namespace Debug
{
	template <class T>
	struct ConsoleCommandHandler
	{
		static void Install()
		{
			if (auto function = RE::SCRIPT_FUNCTION::LocateConsoleCommand(T::OG_COMMAND); function) {
				function->functionName = T::LONG_NAME.data();
				function->shortName = T::SHORT_NAME.data();
				function->helpString = T::HELP.data();
				function->referenceFunction = false;

				if constexpr (requires { T::SCRIPT_PARAMS; }) {
					static RE::SCRIPT_PARAMETER params[] = {
						{ T::SCRIPT_PARAMS }
					};
					function->SetParameters(params);
				} else {
					function->SetParameters();
				}

				function->executeFunction = &T::Execute;
				function->conditionFunction = nullptr;

				logger::info("Installed {} console command", T::LONG_NAME);
			}
		}
	};

	void Install();
}
