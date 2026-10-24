/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

// C ABI of the Rust packet decoders in `Shared/rust/netcode/src/ffi.rs`; keep the two in sync.
//
// Decoders take a `NetworkBitStream`'s buffer, its size in bits and the current read offset in
// bits. On success they fill `out`, advance `*offset` past what they read and return true. On
// failure `out` and `*offset` are left alone.

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
