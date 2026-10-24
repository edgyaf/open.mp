/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "textlabels_rs.hpp"
#include <Impl/pool_impl.hpp>
#include <Server/Components/TextLabels/textlabels.hpp>
#include <Server/Components/Vehicles/vehicles.hpp>
#include <netcode.hpp>
#include <sdk.hpp>

using namespace Impl;

// Label properties and the streaming decision live in Rust (see `rust/`). The attachment data stays
// here because the SDK hands it out by reference.

inline OmpTextLabelsVec3 toRust(const Vector3& v)
{
	return OmpTextLabelsVec3 { v.x, v.y, v.z };
}

inline Vector3 fromRust(const OmpTextLabelsVec3& v)
{
	return Vector3(v.x, v.y, v.z);
}

inline OmpTextLabelsColour toRust(const Colour& colour)
{
	return OmpTextLabelsColour { colour.r, colour.g, colour.b, colour.a };
}

inline Colour fromRust(const OmpTextLabelsColour& colour)
{
	return Colour(colour.r, colour.g, colour.b, colour.a);
}

template <class T>
class TextLabelBase : public T, public PoolIDProvider, public NoCopy
{
protected:
	OmpTextLabelsLabel* const state_;

private:
	TextLabelAttachmentData attachmentData;

public:
	TextLabelBase(StringView text, Colour colour, Vector3 pos, float drawDist, int vw, bool testLOS)
		: state_(omp_textlabels_label_new(text.data(), text.length(), toRust(colour), toRust(pos), drawDist, vw, testLOS))
	{
	}

	~TextLabelBase()
	{
		omp_textlabels_label_free(state_);
	}

	virtual void restream() = 0;

	int getID() const override
	{
		return poolID;
	}

	Vector3 getPosition() const override
	{
		return fromRust(omp_textlabels_label_get_position(state_));
	}

	void setPosition(Vector3 position) override
	{
		omp_textlabels_label_set_position(state_, toRust(position));
		restream();
	}

	GTAQuat getRotation() const override
	{
		// Maybe make this a static shared instance?
		return GTAQuat();
	}

	void setRotation(GTAQuat rotation) override { }

	void setText(StringView txt) override
	{
		omp_textlabels_label_set_text(state_, txt.data(), txt.length());
		restream();
	}

	StringView getText() const override
	{
		const char* ptr;
		size_t len;
		omp_textlabels_label_get_text(state_, &ptr, &len);
		return StringView(ptr, len);
	}

	void setColour(Colour col) override
	{
		omp_textlabels_label_set_colour(state_, toRust(col));
		restream();
	}

	Colour getColour() const override
	{
		return fromRust(omp_textlabels_label_get_colour(state_));
	}

	void setDrawDistance(float dist) override
	{
		omp_textlabels_label_set_draw_distance(state_, dist);
		restream();
	}

	float getDrawDistance() override
	{
		return omp_textlabels_label_get_draw_distance(state_);
	}

	void attachToPlayer(IPlayer& player, Vector3 offset) override
	{
		omp_textlabels_label_set_position(state_, toRust(offset));
		attachmentData.playerID = player.getID();
		restream();
	}

	void attachToVehicle(IVehicle& vehicle, Vector3 offset) override
	{
		omp_textlabels_label_set_position(state_, toRust(offset));
		attachmentData.vehicleID = vehicle.getID();
		restream();
	}

	const TextLabelAttachmentData& getAttachmentData() const override
	{
		return attachmentData;
	}

	void setTestLOS(bool status) override
	{
		omp_textlabels_label_set_test_los(state_, status);
		restream();
	}

	bool getTestLOS() const override
	{
		return omp_textlabels_label_get_test_los(state_);
	}

	void detachFromPlayer(Vector3 position) override
	{
		omp_textlabels_label_set_position(state_, toRust(position));
		attachmentData.playerID = INVALID_PLAYER_ID;
		restream();
	}

	void detachFromVehicle(Vector3 position) override
	{
		omp_textlabels_label_set_position(state_, toRust(position));
		attachmentData.vehicleID = INVALID_VEHICLE_ID;
		restream();
	}

	void streamInForClient(IPlayer& player, bool isPlayerTextLabel)
	{
		NetCode::RPC::PlayerShowTextLabel showTextLabelRPC;
		showTextLabelRPC.PlayerTextLabel = isPlayerTextLabel;
		showTextLabelRPC.TextLabelID = poolID;
		showTextLabelRPC.Col = getColour();
		showTextLabelRPC.Position = getPosition();
		showTextLabelRPC.DrawDistance = omp_textlabels_label_get_draw_distance(state_);
		showTextLabelRPC.LOS = getTestLOS();
		showTextLabelRPC.PlayerAttachID = attachmentData.playerID;
		showTextLabelRPC.VehicleAttachID = attachmentData.vehicleID;
		showTextLabelRPC.Text = getText();
		PacketHelper::send(showTextLabelRPC, player);
	}

	void streamOutForClient(IPlayer& player, bool isPlayerTextLabel)
	{
		NetCode::RPC::PlayerHideTextLabel hideTextLabelRPC;
		hideTextLabelRPC.PlayerTextLabel = isPlayerTextLabel;
		hideTextLabelRPC.TextLabelID = poolID;
		PacketHelper::send(hideTextLabelRPC, player);
	}

	void setColourAndText(Colour col, StringView txt) override
	{
		omp_textlabels_label_set_colour(state_, toRust(col));
		omp_textlabels_label_set_text(state_, txt.data(), txt.length());
		restream();
	}
};

class TextLabel final : public TextLabelBase<ITextLabel>
{
private:
	UniqueIDArray<IPlayer, PLAYER_POOL_SIZE> streamedFor_;

public:
	void removeFor(int pid, IPlayer& player)
	{
		if (streamedFor_.valid(pid))
		{
			streamedFor_.remove(pid, player);
		}
	}

	TextLabel(StringView text, Colour colour, Vector3 pos, float drawDist, int vw, bool los)
		: TextLabelBase(text, colour, pos, drawDist, vw, los)
	{
	}

	/// Whether a player in this virtual world can see the label, if it isn't attached to anything.
	bool worldMatches(int vw) const
	{
		return omp_textlabels_label_world_matches(state_, vw);
	}

	void restream() override
	{
		for (IPlayer* player : streamedFor_.entries())
		{
			streamOutForClient(*player, false);
			streamInForClient(*player, false);
		}
	}

	bool isStreamedInForPlayer(const IPlayer& player) const override
	{
		return streamedFor_.valid(player.getID());
	}

	void streamInForPlayer(IPlayer& player) override
	{
		streamedFor_.add(player.getID(), player);
		streamInForClient(player, false);
	}

	void streamOutForPlayer(IPlayer& player) override
	{
		streamedFor_.remove(player.getID(), player);
		streamOutForClient(player, false);
	}

	int getVirtualWorld() const override
	{
		return omp_textlabels_label_get_virtual_world(state_);
	}

	void setVirtualWorld(int vw) override
	{
		omp_textlabels_label_set_virtual_world(state_, vw);
		restream();
	}

	~TextLabel()
	{
	}

	void destream()
	{
		for (IPlayer* player : streamedFor_.entries())
		{
			streamOutForClient(*player, false);
		}
	}
};

class PlayerTextLabel final : public TextLabelBase<IPlayerTextLabel>
{
private:
	IPlayer& player;

public:
	PlayerTextLabel(IPlayer& player, StringView text, Colour colour, Vector3 pos, float drawDist, bool testLOS)
		: TextLabelBase(text, colour, pos, drawDist, 0, testLOS)
		, player(player)
	{
	}

	void restream() override
	{
		streamOutForClient(player, true);
		streamInForClient(player, true);
	}

	int getVirtualWorld() const override
	{
		return 0;
	}

	void setVirtualWorld(int vw) override
	{
	}

	~PlayerTextLabel()
	{
	}

	void destream()
	{
		streamOutForClient(player, true);
	}
};
