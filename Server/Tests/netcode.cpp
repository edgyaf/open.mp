/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

// Differential tests: `NetCode::Rust::read` (the Rust decoders, as the server uses them) must
// accept exactly the inputs the packets' own `read` methods accept, decode them to the same values
// and consume the same number of bits.

#include "harness.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <netcode.hpp>
#include <netcode_rs.hpp>
#include <random>

namespace
{

struct Rng
{
	std::mt19937 gen { 0x6f6d70 };

	int range(int lo, int hi)
	{
		return std::uniform_int_distribution<int>(lo, hi)(gen);
	}

	bool chance(int percent)
	{
		return range(0, 99) < percent;
	}

	uint8_t byte()
	{
		return uint8_t(range(0, 255));
	}

	/// Mostly ordinary values, sometimes ones on or around the decoders' checks.
	float edgyFloat(float typical)
	{
		static const float edges[] = {
			std::numeric_limits<float>::quiet_NaN(),
			std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity(),
			20000.0f,
			19999.998f,
			-20000.0f,
			-1000.0f,
			-999.99994f,
			200000.0f,
			100.0f,
			100.00001f,
			0.0f,
			-0.0f,
		};
		if (chance(10))
		{
			return edges[range(0, int(sizeof(edges) / sizeof(*edges)) - 1)];
		}
		return std::uniform_real_distribution<float>(-typical, typical)(gen);
	}
};

/// A field of a packet layout: an integer of `bytes` bytes, or (`bytes` == 0) `count` floats of
/// roughly `scale` magnitude.
struct Field
{
	int bytes;
	int count = 1;
	float scale = 0.0f;
};

const Field U8 { 1 };
const Field U16 { 2 };
const Field U32 { 4 };

Field floats(int count, float scale)
{
	return Field { 0, count, scale };
}

struct Bytes
{
	std::vector<uint8_t> data;

	void put(const void* p, size_t n)
	{
		const uint8_t* b = static_cast<const uint8_t*>(p);
		data.insert(data.end(), b, b + n);
	}

	void field(Rng& rng, const Field& f)
	{
		if (f.bytes == 0)
		{
			for (int i = 0; i < f.count; ++i)
			{
				const float v = rng.edgyFloat(f.scale);
				put(&v, sizeof(v));
			}
		}
		else
		{
			for (int i = 0; i < f.bytes; ++i)
			{
				data.push_back(rng.byte());
			}
		}
	}
};

std::vector<uint8_t> build(Rng& rng, std::initializer_list<Field> layout)
{
	Bytes b;
	for (const Field& f : layout)
	{
		b.field(rng, f);
	}
	return b.data;
}

void mutate(Rng& rng, std::vector<uint8_t>& data)
{
	if (!data.empty() && rng.chance(30))
	{
		data[rng.range(0, int(data.size()) - 1)] = rng.byte();
	}
	if (rng.chance(15))
	{
		data.resize(rng.range(0, int(data.size())));
	}
	if (rng.chance(10))
	{
		data.push_back(rng.byte());
	}
}

/// `payload` placed after `skipBits` random bits, so reads start at every alignment.
std::vector<uint8_t> withPrefix(Rng& rng, const std::vector<uint8_t>& payload, int skipBits)
{
	NetworkBitStream bs;
	for (int i = 0; i < skipBits; ++i)
	{
		bs.writeBIT(rng.chance(50));
	}
	for (uint8_t b : payload)
	{
		bs.writeUINT8(b);
	}
	return std::vector<uint8_t>(bs.GetData(), bs.GetData() + bitsToBytes(bs.GetNumberOfBitsUsed()));
}

bool sameBits(float a, float b)
{
	return std::memcmp(&a, &b, sizeof(float)) == 0;
}

bool same(const Vector2& a, const Vector2& b)
{
	return sameBits(a.x, b.x) && sameBits(a.y, b.y);
}

bool same(const Vector3& a, const Vector3& b)
{
	return sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z);
}

bool same(const Vector4& a, const Vector4& b)
{
	return sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z) && sameBits(a.w, b.w);
}

bool same(const GTAQuat& a, const GTAQuat& b)
{
	return sameBits(a.q.w, b.q.w) && sameBits(a.q.x, b.q.x) && sameBits(a.q.y, b.q.y) && sameBits(a.q.z, b.q.z);
}

/// Decode random and plausible-but-mutated inputs with the packet's own `read` and with
/// `NetCode::Rust::read`, which the server uses, and compare.
template <class Packet, class Generate, class Compare>
void differential(Generate generate, Compare compare)
{
	Rng rng;
	int accepted = 0, rejected = 0, mismatches = 0;

	for (int i = 0; i < 100000; ++i)
	{
		std::vector<uint8_t> payload;
		if (rng.chance(20))
		{
			payload.resize(rng.range(0, 140));
			for (uint8_t& b : payload)
			{
				b = rng.byte();
			}
		}
		else
		{
			payload = generate(rng);
			mutate(rng, payload);
		}
		const int skip = rng.range(0, 15);
		std::vector<uint8_t> data = withPrefix(rng, payload, skip);

		NetworkBitStream cppStream(data.data(), unsigned(data.size()), false);
		cppStream.SetReadOffset(skip);
		Packet cpp;
		const bool cppOk = cpp.read(cppStream);

		NetworkBitStream rustStream(data.data(), unsigned(data.size()), false);
		rustStream.SetReadOffset(skip);
		Packet rust;
		const bool rustOk = NetCode::Rust::read(rustStream, rust);

		bool match = cppOk == rustOk;
		if (match && cppOk)
		{
			match = compare(cpp, rust) && cppStream.GetReadOffset() == rustStream.GetReadOffset();
		}
		else if (match)
		{
			// A failed read must leave the stream where it was.
			match = rustStream.GetReadOffset() == skip;
		}
		if (!match)
		{
			if (++mismatches <= 5)
			{
				std::printf("  mismatch at iteration %d: C++ %s, Rust %s, %zu bytes, offset %d\n", i, cppOk ? "accepted" : "rejected", rustOk ? "accepted" : "rejected", data.size(), skip);
			}
		}
		else
		{
			(cppOk ? accepted : rejected) += 1;
		}
	}

	CHECK(mismatches == 0);
	// Make sure both paths were actually exercised.
	CHECK(accepted > 5000);
	CHECK(rejected > 5000);
}

}

TEST_CASE(netcode_foot_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerFootSync>(
		[](Rng& rng)
		{
			auto data = build(rng, { U16, U16, U16, floats(3, 3000.0f), floats(4, 1.0f), U8, U8, U8, U8, floats(3, 60.0f), floats(3, 50.0f) });
			const uint16_t surfing = uint16_t(rng.chance(50) ? rng.range(0, 4100) : rng.range(0, 65535));
			data.push_back(surfing & 0xFF);
			data.push_back(surfing >> 8);
			for (int i = 0; i < 4; ++i)
			{
				data.push_back(rng.byte());
			}
			return data;
		},
		[](const NetCode::Packet::PlayerFootSync& c, const NetCode::Packet::PlayerFootSync& r)
		{
			return c.LeftRight == r.LeftRight && c.UpDown == r.UpDown && c.Keys == r.Keys
				&& c.WeaponAdditionalKey == r.WeaponAdditionalKey && c.Weapon == r.Weapon && c.AdditionalKey == r.AdditionalKey
				&& c.SpecialAction == r.SpecialAction && same(c.Position, r.Position) && same(c.Rotation, r.Rotation)
				&& same(c.HealthArmour, r.HealthArmour) && same(c.Velocity, r.Velocity)
				&& c.AnimationID == r.AnimationID && c.AnimationFlags == r.AnimationFlags
				&& c.SurfingData.type == r.SurfingData.type && c.SurfingData.ID == r.SurfingData.ID && same(c.SurfingData.offset, r.SurfingData.offset);
		});
}

TEST_CASE(netcode_aim_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerAimSync>(
		[](Rng& rng)
		{
			return build(rng, { U8, floats(3, 1.0f), floats(3, 3000.0f), floats(1, 10.0f), U8, U8 });
		},
		[](const NetCode::Packet::PlayerAimSync& c, const NetCode::Packet::PlayerAimSync& r)
		{
			return c.CamMode == r.CamMode && c.ZoomWepState == r.ZoomWepState && c.AspectRatio == r.AspectRatio
				&& same(c.CamFrontVector, r.CamFrontVector) && same(c.CamPos, r.CamPos) && sameBits(c.AimZ, r.AimZ);
		});
}

TEST_CASE(netcode_bullet_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerBulletSync>(
		[](Rng& rng)
		{
			return build(rng, { U8, U16, floats(3, 3000.0f), floats(3, 3000.0f), floats(3, 10.0f), U8 });
		},
		[](const NetCode::Packet::PlayerBulletSync& c, const NetCode::Packet::PlayerBulletSync& r)
		{
			return c.HitType == r.HitType && c.HitID == r.HitID && c.WeaponID == r.WeaponID
				&& same(c.Origin, r.Origin) && same(c.HitPos, r.HitPos) && same(c.Offset, r.Offset);
		});
}

TEST_CASE(netcode_stats_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerStatsSync>(
		[](Rng& rng)
		{
			return build(rng, { U32, U32 });
		},
		[](const NetCode::Packet::PlayerStatsSync& c, const NetCode::Packet::PlayerStatsSync& r)
		{
			return c.Money == r.Money && c.DrunkLevel == r.DrunkLevel;
		});
}

TEST_CASE(netcode_weapons_update_matches_cpp)
{
	differential<NetCode::Packet::PlayerWeaponsUpdate>(
		[](Rng& rng)
		{
			auto data = build(rng, { U16, U16 });
			const int slots = rng.range(0, 15);
			for (int i = 0; i < slots; ++i)
			{
				data.push_back(uint8_t(rng.chance(90) ? rng.range(0, 12) : rng.byte()));
				data.push_back(rng.byte());
				data.push_back(rng.byte());
				data.push_back(rng.byte());
			}
			return data;
		},
		[](const NetCode::Packet::PlayerWeaponsUpdate& c, const NetCode::Packet::PlayerWeaponsUpdate& r)
		{
			if (c.TargetPlayer != r.TargetPlayer || c.TargetActor != r.TargetActor || c.WeaponDataCount != r.WeaponDataCount)
			{
				return false;
			}
			for (int i = 0; i < c.WeaponDataCount; ++i)
			{
				if (c.WeaponData[i].first != r.WeaponData[i].first || c.WeaponData[i].second.id != r.WeaponData[i].second.id || c.WeaponData[i].second.ammo != r.WeaponData[i].second.ammo)
				{
					return false;
				}
			}
			return true;
		});
}

TEST_CASE(netcode_spectator_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerSpectatorSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, U16, U16, floats(3, 3000.0f) });
		},
		[](const NetCode::Packet::PlayerSpectatorSync& c, const NetCode::Packet::PlayerSpectatorSync& r)
		{
			return c.LeftRight == r.LeftRight && c.UpDown == r.UpDown && c.Keys == r.Keys && same(c.Position, r.Position);
		});
}

TEST_CASE(netcode_vehicle_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerVehicleSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, U16, U16, U16, floats(4, 1.0f), floats(3, 3000.0f), floats(3, 60.0f), floats(1, 1000.0f), U8, U8, U8, U8, U8, U16, U32 });
		},
		[](const NetCode::Packet::PlayerVehicleSync& c, const NetCode::Packet::PlayerVehicleSync& r)
		{
			return c.VehicleID == r.VehicleID && c.LeftRight == r.LeftRight && c.UpDown == r.UpDown && c.Keys == r.Keys
				&& same(c.Rotation, r.Rotation) && same(c.Position, r.Position) && same(c.Velocity, r.Velocity) && sameBits(c.Health, r.Health)
				&& same(c.PlayerHealthArmour, r.PlayerHealthArmour) && c.AdditionalKeyWeapon == r.AdditionalKeyWeapon
				&& c.Siren == r.Siren && c.LandingGear == r.LandingGear && c.TrailerID == r.TrailerID && c.HydraThrustAngle == r.HydraThrustAngle;
		});
}

TEST_CASE(netcode_passenger_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerPassengerSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, U16, U8, U8, U16, U16, U16, floats(3, 3000.0f) });
		},
		[](const NetCode::Packet::PlayerPassengerSync& c, const NetCode::Packet::PlayerPassengerSync& r)
		{
			return c.VehicleID == r.VehicleID && c.DriveBySeatAdditionalKeyWeapon == r.DriveBySeatAdditionalKeyWeapon && c.Keys == r.Keys
				&& same(c.HealthArmour, r.HealthArmour) && c.LeftRight == r.LeftRight && c.UpDown == r.UpDown && same(c.Position, r.Position);
		});
}

TEST_CASE(netcode_unoccupied_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerUnoccupiedSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, U8, floats(3, 1.0f), floats(3, 1.0f), floats(3, 3000.0f), floats(3, 60.0f), floats(3, 5.0f), floats(1, 1000.0f) });
		},
		[](const NetCode::Packet::PlayerUnoccupiedSync& c, const NetCode::Packet::PlayerUnoccupiedSync& r)
		{
			return c.VehicleID == r.VehicleID && c.SeatID == r.SeatID && same(c.Roll, r.Roll) && same(c.Rotation, r.Rotation)
				&& same(c.Position, r.Position) && same(c.Velocity, r.Velocity) && same(c.AngularVelocity, r.AngularVelocity) && sameBits(c.Health, r.Health);
		});
}

TEST_CASE(netcode_trailer_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerTrailerSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, floats(3, 3000.0f), floats(4, 1.0f), floats(3, 60.0f), floats(3, 5.0f) });
		},
		[](const NetCode::Packet::PlayerTrailerSync& c, const NetCode::Packet::PlayerTrailerSync& r)
		{
			return c.VehicleID == r.VehicleID && same(c.Position, r.Position) && same(c.Quat, r.Quat) && same(c.Velocity, r.Velocity) && same(c.TurnVelocity, r.TurnVelocity);
		});
}
