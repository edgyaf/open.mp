/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

// Differential tests: the Rust packet decoders must accept exactly the inputs the C++ `read`
// methods accept, decode them to the same values and consume the same number of bits.

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

bool same(const Vector2& a, const OmpNetcodeVec2& b)
{
	return sameBits(a.x, b.x) && sameBits(a.y, b.y);
}

bool same(const Vector3& a, const OmpNetcodeVec3& b)
{
	return sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z);
}

bool same(const Vector4& a, const OmpNetcodeVec4& b)
{
	return sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z) && sameBits(a.w, b.w);
}

bool same(const GTAQuat& a, const OmpNetcodeQuat& b)
{
	return sameBits(a.q.w, b.w) && sameBits(a.q.x, b.x) && sameBits(a.q.y, b.y) && sameBits(a.q.z, b.z);
}

/// Decode random and plausible-but-mutated inputs with both implementations and compare.
template <class CppPacket, class RustPacket, class Generate, class RustRead, class Compare>
void differential(Generate generate, RustRead rustRead, Compare compare)
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

		NetworkBitStream bs(data.data(), unsigned(data.size()), false);
		bs.SetReadOffset(skip);
		CppPacket cpp;
		const bool cppOk = cpp.read(bs);

		size_t offset = skip;
		RustPacket rust {};
		const bool rustOk = rustRead(data.data(), data.size() * 8, &offset, &rust);

		bool match = cppOk == rustOk;
		if (match && cppOk)
		{
			match = compare(cpp, rust) && size_t(bs.GetReadOffset()) == offset;
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
	differential<NetCode::Packet::PlayerFootSync, OmpNetcodeFootSync>(
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
		omp_netcode_read_foot_sync,
		[](const NetCode::Packet::PlayerFootSync& c, const OmpNetcodeFootSync& r)
		{
			return c.LeftRight == r.leftRight && c.UpDown == r.upDown && c.Keys == r.keys
				&& c.WeaponAdditionalKey == r.weaponAdditionalKey && c.Weapon == (r.weaponAdditionalKey & 0x3F) && c.AdditionalKey == (r.weaponAdditionalKey >> 6)
				&& c.SpecialAction == r.specialAction && same(c.Position, r.position) && same(c.Rotation, r.rotation)
				&& same(c.HealthArmour, r.healthArmour) && same(c.Velocity, r.velocity)
				&& c.AnimationID == r.animationID && c.AnimationFlags == r.animationFlags
				&& int(c.SurfingData.type) == r.surfingType && c.SurfingData.ID == r.surfingID && same(c.SurfingData.offset, r.surfingOffset);
		});
}

TEST_CASE(netcode_aim_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerAimSync, OmpNetcodeAimSync>(
		[](Rng& rng)
		{
			return build(rng, { U8, floats(3, 1.0f), floats(3, 3000.0f), floats(1, 10.0f), U8, U8 });
		},
		omp_netcode_read_aim_sync,
		[](const NetCode::Packet::PlayerAimSync& c, const OmpNetcodeAimSync& r)
		{
			return c.CamMode == r.camMode && c.ZoomWepState == r.zoomWeaponState && c.AspectRatio == r.aspectRatio
				&& same(c.CamFrontVector, r.camFrontVector) && same(c.CamPos, r.camPos) && sameBits(c.AimZ, r.aimZ);
		});
}

TEST_CASE(netcode_bullet_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerBulletSync, OmpNetcodeBulletSync>(
		[](Rng& rng)
		{
			return build(rng, { U8, U16, floats(3, 3000.0f), floats(3, 3000.0f), floats(3, 10.0f), U8 });
		},
		omp_netcode_read_bullet_sync,
		[](const NetCode::Packet::PlayerBulletSync& c, const OmpNetcodeBulletSync& r)
		{
			return c.HitType == r.hitType && c.HitID == r.hitID && c.WeaponID == r.weaponID
				&& same(c.Origin, r.origin) && same(c.HitPos, r.hitPos) && same(c.Offset, r.offset);
		});
}

TEST_CASE(netcode_stats_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerStatsSync, OmpNetcodeStatsSync>(
		[](Rng& rng)
		{
			return build(rng, { U32, U32 });
		},
		omp_netcode_read_stats_sync,
		[](const NetCode::Packet::PlayerStatsSync& c, const OmpNetcodeStatsSync& r)
		{
			return c.Money == r.money && c.DrunkLevel == r.drunkLevel;
		});
}

TEST_CASE(netcode_weapons_update_matches_cpp)
{
	differential<NetCode::Packet::PlayerWeaponsUpdate, OmpNetcodeWeaponsUpdate>(
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
		omp_netcode_read_weapons_update,
		[](const NetCode::Packet::PlayerWeaponsUpdate& c, const OmpNetcodeWeaponsUpdate& r)
		{
			if (c.TargetPlayer != r.targetPlayer || c.TargetActor != r.targetActor || c.WeaponDataCount != r.weaponCount)
			{
				return false;
			}
			for (int i = 0; i < c.WeaponDataCount; ++i)
			{
				if (c.WeaponData[i].first != r.weapons[i].slot || c.WeaponData[i].second.id != r.weapons[i].id || c.WeaponData[i].second.ammo != r.weapons[i].ammo)
				{
					return false;
				}
			}
			return true;
		});
}

TEST_CASE(netcode_spectator_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerSpectatorSync, OmpNetcodeSpectatorSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, U16, U16, floats(3, 3000.0f) });
		},
		omp_netcode_read_spectator_sync,
		[](const NetCode::Packet::PlayerSpectatorSync& c, const OmpNetcodeSpectatorSync& r)
		{
			return c.LeftRight == r.leftRight && c.UpDown == r.upDown && c.Keys == r.keys && same(c.Position, r.position);
		});
}

TEST_CASE(netcode_vehicle_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerVehicleSync, OmpNetcodeVehicleSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, U16, U16, U16, floats(4, 1.0f), floats(3, 3000.0f), floats(3, 60.0f), floats(1, 1000.0f), U8, U8, U8, U8, U8, U16, U32 });
		},
		omp_netcode_read_vehicle_sync,
		[](const NetCode::Packet::PlayerVehicleSync& c, const OmpNetcodeVehicleSync& r)
		{
			return c.VehicleID == r.vehicleID && c.LeftRight == r.leftRight && c.UpDown == r.upDown && c.Keys == r.keys
				&& same(c.Rotation, r.rotation) && same(c.Position, r.position) && same(c.Velocity, r.velocity) && sameBits(c.Health, r.health)
				&& same(c.PlayerHealthArmour, r.playerHealthArmour) && c.AdditionalKeyWeapon == r.additionalKeyWeapon
				&& c.Siren == r.siren && c.LandingGear == r.landingGear && c.TrailerID == r.trailerID && c.HydraThrustAngle == r.hydraThrustAngle;
		});
}

TEST_CASE(netcode_passenger_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerPassengerSync, OmpNetcodePassengerSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, U16, U8, U8, U16, U16, U16, floats(3, 3000.0f) });
		},
		omp_netcode_read_passenger_sync,
		[](const NetCode::Packet::PlayerPassengerSync& c, const OmpNetcodePassengerSync& r)
		{
			return c.VehicleID == r.vehicleID && c.DriveBySeatAdditionalKeyWeapon == r.driveBySeatAdditionalKeyWeapon && c.Keys == r.keys
				&& same(c.HealthArmour, r.healthArmour) && c.LeftRight == r.leftRight && c.UpDown == r.upDown && same(c.Position, r.position);
		});
}

TEST_CASE(netcode_unoccupied_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerUnoccupiedSync, OmpNetcodeUnoccupiedSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, U8, floats(3, 1.0f), floats(3, 1.0f), floats(3, 3000.0f), floats(3, 60.0f), floats(3, 5.0f), floats(1, 1000.0f) });
		},
		omp_netcode_read_unoccupied_sync,
		[](const NetCode::Packet::PlayerUnoccupiedSync& c, const OmpNetcodeUnoccupiedSync& r)
		{
			return c.VehicleID == r.vehicleID && c.SeatID == r.seatID && same(c.Roll, r.roll) && same(c.Rotation, r.rotation)
				&& same(c.Position, r.position) && same(c.Velocity, r.velocity) && same(c.AngularVelocity, r.angularVelocity) && sameBits(c.Health, r.health);
		});
}

TEST_CASE(netcode_trailer_sync_matches_cpp)
{
	differential<NetCode::Packet::PlayerTrailerSync, OmpNetcodeTrailerSync>(
		[](Rng& rng)
		{
			return build(rng, { U16, floats(3, 3000.0f), floats(4, 1.0f), floats(3, 60.0f), floats(3, 5.0f) });
		},
		omp_netcode_read_trailer_sync,
		[](const NetCode::Packet::PlayerTrailerSync& c, const OmpNetcodeTrailerSync& r)
		{
			return c.VehicleID == r.vehicleID && same(c.Position, r.position) && same(c.Quat, r.quat) && same(c.Velocity, r.velocity) && same(c.TurnVelocity, r.turnVelocity);
		});
}
