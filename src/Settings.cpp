#include "Settings.h"

namespace HP
{
	namespace
	{
		Settings g_settings;

		std::filesystem::path IniPath()
		{
			return std::filesystem::path{ "Data" } / "SKSE" / "Plugins" / "HairPositioner.ini";
		}

		std::string Trim(std::string a_text)
		{
			std::erase_if(a_text, [](unsigned char c) { return std::isspace(c); });
			return a_text;
		}

		bool KeyIs(const std::string& a_key, std::string_view a_name)
		{
			return std::ranges::equal(a_key, a_name, [](char a_l, char a_r) {
				return std::tolower(static_cast<unsigned char>(a_l)) == std::tolower(static_cast<unsigned char>(a_r));
			});
		}

		bool ToBool(std::string a_text)
		{
			a_text = Trim(std::move(a_text));
			for (auto& c : a_text) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return a_text == "true" || a_text == "1" || a_text == "yes" || a_text == "on";
		}

		// "31,41" -> biped object indices
		std::vector<std::uint32_t> ToWigSlots(std::string a_text)
		{
			std::vector<std::uint32_t> out;
			std::istringstream         in{ Trim(std::move(a_text)) };
			std::string                item;
			while (std::getline(in, item, ',')) {
				try {
					const auto armor = static_cast<std::uint32_t>(std::stoul(item));
					if (armor >= 30 && armor - 30 < RE::BIPED_OBJECTS::kTotal) {
						out.push_back(armor - 30);
					}
				} catch (...) {
				}
			}
			return out;
		}
	}

	const Settings& Settings::Get()
	{
		return g_settings;
	}

	void Settings::Load()
	{
		Settings      s;
		std::ifstream in{ IniPath() };
		if (!in) {
			SKSE::log::info("no HairPositioner.ini -- defaults: StripMorphData = true, WigSlots = 31,41");
			g_settings = s;
			return;
		}
		std::string line;
		while (std::getline(in, line)) {
			if (const auto hash = line.find('#'); hash != std::string::npos) {
				line.erase(hash);
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = Trim(line.substr(0, eq));
			const auto value = line.substr(eq + 1);
			if (KeyIs(key, "StripMorphData")) {
				s.stripMorphData = ToBool(value);
			} else if (KeyIs(key, "WigSlots")) {
				s.wigSlots = ToWigSlots(value);
			}
		}
		g_settings = s;

		std::string slots;
		for (const auto idx : s.wigSlots) {
			slots += std::format("{}{}", slots.empty() ? "" : ",", idx + 30);
		}
		SKSE::log::info("StripMorphData = {}, WigSlots = {}", s.stripMorphData, slots.empty() ? "(none)" : slots);
	}
}
