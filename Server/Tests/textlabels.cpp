/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#include "mocks.hpp"
#include <Server/Components/TextLabels/textlabels.hpp>
#include <netcode.hpp>

using namespace NetCode::RPC;

namespace
{

const Colour Red(255, 0, 0);
const Colour Blue(0, 0, 255);

const std::initializer_list<int> Restreamed = { PlayerHideTextLabel::PacketID, PlayerShowTextLabel::PacketID };

}

TEST_CASE(textlabels_pool_and_properties)
{
	ComponentFixture<ITextLabelsComponent> f("TextLabels");
	CHECK(f.component->getUID() == TextLabelsComponent_UID);

	ITextLabel* label = f.api->create("hello", Red, Vector3(1.0f, 2.0f, 3.0f), 50.0f, 7, true);
	CHECK(label != nullptr);
	CHECK(f.api->get(label->getID()) == label);
	CHECK(f.api->count() == 1);
	CHECK(label->getText() == StringView("hello"));
	CHECK(label->getColour().RGBA() == Red.RGBA());
	CHECK(label->getPosition() == Vector3(1.0f, 2.0f, 3.0f));
	CHECK(label->getDrawDistance() == 50.0f);
	CHECK(label->getVirtualWorld() == 7);
	CHECK(label->getTestLOS());
	CHECK(label->getAttachmentData().playerID == INVALID_PLAYER_ID);
	CHECK(label->getAttachmentData().vehicleID == INVALID_VEHICLE_ID);

	label->setText("a longer text that doesn't fit in the small string buffer");
	CHECK(label->getText() == StringView("a longer text that doesn't fit in the small string buffer"));
	label->setColourAndText(Blue, "bye");
	CHECK(label->getText() == StringView("bye"));
	CHECK(label->getColour().RGBA() == Blue.RGBA());
	label->setDrawDistance(10.0f);
	label->setTestLOS(false);
	label->setPosition(Vector3(4.0f, 5.0f, 6.0f));
	label->setVirtualWorld(-1);
	CHECK(label->getDrawDistance() == 10.0f);
	CHECK(!label->getTestLOS());
	CHECK(label->getPosition() == Vector3(4.0f, 5.0f, 6.0f));
	CHECK(label->getVirtualWorld() == -1);

	// Text is bytes.
	const char raw[] = { 'a', '\0', '\xe9' };
	label->setText(StringView(raw, 3));
	CHECK(label->getText() == StringView(raw, 3));

	f.api->release(label->getID());
	CHECK(f.api->count() == 0);
	f.api->create("x", Red, Vector3(0.0f), 1.0f, 0, false);
	f.component->reset();
	CHECK(f.api->count() == 0);
}

TEST_CASE(textlabels_streaming)
{
	ComponentFixture<ITextLabelsComponent> f("TextLabels");
	MockPlayer player(1, f.network);
	f.core.players.connect(player);
	player.pos = Vector3(0.0f, 0.0f, 0.0f);

	// Players already near get it on creation.
	ITextLabel* label = f.api->create("near", Red, Vector3(199.0f, 0.0f, 0.0f), 50.0f, 0, false);
	CHECK(label->isStreamedInForPlayer(player));
	CHECK(f.network.sentOnly({ PlayerShowTextLabel::PacketID }));
	ITextLabel* distant = f.api->create("far", Red, Vector3(200.0f, 0.0f, 0.0f), 50.0f, 0, false);
	CHECK(!distant->isStreamedInForPlayer(player));
	CHECK(f.network.sentOnly({}));

	// Every change restreams.
	label->setText("t");
	label->setColour(Blue);
	label->setDrawDistance(5.0f);
	label->setTestLOS(true);
	label->setPosition(Vector3(150.0f, 0.0f, 0.0f));
	label->setColourAndText(Red, "u");
	label->setVirtualWorld(0);
	CHECK(f.network.sent.size() == 14);
	f.network.sent.clear();

	// Out of range (the stream radius is exclusive).
	label->setPosition(Vector3(-200.0f, 0.0f, 0.0f));
	f.network.sent.clear();
	f.core.players.update(player);
	CHECK(!label->isStreamedInForPlayer(player));
	CHECK(f.network.sentOnly({ PlayerHideTextLabel::PacketID }));

	// Virtual worlds must match, unless the label is in all worlds (-1).
	label->setPosition(Vector3(0.0f));
	player.virtualWorld = 3;
	f.core.players.update(player);
	CHECK(!label->isStreamedInForPlayer(player));
	label->setVirtualWorld(-1);
	f.core.players.update(player);
	CHECK(label->isStreamedInForPlayer(player));

	// Players with no state don't see labels.
	player.state = PlayerState_None;
	f.core.players.update(player);
	CHECK(!label->isStreamedInForPlayer(player));
	player.state = PlayerState_OnFoot;
	f.network.sent.clear();

	// Releasing hides it from players it's streamed for.
	f.core.players.update(player);
	f.network.sent.clear();
	f.api->release(label->getID());
	CHECK(f.network.sentOnly({ PlayerHideTextLabel::PacketID }));

	f.core.players.disconnect(player);
}

TEST_CASE(textlabels_attached_to_player)
{
	ComponentFixture<ITextLabelsComponent> f("TextLabels");
	MockPlayer target(1, f.network);
	MockPlayer viewer(2, f.network);
	f.core.players.connect(target);
	f.core.players.connect(viewer);
	target.pos = Vector3(1000.0f, 0.0f, 0.0f);
	viewer.pos = Vector3(1010.0f, 0.0f, 0.0f);
	viewer.virtualWorld = 9;

	ITextLabel* label = f.api->create("tag", Red, Vector3(0.0f, 0.0f, 1.0f), 50.0f, 0, false, target);
	CHECK(label->getAttachmentData().playerID == 1);
	CHECK(label->getPosition() == Vector3(0.0f, 0.0f, 1.0f));

	// Attached labels follow the player, and are visible where the player is streamed in,
	// regardless of the label's virtual world.
	f.core.players.update(viewer);
	CHECK(!label->isStreamedInForPlayer(viewer));
	target.streamedFor.insert(&viewer);
	f.core.players.update(viewer);
	CHECK(label->isStreamedInForPlayer(viewer));
	target.streamedFor.erase(&viewer);
	f.core.players.update(viewer);
	CHECK(!label->isStreamedInForPlayer(viewer));

	label->detachFromPlayer(Vector3(5.0f, 5.0f, 5.0f));
	CHECK(label->getAttachmentData().playerID == INVALID_PLAYER_ID);
	CHECK(label->getPosition() == Vector3(5.0f, 5.0f, 5.0f));

	// The attached player leaving detaches it, keeping the offset as its position.
	label->attachToPlayer(target, Vector3(0.0f, 0.0f, 2.0f));
	target.streamedFor.insert(&viewer);
	f.core.players.update(viewer);
	CHECK(label->isStreamedInForPlayer(viewer));
	f.core.players.disconnect(target);
	CHECK(label->getAttachmentData().playerID == INVALID_PLAYER_ID);
	CHECK(label->getPosition() == Vector3(0.0f, 0.0f, 2.0f));

	// Viewers leaving are forgotten.
	f.core.players.disconnect(viewer);
	CHECK(!label->isStreamedInForPlayer(viewer));
}

TEST_CASE(textlabels_player_labels)
{
	ComponentFixture<ITextLabelsComponent> f("TextLabels");
	MockPlayer player(1, f.network);
	MockPlayer target(2, f.network);
	f.core.players.connect(player);
	f.core.players.connect(target);
	IPlayerTextLabelData* data = queryExtension<IPlayerTextLabelData>(player);
	if (!data)
	{
		CHECK(data != nullptr);
		return;
	}

	// Player labels are shown immediately, wherever the player is.
	player.pos = Vector3(10000.0f, 0.0f, 0.0f);
	IPlayerTextLabel* label = data->create("mine", Red, Vector3(0.0f), 10.0f, false);
	CHECK(label != nullptr);
	CHECK(data->get(label->getID()) == label);
	CHECK(f.network.sentOnly({ PlayerShowTextLabel::PacketID }));
	CHECK(label->getVirtualWorld() == 0);
	label->setVirtualWorld(5);
	CHECK(label->getVirtualWorld() == 0);
	CHECK(f.network.sentOnly({}));

	label->setText("changed");
	CHECK(label->getText() == StringView("changed"));
	CHECK(f.network.sentOnly(Restreamed));

	IPlayerTextLabel* attached = data->create("on target", Blue, Vector3(0.0f, 0.0f, 1.0f), 10.0f, true, target);
	CHECK(attached->getAttachmentData().playerID == 2);
	CHECK(f.network.sentOnly(Restreamed));

	data->release(label->getID());
	CHECK(data->get(label->getID()) == nullptr);
	CHECK(f.network.sentOnly({ PlayerHideTextLabel::PacketID }));

	// Labels attached to a player who leaves are detached.
	f.core.players.disconnect(target);
	CHECK(attached->getAttachmentData().playerID == INVALID_PLAYER_ID);
	CHECK(attached->getPosition() == Vector3(0.0f, 0.0f, 1.0f));
	f.network.sent.clear();

	static_cast<IExtension*>(data)->reset();
	CHECK(data->count() == 0);

	f.core.players.disconnect(player);
}
