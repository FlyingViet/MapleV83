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
#include "UIQuit.h"

#include "../KeyAction.h"
#include "../UI.h"
#include "../Window.h"

#include "../Components/MapleButton.h"

#include "../../Audio/Audio.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Session.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIQuit::UIQuit(const CharStats& st) : stats(st)
	{
		nl::node basic = nl::nx::ui["Basic.img"];
		nl::node notice = basic["Notice3"];

		// Frame: t, NOTICE_ROWS centre tiles, s
		sprites.emplace_back(notice["t"], DrawArgument(Point<int16_t>(0, 0)));

		for (int16_t r = 0; r < NOTICE_ROWS; r++)
			sprites.emplace_back(notice["c"], DrawArgument(Point<int16_t>(0, NOTICE_TOP_H + r * NOTICE_ROW_H)));

		int16_t bottom_y = NOTICE_TOP_H + NOTICE_ROWS * NOTICE_ROW_H;
		sprites.emplace_back(notice["s"], DrawArgument(Point<int16_t>(0, bottom_y)));

		// Yes / No pair centred in the grey strip of the bottom piece
		int16_t button_y = bottom_y + NOTICE_STRIP_Y + (NOTICE_STRIP_H - BUTTON_H) / 2;
		int16_t pair_x = NOTICE_W / 2 - BUTTON_W - BUTTON_GAP / 2;

		buttons[Buttons::YES] = std::make_unique<MapleButton>(basic["BtYes2"], Point<int16_t>(pair_x, button_y));
		buttons[Buttons::NO] = std::make_unique<MapleButton>(basic["BtNo2"], Point<int16_t>(pair_x + BUTTON_W + BUTTON_GAP, button_y));

		// Session play time, the one thing kept from the newer askReward summary
		int64_t uptime = Stage::get().get_uptime() / 1000 / 1000;
		int64_t minutes = uptime / 60;
		int64_t hours = minutes / 60;
		minutes -= hours * 60;

		std::string played = std::to_string(hours) + "h " + std::to_string(minutes) + "m";
		message = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "Return to the login screen?", NOTICE_TEXT_W);
		playtime = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "You have played for " + played + " this session.", NOTICE_TEXT_W);

		int16_t width = Constants::Constants::get().get_viewwidth();
		int16_t height = Constants::Constants::get().get_viewheight();

		background = ColorBox(width, height, Color::Name::BLACK, 0.5f);

		dimension = Point<int16_t>(NOTICE_W, bottom_y + NOTICE_BOTTOM_H);
		position = Point<int16_t>((width - dimension.x()) / 2, (height - dimension.y()) / 2);

		Sound(Sound::Name::DLGNOTICE).play();
	}

	void UIQuit::draw(float inter) const
	{
		background.draw(DrawArgument(Point<int16_t>(0, 0)));

		UIElement::draw(inter);

		// Centre both lines in the blue area: from 10px into t down to 11px into s
		int16_t blue_h = (NOTICE_TOP_H - NOTICE_TOP_BLUE_Y) + NOTICE_ROWS * NOTICE_ROW_H + NOTICE_BOTTOM_BLUE_H;
		int16_t lines_h = message.height() + LINE_GAP + playtime.height();
		int16_t text_y = NOTICE_TOP_BLUE_Y + (blue_h - lines_h) / 2;

		message.draw(position + Point<int16_t>(NOTICE_W / 2, text_y));
		playtime.draw(position + Point<int16_t>(NOTICE_W / 2, text_y + message.height() + LINE_GAP));
	}

	void UIQuit::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
				close();
			else if (keycode == KeyAction::Id::RETURN)
				button_pressed(Buttons::YES);
		}
	}

	UIElement::Type UIQuit::get_type() const
	{
		return TYPE;
	}

	Button::State UIQuit::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::NO:
			close();
			break;
		case Buttons::YES:
		{
			Constants::Constants::get().set_viewwidth(1920);
			Constants::Constants::get().set_viewheight(1080);

			float fadestep = 0.025f;

			Window::get().fadeout(
				fadestep,
				[]()
				{
					GraphicsGL::get().clear();

					UI::get().change_state(UI::State::LOGIN);
					UI::get().set_scrollnotice("");
					Session::get().reconnect();

					UI::get().enable();
					Timer::get().start();
					GraphicsGL::get().unlock();
				}
			);

			GraphicsGL::get().lock();
			Stage::get().clear();
			Timer::get().start();
		}
		break;
		default:
			break;
		}

		return Button::State::NORMAL;
	}

	void UIQuit::close()
	{
		deactivate();
	}
}
