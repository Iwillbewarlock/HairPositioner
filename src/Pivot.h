#pragma once

#include "HairOffset.h"
#include "HairScene.h"
#include "MeshPatch.h"

// The point the offset rotates and scales about. One pivot is shared by every
// geometry of the hair (hair, hairline, wig) so they move as a unit.
namespace HP
{
	class PivotSolver
	{
	public:
		[[nodiscard]] static RE::NiPoint3 Solve(PivotMode a_mode, const std::vector<HairGeometry>& a_geos, const MeshPatch::PatchMap& a_patches);

		// Skin-to-bone transform of the bone that owns most of this mesh's
		// vertices (identity when the mesh is not skinned). Maps bind space to
		// that bone's space.
		[[nodiscard]] static RE::NiTransform BindFrame(RE::BSGeometry* a_geo);

		// Bone-space centroid of every rest vertex of a_geos (each mapped through
		// its own BindFrame).
		[[nodiscard]] static RE::NiPoint3 BoneSpaceCentroid(const std::vector<HairGeometry>& a_geos, const MeshPatch::PatchMap& a_patches);

	private:
		static RE::NiPoint3 HeadBone(const std::vector<HairGeometry>& a_geos);
		static RE::NiPoint3 Centroid(const std::vector<HairGeometry>& a_geos, const MeshPatch::PatchMap& a_patches);
	};
}
