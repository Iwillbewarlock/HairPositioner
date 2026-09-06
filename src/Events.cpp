#include "Events.h"

#include "Positioner.h"

// ---------------------------------------------------------------------------
//  Re-application is driven by the engine's own event sources:
//
//    TESObjectLoadedEvent(player)  -> the 3D (incl. facegen) was (re)assembled
//                                     after a cell change / save load / rebuild
//    MenuOpenCloseEvent("RaceSex Menu") -> opening: poll fast (~33ms) so a
//                                     hair swap inside the menu is re-applied
//                                     on practically the next frame; closing:
//                                     re-apply after the head rebuild
//    TESEquipEvent(player)         -> something was (un)equipped, maybe a wig;
//                                     its 3D attaches a little later, so poll
//                                     fast for a moment (no eager re-apply)
//
//  On any of these we queue a main-thread re-apply. The heartbeat stays as a
//  safety net for rebuild paths no event covers.
// ---------------------------------------------------------------------------

namespace Events
{
	namespace
	{
		void QueueReapply()
		{
			if (auto* task = SKSE::GetTaskInterface()) {
				task->AddTask([]() { HP::ReapplyNow(); });
			}
		}

		class Sink :
			public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
			public RE::BSTEventSink<RE::TESEquipEvent>
		{
		public:
			static Sink* GetSingleton()
			{
				static Sink singleton;
				return std::addressof(singleton);
			}

			RE::BSEventNotifyControl ProcessEvent(
				const RE::TESObjectLoadedEvent* a_event,
				RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (a_event && a_event->loaded && player && a_event->formID == player->GetFormID()) {
					QueueReapply();
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(
				const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->menuName == RE::RaceSexMenu::MENU_NAME) {
					HP::SetMenuOpen(a_event->opening);
					if (!a_event->opening) {
						// RaceMenu closing rebuilds the head -- re-apply just after,
						// and keep polling fast while the rebuild settles.
						HP::BoostPolling(std::chrono::milliseconds(3000));
						QueueReapply();
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(
				const RE::TESEquipEvent* a_event,
				RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				// Fires for every item the player (un)equips, weapons included,
				// so no eager re-apply here: just poll fast for a moment and let
				// the heartbeat notice a new wig mesh (its stamp changes).
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (a_event && player && a_event->actor && a_event->actor.get() == player) {
					HP::BoostPolling(std::chrono::milliseconds(3000));
				}
				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			Sink() = default;
		};
	}

	void Install()
	{
		auto* sink = Sink::GetSingleton();

		if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESObjectLoadedEvent>(sink);
			holder->AddEventSink<RE::TESEquipEvent>(sink);
			SKSE::log::info("sink: TESObjectLoadedEvent, TESEquipEvent");
		} else {
			SKSE::log::error("no ScriptEventSourceHolder -- 3D-load re-apply disabled");
		}

		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(sink);
			SKSE::log::info("sink: MenuOpenCloseEvent");
		} else {
			SKSE::log::error("no UI singleton -- RaceMenu-close re-apply disabled");
		}
	}
}
