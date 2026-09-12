//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "UIChannel.h"

#include "../KeyAction.h"
#include "../UI.h"

#include "../Components/MapleButton.h"

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Net/ChannelLoadData.h"
#include "../../Net/Packets/GameplayPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIChannel::UIChannel() : UIDragElement<PosCHANNEL>(Point<int16_t>(WIDTH, DRAG_HEIGHT))
	{
		uint8_t selected_world = Configuration::get().get_worldid();
		current_channel = Configuration::get().get_channelid();
		selected_channel = current_channel;

		// Prefer the real channel count published by the server; fall back
		// to 20 only if the login handshake never populated it.
		uint8_t real_count = ChannelLoadData::get().get_count(
			static_cast<int8_t>(selected_world));
		channel_count = real_count > 0 ? real_count : 20;

		if (current_channel >= channel_count)
			current_channel = 0;

		selected_channel = current_channel;
		rows = static_cast<uint8_t>((channel_count + COLS - 1) / COLS);

		nl::node source = nl::nx::ui["UIWindow.img"]["Channel"];

		// Frame: t on top, two 10px c tiles per 20px channel row, s below
		int16_t body_h = rows * STRIDE_VERT;
		int16_t tiles = (body_h + TILE_H - 1) / TILE_H;

		sprites.emplace_back(source["t"], DrawArgument(Point<int16_t>(0, 0)));

		for (int16_t i = 0; i < tiles; i++)
			sprites.emplace_back(source["c"], DrawArgument(Point<int16_t>(0, TOP_H + i * TILE_H)));

		int16_t strip_y = TOP_H + tiles * TILE_H;
		sprites.emplace_back(source["s"], DrawArgument(Point<int16_t>(0, strip_y)));

		// World badge (icon + name) in the blue banner. v83 ships world/0..19
		// plus test worlds t0/t1; anything out of range just shows no badge.
		nl::node world = source["world"][std::to_string(selected_world)];

		if (world)
			sprites.emplace_back(world, DrawArgument(WORLD_OFFSET));

		// BtChange (91x18) and BtCancel (47x18) side by side, centred in the strip
		int16_t pair_x = (WIDTH - (BT_CHANGE_W + BTN_GAP + BT_CANCEL_W)) / 2;
		int16_t btn_y = strip_y + BTN_Y_IN_STRIP;

		buttons[BT_CHANGE] = std::make_unique<MapleButton>(source["BtChange"], Point<int16_t>(pair_x, btn_y));
		buttons[BT_CANCEL] = std::make_unique<MapleButton>(source["BtCancel"], Point<int16_t>(pair_x + BT_CHANGE_W + BTN_GAP, btn_y));

		cover_normal = source["channel0"];
		cover_current = source["channel1"];
		cover_selected = source["channel2"];

		nl::node ch_src = source["ch"];

		for (uint8_t n = 0; n < channel_count; n++)
			ch_sprites.emplace_back(ch_src[std::to_string(n)], DrawArgument(cell_pos(n) + CH_OFFSET));

		dimension = Point<int16_t>(WIDTH, strip_y + BOTTOM_H);
		dragarea = Point<int16_t>(WIDTH, DRAG_HEIGHT);
	}

	Point<int16_t> UIChannel::cell_pos(uint8_t channel) const
	{
		return Point<int16_t>(
			GRID_X + (channel % COLS) * STRIDE_HORIZ,
			TOP_H + (channel / COLS) * STRIDE_VERT
		);
	}

	void UIChannel::draw(float inter) const
	{
		UIElement::draw_sprites(inter);

		for (uint8_t n = 0; n < channel_count; n++)
		{
			const Texture& cover = (n == selected_channel && selected_channel != current_channel)
				? cover_selected
				: (n == current_channel ? cover_current : cover_normal);

			cover.draw(DrawArgument(position + cell_pos(n)));
		}

		for (auto& sprite : ch_sprites)
			sprite.draw(position, inter);

		UIElement::draw_buttons(inter);
	}

	void UIChannel::update()
	{
		UIElement::update();

		for (auto& sprite : ch_sprites)
			sprite.update();
	}

	int8_t UIChannel::channel_by_pos(Point<int16_t> cursorpos) const
	{
		Point<int16_t> relative = cursorpos - position - Point<int16_t>(GRID_X, TOP_H);

		if (relative.x() < 0 || relative.y() < 0)
			return -1;

		if (relative.x() >= COLS * STRIDE_HORIZ || relative.y() >= rows * STRIDE_VERT)
			return -1;

		int16_t col = relative.x() / STRIDE_HORIZ;
		int16_t row = relative.y() / STRIDE_VERT;

		// The 16px gap to the right of each cover is dead space
		if (relative.x() - col * STRIDE_HORIZ >= CELL_W)
			return -1;

		int16_t ch = row * COLS + col;

		if (ch >= channel_count)
			return -1;

		return static_cast<int8_t>(ch);
	}

	void UIChannel::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed)
			return;

		if (escape)
		{
			deactivate();
			return;
		}

		switch (keycode)
		{
		case KeyAction::Id::RETURN:
			change_channel();
			break;
		case KeyAction::Id::UP:
			if (selected_channel >= COLS)
				selected_channel -= COLS;
			else
				selected_channel += COLS * (rows - 1);

			if (selected_channel >= channel_count)
				selected_channel = current_channel;
			break;
		case KeyAction::Id::DOWN:
			selected_channel += COLS;

			if (selected_channel >= channel_count)
				selected_channel %= COLS;
			break;
		case KeyAction::Id::LEFT:
			if (selected_channel > 0)
				selected_channel--;
			else
				selected_channel = channel_count - 1;
			break;
		case KeyAction::Id::RIGHT:
			if (selected_channel < channel_count - 1)
				selected_channel++;
			else
				selected_channel = 0;
			break;
		}
	}

	Cursor::State UIChannel::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(clicked, cursorpos);

		if (dragged)
			return dstate;

		if (clicked)
		{
			int8_t clicked_ch = channel_by_pos(cursorpos);

			if (clicked_ch >= 0 && clicked_ch != current_channel)
				selected_channel = clicked_ch;
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	UIElement::Type UIChannel::get_type() const
	{
		return TYPE;
	}

	Button::State UIChannel::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case BT_CANCEL:
			deactivate();
			return Button::State::NORMAL;
		case BT_CHANGE:
			change_channel();
			return Button::State::NORMAL;
		default:
			return Button::State::NORMAL;
		}
	}

	void UIChannel::change_channel()
	{
		if (selected_channel != current_channel)
		{
			// Persist the new channel so UIChannel and other consumers of
			// Configuration::get_channelid() pick up the right value after
			// the reconnect completes.
			Configuration::get().set_channelid(selected_channel);
			current_channel = selected_channel;

			UI::get().disable();
			ChangeChannelPacket(selected_channel).dispatch();
		}

		deactivate();
	}
}
