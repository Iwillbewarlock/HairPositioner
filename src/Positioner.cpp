#include "Positioner.h"

#include "Console.h"
#include "HairScene.h"
#include "MeshPatch.h"
#include "Pivot.h"

namespace HP
{
	namespace
	{
		// -------------------------------------------------------------------
		//  Session: everything that lives between save loads.
		// -------------------------------------------------------------------
		class Session
		{
		public:
			static Session& Get()
			{
				static Session instance;
				return instance;
			}

			// ---- offset (any thread) ----
			HairOffset Offset()
			{
				std::lock_guard<std::mutex> guard{ _offsetLock };
				return _offset;
			}

			template <class F>
			void EditOffset(F&& a_edit)
			{
				std::lock_guard<std::mutex> guard{ _offsetLock };
				a_edit(_offset);
			}

			// ---- scene state (main thread only) ----
			void Forget()
			{
				_patches.clear();
				MeshPatch::ClearRestRegistry();
				_key.clear();
				_stamp.clear();
			}

			// Bring the recorded key up to date with the scene. When the hair
			// (or race) changed: cut the old hair loose, drop its patches, but
			// keep the wig patches -- their partitions are still ours and
			// re-capturing them would read transformed positions as rest.
			void SyncKey()
			{
				SyncWornSwitch();
				const auto key = HairScene::Key();
				if (key == _key) {
					return;
				}
				std::unordered_set<RE::BSGeometry*> touched;
				for (const auto& [geo, patch] : _patches) {
					touched.insert(geo);
				}
				for (auto* cut : HairScene::DetachOrphans(touched)) {
					_patches.erase(cut);
				}
				const auto worn = HairScene::Worn();
				std::erase_if(_patches, [&](const auto& a_entry) {
					return std::none_of(worn.begin(), worn.end(), [&](const HairGeometry& a_g) { return a_g.geo == a_entry.first; });
				});
				_key = key;
				_stamp.clear();  // force a full walk on the next refresh
			}

			// The "follow worn items" choice lives in the offset (so it rides
			// along with presets and the co-save); the scene view keeps its own
			// copy. Bring the two in line here, on the main thread. Switching
			// it OFF must first put every worn mesh back where the engine had
			// it -- otherwise a helmet would stay wherever the hair was.
			void SyncWornSwitch()
			{
				const bool want = Offset().worn;
				if (HairScene::FollowWorn() == want) {
					return;
				}
				if (!want) {
					ReleaseWorn();
				}
				HairScene::SetFollowWorn(want);
				_stamp.clear();
				SKSE::log::info("follow worn items: {}", want ? "on" : "off");
			}

			void ReleaseWorn()
			{
				const HairOffset identity;
				for (const auto& g : HairScene::Worn(true)) {
					const auto it = _patches.find(g.geo);
					if (it == _patches.end()) {
						continue;
					}
					if (it->second.HasRest()) {
						const OffsetMap map{ identity, RE::NiPoint3{} };
						it->second.Commit(g.geo, map, false);
					}
					_patches.erase(it);
				}
			}

			// Write the offset into every current hair geometry.
			void Apply(const HairOffset& a_offset, bool a_verbose)
			{
				const auto geos = HairScene::Current();
				if (geos.empty()) {
					ReportNoHair(a_verbose);
					return;
				}

				// A head part rebuilt by RaceMenu (piece converted to a dynamic
				// shape) shows up as a new geometry with the same name and vertex
				// count as one we already patched -- and its buffer holds OUR
				// positions. Hand the rest positions over before capturing, or
				// they would be re-read from the transformed buffer (double apply).
				for (const auto& g : geos) {
					if (_patches.contains(g.geo)) {
						continue;
					}
					const auto name = std::string{ g.geo->name.c_str() ? g.geo->name.c_str() : "" };
					const auto count = MeshPatch::VertexCountOf(g.geo);
					const MeshPatch* source = nullptr;
					for (const auto& [oldGeo, old] : _patches) {
						const bool gone = std::none_of(geos.begin(), geos.end(), [&](const HairGeometry& a_h) { return a_h.geo == oldGeo; });
						if (gone && old.HasRest() && old.Rest().size() == count &&
							HairScene::SameName(name, oldGeo->name.c_str() ? oldGeo->name.c_str() : "")) {
							source = std::addressof(old);
							break;
						}
					}
					if (source) {
						_patches[g.geo].InheritFrom(*source);  // element refs survive a rehash
						SKSE::log::info("[{}] rebuilt -- rest positions carried over ({} verts)", name, count);
					}
				}

				std::vector<HairGeometry> ready;
				for (const auto& g : geos) {
					if (_patches[g.geo].Capture(g.geo, _patches, a_verbose)) {
						ready.push_back(g);
					}
				}
				if (ready.empty()) {
					return;
				}

				// The offset is defined in head-bone space, shared by every piece,
				// and pulled back into each piece's own bind space. Pivot:
				//   bone   -> the bone origin (0,0,0 in bone space)
				//   center -> centroid of all rest vertices, in bone space
				//   origin -> each piece's own bind-space origin
				const RE::NiPoint3 shared = a_offset.pivot == PivotMode::kObject ? PivotSolver::BoneSpaceCentroid(ready, _patches) : RE::NiPoint3{};

				int done = 0;
				for (const auto& g : ready) {
					const auto         frame = PivotSolver::BindFrame(g.geo);
					const RE::NiPoint3 pivot = a_offset.pivot == PivotMode::kSpace ? frame * RE::NiPoint3{} : shared;
					const OffsetMap    map{ a_offset, pivot, frame };
					if (_patches[g.geo].Commit(g.geo, map, a_verbose)) {
						++done;
					}
				}
				_stamp = HairScene::TakeStamp();
				if (a_verbose) {
					Con::Say("applied to {} of {} geometry(ies), pivot mode {}", done, geos.size(), static_cast<int>(a_offset.pivot));
				}
			}

			// Heartbeat body. Cheap path first: if nothing in the scene was
			// rebuilt (same stamp) and every patched mesh still shows our
			// positions, there is nothing to do and no traversal happens.
			// Otherwise re-walk the scene, drop patches for geometry that is
			// gone, and re-apply if any mesh lost our write.
			void Refresh()
			{
				SyncKey();
				const auto offset = Offset();
				if (offset.IsIdentity() && _patches.empty()) {
					return;
				}
				const auto stamp = HairScene::TakeStamp();
				if (stamp == _stamp && AllHold()) {
					return;
				}
				const auto geos = HairScene::Current();
				if (geos.empty()) {
					_stamp = stamp;
					return;
				}
				const bool lost = std::any_of(geos.begin(), geos.end(), [&](const HairGeometry& a_g) {
					const auto it = _patches.find(a_g.geo);
					return it == _patches.end() || !it->second.Settled(a_g.geo);
				});
				if (lost) {
					Apply(offset, false);  // may carry rest over from patches about to be pruned
				} else {
					_stamp = stamp;
				}
				std::erase_if(_patches, [&](const auto& a_entry) {
					return std::none_of(geos.begin(), geos.end(), [&](const HairGeometry& a_g) { return a_g.geo == a_entry.first; });
				});
			}

			void Probe()
			{
				Con::Say("=== HairPositioner ===");
				Con::Say("key: {}", HairScene::Key());
				const auto geos = HairScene::Current();
				Con::Say("matched {} geometry(ies)", geos.size());
				if (geos.empty()) {
					Con::SayErr("wanted: {}", HairScene::WantedNames());
					Con::SayErr("found : {}", HairScene::DescribeFaceNode());
					return;
				}
				for (const auto& g : geos) {
					const auto it = _patches.find(g.geo);
					const bool mine = it != _patches.end() && it->second.Holds(g.geo);
					const auto storage = MeshPatch::StorageOf(g.geo);
					if (storage == MeshPatch::Storage::kDynamicBuffer) {
						auto*       dyn = netimmerse_cast<RE::BSDynamicTriShape*>(g.geo);
						const auto& rt = dyn->GetDynamicTrishapeRuntimeData();
						Con::Say("[{}] DYNAMIC verts {} (size field {}) {} {}", g.label, MeshPatch::DynamicVertexCount(g.geo, dyn), rt.dataSize,
							MeshPatch::HasMorphBase(g.geo) ? "with-morph-base" : "no-morph-base", mine ? "[ours]" : "[engine]");
					} else if (storage == MeshPatch::Storage::kSkinPartition) {
						auto*      part = g.geo->GetGeometryRuntimeData().skinInstance->skinPartition.get();
						const auto raw = MeshPatch::RawDesc(part->partitions[0].vertexDesc);
						const bool dynRtti = netimmerse_cast<RE::BSDynamicTriShape*>(g.geo) != nullptr;
						Con::Say("[{}] PARTITION verts {} parts {} stride {} {}{}", g.label, part->vertexCount, part->numPartitions,
							(raw & 0xF) * 4, mine ? "[ours]" : "[engine]", dynRtti ? " (dyn-rtti, nibble 0)" : "");
					} else {
						Con::SayErr("[{}] no skin partition", g.label);
					}
					const auto f = PivotSolver::BindFrame(g.geo);
					Con::Say("  bind: scale {:.3f} t ({:.2f} {:.2f} {:.2f}) rot00 {:.3f}  geo scale {:.3f}",
						f.scale, f.translate.x, f.translate.y, f.translate.z, f.rotate.entry[0][0], g.geo->world.scale);
					if (it != _patches.end() && !it->second.LastSkip().empty()) {
						Con::SayWarn("  last skip: {}", it->second.LastSkip());
					}
				}
			}

			std::atomic<bool>      applyQueued{ false };
			std::atomic<bool>      menuOpen{ false };
			std::atomic<long long> boostUntilMs{ 0 };

		private:
			Session() = default;

			// Every tracked mesh is settled (holds our write, or is one we gave
			// up on). Safe to call at any time: each patch keeps its geometry
			// alive.
			bool AllHold() const
			{
				return std::all_of(_patches.begin(), _patches.end(), [](const auto& a_entry) {
					return a_entry.second.Settled(a_entry.first);
				});
			}

			void ReportNoHair(bool a_verbose)
			{
				const auto line = std::format("no hair geometry matched -- wanted {}-- {}", HairScene::WantedNames(), HairScene::DescribeFaceNode());
				if (a_verbose) {
					Con::Err(line);
					return;
				}
				const auto key = HairScene::Key();
				if (_reportedKey != key) {
					SKSE::log::warn("[{}] {}", key, line);
					_reportedKey = key;
				}
			}

			std::mutex          _offsetLock;
			HairOffset          _offset;
			MeshPatch::PatchMap _patches;
			std::string         _key;
			std::string         _reportedKey;
			HairScene::Stamp    _stamp;
		};

		long long NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		bool PlayerHas3D()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			return player && player->Get3D();
		}
	}

	// -----------------------------------------------------------------------
	std::string CurrentKey()
	{
		return HairScene::Key();
	}

	HairOffset OffsetSnapshot()
	{
		return Session::Get().Offset();
	}

	void ReplaceOffset(const HairOffset& a_offset)
	{
		Session::Get().EditOffset([&](HairOffset& a_o) { a_o = a_offset; });
	}

	// One pending apply at a time; the task itself clears the flag.
	void QueueApply()
	{
		auto& s = Session::Get();
		if (s.applyQueued.exchange(true)) {
			return;
		}
		auto* task = SKSE::GetTaskInterface();
		if (!task) {
			s.applyQueued = false;
			return;
		}
		task->AddTask([]() {
			auto& session = Session::Get();
			session.applyQueued = false;
			session.SyncKey();
			session.Apply(session.Offset(), false);
		});
	}

	// ---- Papyrus-facing -------------------------------------------------------
	void SetChannel(std::int32_t a_channel, float a_value)
	{
		if (a_channel < 0 || a_channel >= kChannelCount) {
			return;
		}
		Session::Get().EditOffset([&](HairOffset& a_o) { a_o.SetChannel(a_channel, a_value); });
		QueueApply();
	}

	float GetChannel(std::int32_t a_channel)
	{
		return Session::Get().Offset().Channel(a_channel);
	}

	void SetPivot(PivotMode a_mode)
	{
		Session::Get().EditOffset([&](HairOffset& a_o) { a_o.pivot = a_mode; });
		QueueApply();
	}

	PivotMode GetPivot()
	{
		return Session::Get().Offset().pivot;
	}

	void SetFollowWorn(bool a_on)
	{
		Session::Get().EditOffset([&](HairOffset& a_o) { a_o.worn = a_on; });
		QueueApply();
	}

	bool GetFollowWorn()
	{
		return Session::Get().Offset().worn;
	}

	void ResetAdjust()
	{
		ReplaceOffset(HairOffset{});
		QueueApply();
	}

	bool HasTarget()
	{
		return !HairScene::Key().empty();
	}

	void QueueProbe()
	{
		if (auto* task = SKSE::GetTaskInterface()) {
			task->AddTask([]() { Probe(); });
		}
	}

	void QueueShow()
	{
		if (auto* task = SKSE::GetTaskInterface()) {
			task->AddTask([]() { Show(); });
		}
	}

	// ---- main thread ----------------------------------------------------------
	void Show()
	{
		const auto o = Session::Get().Offset();
		Con::Say("[{}]", HairScene::Key());
		Con::Say("  move   x {:.2f}  y {:.2f}  z {:.2f}", o.move.x, o.move.y, o.move.z);
		Con::Say("  rotate x {:.2f}  y {:.2f}  z {:.2f}", o.rotate.x, o.rotate.y, o.rotate.z);
		Con::Say("  scale  x {:.3f} y {:.3f} z {:.3f}", o.scale.x, o.scale.y, o.scale.z);
		Con::Say("  pivot  {} ({})", static_cast<std::uint32_t>(o.pivot),
			o.pivot == PivotMode::kBone ? "bone" : o.pivot == PivotMode::kObject ? "center" : "origin");
		Con::Say("  worn   {}", o.worn ? "follow (wig slots)" : "off");
	}

	void Probe()
	{
		Session::Get().Probe();
		Show();
	}

	void ReapplyNow()
	{
		if (!PlayerHas3D()) {
			return;
		}
		auto& s = Session::Get();
		s.SyncKey();
		const auto offset = s.Offset();
		if (!offset.IsIdentity()) {
			s.Apply(offset, false);
		}
	}

	void Tick()
	{
		if (!PlayerHas3D() || HairScene::Key().empty()) {
			return;
		}
		Session::Get().Refresh();
	}

	// ---- polling cadence ------------------------------------------------------
	void SetMenuOpen(bool a_open)
	{
		Session::Get().menuOpen = a_open;
	}

	void BoostPolling(std::chrono::milliseconds a_for)
	{
		auto&      until = Session::Get().boostUntilMs;
		const auto want = NowMs() + a_for.count();
		long long  have = until.load();
		while (have < want && !until.compare_exchange_weak(have, want)) {
		}
	}

	std::chrono::milliseconds TickInterval()
	{
		auto& s = Session::Get();
		return (s.menuOpen || NowMs() < s.boostUntilMs.load()) ? std::chrono::milliseconds(33) : std::chrono::milliseconds(500);
	}

	// ---- co-save ----------------------------------------------------------------
	namespace
	{
		constexpr std::uint32_t kRecordType = 'HPAJ';
		constexpr std::uint32_t kRecordVersion = 3;  // v3: pad[0] = follow worn items (v2 records read as off)

		struct Record
		{
			std::uint32_t slot;
			float         move[3];
			float         rotate[3];
			float         scale[3];
			std::uint32_t pivot;
			std::uint8_t  pad[4];
		};
		static_assert(sizeof(Record) == 48);
	}

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		const auto o = OffsetSnapshot();
		Record     r{};
		r.slot = kHairSlot;
		r.move[0] = o.move.x;     r.move[1] = o.move.y;     r.move[2] = o.move.z;
		r.rotate[0] = o.rotate.x; r.rotate[1] = o.rotate.y; r.rotate[2] = o.rotate.z;
		r.scale[0] = o.scale.x;   r.scale[1] = o.scale.y;   r.scale[2] = o.scale.z;
		r.pivot = static_cast<std::uint32_t>(o.pivot);
		r.pad[0] = o.worn ? 1 : 0;
		if (!a_intfc->WriteRecord(kRecordType, kRecordVersion, r)) {
			SKSE::log::error("co-save write failed");
		}
	}

	void OnLoad(SKSE::SerializationInterface* a_intfc)
	{
		HairOffset    loaded;
		std::uint32_t type, version, length;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type != kRecordType || (version != 2 && version != kRecordVersion) || length != sizeof(Record)) {
				continue;
			}
			Record r{};
			if (a_intfc->ReadRecordData(r) != sizeof(Record) || r.slot != kHairSlot) {
				continue;
			}
			loaded.move = { r.move[0], r.move[1], r.move[2] };
			loaded.rotate = { r.rotate[0], r.rotate[1], r.rotate[2] };
			loaded.scale = { r.scale[0], r.scale[1], r.scale[2] };
			loaded.pivot = r.pivot < static_cast<std::uint32_t>(PivotMode::kCount) ? static_cast<PivotMode>(r.pivot) : PivotMode::kBone;
			loaded.worn = version >= 3 && r.pad[0] != 0;
		}
		auto& s = Session::Get();
		ReplaceOffset(loaded);
		s.Forget();
		SKSE::log::info("co-save loaded: move ({:.2f} {:.2f} {:.2f}) rotate ({:.1f} {:.1f} {:.1f}) scale ({:.2f} {:.2f} {:.2f}) pivot {} worn {}",
			loaded.move.x, loaded.move.y, loaded.move.z, loaded.rotate.x, loaded.rotate.y, loaded.rotate.z,
			loaded.scale.x, loaded.scale.y, loaded.scale.z, static_cast<std::uint32_t>(loaded.pivot), loaded.worn);
	}

	void OnRevert(SKSE::SerializationInterface*)
	{
		ReplaceOffset(HairOffset{});
		Session::Get().Forget();
	}
}
