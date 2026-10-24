/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

// Differential tests: `NetCode::Rust::read` (the Rust decoders, as the server uses them) must
// accept exactly the inputs the packets' and RPCs' own `read` methods accept, decode them to the
// same values and consume the same number of bits. The exception is inputs the C++ accepts with
// fields it never read, which the Rust rejects on purpose.

#include "harness.hpp"
#include <algorithm>
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

/// Bits a stream over `data` has left to read from `skip`.
int bitsAfter(const std::vector<uint8_t>& data, int skip)
{
	return int(data.size()) * 8 - skip;
}

/// The byte starting at bit `bit` of `data`, which must have 8 bits there.
uint8_t byteAt(const std::vector<uint8_t>& data, int bit)
{
	const int shift = bit & 7;
	const size_t i = size_t(bit >> 3);
	return uint8_t(data[i] << shift | (shift ? data[i + 1] >> (8 - shift) : 0));
}

/// Whether the Rust decoder must reject an input the C++ accepts.
enum class Expect
{
	Same,
	RustRejects,
};

/// Decode random and plausible-but-mutated inputs with the packet's own `read` and with
/// `NetCode::Rust::read`, which the server uses, and compare. `expect(data, skip)` picks out
/// inputs where the C++ reads fields that aren't there; for those the C++ isn't run (it would
/// read uninitialised memory or assert) and the Rust must reject.
template <class Packet, class Generate, class Compare, class Expectation>
void differential(Generate generate, Compare compare, Expectation expect, bool hasExpectations = true)
{
	Rng rng;
	int accepted = 0, rejected = 0, ported = 0, mismatches = 0, portedMismatches = 0;

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

		if (expect(data, skip) == Expect::RustRejects)
		{
			NetworkBitStream rustStream(data.data(), unsigned(data.size()), false);
			rustStream.SetReadOffset(skip);
			Packet rust;
			if (NetCode::Rust::read(rustStream, rust) || rustStream.GetReadOffset() != skip)
			{
				++portedMismatches;
			}
			else
			{
				++rejected;
				++ported;
			}
			continue;
		}

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
	CHECK_PORTED(portedMismatches == 0);
	// Make sure both paths were actually exercised.
	CHECK(accepted > 5000);
	CHECK(rejected > 2000);
	// And that the inputs the Rust rejects on purpose actually came up.
	CHECK(!hasExpectations || ported >= 50);
}

template <class Packet, class Generate, class Compare>
void differential(Generate generate, Compare compare)
{
	differential<Packet>(
		generate, compare, [](const std::vector<uint8_t>&, int)
		{
			return Expect::Same;
		},
		false);
}

/// Mostly printable bytes for a message, `len` long.
void appendText(Rng& rng, std::vector<uint8_t>& data, int len)
{
	for (int i = 0; i < len; ++i)
	{
		data.push_back(uint8_t(rng.chance(95) ? rng.range(0x20, 0x7E) : rng.byte()));
	}
}

/// A length prefix that's mostly right, sometimes a little off and sometimes anything.
int lengthFor(Rng& rng, int len, int max)
{
	if (rng.chance(80))
	{
		return len;
	}
	return rng.chance(50) ? std::max(0, len + rng.range(-2, 2)) : rng.range(0, max);
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

TEST_CASE(netcode_click_map_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerClickMap>(
		[](Rng& rng)
		{
			return build(rng, { floats(3, 25000.0f) });
		},
		[](const NetCode::RPC::OnPlayerClickMap& c, const NetCode::RPC::OnPlayerClickMap& r)
		{
			return same(c.Pos, r.Pos);
		});
}

TEST_CASE(netcode_click_player_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerClickPlayer>(
		[](Rng& rng)
		{
			return build(rng, { U16, U8 });
		},
		[](const NetCode::RPC::OnPlayerClickPlayer& c, const NetCode::RPC::OnPlayerClickPlayer& r)
		{
			return c.PlayerID == r.PlayerID && c.Source == r.Source;
		},
		[](const std::vector<uint8_t>& data, int skip)
		{
			// The C++ accepts these with an unread player ID.
			const int bits = bitsAfter(data, skip);
			return bits >= 8 && bits < 16 ? Expect::RustRejects : Expect::Same;
		});
}

TEST_CASE(netcode_give_take_damage_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerGiveTakeDamage>(
		[](Rng& rng)
		{
			NetworkBitStream bs;
			bs.writeBIT(rng.chance(50));
			bs.writeUINT16(rng.range(0, 1100));
			bs.writeFLOAT(rng.edgyFloat(200.0f));
			bs.writeUINT32(rng.chance(80) ? rng.range(0, 60) : rng.range(0, 1 << 30));
			bs.writeUINT32(rng.chance(80) ? rng.range(0, 10) : rng.range(0, 1 << 30));
			return std::vector<uint8_t>(bs.GetData(), bs.GetData() + bitsToBytes(bs.GetNumberOfBitsUsed()));
		},
		[](const NetCode::RPC::OnPlayerGiveTakeDamage& c, const NetCode::RPC::OnPlayerGiveTakeDamage& r)
		{
			return c.Taking == r.Taking && c.PlayerID == r.PlayerID && sameBits(c.Damage, r.Damage) && c.WeaponID == r.WeaponID && c.Bodypart == r.Bodypart;
		});
}

TEST_CASE(netcode_interior_change_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerInteriorChange>(
		[](Rng& rng)
		{
			return build(rng, { U8 });
		},
		[](const NetCode::RPC::OnPlayerInteriorChange& c, const NetCode::RPC::OnPlayerInteriorChange& r)
		{
			return c.Interior == r.Interior;
		});
}

TEST_CASE(netcode_death_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerDeath>(
		[](Rng& rng)
		{
			return build(rng, { U8, U16 });
		},
		[](const NetCode::RPC::OnPlayerDeath& c, const NetCode::RPC::OnPlayerDeath& r)
		{
			return c.Reason == r.Reason && c.KillerID == r.KillerID;
		});
}

TEST_CASE(netcode_camera_target_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerCameraTarget>(
		[](Rng& rng)
		{
			return build(rng, { U16, U16, U16, U16 });
		},
		[](const NetCode::RPC::OnPlayerCameraTarget& c, const NetCode::RPC::OnPlayerCameraTarget& r)
		{
			return c.TargetObjectID == r.TargetObjectID && c.TargetVehicleID == r.TargetVehicleID
				&& c.TargetPlayerID == r.TargetPlayerID && c.TargetActorID == r.TargetActorID;
		});
}

TEST_CASE(netcode_client_check_matches_cpp)
{
	differential<NetCode::RPC::ClientCheck>(
		[](Rng& rng)
		{
			return build(rng, { U8, U32, U8 });
		},
		[](const NetCode::RPC::ClientCheck& c, const NetCode::RPC::ClientCheck& r)
		{
			return c.Type == r.Type && c.Address == r.Address && c.Results == r.Results;
		},
		[](const std::vector<uint8_t>& data, int skip)
		{
			// The C++ always accepts, leaving whatever it couldn't read uninitialised.
			return bitsAfter(data, skip) < 48 ? Expect::RustRejects : Expect::Same;
		});
}

template <size_t Size>
bool sameMessage(const HybridString<Size>& a, const HybridString<Size>& b)
{
	return a.length() == b.length() && std::memcmp(a.data(), b.data(), a.length()) == 0;
}

TEST_CASE(netcode_chat_message_matches_cpp)
{
	differential<NetCode::RPC::PlayerRequestChatMessage>(
		[](Rng& rng)
		{
			const int len = rng.chance(5) ? 0 : rng.range(1, 160);
			std::vector<uint8_t> data { uint8_t(lengthFor(rng, len, 255)) };
			appendText(rng, data, len);
			return data;
		},
		[](const NetCode::RPC::PlayerRequestChatMessage& c, const NetCode::RPC::PlayerRequestChatMessage& r)
		{
			return sameMessage(c.message, r.message);
		},
		[](const std::vector<uint8_t>& data, int skip)
		{
			// An empty message off a byte boundary has the C++ call `ReadBits` for zero bits,
			// which asserts (and, without asserts, fails).
			const bool empty = bitsAfter(data, skip) >= 8 && byteAt(data, skip) == 0;
			return empty && skip % 8 != 0 ? Expect::RustRejects : Expect::Same;
		});
}

TEST_CASE(netcode_command_message_matches_cpp)
{
	differential<NetCode::RPC::PlayerRequestCommandMessage>(
		[](Rng& rng)
		{
			const int len = rng.chance(5) ? rng.range(0, 2) : rng.range(2, 260);
			const uint32_t prefix = rng.chance(5) ? uint32_t(rng.range(0, 1 << 30)) * 4 : uint32_t(lengthFor(rng, len, 300));
			std::vector<uint8_t> data;
			for (int i = 0; i < 4; ++i)
			{
				data.push_back(uint8_t(prefix >> (8 * i)));
			}
			if (len > 0)
			{
				data.push_back('/');
				appendText(rng, data, len - 1);
			}
			return data;
		},
		[](const NetCode::RPC::PlayerRequestCommandMessage& c, const NetCode::RPC::PlayerRequestCommandMessage& r)
		{
			return sameMessage(c.message, r.message);
		});
}

TEST_CASE(netcode_damage_actor_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerDamageActor>(
		[](Rng& rng)
		{
			NetworkBitStream bs;
			bs.writeBIT(rng.chance(50));
			bs.writeUINT16(rng.range(0, 1100));
			bs.writeFLOAT(rng.edgyFloat(200.0f));
			bs.writeUINT32(rng.chance(80) ? rng.range(0, 60) : rng.range(0, 1 << 30));
			bs.writeUINT32(rng.chance(80) ? rng.range(0, 10) : rng.range(0, 1 << 30));
			return std::vector<uint8_t>(bs.GetData(), bs.GetData() + bitsToBytes(bs.GetNumberOfBitsUsed()));
		},
		[](const NetCode::RPC::OnPlayerDamageActor& c, const NetCode::RPC::OnPlayerDamageActor& r)
		{
			return c.Unknown == r.Unknown && c.ActorID == r.ActorID && sameBits(c.Damage, r.Damage) && c.WeaponID == r.WeaponID && c.Bodypart == r.Bodypart;
		});
}

TEST_CASE(netcode_request_class_matches_cpp)
{
	differential<NetCode::RPC::PlayerRequestClass>(
		[](Rng& rng)
		{
			return build(rng, { U16 });
		},
		[](const NetCode::RPC::PlayerRequestClass& c, const NetCode::RPC::PlayerRequestClass& r)
		{
			return c.Classid == r.Classid;
		});
}

TEST_CASE(netcode_dialog_response_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerDialogResponse>(
		[](Rng& rng)
		{
			if (rng.chance(5))
			{
				// Cut short after the response, with a byte that reads as an empty text's length.
				auto data = build(rng, { U16, U8 });
				data.push_back(0);
				return data;
			}
			auto data = build(rng, { U16, U8, U16 });
			const int len = rng.chance(5) ? 0 : rng.range(1, 255);
			data.push_back(uint8_t(lengthFor(rng, len, 255)));
			appendText(rng, data, len);
			return data;
		},
		[](const NetCode::RPC::OnPlayerDialogResponse& c, const NetCode::RPC::OnPlayerDialogResponse& r)
		{
			return c.ID == r.ID && c.Response == r.Response && c.ListItem == r.ListItem && sameMessage(c.Text, r.Text);
		},
		[](const std::vector<uint8_t>& data, int skip)
		{
			const int bits = bitsAfter(data, skip);
			// Too short for the list item but long enough for an empty text's length: the C++
			// accepts with an unread list item.
			if (bits >= 32 && bits < 40 && byteAt(data, skip + 24) == 0)
			{
				return Expect::RustRejects;
			}
			// An empty text off a byte boundary asserts in `ReadBits`.
			if (bits >= 48 && byteAt(data, skip + 40) == 0 && skip % 8 != 0)
			{
				return Expect::RustRejects;
			}
			return Expect::Same;
		});
}

TEST_CASE(netcode_selected_menu_row_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerSelectedMenuRow>(
		[](Rng& rng)
		{
			return build(rng, { U8 });
		},
		[](const NetCode::RPC::OnPlayerSelectedMenuRow& c, const NetCode::RPC::OnPlayerSelectedMenuRow& r)
		{
			return c.MenuRow == r.MenuRow;
		});
}

TEST_CASE(netcode_select_object_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerSelectObject>(
		[](Rng& rng)
		{
			return build(rng, { U32, U16, U32, floats(3, 3000.0f) });
		},
		[](const NetCode::RPC::OnPlayerSelectObject& c, const NetCode::RPC::OnPlayerSelectObject& r)
		{
			return c.SelectType == r.SelectType && c.ObjectID == r.ObjectID && c.Model == r.Model && same(c.Position, r.Position);
		});
}

TEST_CASE(netcode_edit_object_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerEditObject>(
		[](Rng& rng)
		{
			NetworkBitStream bs;
			bs.writeBIT(rng.chance(50));
			bs.writeUINT16(rng.range(0, 2100));
			bs.writeUINT32(rng.range(0, 3));
			for (int i = 0; i < 6; ++i)
			{
				bs.writeFLOAT(rng.edgyFloat(i < 3 ? 3000.0f : 360.0f));
			}
			return std::vector<uint8_t>(bs.GetData(), bs.GetData() + bitsToBytes(bs.GetNumberOfBitsUsed()));
		},
		[](const NetCode::RPC::OnPlayerEditObject& c, const NetCode::RPC::OnPlayerEditObject& r)
		{
			return c.PlayerObject == r.PlayerObject && c.ObjectID == r.ObjectID && c.Response == r.Response && same(c.Offset, r.Offset) && same(c.Rotation, r.Rotation);
		});
}

TEST_CASE(netcode_edit_attached_object_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerEditAttachedObject>(
		[](Rng& rng)
		{
			return build(rng, { U32, U32, U32, U32, floats(3, 2.0f), floats(3, 360.0f), floats(3, 2.0f), U32, U32 });
		},
		[](const NetCode::RPC::OnPlayerEditAttachedObject& c, const NetCode::RPC::OnPlayerEditAttachedObject& r)
		{
			const ObjectAttachmentSlotData& a = c.AttachmentData;
			const ObjectAttachmentSlotData& b = r.AttachmentData;
			return c.Response == r.Response && c.Index == r.Index && a.model == b.model && a.bone == b.bone
				&& same(a.offset, b.offset) && same(a.rotation, b.rotation) && same(a.scale, b.scale)
				&& a.colour1.RGBA() == b.colour1.RGBA() && a.colour2.RGBA() == b.colour2.RGBA();
		});
}

TEST_CASE(netcode_pick_up_pickup_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerPickUpPickup>(
		[](Rng& rng)
		{
			return build(rng, { U32 });
		},
		[](const NetCode::RPC::OnPlayerPickUpPickup& c, const NetCode::RPC::OnPlayerPickUpPickup& r)
		{
			return c.PickupID == r.PickupID;
		});
}

TEST_CASE(netcode_select_textdraw_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerSelectTextDraw>(
		[](Rng& rng)
		{
			const int id = rng.chance(10) ? 0xFFFF : rng.range(0, 2048 + 256);
			return std::vector<uint8_t> { uint8_t(id), uint8_t(id >> 8) };
		},
		[](const NetCode::RPC::OnPlayerSelectTextDraw& c, const NetCode::RPC::OnPlayerSelectTextDraw& r)
		{
			// The C++ leaves PlayerTextDraw unset for a cancelled selection.
			return c.Invalid == r.Invalid && c.TextDrawID == r.TextDrawID && (c.Invalid || c.PlayerTextDraw == r.PlayerTextDraw);
		});
}

TEST_CASE(netcode_enter_vehicle_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerEnterVehicle>(
		[](Rng& rng)
		{
			return build(rng, { U16, U8 });
		},
		[](const NetCode::RPC::OnPlayerEnterVehicle& c, const NetCode::RPC::OnPlayerEnterVehicle& r)
		{
			return c.VehicleID == r.VehicleID && c.Passenger == r.Passenger;
		},
		[](const std::vector<uint8_t>& data, int skip)
		{
			// The C++ accepts these with an unread vehicle ID.
			const int bits = bitsAfter(data, skip);
			return bits >= 8 && bits < 16 ? Expect::RustRejects : Expect::Same;
		});
}

TEST_CASE(netcode_exit_vehicle_matches_cpp)
{
	differential<NetCode::RPC::OnPlayerExitVehicle>(
		[](Rng& rng)
		{
			return build(rng, { U16 });
		},
		[](const NetCode::RPC::OnPlayerExitVehicle& c, const NetCode::RPC::OnPlayerExitVehicle& r)
		{
			return c.VehicleID == r.VehicleID;
		});
}

TEST_CASE(netcode_vehicle_death_matches_cpp)
{
	differential<NetCode::RPC::VehicleDeath>(
		[](Rng& rng)
		{
			return build(rng, { U16 });
		},
		[](const NetCode::RPC::VehicleDeath& c, const NetCode::RPC::VehicleDeath& r)
		{
			return c.VehicleID == r.VehicleID;
		});
}

TEST_CASE(netcode_vehicle_damage_status_matches_cpp)
{
	differential<NetCode::RPC::SetVehicleDamageStatus>(
		[](Rng& rng)
		{
			return build(rng, { U16, U32, U32, U8, U8 });
		},
		[](const NetCode::RPC::SetVehicleDamageStatus& c, const NetCode::RPC::SetVehicleDamageStatus& r)
		{
			return c.VehicleID == r.VehicleID && c.PanelStatus == r.PanelStatus && c.DoorStatus == r.DoorStatus
				&& c.LightStatus == r.LightStatus && c.TyreStatus == r.TyreStatus;
		},
		[](const std::vector<uint8_t>& data, int skip)
		{
			// The C++ accepts these with the panel or door status unread.
			const int bits = bitsAfter(data, skip);
			return (bits >= 32 && bits < 48) || (bits >= 64 && bits < 80) ? Expect::RustRejects : Expect::Same;
		});
}

TEST_CASE(netcode_scm_event_matches_cpp)
{
	differential<NetCode::RPC::SCMEvent>(
		[](Rng& rng)
		{
			return build(rng, { U32, U32, U32, U32 });
		},
		[](const NetCode::RPC::SCMEvent& c, const NetCode::RPC::SCMEvent& r)
		{
			return c.VehicleID == r.VehicleID && c.Arg1 == r.Arg1 && c.Arg2 == r.Arg2 && c.EventType == r.EventType;
		},
		[](const std::vector<uint8_t>& data, int skip)
		{
			// The C++ always accepts, leaving whatever it couldn't read uninitialised.
			return bitsAfter(data, skip) < 128 ? Expect::RustRejects : Expect::Same;
		});
}

TEST_CASE(netcode_rcon_command_matches_cpp)
{
	differential<NetCode::Packet::PlayerRconCommand>(
		[](Rng& rng)
		{
			// Long enough to need HybridString's heap storage sometimes.
			const int len = rng.chance(5) ? 0 : rng.range(1, 120);
			const uint32_t prefix = rng.chance(5) ? uint32_t(rng.range(0, 1 << 30)) * 4 : uint32_t(lengthFor(rng, len, 200));
			std::vector<uint8_t> data;
			for (int i = 0; i < 4; ++i)
			{
				data.push_back(uint8_t(prefix >> (8 * i)));
			}
			appendText(rng, data, len);
			return data;
		},
		[](const NetCode::Packet::PlayerRconCommand& c, const NetCode::Packet::PlayerRconCommand& r)
		{
			return sameMessage(c.cmd, r.cmd);
		},
		[](const std::vector<uint8_t>& data, int skip)
		{
			// An empty command off a byte boundary asserts in `ReadBits`.
			bool empty = bitsAfter(data, skip) >= 32;
			for (int i = 0; empty && i < 4; ++i)
			{
				empty = byteAt(data, skip + 8 * i) == 0;
			}
			return empty && skip % 8 != 0 ? Expect::RustRejects : Expect::Same;
		});
}
