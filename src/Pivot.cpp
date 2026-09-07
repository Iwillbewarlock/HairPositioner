#include "Pivot.h"

namespace HP
{
	namespace
	{
		constexpr std::string_view kHeadBoneName = "NPC Head [Head]";
	}

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

	RE::NiTransform PivotSolver::BindFrame(RE::BSGeometry* a_geo)
	{
		RE::NiTransform frame;  // identity
		auto* skin = a_geo ? a_geo->GetGeometryRuntimeData().skinInstance.get() : nullptr;
		auto* data = skin ? skin->skinData.get() : nullptr;
		if (!skin || !data || !data->boneData || data->bones == 0) {
			return frame;
		}
		REX::W32::EnterCriticalSection(std::addressof(skin->lock));
		// Every piece of one hair has to be pulled back through the SAME bone,
		// or the shared offset lands differently on each piece. Hair is meant
		// to sit on the head bone, so take that whenever the piece is skinned
		// to it at all -- even if a physics bone happens to own more of its
		// vertices (SMP pieces). Only a piece with no head bone falls back to
		// its heaviest bone.
		bool headFound = false;
		if (skin->bones) {
			for (std::uint32_t i = 0; i < data->bones && !headFound; ++i) {
				auto* bone = skin->bones[i];
				const char* name = bone ? bone->name.c_str() : nullptr;
				if (name && std::string_view{ name } == kHeadBoneName) {
					frame = data->boneData[i].skinToBone;
					headFound = true;
				}
			}
		}
		if (!headFound) {
			std::uint32_t best = 0;
			for (std::uint32_t i = 0; i < data->bones; ++i) {
				if (data->boneData[i].verts > best) {
					best = data->boneData[i].verts;
					frame = data->boneData[i].skinToBone;
				}
			}
		}
		REX::W32::LeaveCriticalSection(std::addressof(skin->lock));
		return frame;
	}

	std::string PivotSolver::BindBoneName(RE::BSGeometry* a_geo)
	{
		auto* skin = a_geo ? a_geo->GetGeometryRuntimeData().skinInstance.get() : nullptr;
		auto* data = skin ? skin->skinData.get() : nullptr;
		if (!skin || !data || !data->boneData || data->bones == 0 || !skin->bones) {
			return "(unskinned)";
		}
		REX::W32::EnterCriticalSection(std::addressof(skin->lock));
		std::string   picked;
		std::uint32_t best = 0;
		for (std::uint32_t i = 0; i < data->bones; ++i) {
			auto*       bone = skin->bones[i];
			const char* name = bone && bone->name.c_str() ? bone->name.c_str() : "?";
			if (std::string_view{ name } == kHeadBoneName) {
				picked = std::string{ name } + " (head)";
				break;
			}
			if (data->boneData[i].verts > best) {
				best = data->boneData[i].verts;
				picked = std::string{ name } + " (heaviest, no head bone)";
			}
		}
		REX::W32::LeaveCriticalSection(std::addressof(skin->lock));
		return picked;
	}

	RE::NiPoint3 PivotSolver::BoneSpaceCentroid(const std::vector<HairGeometry>& a_geos, const MeshPatch::PatchMap& a_patches)
	{
		RE::NiPoint3  sum{};
		std::uint64_t n = 0;
		for (const auto& g : a_geos) {
			const auto it = a_patches.find(g.geo);
			if (it == a_patches.end() || !it->second.HasRest()) {
				continue;
			}
			const auto frame = BindFrame(g.geo);
			for (const auto& p : it->second.Rest()) {
				const RE::NiPoint3 q = frame * p;
				sum.x += q.x;
				sum.y += q.y;
				sum.z += q.z;
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
