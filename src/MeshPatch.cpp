#include "MeshPatch.h"

#include "Console.h"
#include "Settings.h"

namespace HP
{
	namespace
	{
		using Vertex = RE::BSGraphics::Vertex;

		constexpr std::size_t kDynamicStride = 16;  // float4 per vertex

		RE::NiSkinInstance* SkinOf(RE::BSGeometry* a_geo)
		{
			return a_geo ? a_geo->GetGeometryRuntimeData().skinInstance.get() : nullptr;
		}

		RE::NiSkinPartition* PartitionOf(RE::BSGeometry* a_geo)
		{
			auto* skin = SkinOf(a_geo);
			return skin ? skin->skinPartition.get() : nullptr;
		}

		RE::NiPoint3 ReadFloat3(const void* a_base, std::size_t a_index, std::size_t a_stride)
		{
			const auto* p = reinterpret_cast<const float*>(static_cast<const std::uint8_t*>(a_base) + a_index * a_stride);
			return { p[0], p[1], p[2] };
		}

		// Only x/y/z are written; the fourth float (bitangent X) stays.
		void WriteFloat3(void* a_base, std::size_t a_index, std::size_t a_stride, const RE::NiPoint3& a_p)
		{
			auto* p = reinterpret_cast<float*>(static_cast<std::uint8_t*>(a_base) + a_index * a_stride);
			p[0] = a_p.x;
			p[1] = a_p.y;
			p[2] = a_p.z;
		}

		bool Close(const RE::NiPoint3& a_a, const RE::NiPoint3& a_b)
		{
			return std::fabs(a_a.x - a_b.x) < 1e-3f && std::fabs(a_a.y - a_b.y) < 1e-3f && std::fabs(a_a.z - a_b.z) < 1e-3f;
		}

		// "FOD": the FaceGen base-morph block the engine rebuilds dynamic
		// buffers from.
		class BaseMorphData : public RE::NiExtraData
		{
		public:
			RE::NiPoint3* vertexData;        // 18
			std::uint32_t modelVertexCount;  // 20
			std::uint32_t vertexCount;       // 24
		};
		static_assert(offsetof(BaseMorphData, vertexData) == 0x18);

		BaseMorphData* FindBaseMorph(RE::BSGeometry* a_geo)
		{
			const auto n = a_geo->GetExtraDataSize();
			for (std::uint16_t i = 0; i < n; ++i) {
				auto* extra = a_geo->GetExtraDataAt(i);
				if (!extra) {
					continue;
				}
				const auto* rtti = extra->GetRTTI();
				if (rtti && rtti->GetName() && std::strcmp(rtti->GetName(), "BSFaceGenBaseMorphExtraData") == 0) {
					return static_cast<BaseMorphData*>(extra);
				}
			}
			return nullptr;
		}

		// Rest positions keyed by the skin partition they belong to. This
		// outlives individual MeshPatch objects, so a head part that RaceMenu
		// swaps in place (reusing a partition we already transformed) still finds
		// the untouched positions instead of reading our transformed buffer back
		// as if it were the rest pose. NiPointer keeps each partition alive, so a
		// key can never alias a different object at a reused address.
		struct RestEntry
		{
			RE::NiPointer<RE::NiSkinPartition> partition;
			std::vector<RE::NiPoint3>          rest;
		};
		std::vector<RestEntry> g_restByPartition;

		const std::vector<RE::NiPoint3>* FindRest(RE::NiSkinPartition* a_part)
		{
			for (const auto& e : g_restByPartition) {
				if (e.partition.get() == a_part) {
					return std::addressof(e.rest);
				}
			}
			return nullptr;
		}

		void RememberRest(RE::NiSkinPartition* a_part, const std::vector<RE::NiPoint3>& a_rest)
		{
			for (auto& e : g_restByPartition) {
				if (e.partition.get() == a_part) {
					e.rest = a_rest;
					return;
				}
			}
			g_restByPartition.push_back({ RE::NiPointer<RE::NiSkinPartition>(a_part), a_rest });
		}

		// StripMorphData: without the base block the engine cannot rebuild the
		// dynamic buffer from untouched positions and silently undo us.
		void StripBaseMorph(RE::BSGeometry* a_geo)
		{
			if (!Settings::Get().stripMorphData || !FindBaseMorph(a_geo)) {
				return;
			}
			static const RE::BSFixedString fod{ "FOD" };
			if (a_geo->RemoveExtraData(fod)) {
				SKSE::log::info("[{}] stripped FaceGen morph data", a_geo->name.c_str());
			}
		}
	}

	// -----------------------------------------------------------------------
	std::uint64_t MeshPatch::RawDesc(const RE::BSGraphics::VertexDesc& a_desc)
	{
		static_assert(sizeof(RE::BSGraphics::VertexDesc) == sizeof(std::uint64_t));
		std::uint64_t raw{};
		std::memcpy(&raw, std::addressof(a_desc), sizeof(raw));
		return raw;
	}

	bool MeshPatch::HasMorphBase(RE::BSGeometry* a_geo)
	{
		return FindBaseMorph(a_geo) != nullptr;
	}

	// RaceMenu takes the count from BSTriShape::vertexCount and falls back to
	// the skin partition; the runtime field NG calls "dataSize" holds a vertex
	// count on 1.6.1170, not bytes, so it is only reported, never trusted.
	std::uint32_t MeshPatch::DynamicVertexCount(RE::BSGeometry* a_geo, RE::BSDynamicTriShape* a_dyn)
	{
		std::uint32_t count = a_dyn->GetTrishapeRuntimeData().vertexCount;
		if (count == 0) {
			if (auto* part = PartitionOf(a_geo)) {
				count = part->vertexCount;
			}
		}
		return count;
	}

	MeshPatch::Storage MeshPatch::StorageOf(RE::BSGeometry* a_geo)
	{
		auto* part = PartitionOf(a_geo);
		if (!part || part->numPartitions == 0) {
			return Storage::kNone;
		}
		const auto dynamicNibble = (RawDesc(part->partitions[0].vertexDesc) >> 4) & 0xF;
		if (dynamicNibble != 0) {
			if (auto* dyn = netimmerse_cast<RE::BSDynamicTriShape*>(a_geo)) {
				if (dyn->GetDynamicTrishapeRuntimeData().dynamicData) {
					return Storage::kDynamicBuffer;
				}
			}
		}
		return Storage::kSkinPartition;
	}

	void MeshPatch::ClearRestRegistry()
	{
		g_restByPartition.clear();
	}

	bool MeshPatch::Skip(RE::BSGeometry* a_geo, bool a_verbose, std::string a_reason, bool a_permanent)
	{
		const std::string line = std::format("[{}] {}", a_geo->name.c_str(), a_reason);
		if (a_verbose) {
			Con::Err(line);
		} else if (_lastSkip != line) {
			SKSE::log::warn("{}", line);
		}
		_lastSkip = line;
		if (a_permanent) {
			_gaveUp = true;  // stop re-attempting this exact geometry
		}
		return false;
	}

	void MeshPatch::Remember(const OffsetMap& a_map)
	{
		if (_rest.empty()) {
			_sample = {};
		} else {
			_sample = { a_map(_rest.front()), a_map(_rest[_rest.size() / 2]), a_map(_rest.back()) };
		}
		_lastSkip.clear();
	}

	// -----------------------------------------------------------------------
	//  capture
	// -----------------------------------------------------------------------
	bool MeshPatch::Capture(RE::BSGeometry* a_geo, const PatchMap& a_others, bool a_verbose)
	{
		if (_keepAlive.get() != a_geo) {
			_keepAlive.reset(a_geo);
		}
		switch (StorageOf(a_geo)) {
		case Storage::kDynamicBuffer:
			return CaptureDynamic(a_geo, netimmerse_cast<RE::BSDynamicTriShape*>(a_geo), a_verbose);
		case Storage::kSkinPartition:
			return CapturePartition(a_geo, a_others, a_verbose);
		default:
			return Skip(a_geo, a_verbose, "no skin partition", true);
		}
	}

	bool MeshPatch::CaptureDynamic(RE::BSGeometry* a_geo, RE::BSDynamicTriShape* a_dyn, bool a_verbose)
	{
		_storage = Storage::kDynamicBuffer;
		StripBaseMorph(a_geo);
		auto&      rt = a_dyn->GetDynamicTrishapeRuntimeData();
		const auto count = DynamicVertexCount(a_geo, a_dyn);
		if (!rt.dynamicData || count == 0) {
			return Skip(a_geo, a_verbose, std::format("dynamic tri shape without usable dynamic data (verts {}, field {})", count, rt.dataSize), true);
		}
		if (!DynamicHolds(a_geo, a_dyn)) {
			// The engine's own positions are in the buffer: that is the rest pose.
			_rest.resize(count);
			rt.lock.Lock();
			for (std::uint32_t i = 0; i < count; ++i) {
				_rest[i] = ReadFloat3(rt.dynamicData, i, kDynamicStride);
			}
			rt.lock.Unlock();
			_hasRest = true;
			_stride = kDynamicStride;
		}
		return true;
	}

	bool MeshPatch::CapturePartition(RE::BSGeometry* a_geo, const PatchMap& a_others, bool a_verbose)
	{
		_storage = Storage::kSkinPartition;
		auto*       current = PartitionOf(a_geo);
		const auto& desc = current->partitions[0].vertexDesc;
		const auto  raw = RawDesc(desc);
		// The position is a float4 at the start of each vertex. VF_FULLPREC is
		// deliberately ignored: on runtime skin partitions it reads as clear
		// even though the buffer holds floats, and honouring it would write
		// half-encoded values into float slots. The size nibble is the stride.
		const auto stride = static_cast<std::uint32_t>(raw & 0xF) * 4;
		const auto count = current->vertexCount;
		if (!desc.HasFlag(Vertex::VF_VERTEX) || stride < 16u || count == 0) {
			return Skip(a_geo, a_verbose, std::format("vertex layout not usable (desc {:#x}, stride {}, verts {})", raw, stride, count), true);
		}
		if (_refused && _refused.get() == current) {
			return false;
		}

		const bool ownedIsCurrent = _owned && _owned.get() == current;
		if (ownedIsCurrent) {
			if (_rest.size() != count) {
				return Skip(a_geo, a_verbose, "recorded vertex count changed under us");
			}
			_stride = stride;
			return true;
		}

		// This partition may already be one we transformed -- because a sibling
		// geometry (the hairline shares the hair's skin instance) swapped it in,
		// or because RaceMenu re-pointed a freshly built head part at it. Its
		// buffer now holds transformed positions, so take the untouched rest from
		// the registry instead of reading the buffer back as if it were the rest
		// pose (which would apply the offset twice).
		if (const auto* saved = FindRest(current); saved && saved->size() == count) {
			_rest = *saved;
			_hasRest = true;
			_owned.reset(current);
			_stride = stride;
			return true;
		}
		(void)a_others;

		auto* buff = current->partitions[0].buffData;
		if (!buff || !buff->rawVertexData) {
			return Skip(a_geo, a_verbose, "partition has no CPU-side vertex data");
		}
		_rest.resize(count);
		for (std::uint32_t i = 0; i < count; ++i) {
			_rest[i] = ReadFloat3(buff->rawVertexData, i, stride);
		}
		_hasRest = true;
		_stride = stride;
		return true;
	}

	// -----------------------------------------------------------------------
	//  holds?
	// -----------------------------------------------------------------------
	bool MeshPatch::DynamicHolds(RE::BSGeometry* a_geo, RE::BSDynamicTriShape* a_dyn) const
	{
		auto& rt = a_dyn->GetDynamicTrishapeRuntimeData();
		const auto  count = DynamicVertexCount(a_geo, a_dyn);
		if (!rt.dynamicData || count == 0 || !_hasRest || _rest.size() != count) {
			return false;
		}
		rt.lock.Lock();
		const RE::NiPoint3 a = ReadFloat3(rt.dynamicData, 0, kDynamicStride);
		const RE::NiPoint3 b = ReadFloat3(rt.dynamicData, count / 2, kDynamicStride);
		const RE::NiPoint3 c = ReadFloat3(rt.dynamicData, count - 1, kDynamicStride);
		rt.lock.Unlock();
		return Close(a, _sample[0]) && Close(b, _sample[1]) && Close(c, _sample[2]);
	}

	bool MeshPatch::Holds(RE::BSGeometry* a_geo) const
	{
		if (StorageOf(a_geo) == Storage::kDynamicBuffer) {
			if (auto* dyn = netimmerse_cast<RE::BSDynamicTriShape*>(a_geo)) {
				return DynamicHolds(a_geo, dyn);
			}
		}
		return _owned && _owned.get() == PartitionOf(a_geo);
	}

	bool MeshPatch::Settled(RE::BSGeometry* a_geo) const
	{
		return _gaveUp || Holds(a_geo);
	}

	// -----------------------------------------------------------------------
	//  commit
	// -----------------------------------------------------------------------
	bool MeshPatch::Commit(RE::BSGeometry* a_geo, const OffsetMap& a_map, bool a_verbose)
	{
		if (!_hasRest) {
			return false;
		}
		if (StorageOf(a_geo) == Storage::kDynamicBuffer) {
			return CommitDynamic(a_geo, netimmerse_cast<RE::BSDynamicTriShape*>(a_geo), a_map);
		}
		return CommitPartition(a_geo, a_map, a_verbose);
	}

	bool MeshPatch::CommitDynamic(RE::BSGeometry* a_geo, RE::BSDynamicTriShape* a_dyn, const OffsetMap& a_map)
	{
		auto&      rt = a_dyn->GetDynamicTrishapeRuntimeData();
		const auto count = DynamicVertexCount(a_geo, a_dyn);
		if (_rest.size() != count || !rt.dynamicData) {
			return false;
		}
		// With StripMorphData off the base block is still there: keep it in
		// step (as RaceMenu's sculpt does) so a morph rebuild lands on our
		// positions rather than the originals.
		auto* base = FindBaseMorph(a_geo);
		auto* mirror = (base && base->vertexData && base->vertexCount == count) ? base->vertexData : nullptr;

		rt.lock.Lock();
		for (std::uint32_t i = 0; i < count; ++i) {
			const RE::NiPoint3 p = a_map(_rest[i]);
			WriteFloat3(rt.dynamicData, i, kDynamicStride, p);
			if (mirror) {
				mirror[i] = p;
			}
		}
		rt.lock.Unlock();

		Remember(a_map);
		if (!_announced) {
			SKSE::log::info("[{}] applied to dynamic geometry ({} verts)", a_geo->name.c_str(), count);
			_announced = true;
		}
		return true;
	}

	bool MeshPatch::CommitPartition(RE::BSGeometry* a_geo, const OffsetMap& a_map, bool a_verbose)
	{
		auto* skin = SkinOf(a_geo);
		auto* current = skin ? skin->skinPartition.get() : nullptr;
		if (!skin || !current) {
			return false;
		}
		const auto count = current->vertexCount;
		const auto stride = _stride;
		if (_rest.size() != count) {
			return false;
		}

		// 1. Our own copy of the partition (deep copy the first time; after
		//    that the swapped-in partition already is ours).
		RE::NiPointer<RE::NiObject> holder;
		RE::NiSkinPartition*        target = current;
		if (!(_owned && _owned.get() == current)) {
			current->CreateDeepCopy(holder);
			target = netimmerse_cast<RE::NiSkinPartition*>(holder.get());
			if (!target || target == current) {
				return Skip(a_geo, a_verbose, "CreateDeepCopy did not produce a new partition");
			}
			for (std::uint32_t i = 0; i < target->numPartitions; ++i) {
				auto* was = current->partitions[i].buffData;
				auto* now = target->partitions[i].buffData;
				if (!was || !now || was == now || was->vertexBuffer == now->vertexBuffer) {
					_refused.reset(current);
					return Skip(a_geo, a_verbose, "deep copy still shares GPU buffers -- this hair is left untouched", true);
				}
			}
		}

		// 2. New positions into partition 0, then into every same-layout
		//    partition that has its own buffer (a shared buffer is already done).
		//    Only xyz is written; bone indices/weights and the rest of the vertex
		//    stay as the engine laid them out.
		auto* buff0 = target->partitions[0].buffData;
		if (!buff0 || !buff0->rawVertexData) {
			_refused.reset(current);
			return Skip(a_geo, a_verbose, "partition 0 has no CPU-side vertex data", true);
		}
		for (std::uint32_t i = 0; i < count; ++i) {
			WriteFloat3(buff0->rawVertexData, i, stride, a_map(_rest[i]));
		}
		const std::size_t   bytes = static_cast<std::size_t>(count) * stride;
		const std::uint64_t layout0 = RawDesc(target->partitions[0].vertexDesc);
		for (std::uint32_t p = 1; p < target->numPartitions; ++p) {
			auto* buff = target->partitions[p].buffData;
			if (buff && buff->rawVertexData && buff->rawVertexData != buff0->rawVertexData &&
				RawDesc(target->partitions[p].vertexDesc) == layout0) {
				for (std::uint32_t i = 0; i < count; ++i) {
					WriteFloat3(buff->rawVertexData, i, stride, a_map(_rest[i]));
				}
			}
		}

		// 3. Upload, once per distinct GPU buffer.
		auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		auto* rdata = RE::BSGraphics::Renderer::GetRendererData();
		if (!renderer || !rdata || !rdata->context) {
			return false;
		}
		std::vector<RE::ID3D11Buffer*> done;
		renderer->Lock();
		for (std::uint32_t i = 0; i < target->numPartitions; ++i) {
			auto* buff = target->partitions[i].buffData;
			if (!buff || !buff->vertexBuffer || !buff->rawVertexData) {
				continue;
			}
			if (i > 0 && RawDesc(target->partitions[i].vertexDesc) != layout0) {
				continue;
			}
			if (std::find(done.begin(), done.end(), buff->vertexBuffer) != done.end()) {
				continue;
			}
			rdata->context->UpdateSubresource(
				reinterpret_cast<REX::W32::ID3D11Resource*>(buff->vertexBuffer),
				0, nullptr, buff->rawVertexData, static_cast<std::uint32_t>(bytes), 0);
			done.push_back(buff->vertexBuffer);
		}
		renderer->Unlock();

		// 4. Point the skin instance at our partition.
		if (target != current) {
			REX::W32::EnterCriticalSection(std::addressof(skin->lock));
			skin->skinPartition.reset(target);
			REX::W32::LeaveCriticalSection(std::addressof(skin->lock));
		}
		_owned.reset(target);
		RememberRest(target, _rest);

		Remember(a_map);
		if (!_announced) {
			SKSE::log::info("[{}] applied to skin partition ({} verts, stride {}, desc {:#x})",
				a_geo->name.c_str(), count, stride, RawDesc(target->partitions[0].vertexDesc));
			_announced = true;
		}
		return true;
	}
}
