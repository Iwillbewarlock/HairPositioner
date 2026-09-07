#include "Papyrus.h"

#include "Positioner.h"

// ---------------------------------------------------------------------------
//  Native side of HairPositioner.psc.
//
//  Everything here runs on the Papyrus VM thread: nothing touches the
//  scenegraph, the real work is queued onto the main thread by HP.
// ---------------------------------------------------------------------------

namespace Papyrus
{
	namespace
	{
		constexpr auto kScript = "HairPositioner"sv;

		HP::PivotMode Pivot(std::int32_t a_mode)
		{
			return a_mode >= 0 && a_mode < static_cast<std::int32_t>(HP::PivotMode::kCount) ?
			           static_cast<HP::PivotMode>(a_mode) :
			           HP::PivotMode::kBone;
		}

		bool              IsReady(RE::StaticFunctionTag*) { return true; }
		bool              HasHair(RE::StaticFunctionTag*) { return HP::HasTarget(); }
		RE::BSFixedString GetKey(RE::StaticFunctionTag*) { return RE::BSFixedString{ HP::CurrentKey() }; }

		std::int32_t GetChannelCount(RE::StaticFunctionTag*) { return HP::kChannelCount; }
		void         SetChannel(RE::StaticFunctionTag*, std::int32_t a_channel, float a_value) { HP::SetChannel(a_channel, a_value); }
		float        GetChannel(RE::StaticFunctionTag*, std::int32_t a_channel) { return HP::GetChannel(a_channel); }

		std::int32_t GetPivotCount(RE::StaticFunctionTag*) { return static_cast<std::int32_t>(HP::PivotMode::kCount); }
		void         SetPivot(RE::StaticFunctionTag*, std::int32_t a_mode) { HP::SetPivot(Pivot(a_mode)); }
		std::int32_t GetPivot(RE::StaticFunctionTag*) { return static_cast<std::int32_t>(HP::GetPivot()); }
		void         SetFollowWorn(RE::StaticFunctionTag*, bool a_on) { HP::SetFollowWorn(a_on); }
		bool         GetFollowWorn(RE::StaticFunctionTag*) { return HP::GetFollowWorn(); }

		void Reset(RE::StaticFunctionTag*) { HP::ResetAdjust(); }

		void Probe(RE::StaticFunctionTag*) { HP::QueueProbe(); }
		void Show(RE::StaticFunctionTag*) { HP::QueueShow(); }
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			return false;
		}
		a_vm->RegisterFunction("IsReady", kScript, IsReady);
		a_vm->RegisterFunction("HasHair", kScript, HasHair);
		a_vm->RegisterFunction("GetKey", kScript, GetKey);
		a_vm->RegisterFunction("GetChannelCount", kScript, GetChannelCount);
		a_vm->RegisterFunction("SetChannel", kScript, SetChannel);
		a_vm->RegisterFunction("GetChannel", kScript, GetChannel);
		a_vm->RegisterFunction("GetPivotCount", kScript, GetPivotCount);
		a_vm->RegisterFunction("SetPivot", kScript, SetPivot);
		a_vm->RegisterFunction("GetPivot", kScript, GetPivot);
		a_vm->RegisterFunction("SetFollowWorn", kScript, SetFollowWorn);
		a_vm->RegisterFunction("GetFollowWorn", kScript, GetFollowWorn);
		a_vm->RegisterFunction("Reset", kScript, Reset);
		a_vm->RegisterFunction("Probe", kScript, Probe);
		a_vm->RegisterFunction("Show", kScript, Show);

		SKSE::log::info("papyrus natives registered on {}", kScript);
		return true;
	}
}
