#pragma once

#include "HairOffset.h"

// ---------------------------------------------------------------------------
//  One patched geometry: its untouched ("rest") vertex positions and whatever
//  is needed to (a) recognise that our positions are still in place and
//  (b) write a new set.
//
//  Positions live in one of two places, and it is NOT the RTTI that decides:
//
//    skin partition  -- the vertexDesc dynamic-size nibble is 0. RaceMenu turns
//                       head parts into BSDynamicTriShape, but with that nibble
//                       at 0 the dynamic buffer is empty and the positions are
//                       still in the (shared, GPU-backed) skin partition. We
//                       deep-copy the partition, rewrite it, upload it and swap
//                       it into the skin instance.
//    dynamic buffer  -- nibble != 0 and a live dynamicData block: write there
//                       under the shape's spin lock.
// ---------------------------------------------------------------------------

namespace HP
{
	class MeshPatch
	{
	public:
		enum class Storage
		{
			kNone,
			kSkinPartition,
			kDynamicBuffer
		};

		using Positions = std::vector<RE::NiPoint3>;
		using PatchMap = std::unordered_map<RE::BSGeometry*, MeshPatch>;

		[[nodiscard]] static Storage StorageOf(RE::BSGeometry* a_geo);

		// Make sure rest positions are held (capturing them from the engine's
		// buffer only while the engine still owns it). Returns false, with a
		// logged reason, when this geometry cannot be patched.
		bool Capture(RE::BSGeometry* a_geo, const PatchMap& a_others, bool a_verbose);

		// Write a_map(rest[i]) for every vertex straight into the mesh.
		bool Commit(RE::BSGeometry* a_geo, const OffsetMap& a_map, bool a_verbose);

		// Are the positions we last wrote still what the mesh shows?
		[[nodiscard]] bool Holds(RE::BSGeometry* a_geo) const;

		// Nothing left to do for this mesh: it holds our write, or it is one
		// we could not patch and retrying would only churn.
		[[nodiscard]] bool Settled(RE::BSGeometry* a_geo) const;

		// Drop the session-wide rest-position registry (call on save load).
		static void ClearRestRegistry();

		[[nodiscard]] bool             HasRest() const { return _hasRest; }
		[[nodiscard]] const Positions& Rest() const { return _rest; }
		[[nodiscard]] const std::string& LastSkip() const { return _lastSkip; }

		// diagnostics
		[[nodiscard]] static std::uint32_t DynamicVertexCount(RE::BSGeometry* a_geo, RE::BSDynamicTriShape* a_dyn);
		[[nodiscard]] static bool          HasMorphBase(RE::BSGeometry* a_geo);
		[[nodiscard]] static std::uint64_t RawDesc(const RE::BSGraphics::VertexDesc& a_desc);

	private:
		bool Skip(RE::BSGeometry* a_geo, bool a_verbose, std::string a_reason, bool a_permanent = false);
		bool CaptureDynamic(RE::BSGeometry* a_geo, RE::BSDynamicTriShape* a_dyn, bool a_verbose);
		bool CapturePartition(RE::BSGeometry* a_geo, const PatchMap& a_others, bool a_verbose);
		bool CommitDynamic(RE::BSGeometry* a_geo, RE::BSDynamicTriShape* a_dyn, const OffsetMap& a_map);
		bool CommitPartition(RE::BSGeometry* a_geo, const OffsetMap& a_map, bool a_verbose);
		bool DynamicHolds(RE::BSGeometry* a_geo, RE::BSDynamicTriShape* a_dyn) const;
		void Remember(const OffsetMap& a_map);

		RE::NiPointer<RE::BSGeometry>      _keepAlive;  // the geometry stays valid while we track it
		Storage                            _storage{ Storage::kNone };
		RE::NiPointer<RE::NiSkinPartition> _owned;    // partition we swapped in
		RE::NiPointer<RE::NiSkinPartition> _refused;  // partition we could not copy; never retry
		std::uint32_t                      _stride{ 0 };

		Positions                   _rest;
		bool                        _hasRest{ false };
		std::array<RE::NiPoint3, 3> _sample{};  // first / middle / last of the last write
		std::string                 _lastSkip;
		bool                        _announced{ false };
		bool                        _gaveUp{ false };  // permanent skip: never becomes patchable
	};
}
