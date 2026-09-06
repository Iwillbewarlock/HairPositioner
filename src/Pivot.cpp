#include "Pivot.h"

namespace HP
{
	RE::NiPoint3 PivotSolver::Solve(PivotMode a_mode, const std::vector<HairGeometry>& a_geos, const MeshPatch::PatchMap& a_patches)
	{
		switch (a_mode) {
		case PivotMode::kObject:
			return Centroid(a_geos, a_patches);
		case PivotMode::kBone:
			return HeadBone(a_geos);
		case PivotMode::kSpace:
		default:
			return {};
		}
	}

	// The bone that owns the most skinned vertices across all the hair's
	// geometry -- for hair that is the head bone. Its skin-to-bone translation
	// negated is where that bone sits in the mesh's own space. Bones are told
	// apart by that translation, so the same bone reached through several skin
	// instances pools its votes.
	RE::NiPoint3 PivotSolver::HeadBone(const std::vector<HairGeometry>& a_geos)
	{
		struct Candidate
		{
			RE::NiPoint3  skinToBone;
			std::uint32_t weight{ 0 };
		};
		std::vector<Candidate> candidates;

		for (const auto& g : a_geos) {
			auto* skin = g.geo ? g.geo->GetGeometryRuntimeData().skinInstance.get() : nullptr;
			auto* data = skin ? skin->skinData.get() : nullptr;
			if (!skin || !data || !data->boneData) {
				continue;
			}
			REX::W32::EnterCriticalSection(std::addressof(skin->lock));
			for (std::uint32_t i = 0; i < data->bones; ++i) {
				const auto& bone = data->boneData[i];
				const auto& t = bone.skinToBone.translate;
				auto        found = std::find_if(candidates.begin(), candidates.end(), [&](const Candidate& a_c) {
                    return a_c.skinToBone.x == t.x && a_c.skinToBone.y == t.y && a_c.skinToBone.z == t.z;
				});
				if (found == candidates.end()) {
					candidates.push_back({ t, 0 });
					found = std::prev(candidates.end());
				}
				found->weight += bone.verts;
			}
			REX::W32::LeaveCriticalSection(std::addressof(skin->lock));
		}

		// First strict maximum wins.
		RE::NiPoint3  best{};
		std::uint32_t bestWeight = 0;
		for (const auto& c : candidates) {
			if (c.weight > bestWeight) {
				bestWeight = c.weight;
				best = c.skinToBone;
			}
		}
		return { -best.x, -best.y, -best.z };
	}

	RE::NiPoint3 PivotSolver::Centroid(const std::vector<HairGeometry>& a_geos, const MeshPatch::PatchMap& a_patches)
	{
		RE::NiPoint3  sum{};
		std::uint64_t n = 0;
		for (const auto& g : a_geos) {
			const auto it = a_patches.find(g.geo);
			if (it == a_patches.end() || !it->second.HasRest()) {
				continue;
			}
			for (const auto& p : it->second.Rest()) {
				sum.x += p.x;
				sum.y += p.y;
				sum.z += p.z;
			}
			n += it->second.Rest().size();
		}
		if (n == 0) {
			return {};
		}
		const float inv = 1.0f / static_cast<float>(n);
		return { sum.x * inv, sum.y * inv, sum.z * inv };
	}
}
