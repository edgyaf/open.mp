/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

// Rust decoders for packets sent by clients (`Shared/rust/netcode`), and `NetCode::Rust::read`
// overloads that use them in place of the packets' own `read` methods.
//
// The C ABI below mirrors `Shared/rust/netcode/src/ffi.rs`; keep the two in sync. Decoders take a
// `NetworkBitStream`'s buffer, its size in bits and the current read offset in bits. On success
// they fill `out`, advance `*offset` past what they read and return true. On failure `out` and
// `*offset` are left alone.

#include "netcode.hpp"
#include <cstddef>
#include <cstdint>

extern "C"
{
	struct OmpNetcodeVec2
	{
		float x, y;
	};

	struct OmpNetcodeVec3
	{
		float x, y, z;
	};

	struct OmpNetcodeQuat
	{
		float w, x, y, z;
	};

	struct OmpNetcodeFootSync
	{
		uint16_t leftRight;
		uint16_t upDown;
		uint16_t keys;
		/// Low 6 bits: weapon; high 2 bits: additional key.
		uint8_t weaponAdditionalKey;
		uint8_t specialAction;
		OmpNetcodeVec3 position;
		OmpNetcodeQuat rotation;
		OmpNetcodeVec2 healthArmour;
		OmpNetcodeVec3 velocity;
		uint16_t animationID;
		uint16_t animationFlags;
		/// `PlayerSurfingData::Type`.
		int32_t surfingType;
		int32_t surfingID;
		OmpNetcodeVec3 surfingOffset;
	};

	struct OmpNetcodeVec4
	{
		float x, y, z, w;
	};

	struct OmpNetcodeAimSync
	{
		uint8_t camMode;
		/// Low 6 bits: camera zoom; high 2 bits: weapon state.
		uint8_t zoomWeaponState;
		uint8_t aspectRatio;
		OmpNetcodeVec3 camFrontVector;
		OmpNetcodeVec3 camPos;
		float aimZ;
	};

	struct OmpNetcodeBulletSync
	{
		uint8_t hitType;
		uint8_t weaponID;
		uint16_t hitID;
		OmpNetcodeVec3 origin;
		OmpNetcodeVec3 hitPos;
		OmpNetcodeVec3 offset;
	};

	struct OmpNetcodeStatsSync
	{
		int32_t money;
		int32_t drunkLevel;
	};

	struct OmpNetcodeWeaponSlot
	{
		uint8_t slot;
		uint8_t id;
		uint32_t ammo;
	};

	struct OmpNetcodeWeaponsUpdate
	{
		int32_t targetPlayer;
		int32_t targetActor;
		int32_t weaponCount;
		OmpNetcodeWeaponSlot weapons[13];
	};

	struct OmpNetcodeSpectatorSync
	{
		uint16_t leftRight;
		uint16_t upDown;
		uint16_t keys;
		OmpNetcodeVec3 position;
	};

	struct OmpNetcodeVehicleSync
	{
		uint16_t vehicleID;
		uint16_t leftRight;
		uint16_t upDown;
		uint16_t keys;
		OmpNetcodeQuat rotation;
		OmpNetcodeVec3 position;
		OmpNetcodeVec3 velocity;
		float health;
		OmpNetcodeVec2 playerHealthArmour;
		/// Low 6 bits: weapon; high 2 bits: additional key.
		uint8_t additionalKeyWeapon;
		uint8_t siren;
		uint8_t landingGear;
		uint16_t trailerID;
		/// Also the train speed, as a float.
		uint32_t hydraThrustAngle;
	};

	struct OmpNetcodePassengerSync
	{
		int32_t vehicleID;
		/// Bits 0-5: seat; 6: drive-by; 7: cuffed; 8-13: weapon; 14-15: additional key.
		uint16_t driveBySeatAdditionalKeyWeapon;
		uint16_t keys;
		OmpNetcodeVec2 healthArmour;
		uint16_t leftRight;
		uint16_t upDown;
		OmpNetcodeVec3 position;
	};

	struct OmpNetcodeUnoccupiedSync
	{
		int32_t vehicleID;
		uint8_t seatID;
		OmpNetcodeVec3 roll;
		OmpNetcodeVec3 rotation;
		OmpNetcodeVec3 position;
		OmpNetcodeVec3 velocity;
		OmpNetcodeVec3 angularVelocity;
		float health;
	};

	struct OmpNetcodeTrailerSync
	{
		int32_t vehicleID;
		OmpNetcodeVec3 position;
		OmpNetcodeVec4 quat;
		OmpNetcodeVec3 velocity;
		OmpNetcodeVec3 turnVelocity;
	};

	bool omp_netcode_read_foot_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeFootSync* out);
	bool omp_netcode_read_aim_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeAimSync* out);
	bool omp_netcode_read_bullet_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeBulletSync* out);
	bool omp_netcode_read_stats_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeStatsSync* out);
	bool omp_netcode_read_weapons_update(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeWeaponsUpdate* out);
	bool omp_netcode_read_spectator_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeSpectatorSync* out);
	bool omp_netcode_read_vehicle_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeVehicleSync* out);
	bool omp_netcode_read_passenger_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodePassengerSync* out);
	bool omp_netcode_read_unoccupied_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeUnoccupiedSync* out);
	bool omp_netcode_read_trailer_sync(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeTrailerSync* out);
}

// Must match the Rust definitions, which assert the same sizes.
static_assert(sizeof(OmpNetcodeFootSync) == 80, "OmpNetcodeFootSync layout differs from Rust");
static_assert(sizeof(OmpNetcodeAimSync) == 32, "OmpNetcodeAimSync layout differs from Rust");
static_assert(sizeof(OmpNetcodeBulletSync) == 40, "OmpNetcodeBulletSync layout differs from Rust");
static_assert(sizeof(OmpNetcodeStatsSync) == 8, "OmpNetcodeStatsSync layout differs from Rust");
static_assert(sizeof(OmpNetcodeWeaponsUpdate) == 116, "OmpNetcodeWeaponsUpdate layout differs from Rust");
static_assert(sizeof(OmpNetcodeSpectatorSync) == 20, "OmpNetcodeSpectatorSync layout differs from Rust");
static_assert(sizeof(OmpNetcodeVehicleSync) == 72, "OmpNetcodeVehicleSync layout differs from Rust");
static_assert(sizeof(OmpNetcodePassengerSync) == 32, "OmpNetcodePassengerSync layout differs from Rust");
static_assert(sizeof(OmpNetcodeUnoccupiedSync) == 72, "OmpNetcodeUnoccupiedSync layout differs from Rust");
static_assert(sizeof(OmpNetcodeTrailerSync) == 56, "OmpNetcodeTrailerSync layout differs from Rust");

namespace NetCode
{
namespace Rust
{
	namespace detail
	{
		inline Vector2 toCpp(const OmpNetcodeVec2& v)
		{
			return Vector2(v.x, v.y);
		}

		inline Vector3 toCpp(const OmpNetcodeVec3& v)
		{
			return Vector3(v.x, v.y, v.z);
		}

		inline Vector4 toCpp(const OmpNetcodeVec4& v)
		{
			return Vector4(v.x, v.y, v.z, v.w);
		}

		inline GTAQuat toCpp(const OmpNetcodeQuat& q)
		{
			return GTAQuat(q.w, q.x, q.y, q.z);
		}

		/// Decode from `bs`'s read offset with a Rust decoder, filling `out` with `fill` and
		/// advancing the read offset on success.
		template <class RustPacket, class Fill>
		bool decode(NetworkBitStream& bs, bool (*read)(const uint8_t*, size_t, size_t*, RustPacket*), Fill fill)
		{
			size_t offset = size_t(bs.GetReadOffset());
			RustPacket packet;
			if (!read(bs.GetData(), size_t(bs.GetNumberOfBitsUsed()), &offset, &packet))
			{
				return false;
			}
			fill(packet);
			bs.SetReadOffset(int(offset));
			return true;
		}
	}

	/// Drop-in replacements for the packets' `read` methods: same inputs accepted, same fields
	/// filled in, same number of bits consumed.

	inline bool read(NetworkBitStream& bs, Packet::PlayerFootSync& out)
	{
		return detail::decode(bs, omp_netcode_read_foot_sync, [&out](const OmpNetcodeFootSync& r)
			{
				out.LeftRight = r.leftRight;
				out.UpDown = r.upDown;
				out.Keys = r.keys;
				out.WeaponAdditionalKey = r.weaponAdditionalKey;
				out.SpecialAction = r.specialAction;
				out.Position = detail::toCpp(r.position);
				out.Rotation = detail::toCpp(r.rotation);
				out.HealthArmour = detail::toCpp(r.healthArmour);
				out.Velocity = detail::toCpp(r.velocity);
				out.AnimationID = r.animationID;
				out.AnimationFlags = r.animationFlags;
				out.SurfingData.type = PlayerSurfingData::Type(r.surfingType);
				out.SurfingData.ID = r.surfingID;
				out.SurfingData.offset = detail::toCpp(r.surfingOffset);
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerAimSync& out)
	{
		return detail::decode(bs, omp_netcode_read_aim_sync, [&out](const OmpNetcodeAimSync& r)
			{
				out.CamMode = r.camMode;
				out.ZoomWepState = r.zoomWeaponState;
				out.AspectRatio = r.aspectRatio;
				out.CamFrontVector = detail::toCpp(r.camFrontVector);
				out.CamPos = detail::toCpp(r.camPos);
				out.AimZ = r.aimZ;
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerBulletSync& out)
	{
		return detail::decode(bs, omp_netcode_read_bullet_sync, [&out](const OmpNetcodeBulletSync& r)
			{
				out.HitType = r.hitType;
				out.HitID = r.hitID;
				out.WeaponID = r.weaponID;
				out.Origin = detail::toCpp(r.origin);
				out.HitPos = detail::toCpp(r.hitPos);
				out.Offset = detail::toCpp(r.offset);
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerStatsSync& out)
	{
		return detail::decode(bs, omp_netcode_read_stats_sync, [&out](const OmpNetcodeStatsSync& r)
			{
				out.Money = r.money;
				out.DrunkLevel = r.drunkLevel;
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerWeaponsUpdate& out)
	{
		return detail::decode(bs, omp_netcode_read_weapons_update, [&out](const OmpNetcodeWeaponsUpdate& r)
			{
				out.TargetPlayer = r.targetPlayer;
				out.TargetActor = r.targetActor;
				out.WeaponDataCount = r.weaponCount;
				for (int i = 0; i < r.weaponCount; ++i)
				{
					WeaponSlotData data;
					data.id = r.weapons[i].id;
					data.ammo = r.weapons[i].ammo;
					out.WeaponData[i] = std::make_pair(r.weapons[i].slot, data);
				}
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerSpectatorSync& out)
	{
		return detail::decode(bs, omp_netcode_read_spectator_sync, [&out](const OmpNetcodeSpectatorSync& r)
			{
				out.LeftRight = r.leftRight;
				out.UpDown = r.upDown;
				out.Keys = r.keys;
				out.Position = detail::toCpp(r.position);
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerVehicleSync& out)
	{
		return detail::decode(bs, omp_netcode_read_vehicle_sync, [&out](const OmpNetcodeVehicleSync& r)
			{
				out.VehicleID = r.vehicleID;
				out.LeftRight = r.leftRight;
				out.UpDown = r.upDown;
				out.Keys = r.keys;
				out.Rotation = detail::toCpp(r.rotation);
				out.Position = detail::toCpp(r.position);
				out.Velocity = detail::toCpp(r.velocity);
				out.Health = r.health;
				out.PlayerHealthArmour = detail::toCpp(r.playerHealthArmour);
				out.AdditionalKeyWeapon = r.additionalKeyWeapon;
				out.Siren = r.siren;
				out.LandingGear = r.landingGear;
				out.TrailerID = r.trailerID;
				out.HydraThrustAngle = r.hydraThrustAngle;
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerPassengerSync& out)
	{
		return detail::decode(bs, omp_netcode_read_passenger_sync, [&out](const OmpNetcodePassengerSync& r)
			{
				out.VehicleID = r.vehicleID;
				out.DriveBySeatAdditionalKeyWeapon = r.driveBySeatAdditionalKeyWeapon;
				out.Keys = r.keys;
				out.HealthArmour = detail::toCpp(r.healthArmour);
				out.LeftRight = r.leftRight;
				out.UpDown = r.upDown;
				out.Position = detail::toCpp(r.position);
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerUnoccupiedSync& out)
	{
		return detail::decode(bs, omp_netcode_read_unoccupied_sync, [&out](const OmpNetcodeUnoccupiedSync& r)
			{
				out.VehicleID = r.vehicleID;
				out.SeatID = r.seatID;
				out.Roll = detail::toCpp(r.roll);
				out.Rotation = detail::toCpp(r.rotation);
				out.Position = detail::toCpp(r.position);
				out.Velocity = detail::toCpp(r.velocity);
				out.AngularVelocity = detail::toCpp(r.angularVelocity);
				out.Health = r.health;
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerTrailerSync& out)
	{
		return detail::decode(bs, omp_netcode_read_trailer_sync, [&out](const OmpNetcodeTrailerSync& r)
			{
				out.VehicleID = r.vehicleID;
				out.Position = detail::toCpp(r.position);
				out.Quat = detail::toCpp(r.quat);
				out.Velocity = detail::toCpp(r.velocity);
				out.TurnVelocity = detail::toCpp(r.turnVelocity);
			});
	}
}
}
