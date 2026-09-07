#include "HairScene.h"

#include "Settings.h"

namespace HP
{
	namespace
	{
		RE::PlayerCharacter* Player()
		{
			return RE::PlayerCharacter::GetSingleton();
		}

		RE::TESNPC* PlayerBase()
		{
			auto* player = Player();
			return player ? player->GetActorBase() : nullptr;
		}

		RE::BGSHeadPart* CurrentHair()
		{
			auto* base = PlayerBase();
			return base ? base->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kHair) : nullptr;
		}

		std::string NameOf(RE::NiAVObject* a_obj)
		{
			const char* raw = a_obj ? a_obj->name.c_str() : nullptr;
			return raw ? raw : "";
		}
	}

	bool HairScene::SameName(std::string_view a_lhs, std::string_view a_rhs)
	{
		return std::ranges::equal(a_lhs, a_rhs, [](char a_l, char a_r) {
			return std::tolower(static_cast<unsigned char>(a_l)) == std::tolower(static_cast<unsigned char>(a_r));
		});
	}

	const char* HairScene::EditorID(RE::TESForm* a_form)
	{
		const char* id = a_form ? a_form->GetFormEditorID() : nullptr;
		return (id && *id) ? id : nullptr;
	}

	std::string HairScene::Key()
	{
		auto* player = Player();
		auto* base = PlayerBase();
		if (!player || !base) {
			return {};
		}
		const char* race = EditorID(player->GetRace());
		const char* hair = EditorID(CurrentHair());
		return (race && hair) ? std::format("{}|{}", race, hair) : std::string{};
	}

	// The engine names each head-part geometry under the face node after the
	// head part's editor ID; the hair's extra parts (hairline) follow suit.
	std::vector<std::string> HairScene::HairNames()
	{
		std::vector<std::string> names;
		auto*                    hair = CurrentHair();
		if (!hair) {
			return names;
		}
		if (auto* id = EditorID(hair)) {
			names.emplace_back(id);
		}
		for (auto* extra : hair->extraParts) {
			if (auto* id = EditorID(extra)) {
				names.emplace_back(id);
			}
		}
		return names;
	}

	RE::NiAVObject* HairScene::FaceRoot(const char** a_which)
	{
		auto* player = Player();
		if (!player) {
			return nullptr;
		}
		RE::NiAVObject* root = player->GetFaceNodeSkinned();
		if (a_which) {
			*a_which = "face node";
		}
		if (!root) {
			root = player->GetCurrent3D();
			if (a_which) {
				*a_which = "3D root";
			}
		}
		return root;
	}

	// Wig slots: the whole worn item follows the hair, so every geometry under
	// its partClone counts -- no name filter. Only when following worn items
	// is switched on (or the caller insists).
	void HairScene::AppendWorn(std::vector<HairGeometry>& a_out, bool a_evenIfOff)
	{
		auto*       player = Player();
		const auto& slots = Settings::Get().wigSlots;
		if (!player || slots.empty() || (!_followWorn && !a_evenIfOff)) {
			return;
		}
		const auto& biped = player->GetCurrentBiped();
		if (!biped) {
			return;
		}
		for (const auto idx : slots) {
			if (idx >= RE::BIPED_OBJECTS::kTotal) {
				continue;
			}
			auto* clone = biped->objects[idx].partClone.get();
			if (!clone) {
				continue;
			}
			RE::BSVisit::TraverseScenegraphGeometries(clone, [&](RE::BSGeometry* a_geo) {
				const bool seen = std::any_of(a_out.begin(), a_out.end(), [&](const HairGeometry& a_g) { return a_g.geo == a_geo; });
				if (!seen) {
					a_out.push_back({ a_geo, std::format("biped{}:{}", idx + 30, NameOf(a_geo)), true });
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
	}

	// A head part whose NIF holds a single shape ends up as one geometry named
	// after the head part's editor ID. A head part whose NIF holds several
	// shapes (modular / SMP hair pieces) ends up as a NiNode named after the
	// editor ID with the shapes as children keeping their NIF names -- so the
	// match has to happen at the face node's children and then take everything
	// underneath a matching node.
	std::vector<HairGeometry> HairScene::Current()
	{
		std::vector<HairGeometry> out;
		const auto                names = HairNames();
		auto*                     root = FaceRoot();
		auto*                     node = root ? root->AsNode() : nullptr;
		if (names.empty() || !root) {
			AppendWorn(out);
			return out;
		}
		auto wanted = [&](const std::string& a_name) {
			return std::any_of(names.begin(), names.end(), [&](const std::string& a_n) { return SameName(a_name, a_n); });
		};
		auto add = [&](RE::BSGeometry* a_geo, const std::string& a_label) {
			if (std::none_of(out.begin(), out.end(), [&](const HairGeometry& a_g) { return a_g.geo == a_geo; })) {
				out.push_back({ a_geo, a_label, false });
			}
		};
		if (node) {
			for (const auto& child : node->GetChildren()) {
				auto* obj = child.get();
				if (!obj) {
					continue;
				}
				const auto name = NameOf(obj);
				if (!wanted(name)) {
					continue;
				}
				if (auto* geo = obj->AsGeometry()) {
					add(geo, name);
				} else if (obj->AsNode()) {
					RE::BSVisit::TraverseScenegraphGeometries(obj, [&](RE::BSGeometry* a_geo) {
						add(a_geo, std::format("{}/{}", name, NameOf(a_geo)));
						return RE::BSVisit::BSVisitControl::kContinue;
					});
				}
			}
		}
		// Fallback: any geometry anywhere under the root whose own name matches.
		RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geo) {
			const auto name = NameOf(a_geo);
			if (wanted(name)) {
				add(a_geo, name);
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		AppendWorn(out);
		return out;
	}

	HairScene::Stamp HairScene::TakeStamp()
	{
		Stamp stamp;
		auto* root = FaceRoot();
		stamp.push_back(root);
		if (auto* node = root ? root->AsNode() : nullptr) {
			// Head-part geometry hangs directly under the face node. RaceMenu
			// swaps those children in place (BSTriShape -> BSDynamicTriShape)
			// without touching the node itself, so the children's identities
			// have to be part of the stamp, not just their count.
			for (const auto& child : node->GetChildren()) {
				stamp.push_back(child.get());
				// multi-shape head parts: their shapes sit one level deeper
				if (auto* sub = child ? child->AsNode() : nullptr) {
					for (const auto& grand : sub->GetChildren()) {
						stamp.push_back(grand.get());
					}
				}
			}
		}
		if (auto* player = Player()) {
			const auto& biped = player->GetCurrentBiped();
			stamp.push_back(biped.get());
			if (biped && _followWorn) {
				for (const auto idx : Settings::Get().wigSlots) {
					if (idx < RE::BIPED_OBJECTS::kTotal) {
						stamp.push_back(biped->objects[idx].partClone.get());
					}
				}
			}
		}
		return stamp;
	}

	std::vector<HairGeometry> HairScene::Worn(bool a_evenIfOff)
	{
		std::vector<HairGeometry> out;
		AppendWorn(out, a_evenIfOff);
		return out;
	}

	// After a hair swap the engine can leave the old hair geometry parented
	// under the face node -- in particular geometry whose skin partition we
	// replaced, which its own removal path misses. Only geometry we touched
	// AND that no longer matches any current head part is cut; anything else
	// is left alone.
	std::vector<RE::BSGeometry*> HairScene::DetachOrphans(const std::unordered_set<RE::BSGeometry*>& a_touched)
	{
		std::vector<RE::BSGeometry*> cut;
		auto*                        base = PlayerBase();
		if (!base || a_touched.empty()) {
			return cut;
		}

		std::vector<std::string> keep;
		for (std::uint32_t t = 0; t <= 6; ++t) {
			auto* part = base->GetCurrentHeadPartByType(static_cast<RE::BGSHeadPart::HeadPartType>(t));
			if (!part) {
				continue;
			}
			if (auto* id = EditorID(part)) {
				keep.emplace_back(id);
			}
			for (auto* extra : part->extraParts) {
				if (auto* id = EditorID(extra)) {
					keep.emplace_back(id);
				}
			}
		}

		auto* root = FaceRoot();
		auto* node = root ? root->AsNode() : nullptr;
		if (!node) {
			return cut;
		}

		std::vector<RE::NiPointer<RE::NiAVObject>> stale;
		RE::BSVisit::TraverseScenegraphGeometries(node, [&](RE::BSGeometry* a_geo) {
			if (a_touched.contains(a_geo)) {
				const auto name = NameOf(a_geo);
				const bool wanted = std::any_of(keep.begin(), keep.end(), [&](const std::string& a_k) { return SameName(name, a_k); });
				if (!wanted) {
					stale.emplace_back(RE::NiPointer<RE::NiAVObject>(a_geo));
				}
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		for (auto& obj : stale) {
			if (auto* parent = obj->parent) {
				SKSE::log::info("removed stale hair \"{}\"", obj->name.c_str());
				parent->DetachChild(obj.get());
			}
			cut.push_back(static_cast<RE::BSGeometry*>(obj.get()));
		}
		return cut;
	}

	std::string HairScene::DescribeFaceNode()
	{
		const char* which = "";
		auto*       root = FaceRoot(&which);
		if (!root) {
			return "<no 3D>";
		}
		std::string out = std::format("{} \"{}\":", which, root->name.c_str());
		RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geo) {
			const char* raw = a_geo->name.c_str();
			out += std::format(" \"{}\"", raw ? raw : "<null>");
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		return out;
	}

	std::string HairScene::WantedNames()
	{
		std::string out;
		for (const auto& n : HairNames()) {
			out += std::format("\"{}\" ", n);
		}
		return out.empty() ? "<no hair head part>" : out;
	}
}
