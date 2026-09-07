#include "HairOffset.h"

namespace HP
{
	namespace
	{
		constexpr float kDegToRad = 0.017453292f;

		bool Near(float a_a, float a_b)
		{
			return std::fabs(a_a - a_b) < 1e-5f;
		}

		float& Component(RE::NiPoint3& a_p, std::int32_t a_axis)
		{
			switch (a_axis) {
			case 0:  return a_p.x;
			case 1:  return a_p.y;
			default: return a_p.z;
			}
		}
	}

	bool HairOffset::IsIdentity() const
	{
		return Near(move.x, 0.0f) && Near(move.y, 0.0f) && Near(move.z, 0.0f) &&
		       Near(rotate.x, 0.0f) && Near(rotate.y, 0.0f) && Near(rotate.z, 0.0f) &&
		       Near(scale.x, 1.0f) && Near(scale.y, 1.0f) && Near(scale.z, 1.0f);
	}

	float HairOffset::Channel(std::int32_t a_channel) const
	{
		if (a_channel < 0 || a_channel >= kChannelCount) {
			return 0.0f;
		}
		HairOffset copy = *this;
		RE::NiPoint3* group[]{ &copy.move, &copy.rotate, &copy.scale };
		return Component(*group[a_channel / 3], a_channel % 3);
	}

	void HairOffset::SetChannel(std::int32_t a_channel, float a_value)
	{
		if (a_channel < 0 || a_channel >= kChannelCount) {
			return;
		}
		RE::NiPoint3* group[]{ &move, &rotate, &scale };
		Component(*group[a_channel / 3], a_channel % 3) = a_value;
	}

	// -----------------------------------------------------------------------
	RE::NiMatrix3 OffsetMap::Rotation(const RE::NiPoint3& a_deg)
	{
		// Ry(b) * Rz(c) * Rx(a), written out so the composition order is
		// explicit and no library convention can silently flip it.
		const float a = a_deg.x * kDegToRad, b = a_deg.y * kDegToRad, c = a_deg.z * kDegToRad;
		const float ca = std::cos(a), sa = std::sin(a);
		const float cb = std::cos(b), sb = std::sin(b);
		const float cc = std::cos(c), sc = std::sin(c);

		RE::NiMatrix3 r;
		r.entry[0][0] = cb * cc;
		r.entry[0][1] = sb * sa - cb * sc * ca;
		r.entry[0][2] = cb * sc * sa + sb * ca;
		r.entry[1][0] = sc;
		r.entry[1][1] = cc * ca;
		r.entry[1][2] = -cc * sa;
		r.entry[2][0] = -sb * cc;
		r.entry[2][1] = sb * sc * ca + cb * sa;
		r.entry[2][2] = cb * ca - sb * sc * sa;
		return r;
	}

	OffsetMap::OffsetMap(const HairOffset& a_offset, const RE::NiPoint3& a_pivot)
	{
		const RE::NiMatrix3 r = Rotation(a_offset.rotate);
		const RE::NiPoint3& s = a_offset.scale;

		// A = diag(s) * R  -- scale each row of R
		for (int row = 0; row < 3; ++row) {
			const float k = row == 0 ? s.x : row == 1 ? s.y : s.z;
			for (int col = 0; col < 3; ++col) {
				_a.entry[row][col] = k * r.entry[row][col];
			}
		}

		// b = s (.) (move - R * pivot) + pivot
		const RE::NiPoint3 rp = r * a_pivot;
		_b = {
			s.x * (a_offset.move.x - rp.x) + a_pivot.x,
			s.y * (a_offset.move.y - rp.y) + a_pivot.y,
			s.z * (a_offset.move.z - rp.z) + a_pivot.z
		};
	}

	OffsetMap::OffsetMap(const HairOffset& a_offset, const RE::NiPoint3& a_pivotBone, const RE::NiTransform& a_bind) :
		OffsetMap(a_offset, a_pivotBone)
	{
		// bind(v) = k R v + t   ;   bone-space map M(p) = A p + b
		// v' = bind^-1(M(bind(v))) = (R^T A R) v + R^T (A t + b - t) / k
		const RE::NiMatrix3& r = a_bind.rotate;
		const RE::NiPoint3&  t = a_bind.translate;
		const float          k = a_bind.scale != 0.0f ? a_bind.scale : 1.0f;
		const RE::NiMatrix3  rt = r.Transpose();

		const RE::NiPoint3 at = _a * t;
		const RE::NiPoint3 inner{ at.x + _b.x - t.x, at.y + _b.y - t.y, at.z + _b.z - t.z };
		const RE::NiPoint3 rb = rt * inner;

		_a = rt * _a * r;
		_b = { rb.x / k, rb.y / k, rb.z / k };
	}

	RE::NiPoint3 OffsetMap::operator()(const RE::NiPoint3& a_v) const
	{
		const RE::NiPoint3 av = _a * a_v;
		return { av.x + _b.x, av.y + _b.y, av.z + _b.z };
	}
}
