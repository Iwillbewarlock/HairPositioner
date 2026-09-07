#pragma once

// ---------------------------------------------------------------------------
//  The adjustment itself and the affine map it produces.
//
//  Nine channels (move xyz, rotate xyz in degrees, scale xyz) plus a pivot
//  choice. A vertex v in the mesh's own space is moved to
//
//      v' = A * v + b
//
//  where A = diag(scale) * R and b = scale (.) (move - R * pivot) + pivot.
//  That is the same as scaling (about the pivot) the rotated-and-shifted
//  vertex; precomputing A and b keeps the per-vertex work to one matrix
//  multiply and one add.
// ---------------------------------------------------------------------------

namespace HP
{
	enum class PivotMode : std::uint32_t
	{
		kBone = 0,    // head bone (default)
		kObject = 1,  // mean of the hair's rest vertices
		kSpace = 2,   // model origin
		kCount = 3
	};

	// Channels: 0-2 move x/y/z, 3-5 rotate x/y/z (degrees), 6-8 scale x/y/z.
	inline constexpr std::int32_t kChannelCount = 9;

	struct HairOffset
	{
		RE::NiPoint3 move{ 0.0f, 0.0f, 0.0f };
		RE::NiPoint3 rotate{ 0.0f, 0.0f, 0.0f };  // degrees
		RE::NiPoint3 scale{ 1.0f, 1.0f, 1.0f };
		PivotMode    pivot{ PivotMode::kBone };
		bool         worn{ false };  // also move worn items in the wig slots (helmets included!)

		// Identity of the nine channels only; `worn` is a target choice, not a transform.
		[[nodiscard]] bool  IsIdentity() const;
		[[nodiscard]] float Channel(std::int32_t a_channel) const;
		void                SetChannel(std::int32_t a_channel, float a_value);
	};

	// The affine map for one offset and one pivot point.
	class OffsetMap
	{
	public:
		// Map in the mesh's own (bind) space about a_pivot.
		OffsetMap(const HairOffset& a_offset, const RE::NiPoint3& a_pivot);

		// Map defined in BONE space (about a_pivotBone, in bone coordinates) and
		// pulled back into this mesh's bind space through a_bind, the mesh's
		// skin-to-bone transform of its dominant bone: v' = bind^-1(M(bind(v))).
		// Pieces of one hair then move by the same amount on screen even when
		// their bind transforms carry different scales or rotations.
		OffsetMap(const HairOffset& a_offset, const RE::NiPoint3& a_pivotBone, const RE::NiTransform& a_bind);

		[[nodiscard]] RE::NiPoint3 operator()(const RE::NiPoint3& a_v) const;

		// R = Ry(yaw) * Rz(roll) * Rx(pitch), degrees in.
		[[nodiscard]] static RE::NiMatrix3 Rotation(const RE::NiPoint3& a_deg);

	private:
		RE::NiMatrix3 _a;
		RE::NiPoint3  _b;
	};
}
