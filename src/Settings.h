#pragma once

// Data/SKSE/Plugins/HairPositioner.ini
namespace HP
{
	struct Settings
	{
		bool                       stripMorphData{ true };  // drop "FOD" base-morph data from adjusted meshes
		std::vector<std::uint32_t> wigSlots{ 1, 11 };       // biped object indices (armor slot - 30); default 31,41

		static const Settings& Get();
		static void            Load();  // call once at kDataLoaded
	};
}
