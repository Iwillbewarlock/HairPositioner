#pragma once

// Small helper so every probe line lands in BOTH the log file and the in-game
// console. ConsoleLog::Print is printf-style, so the message is always passed
// through "%s" -- never as the format string itself.
namespace Con
{
	void Line(const std::string& a_msg);
	void Warn(const std::string& a_msg);
	void Err(const std::string& a_msg);

	template <class... Args>
	void Say(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		Line(std::format(a_fmt, std::forward<Args>(a_args)...));
	}

	template <class... Args>
	void SayWarn(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		Warn(std::format(a_fmt, std::forward<Args>(a_args)...));
	}

	template <class... Args>
	void SayErr(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		Err(std::format(a_fmt, std::forward<Args>(a_args)...));
	}
}
