/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

// Rust decoders for packets and RPCs sent by clients (`Shared/rust/netcode`), and
// `NetCode::Rust::read` overloads that use them in place of their own `read` methods. Also the
// checks PlayerPool applies to decoded sync data that don't depend on server state.
//
// The C ABI below mirrors `Shared/rust/netcode/src/ffi.rs`; keep the two in sync. Decoders take a
// `NetworkBitStream`'s buffer, its size in bits and the current read offset in bits. On success
// they fill `out`, advance `*offset` past what they read and return true. On failure `out` and
// `*offset` are left alone.

#include "netcode.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

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

	struct OmpNetcodeClickMap
	{
		OmpNetcodeVec3 pos;
	};

	struct OmpNetcodeClickPlayer
	{
		int32_t playerID;
		uint8_t source;
	};

	struct OmpNetcodeGiveTakeDamage
	{
		bool taking;
		int32_t playerID;
		float damage;
		uint32_t weaponID;
		uint32_t bodypart;
	};

	struct OmpNetcodeInteriorChange
	{
		uint32_t interior;
	};

	struct OmpNetcodeDeath
	{
		uint8_t reason;
		uint16_t killerID;
	};

	struct OmpNetcodeCameraTarget
	{
		int32_t objectID;
		int32_t vehicleID;
		int32_t playerID;
		int32_t actorID;
	};

	struct OmpNetcodeClientCheckResponse
	{
		int32_t type;
		int32_t address;
		int32_t results;
	};

	/// Raw bytes, which may include NULs.
	struct OmpNetcodeMessage
	{
		uint32_t len;
		uint8_t data[256];
	};

	struct OmpNetcodeChatMessage
	{
		OmpNetcodeMessage message;
	};

	struct OmpNetcodeCommandMessage
	{
		OmpNetcodeMessage message;
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

	bool omp_netcode_read_click_map(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeClickMap* out);
	bool omp_netcode_read_click_player(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeClickPlayer* out);
	bool omp_netcode_read_give_take_damage(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeGiveTakeDamage* out);
	bool omp_netcode_read_interior_change(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeInteriorChange* out);
	bool omp_netcode_read_death(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeDeath* out);
	bool omp_netcode_read_camera_target(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeCameraTarget* out);
	bool omp_netcode_read_client_check_response(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeClientCheckResponse* out);
	struct OmpNetcodeRequestClass
	{
		int32_t classID;
	};

	struct OmpNetcodeDialogResponse
	{
		int32_t id;
		uint8_t response;
		int32_t listItem;
		OmpNetcodeMessage text;
	};

	struct OmpNetcodeSelectedMenuRow
	{
		uint8_t row;
	};

	struct OmpNetcodeSelectObject
	{
		int32_t selectType;
		int32_t objectID;
		int32_t model;
		OmpNetcodeVec3 position;
	};

	struct OmpNetcodeEditObject
	{
		bool playerObject;
		int32_t objectID;
		int32_t response;
		OmpNetcodeVec3 offset;
		OmpNetcodeVec3 rotation;
	};

	struct OmpNetcodeEditAttachedObject
	{
		uint32_t response;
		uint32_t index;
		int32_t model;
		int32_t bone;
		OmpNetcodeVec3 offset;
		OmpNetcodeVec3 rotation;
		OmpNetcodeVec3 scale;
		/// ARGB, as sent.
		uint32_t colour1;
		uint32_t colour2;
	};

	struct OmpNetcodePickUpPickup
	{
		int32_t pickupID;
	};

	struct OmpNetcodeSelectTextDraw
	{
		bool playerTextDraw;
		bool invalid;
		int32_t textDrawID;
	};

	struct OmpNetcodeEnterVehicle
	{
		int32_t vehicleID;
		uint8_t passenger;
	};

	struct OmpNetcodeVehicleID
	{
		int32_t vehicleID;
	};

	struct OmpNetcodeVehicleDamageStatus
	{
		int32_t vehicleID;
		uint32_t doorStatus;
		uint32_t panelStatus;
		uint8_t lightStatus;
		uint8_t tyreStatus;
	};

	struct OmpNetcodeScmEvent
	{
		int32_t vehicleID;
		uint32_t arg1;
		uint32_t arg2;
		uint32_t eventType;
	};

	/// Returns a buffer of at least `len` bytes for a decoded string, or null to fail the decode.
	typedef uint8_t* (*OmpNetcodeReserveString)(void* context, size_t len);

	bool omp_netcode_read_chat_message(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeChatMessage* out);
	bool omp_netcode_read_command_message(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeCommandMessage* out);

	bool omp_netcode_read_damage_actor(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeGiveTakeDamage* out);
	bool omp_netcode_read_request_class(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeRequestClass* out);
	bool omp_netcode_read_dialog_response(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeDialogResponse* out);
	bool omp_netcode_read_selected_menu_row(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeSelectedMenuRow* out);
	bool omp_netcode_read_select_object(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeSelectObject* out);
	bool omp_netcode_read_edit_object(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeEditObject* out);
	bool omp_netcode_read_edit_attached_object(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeEditAttachedObject* out);
	bool omp_netcode_read_pick_up_pickup(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodePickUpPickup* out);
	bool omp_netcode_read_select_textdraw(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeSelectTextDraw* out);
	bool omp_netcode_read_enter_vehicle(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeEnterVehicle* out);
	bool omp_netcode_read_vehicle_id(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeVehicleID* out);
	bool omp_netcode_read_vehicle_damage_status(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeVehicleDamageStatus* out);
	bool omp_netcode_read_scm_event(const uint8_t* data, size_t bits, size_t* offset, OmpNetcodeScmEvent* out);
	bool omp_netcode_read_rcon_command(const uint8_t* data, size_t bits, size_t* offset, void* context, OmpNetcodeReserveString reserve);

	int8_t omp_netcode_weapon_slot(uint8_t weapon);
	bool omp_netcode_weapon_shootable(uint8_t weapon);
	uint32_t omp_netcode_keys_with_additional_key(uint16_t keys, uint8_t additionalKey);
	bool omp_netcode_vehicle_surfing_offset_ok(OmpNetcodeVec3 offset);
	uint16_t omp_netcode_foot_keys_for_broadcast(uint16_t keys, uint32_t armedWeapon);
	bool omp_netcode_aim_front_vector_ok(OmpNetcodeVec3 front);
	uint8_t omp_netcode_aim_cam_mode(uint8_t mode);
	float omp_netcode_aim_cam_zoom(uint8_t zoom);
	float omp_netcode_aim_aspect_ratio(uint8_t aspectRatio);
	bool omp_netcode_bullet_in_bounds(uint8_t hitType, OmpNetcodeVec3 origin, OmpNetcodeVec3 offset);
	bool omp_netcode_unoccupied_motion_ok(OmpNetcodeVec3 angularVelocity, OmpNetcodeVec3 roll, OmpNetcodeVec3 rotation);
	bool omp_netcode_trailer_turn_velocity_ok(OmpNetcodeVec3 turnVelocity);
	OmpNetcodeVec4 omp_netcode_trailer_quat_normalised(OmpNetcodeVec4 quat);
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
static_assert(sizeof(OmpNetcodeClickMap) == 12, "OmpNetcodeClickMap layout differs from Rust");
static_assert(sizeof(OmpNetcodeClickPlayer) == 8, "OmpNetcodeClickPlayer layout differs from Rust");
static_assert(sizeof(OmpNetcodeGiveTakeDamage) == 20, "OmpNetcodeGiveTakeDamage layout differs from Rust");
static_assert(sizeof(OmpNetcodeInteriorChange) == 4, "OmpNetcodeInteriorChange layout differs from Rust");
static_assert(sizeof(OmpNetcodeDeath) == 4, "OmpNetcodeDeath layout differs from Rust");
static_assert(sizeof(OmpNetcodeCameraTarget) == 16, "OmpNetcodeCameraTarget layout differs from Rust");
static_assert(sizeof(OmpNetcodeClientCheckResponse) == 12, "OmpNetcodeClientCheckResponse layout differs from Rust");
static_assert(sizeof(OmpNetcodeChatMessage) == 260, "OmpNetcodeChatMessage layout differs from Rust");
static_assert(sizeof(OmpNetcodeCommandMessage) == 260, "OmpNetcodeCommandMessage layout differs from Rust");
static_assert(sizeof(OmpNetcodeRequestClass) == 4, "OmpNetcodeRequestClass layout differs from Rust");
static_assert(sizeof(OmpNetcodeDialogResponse) == 272, "OmpNetcodeDialogResponse layout differs from Rust");
static_assert(sizeof(OmpNetcodeSelectedMenuRow) == 1, "OmpNetcodeSelectedMenuRow layout differs from Rust");
static_assert(sizeof(OmpNetcodeSelectObject) == 24, "OmpNetcodeSelectObject layout differs from Rust");
static_assert(sizeof(OmpNetcodeEditObject) == 36, "OmpNetcodeEditObject layout differs from Rust");
static_assert(sizeof(OmpNetcodeEditAttachedObject) == 60, "OmpNetcodeEditAttachedObject layout differs from Rust");
static_assert(sizeof(OmpNetcodePickUpPickup) == 4, "OmpNetcodePickUpPickup layout differs from Rust");
static_assert(sizeof(OmpNetcodeSelectTextDraw) == 8, "OmpNetcodeSelectTextDraw layout differs from Rust");
static_assert(sizeof(OmpNetcodeEnterVehicle) == 8, "OmpNetcodeEnterVehicle layout differs from Rust");
static_assert(sizeof(OmpNetcodeVehicleID) == 4, "OmpNetcodeVehicleID layout differs from Rust");
static_assert(sizeof(OmpNetcodeVehicleDamageStatus) == 16, "OmpNetcodeVehicleDamageStatus layout differs from Rust");
static_assert(sizeof(OmpNetcodeScmEvent) == 16, "OmpNetcodeScmEvent layout differs from Rust");

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

		inline OmpNetcodeVec3 toRust(const Vector3& v)
		{
			return OmpNetcodeVec3 { v.x, v.y, v.z };
		}

		inline OmpNetcodeVec4 toRust(const Vector4& v)
		{
			return OmpNetcodeVec4 { v.x, v.y, v.z, v.w };
		}

		template <size_t Size>
		inline void toCpp(const OmpNetcodeMessage& message, HybridString<Size>& out)
		{
			out.reserve(message.len);
			memcpy(out.data(), message.data, message.len);
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

	/// RPCs. Unlike the packets' own `read`, these reject a truncated `OnPlayerClickPlayer` or
	/// `ClientCheck` instead of leaving fields uninitialised.

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerClickMap& out)
	{
		return detail::decode(bs, omp_netcode_read_click_map, [&out](const OmpNetcodeClickMap& r)
			{
				out.Pos = detail::toCpp(r.pos);
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerClickPlayer& out)
	{
		return detail::decode(bs, omp_netcode_read_click_player, [&out](const OmpNetcodeClickPlayer& r)
			{
				out.PlayerID = r.playerID;
				out.Source = r.source;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerGiveTakeDamage& out)
	{
		return detail::decode(bs, omp_netcode_read_give_take_damage, [&out](const OmpNetcodeGiveTakeDamage& r)
			{
				out.Taking = r.taking;
				out.PlayerID = r.playerID;
				out.Damage = r.damage;
				out.WeaponID = r.weaponID;
				out.Bodypart = r.bodypart;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerInteriorChange& out)
	{
		return detail::decode(bs, omp_netcode_read_interior_change, [&out](const OmpNetcodeInteriorChange& r)
			{
				out.Interior = r.interior;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerDeath& out)
	{
		return detail::decode(bs, omp_netcode_read_death, [&out](const OmpNetcodeDeath& r)
			{
				out.Reason = r.reason;
				out.KillerID = r.killerID;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerCameraTarget& out)
	{
		return detail::decode(bs, omp_netcode_read_camera_target, [&out](const OmpNetcodeCameraTarget& r)
			{
				out.TargetObjectID = r.objectID;
				out.TargetVehicleID = r.vehicleID;
				out.TargetPlayerID = r.playerID;
				out.TargetActorID = r.actorID;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::ClientCheck& out)
	{
		return detail::decode(bs, omp_netcode_read_client_check_response, [&out](const OmpNetcodeClientCheckResponse& r)
			{
				out.Type = r.type;
				out.Address = r.address;
				out.Results = r.results;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::PlayerRequestChatMessage& out)
	{
		return detail::decode(bs, omp_netcode_read_chat_message, [&out](const OmpNetcodeChatMessage& r)
			{
				detail::toCpp(r.message, out.message);
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::PlayerRequestCommandMessage& out)
	{
		return detail::decode(bs, omp_netcode_read_command_message, [&out](const OmpNetcodeCommandMessage& r)
			{
				detail::toCpp(r.message, out.message);
			});
	}

	/// RPCs and packets handled by components. Truncated `OnPlayerEnterVehicle`,
	/// `SetVehicleDamageStatus`, `SCMEvent` and `OnPlayerDialogResponse` RPCs are rejected rather
	/// than read with fields left uninitialised.

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerDamageActor& out)
	{
		return detail::decode(bs, omp_netcode_read_damage_actor, [&out](const OmpNetcodeGiveTakeDamage& r)
			{
				out.Unknown = r.taking;
				out.ActorID = r.playerID;
				out.Damage = r.damage;
				out.WeaponID = r.weaponID;
				out.Bodypart = r.bodypart;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::PlayerRequestClass& out)
	{
		return detail::decode(bs, omp_netcode_read_request_class, [&out](const OmpNetcodeRequestClass& r)
			{
				out.Classid = r.classID;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerDialogResponse& out)
	{
		return detail::decode(bs, omp_netcode_read_dialog_response, [&out](const OmpNetcodeDialogResponse& r)
			{
				out.ID = r.id;
				out.Response = r.response;
				out.ListItem = r.listItem;
				detail::toCpp(r.text, out.Text);
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerSelectedMenuRow& out)
	{
		return detail::decode(bs, omp_netcode_read_selected_menu_row, [&out](const OmpNetcodeSelectedMenuRow& r)
			{
				out.MenuRow = r.row;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerSelectObject& out)
	{
		return detail::decode(bs, omp_netcode_read_select_object, [&out](const OmpNetcodeSelectObject& r)
			{
				out.SelectType = r.selectType;
				out.ObjectID = r.objectID;
				out.Model = r.model;
				out.Position = detail::toCpp(r.position);
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerEditObject& out)
	{
		return detail::decode(bs, omp_netcode_read_edit_object, [&out](const OmpNetcodeEditObject& r)
			{
				out.PlayerObject = r.playerObject;
				out.ObjectID = r.objectID;
				out.Response = r.response;
				out.Offset = detail::toCpp(r.offset);
				out.Rotation = detail::toCpp(r.rotation);
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerEditAttachedObject& out)
	{
		return detail::decode(bs, omp_netcode_read_edit_attached_object, [&out](const OmpNetcodeEditAttachedObject& r)
			{
				out.Response = r.response;
				out.Index = r.index;
				out.AttachmentData.model = r.model;
				out.AttachmentData.bone = r.bone;
				out.AttachmentData.offset = detail::toCpp(r.offset);
				out.AttachmentData.rotation = detail::toCpp(r.rotation);
				out.AttachmentData.scale = detail::toCpp(r.scale);
				out.AttachmentData.colour1 = Colour::FromARGB(r.colour1);
				out.AttachmentData.colour2 = Colour::FromARGB(r.colour2);
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerPickUpPickup& out)
	{
		return detail::decode(bs, omp_netcode_read_pick_up_pickup, [&out](const OmpNetcodePickUpPickup& r)
			{
				out.PickupID = r.pickupID;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerSelectTextDraw& out)
	{
		return detail::decode(bs, omp_netcode_read_select_textdraw, [&out](const OmpNetcodeSelectTextDraw& r)
			{
				out.PlayerTextDraw = r.playerTextDraw;
				out.Invalid = r.invalid;
				out.TextDrawID = r.textDrawID;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerEnterVehicle& out)
	{
		return detail::decode(bs, omp_netcode_read_enter_vehicle, [&out](const OmpNetcodeEnterVehicle& r)
			{
				out.VehicleID = r.vehicleID;
				out.Passenger = r.passenger;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::OnPlayerExitVehicle& out)
	{
		return detail::decode(bs, omp_netcode_read_vehicle_id, [&out](const OmpNetcodeVehicleID& r)
			{
				out.VehicleID = r.vehicleID;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::VehicleDeath& out)
	{
		return detail::decode(bs, omp_netcode_read_vehicle_id, [&out](const OmpNetcodeVehicleID& r)
			{
				out.VehicleID = r.vehicleID;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::SetVehicleDamageStatus& out)
	{
		return detail::decode(bs, omp_netcode_read_vehicle_damage_status, [&out](const OmpNetcodeVehicleDamageStatus& r)
			{
				out.VehicleID = r.vehicleID;
				out.DoorStatus = r.doorStatus;
				out.PanelStatus = r.panelStatus;
				out.LightStatus = r.lightStatus;
				out.TyreStatus = r.tyreStatus;
			});
	}

	inline bool read(NetworkBitStream& bs, RPC::SCMEvent& out)
	{
		return detail::decode(bs, omp_netcode_read_scm_event, [&out](const OmpNetcodeScmEvent& r)
			{
				out.VehicleID = r.vehicleID;
				out.Arg1 = r.arg1;
				out.Arg2 = r.arg2;
				out.EventType = r.eventType;
			});
	}

	inline bool read(NetworkBitStream& bs, Packet::PlayerRconCommand& out)
	{
		size_t offset = size_t(bs.GetReadOffset());
		const auto reserve = [](void* context, size_t len)
		{
			HybridString<64>& cmd = *static_cast<HybridString<64>*>(context);
			cmd.reserve(len);
			return reinterpret_cast<uint8_t*>(cmd.data());
		};
		if (!omp_netcode_read_rcon_command(bs.GetData(), size_t(bs.GetNumberOfBitsUsed()), &offset, &out.cmd, reserve))
		{
			return false;
		}
		bs.SetReadOffset(int(offset));
		return true;
	}

	/// Checks and fix-ups for decoded sync data (`checks.rs`).

	/// `WeaponSlotData::slot`.
	inline int8_t weaponSlot(uint8_t weapon)
	{
		return omp_netcode_weapon_slot(weapon);
	}

	/// `WeaponSlotData::shootable`.
	inline bool weaponShootable(uint8_t weapon)
	{
		return omp_netcode_weapon_shootable(weapon);
	}

	/// A sync packet's keys plus the extra key packed beside its weapon.
	inline uint32_t keysWithAdditionalKey(uint16_t keys, uint8_t additionalKey)
	{
		return omp_netcode_keys_with_additional_key(keys, additionalKey);
	}

	/// Whether to believe a player is surfing a vehicle at this offset (a far one is the old
	/// "invisible" cheat).
	inline bool vehicleSurfingOffsetOk(const Vector3& offset)
	{
		return omp_netcode_vehicle_surfing_offset_ok(detail::toRust(offset));
	}

	/// The keys to send other players for someone on foot holding `armedWeapon` (no firing goggles,
	/// no aiming detonators).
	inline uint16_t footKeysForBroadcast(uint16_t keys, uint32_t armedWeapon)
	{
		return omp_netcode_foot_keys_for_broadcast(keys, armedWeapon);
	}

	inline bool aimFrontVectorOk(const Vector3& front)
	{
		return omp_netcode_aim_front_vector_ok(detail::toRust(front));
	}

	/// The camera mode to keep: invalid ones become 4.
	inline uint8_t aimCamMode(uint8_t mode)
	{
		return omp_netcode_aim_cam_mode(mode);
	}

	inline float aimCamZoom(uint8_t zoom)
	{
		return omp_netcode_aim_cam_zoom(zoom);
	}

	inline float aimAspectRatio(uint8_t aspectRatio)
	{
		return omp_netcode_aim_aspect_ratio(aspectRatio);
	}

	inline bool bulletInBounds(uint8_t hitType, const Vector3& origin, const Vector3& offset)
	{
		return omp_netcode_bullet_in_bounds(hitType, detail::toRust(origin), detail::toRust(offset));
	}

	inline bool unoccupiedMotionOk(const Vector3& angularVelocity, const Vector3& roll, const Vector3& rotation)
	{
		return omp_netcode_unoccupied_motion_ok(detail::toRust(angularVelocity), detail::toRust(roll), detail::toRust(rotation));
	}

	inline bool trailerTurnVelocityOk(const Vector3& turnVelocity)
	{
		return omp_netcode_trailer_turn_velocity_ok(detail::toRust(turnVelocity));
	}

	inline Vector4 trailerQuatNormalised(const Vector4& quat)
	{
		return detail::toCpp(omp_netcode_trailer_quat_normalised(detail::toRust(quat)));
	}
}
}
