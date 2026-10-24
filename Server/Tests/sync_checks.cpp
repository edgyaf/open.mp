/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

// Differential tests for the sync data checks PlayerPool now takes from Rust: each must agree with
// the C++ expression it replaced, copied below from `Server/Source/player_pool.hpp` (as of
// cae4a7f4). Byte inputs are checked exhaustively, vectors with random and edge-case values.
//
// Floating-point results must match bit for bit, except where the compiler may fuse multiply-adds
// in the C++ (`__FP_FAST_FMAF`, e.g. ARM), which the Rust never does; there one ulp is allowed.

#include "harness.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <netcode_rs.hpp>
#include <random>

namespace
{

namespace Original
{
	int8_t weaponSlot(uint8_t weapon)
	{
		return WeaponSlotData(weapon).slot();
	}

	bool weaponShootable(uint8_t weapon)
	{
		return WeaponSlotData { weapon }.shootable();
	}

	uint32_t keysWithAdditionalKey(uint16_t keys, uint8_t additionalKey)
	{
		uint32_t newKeys = keys;
		switch (additionalKey)
		{
		case 1:
			newKeys |= Key::YES;
			break;
		case 2:
			newKeys |= Key::NO;
			break;
		case 3:
			newKeys |= Key::CTRL_BACK;
			break;
		}
		return newKeys;
	}

	bool vehicleSurfingOffsetOk(Vector3 offset)
	{
		// Fix for old 'Invisible' cheat.
		if (glm::dot(offset, offset) >= 50.0f * 50.0f)
		{
			return false;
		}
		return true;
	}

	uint16_t footKeysForBroadcast(uint16_t keys, uint32_t armedWeapon)
	{
		NetCode::Packet::PlayerFootSync footSync;
		footSync.Keys = keys;
		// Fix Night Vision & Thermal Goggles visual effect show for all streamed players.
		if ((armedWeapon == PlayerWeapon_Night_Vis_Goggles || armedWeapon == PlayerWeapon_Thermal_Goggles))
		{
			footSync.Keys &= ~Key::FIRE;
		}

		// Fix detonator crasher.
		if (armedWeapon == PlayerWeapon_Bomb)
		{
			footSync.Keys &= ~Key::AIM;
		}
		return footSync.Keys;
	}

	bool aimFrontVectorOk(Vector3 camFrontVector)
	{
		const float frontvec = glm::dot(camFrontVector, camFrontVector);
		return frontvec > 0.0 && frontvec < 1.5;
	}

	uint8_t aimCamMode(uint8_t camMode)
	{
		NetCode::Packet::PlayerAimSync aimSync;
		aimSync.CamMode = camMode;
		if (aimSync.CamMode < 3u || aimSync.CamMode == 5u || aimSync.CamMode == 6u
			|| (aimSync.CamMode >= 9u && aimSync.CamMode <= 13u) || aimSync.CamMode == 17u
			|| (aimSync.CamMode >= 19u && aimSync.CamMode <= 21u)
			|| (aimSync.CamMode >= 23u && aimSync.CamMode <= 28u)
			|| (aimSync.CamMode >= 30u && aimSync.CamMode <= 45u)
			|| (aimSync.CamMode >= 48u && aimSync.CamMode <= 50u)
			|| aimSync.CamMode == 52u || aimSync.CamMode == 54u
			|| aimSync.CamMode == 60u || aimSync.CamMode == 61u || aimSync.CamMode > 64u)
		{
			aimSync.CamMode = 4u;
		}
		return aimSync.CamMode;
	}

	float aimCamZoom(uint8_t zoom)
	{
		NetCode::Packet::PlayerAimSync aimSync;
		aimSync.CamZoom = zoom;
		return aimSync.CamZoom * 0.015873017f * 35.f + 35.f;
	}

	float aimAspectRatio(uint8_t aspectRatio)
	{
		NetCode::Packet::PlayerAimSync aimSync;
		aimSync.AspectRatio = aspectRatio;
		return (aimSync.AspectRatio * 1.f / 255) + 1.f;
	}

	bool bulletInBounds(uint8_t hitType, Vector3 origin, Vector3 offset)
	{
		static const float bounds = 20000.0f * 20000.0f;
		if (glm::dot(origin, origin) > bounds)
		{
			return false; // OOB origin
		}
		if (hitType == PlayerBulletHitType_None && glm::dot(offset, offset) > bounds)
		{
			return false; // OOB shot
		}
		else if (hitType != PlayerBulletHitType_None && glm::dot(offset, offset) > 1000.0f * 1000.0f)
		{
			return false; // OOB shot
		}
		return true;
	}

	bool unoccupiedMotionOk(Vector3 angularVelocity, Vector3 roll, Vector3 rotation)
	{
		if (angularVelocity.x < -1.0f || angularVelocity.x > 1.0f
			|| angularVelocity.y < -1.0f || angularVelocity.y > 1.0f
			|| angularVelocity.z < -1.0f || angularVelocity.z > 1.0f)
		{
			return false;
		}

		if (glm::abs(1.0 - glm::length(roll)) >= 0.000001
			|| glm::abs(1.0 - glm::length(rotation)) >= 0.000001
			|| glm::abs(glm::dot(roll, rotation)) >= 0.000001)
		{
			return false;
		}
		return true;
	}

	bool trailerTurnVelocityOk(Vector3 turnVelocity)
	{
		if (turnVelocity.x < -1.0f || turnVelocity.x > 1.0f
			|| turnVelocity.y < -1.0f || turnVelocity.y > 1.0f
			|| turnVelocity.z < -1.0f || turnVelocity.z > 1.0f)
		{
			return false;
		}
		return true;
	}

	Vector4 trailerQuatNormalised(Vector4 quat)
	{
		// Normalise quaternions
		float magnitude = glm::length(quat);
		if (std::abs(1.0f - magnitude) >= 0.000001f)
		{
			if (magnitude < 0.1f)
			{
				quat = glm::vec4(0.5f);
			}
			else
			{
				quat /= magnitude;
			}
		}
		return quat;
	}
}

uint32_t floatBits(float f)
{
	uint32_t bits;
	std::memcpy(&bits, &f, sizeof(bits));
	return bits;
}

/// Bit for bit, or (where the C++ may have fused multiply-adds) within one ulp.
bool sameFloat(float a, float b)
{
	if (floatBits(a) == floatBits(b))
	{
		return true;
	}
#ifdef __FP_FAST_FMAF
	return std::nextafter(a, b) == b;
#else
	return false;
#endif
}

struct Gen
{
	std::mt19937 gen { 0x636b73 };

	bool chance(int percent)
	{
		return std::uniform_int_distribution<int>(0, 99)(gen) < percent;
	}

	/// Around `scale`, often exactly on or next to an edge the checks care about.
	float value(float scale)
	{
		static const float edges[] = {
			0.0f,
			-0.0f,
			1.0f,
			-1.0f,
			0.1f,
			0.5f,
			1.5f,
			50.0f,
			1000.0f,
			20000.0f,
			std::numeric_limits<float>::quiet_NaN(),
			std::numeric_limits<float>::infinity(),
			-std::numeric_limits<float>::infinity(),
			std::numeric_limits<float>::denorm_min(),
			std::numeric_limits<float>::max(),
		};
		if (chance(15))
		{
			float edge = edges[std::uniform_int_distribution<int>(0, int(sizeof(edges) / sizeof(*edges)) - 1)(gen)];
			if (chance(30))
			{
				edge = std::nextafter(edge, chance(50) ? INFINITY : -INFINITY);
			}
			return chance(50) ? edge : -edge;
		}
		return std::uniform_real_distribution<float>(-scale, scale)(gen);
	}

	Vector3 vec3(float scale)
	{
		return Vector3(value(scale), value(scale), value(scale));
	}

	/// A unit vector, perhaps nudged off unit length.
	Vector3 unit()
	{
		Vector3 v = glm::normalize(Vector3(value(1.0f), value(1.0f), value(1.0f)));
		if (chance(30))
		{
			v.x = std::nextafter(v.x, chance(50) ? INFINITY : -INFINITY);
		}
		return v;
	}

	Vector4 vec4(float scale)
	{
		Vector4 v(value(scale), value(scale), value(scale), value(scale));
		if (chance(30))
		{
			v = glm::normalize(v);
		}
		return v;
	}
};

const int Samples = 1000000;

}

TEST_CASE(sync_checks_byte_inputs_match_cpp)
{
	int mismatches = 0;
	for (int i = 0; i < 256; ++i)
	{
		const uint8_t b = uint8_t(i);
		mismatches += NetCode::Rust::weaponSlot(b) != Original::weaponSlot(b);
		mismatches += NetCode::Rust::weaponShootable(b) != Original::weaponShootable(b);
		mismatches += NetCode::Rust::aimCamMode(b) != Original::aimCamMode(b);
		mismatches += !sameFloat(NetCode::Rust::aimAspectRatio(b), Original::aimAspectRatio(b));
		if (b < 64)
		{
			mismatches += !sameFloat(NetCode::Rust::aimCamZoom(b), Original::aimCamZoom(b));
		}
		for (uint8_t additional = 0; additional < 4; ++additional)
		{
			for (uint16_t keys : { uint16_t(0), uint16_t(b), uint16_t(b << 8 | b), uint16_t(0xFFFF) })
			{
				mismatches += NetCode::Rust::keysWithAdditionalKey(keys, additional) != Original::keysWithAdditionalKey(keys, additional);
			}
		}
		for (uint16_t keys : { uint16_t(0), uint16_t(0xFFFF), uint16_t(b * 257) })
		{
			mismatches += NetCode::Rust::footKeysForBroadcast(keys, b) != Original::footKeysForBroadcast(keys, b);
		}
	}
	CHECK(mismatches == 0);
}

TEST_CASE(sync_checks_vector_inputs_match_cpp)
{
	Gen gen;
	int mismatches = 0;
	int surfingRejected = 0, aimAccepted = 0, bulletAccepted = 0, motionAccepted = 0, quatNormalised = 0;
	for (int i = 0; i < Samples; ++i)
	{
		const Vector3 offset = gen.vec3(60.0f);
		const bool surfing = NetCode::Rust::vehicleSurfingOffsetOk(offset);
		mismatches += surfing != Original::vehicleSurfingOffsetOk(offset);
		surfingRejected += !surfing;

		const Vector3 front = gen.vec3(1.0f);
		const bool aim = NetCode::Rust::aimFrontVectorOk(front);
		mismatches += aim != Original::aimFrontVectorOk(front);
		aimAccepted += aim;

		const uint8_t hitType = uint8_t(gen.chance(50) ? 0 : std::uniform_int_distribution<int>(1, 5)(gen.gen));
		const Vector3 origin = gen.vec3(25000.0f);
		const Vector3 shot = gen.vec3(hitType ? 1200.0f : 25000.0f);
		const bool bullet = NetCode::Rust::bulletInBounds(hitType, origin, shot);
		mismatches += bullet != Original::bulletInBounds(hitType, origin, shot);
		bulletAccepted += bullet;

		const Vector3 angular = gen.vec3(1.2f);
		const Vector3 roll = gen.chance(90) ? gen.unit() : gen.vec3(1.0f);
		const Vector3 rotation = gen.chance(70) ? glm::normalize(glm::cross(roll, gen.unit())) : gen.unit();
		const bool motion = NetCode::Rust::unoccupiedMotionOk(angular, roll, rotation);
		mismatches += motion != Original::unoccupiedMotionOk(angular, roll, rotation);
		motionAccepted += motion;

		const Vector3 turn = gen.vec3(1.2f);
		mismatches += NetCode::Rust::trailerTurnVelocityOk(turn) != Original::trailerTurnVelocityOk(turn);

		const Vector4 quat = gen.vec4(gen.chance(20) ? 0.1f : 2.0f);
		const Vector4 rust = NetCode::Rust::trailerQuatNormalised(quat);
		const Vector4 cpp = Original::trailerQuatNormalised(quat);
		const bool sameQuat = sameFloat(rust.x, cpp.x) && sameFloat(rust.y, cpp.y) && sameFloat(rust.z, cpp.z) && sameFloat(rust.w, cpp.w);
		mismatches += !sameQuat;
		quatNormalised += floatBits(cpp.x) != floatBits(quat.x);
	}
	CHECK(mismatches == 0);
	// Make sure both outcomes of each check came up.
	CHECK(surfingRejected > Samples / 100 && surfingRejected < Samples * 99 / 100);
	CHECK(aimAccepted > Samples / 100 && aimAccepted < Samples * 99 / 100);
	CHECK(bulletAccepted > Samples / 100 && bulletAccepted < Samples * 99 / 100);
	CHECK(motionAccepted > Samples / 100 && motionAccepted < Samples * 99 / 100);
	CHECK(quatNormalised > Samples / 100);
}

TEST_CASE(sync_checks_edge_cases_match_cpp)
{
	// Roll and rotation whose dot product is exactly 1e-6f, just under the double 1e-6 the C++
	// compares against: accepted, but only if the comparison is done in double.
	const Vector3 roll(1.0f, 0.0f, 0.0f);
	const Vector3 rotation(1e-6f, 1.0f, 0.0f);
	CHECK(Original::unoccupiedMotionOk(Vector3(0.0f), roll, rotation));
	CHECK(NetCode::Rust::unoccupiedMotionOk(Vector3(0.0f), roll, rotation));

	// A surfing offset exactly 50 away is the cheat; just under isn't.
	CHECK(!NetCode::Rust::vehicleSurfingOffsetOk(Vector3(30.0f, 40.0f, 0.0f)));
	CHECK(NetCode::Rust::vehicleSurfingOffsetOk(Vector3(30.0f, std::nextafter(40.0f, 0.0f), 0.0f)));
}
