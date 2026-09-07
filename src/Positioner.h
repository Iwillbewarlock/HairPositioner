#pragma once

#include "HairOffset.h"

// ---------------------------------------------------------------------------
//  Public face of the plugin: holds the player's hair offset, decides when to
//  (re)apply it, and exposes the Papyrus / console entry points.
//
//  Threading: the Papyrus VM calls the setters; they only touch the offset
//  under a lock and queue the scenegraph work onto the game's main thread via
//  the SKSE task interface. Everything that touches meshes runs there.
//
//  Modules:  HairOffset  -- the nine channels and the affine map they make
//            HairScene   -- which geometry is "the hair" right now
//            MeshPatch   -- rest positions + writing a mesh's vertices
//            PivotSolver -- the shared pivot point
//            Settings    -- ini
// ---------------------------------------------------------------------------

namespace HP
{
	inline constexpr std::uint32_t kHairSlot = 3;  // BGSHeadPart::HeadPartType::kHair

	[[nodiscard]] std::string CurrentKey();

	// ---- Papyrus-facing (VM thread) -----------------------------------------
	void      SetChannel(std::int32_t a_channel, float a_value);
	float     GetChannel(std::int32_t a_channel);
	void      SetPivot(PivotMode a_mode);
	PivotMode GetPivot();
	void      SetFollowWorn(bool a_on);  // move worn items in the wig slots too (off by default)
	bool      GetFollowWorn();
	void      ResetAdjust();
	bool      HasTarget();
	void      QueueProbe();
	void      QueueShow();

	// ---- polling cadence ------------------------------------------------------
	// Fast while RaceMenu is open or right after an equip, so a hair swap is
	// re-applied on practically the next frame; relaxed otherwise.
	void                      SetMenuOpen(bool a_open);
	void                      BoostPolling(std::chrono::milliseconds a_for);
	std::chrono::milliseconds TickInterval();

	// ---- main thread ----------------------------------------------------------
	void Probe();
	void Show();
	void Tick();        // heartbeat: re-apply only if our write was lost
	void ReapplyNow();  // from the event sinks

	// ---- co-save ----------------------------------------------------------------
	void OnSave(SKSE::SerializationInterface* a_intfc);
	void OnLoad(SKSE::SerializationInterface* a_intfc);
	void OnRevert(SKSE::SerializationInterface* a_intfc);

	HairOffset OffsetSnapshot();
	void       ReplaceOffset(const HairOffset& a_offset);
	void       QueueApply();
}
