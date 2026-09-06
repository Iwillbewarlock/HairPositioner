#include "Events.h"
#include "Papyrus.h"
#include "Positioner.h"
#include "Settings.h"

// ---------------------------------------------------------------------------
//  Plugin entry. Deliberately hook-free.
//
//  There are NO code hooks in this plugin any more. Every interaction with the
//  engine is a plain function call through an address-library id, and every
//  one of those ids has been checked to exist in the 1.6.1170 database. The
//  console commands live in HairPositioner.psc and are reached through
//  the vanilla `cgf` console command, so nothing here has to patch the console.
//
//  (The earlier build hooked Script::CompileAndRun with write_branch<5> on the
//  function entry. That was wrong twice over: write_branch patches an existing
//  jmp/call site, not a function prologue, and the id it used, 21890, does not
//  exist in 1.6.1130+ where CommonLibSSE-NG silently resolves it to the next
//  id. Removed rather than repaired.)
// ---------------------------------------------------------------------------

namespace
{
	void InitLogging()
	{
		auto path = SKSE::log::log_directory();
		if (!path) {
			return;
		}
		*path /= "HairPositioner.log";

		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto logger = std::make_shared<spdlog::logger>("global", std::move(sink));
		logger->set_level(spdlog::level::info);
		logger->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(logger));
		spdlog::set_pattern("[%H:%M:%S.%e] %v");
	}

	// Persistence needs a heartbeat: something that periodically checks whether
	// the engine has rebuilt the player's 3D and re-applies.
	//
	// The heartbeat is a detached thread that sleeps, NOT a task that re-queues
	// itself. SKSE drains its task queue with `while (!empty)`, so a task that
	// calls AddTask from inside its own callback is picked up by the very same
	// drain pass and the main thread never leaves the loop -- the game hangs
	// with no crash and no log. Sleeping off-thread and posting one short-lived
	// task per tick cannot do that.
	std::atomic<bool> g_tickerStarted{ false };

	void StartTicker()
	{
		bool expected = false;
		if (!g_tickerStarted.compare_exchange_strong(expected, true)) {
			return;  // already running
		}

		std::thread([]() {
			for (;;) {
				std::this_thread::sleep_for(HP::TickInterval());
				auto* task = SKSE::GetTaskInterface();
				if (!task) {
					continue;
				}
				task->AddTask([]() { HP::Tick(); });
			}
		}).detach();

		SKSE::log::info("heartbeat started (500ms; 33ms while RaceMenu is open or after an equip)");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			// Settings only. Nothing that touches the scenegraph runs while the
			// main menu is up -- there is no player there to adjust.
			HP::Settings::Load();
			SKSE::log::info("ready -- console: cgf \"HairPositioner.Probe\"");
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Events::Install();  // lifecycle re-apply, hook-free
			StartTicker();
			HP::ReapplyNow();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	InitLogging();

	SKSE::log::info("HairPositioner loaded (runtime {})", REL::Module::get().version().string());

	if (auto* papyrus = SKSE::GetPapyrusInterface()) {
		papyrus->Register(Papyrus::Register);
	} else {
		SKSE::log::error("no papyrus interface -- the RaceMenu sliders will not work");
	}

	// The adjustment lives in the co-save, per actor.
	if (auto* serialization = SKSE::GetSerializationInterface()) {
		serialization->SetUniqueID('HPOS');
		serialization->SetSaveCallback(HP::OnSave);
		serialization->SetLoadCallback(HP::OnLoad);
		serialization->SetRevertCallback(HP::OnRevert);
	} else {
		SKSE::log::error("no serialization interface -- adjustments will not be saved");
	}

	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
