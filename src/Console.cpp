#include "Console.h"

namespace Con
{
	namespace
	{
		void Echo(const std::string& a_msg)
		{
			if (auto* console = RE::ConsoleLog::GetSingleton()) {
				console->Print("%s", a_msg.c_str());
			}
		}
	}

	void Line(const std::string& a_msg)
	{
		SKSE::log::info("{}", a_msg);
		Echo(a_msg);
	}

	void Warn(const std::string& a_msg)
	{
		SKSE::log::warn("{}", a_msg);
		Echo("! " + a_msg);
	}

	void Err(const std::string& a_msg)
	{
		SKSE::log::error("{}", a_msg);
		Echo("!! " + a_msg);
	}
}
