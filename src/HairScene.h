#pragma once

// ---------------------------------------------------------------------------
//  Read-only view of the player's scenegraph: which geometry currently *is*
//  the hair (head-part hair + hairline under the face node, plus anything worn
//  in a wig slot), and a key that changes whenever that set would.
// ---------------------------------------------------------------------------

namespace HP
{
	struct HairGeometry
	{
		RE::BSGeometry* geo{ nullptr };
		std::string     label;          // for logs
		bool            worn{ false };  // came from a biped (wig) slot
	};

	class HairScene
	{
	public:
		// "<race>|<hair>" -- empty when the player has no usable hair head part.
		[[nodiscard]] static std::string Key();

		// Every geometry the offset applies to right now.
		[[nodiscard]] static std::vector<HairGeometry> Current();

		// Only the worn (wig-slot) part of Current(). With a_evenIfOff the
		// slots are walked even while following worn items is switched off --
		// used to find geometry that must be put back when the switch flips.
		[[nodiscard]] static std::vector<HairGeometry> Worn(bool a_evenIfOff = false);

		// Whether worn items in the wig slots are part of Current(). Off by
		// default: those slots are shared with helmets and hoods, which most
		// players do not want to move with the hair.
		static void               SetFollowWorn(bool a_on) { _followWorn = a_on; }
		[[nodiscard]] static bool FollowWorn() { return _followWorn; }

		// Cheap identity of the scene objects Current() would walk: the face
		// node and its direct children, plus the worn item roots. Equal stamps
		// mean nothing was rebuilt, so a full traversal can be skipped.
		using Stamp = std::vector<const void*>;
		[[nodiscard]] static Stamp TakeStamp();

		// Detach geometry under the face node that (a) is in a_touched and
		// (b) no longer belongs to any current head part. Returns what was cut.
		static std::vector<RE::BSGeometry*> DetachOrphans(const std::unordered_set<RE::BSGeometry*>& a_touched);

		// Diagnostics.
		[[nodiscard]] static std::string DescribeFaceNode();
		[[nodiscard]] static std::string WantedNames();

		static bool SameName(std::string_view a_lhs, std::string_view a_rhs);
		static const char* EditorID(RE::TESForm* a_form);

	private:
		static std::vector<std::string> HairNames();
		static RE::NiAVObject*          FaceRoot(const char** a_which = nullptr);
		static void                     AppendWorn(std::vector<HairGeometry>& a_out, bool a_evenIfOff = false);

		static inline std::atomic<bool> _followWorn{ false };
	};
}
