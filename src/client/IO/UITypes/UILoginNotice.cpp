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
#include "UILoginNotice.h"

#include "../UI.h"

#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../UIScale.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// v83 has no art for the key-layout chooser (UIWindow2.img/KeyConfig/
		// KeyType) or the class confirmation (Login.img/RaceSelect_new), so
		// those popups are built from the plain blue Basic.img/Notice3 frame:
		// a 21px top (blue from 10px down), `rows` 20px centre tiles and a 55px
		// bottom whose grey strip (12..51px into it) holds the buttons.
		constexpr int16_t NOTICE_W = 266;
		constexpr int16_t NOTICE_TOP_H = 21;
		constexpr int16_t NOTICE_ROW_H = 20;
		constexpr int16_t NOTICE_BOTTOM_H = 55;
		constexpr int16_t NOTICE_STRIP_Y = 12;
		constexpr int16_t NOTICE_STRIP_H = 40;
		constexpr int16_t NOTICE_TEXT_W = 230;
		constexpr int16_t NOTICE_CENTER_X = NOTICE_W / 2;
		// Basic.img/BtOK2 and BtCancel2 are 47x18
		constexpr int16_t NOTICE_BUTTON_W = 47;
		constexpr int16_t NOTICE_BUTTON_H = 18;
		constexpr int16_t NOTICE_BUTTON_GAP = 7;

		// Appends the frame pieces to `sprites` and returns the frame's size
		Point<int16_t> build_notice_frame(std::vector<Sprite>& sprites, int16_t rows)
		{
			nl::node notice = nl::nx::ui["Basic.img"]["Notice3"];

			sprites.emplace_back(notice["t"], Point<int16_t>(0, 0));

			for (int16_t r = 0; r < rows; r++)
				sprites.emplace_back(notice["c"], Point<int16_t>(0, NOTICE_TOP_H + r * NOTICE_ROW_H));

			sprites.emplace_back(notice["s"], Point<int16_t>(0, NOTICE_TOP_H + rows * NOTICE_ROW_H));

			return Point<int16_t>(NOTICE_W, NOTICE_TOP_H + rows * NOTICE_ROW_H + NOTICE_BOTTOM_H);
		}

		// y of the button row centred in the grey strip of a `rows`-tile frame
		int16_t notice_button_y(int16_t rows)
		{
			return NOTICE_TOP_H + rows * NOTICE_ROW_H + NOTICE_STRIP_Y + (NOTICE_STRIP_H - NOTICE_BUTTON_H) / 2;
		}
	}

	UILoginNotice::UILoginNotice(uint16_t message, std::function<void()> okhandler, std::function<void()> cancelhandler) : okhandler(okhandler), cancelhandler(cancelhandler)
	{
		multiple = false;

		nl::node Notice = nl::nx::ui["Login.img"]["Notice"];
		nl::node backgrnd;

		switch (message)
		{
			case Message::NAME_IN_USE:
			case Message::ILLEGAL_NAME:
			case Message::BLOCKED_ID:
			case Message::INCORRECT_PIC:
				backgrnd = Notice["backgrnd"]["1"];
				break;
			default:
				backgrnd = Notice["backgrnd"]["0"];
				break;
		}

		sprites.emplace_back(backgrnd);
		sprites.emplace_back(Notice["text"][message], Point<int16_t>(17, 13));

		if (message == Message::DELETE_CONFIRMATION)
		{
			multiple = true;

			buttons[Buttons::YES] = std::make_unique<MapleButton>(Notice["BtYes"], Point<int16_t>(70, 106));
			buttons[Buttons::NO] = std::make_unique<MapleButton>(Notice["BtNo"], Point<int16_t>(130, 106));
		}
		else
		{
			buttons[Buttons::YES] = std::make_unique<MapleButton>(Notice["BtYes"], Point<int16_t>(100, 106));
		}

		position = UIScale::at(275, 209);
		dimension = Texture(backgrnd).get_dimensions();
	}

	UILoginNotice::UILoginNotice(uint16_t message, std::function<void()> okhandler) : UILoginNotice(message, okhandler, []() {}) {}
	UILoginNotice::UILoginNotice(uint16_t message) : UILoginNotice(message, []() {}) {}

	void UILoginNotice::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				if (!multiple)
					okhandler();
				else
					cancelhandler();

				deactivate();
			}
			else if (keycode == KeyAction::RETURN)
			{
				okhandler();
				deactivate();
			}
		}
	}

	UIElement::Type UILoginNotice::get_type() const
	{
		return TYPE;
	}

	Button::State UILoginNotice::button_pressed(uint16_t buttonid)
	{
		if (buttonid == Buttons::YES)
			okhandler();
		else if (buttonid == Buttons::NO)
			cancelhandler();

		deactivate();

		return Button::State::NORMAL;
	}

	UIQuitConfirm::UIQuitConfirm()
	{
		nl::node notice = nl::nx::ui["Login.img"]["Notice"];
		nl::node backgrnd = notice["backgrnd"]["0"];

		sprites.emplace_back(backgrnd);
		sprites.emplace_back(notice["text"][UILoginNotice::Message::CONFIRM_EXIT], Point<int16_t>(17, 13));

		buttons[Buttons::BT_OK] = std::make_unique<MapleButton>(notice["BtYes"], Point<int16_t>(70, 106));
		buttons[Buttons::BT_CANCEL] = std::make_unique<MapleButton>(notice["BtNo"], Point<int16_t>(130, 106));

		position = UIScale::at(275, 209);
		dimension = Texture(backgrnd).get_dimensions();
	}

	void UIQuitConfirm::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				deactivate();
			}
			else if (keycode == KeyAction::RETURN)
			{
				UI::get().quit();
				deactivate();
			}
		}
	}

	UIElement::Type UIQuitConfirm::get_type() const
	{
		return TYPE;
	}

	Button::State UIQuitConfirm::button_pressed(uint16_t buttonid)
	{
		if (buttonid == BT_OK)
			UI::get().quit();

		deactivate();

		return Button::PRESSED;
	}

	UIClassConfirm::UIClassConfirm(uint16_t selected_class, bool unavailable, std::function<void()> okhandler) : okhandler(okhandler)
	{
		// v83 has no RaceSelect_new confirm / deny art (its class art lives in
		// the explorer creation flow), so this is a plain notice with a message.
		constexpr int16_t ROWS = 3;

		dimension = build_notice_frame(sprites, ROWS);

		std::string text = unavailable
			? "This class is not available yet."
			: "Would you like to create a character of this class?";
		message = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, text, NOTICE_TEXT_W);

		nl::node basic = nl::nx::ui["Basic.img"];
		int16_t button_y = notice_button_y(ROWS);

		if (unavailable)
		{
			buttons[Buttons::OK] = std::make_unique<MapleButton>(basic["BtOK2"], Point<int16_t>(NOTICE_CENTER_X - NOTICE_BUTTON_W / 2, button_y));
		}
		else
		{
			int16_t pair_x = NOTICE_CENTER_X - NOTICE_BUTTON_W - NOTICE_BUTTON_GAP / 2;

			buttons[Buttons::OK] = std::make_unique<MapleButton>(basic["BtOK2"], Point<int16_t>(pair_x, button_y));
			buttons[Buttons::CANCEL] = std::make_unique<MapleButton>(basic["BtCancel2"], Point<int16_t>(pair_x + NOTICE_BUTTON_W + NOTICE_BUTTON_GAP, button_y));
		}

		position = UIScale::at((800 - dimension.x()) / 2, (600 - dimension.y()) / 2);
	}

	void UIClassConfirm::draw(float inter) const
	{
		UIElement::draw(inter);

		// Centred in the blue panel (10px into the top piece .. 11px into the bottom)
		int16_t blue_h = dimension.y() - 10 - (NOTICE_BOTTOM_H - 11);
		message.draw(position + Point<int16_t>(NOTICE_CENTER_X, 10 + (blue_h - message.height()) / 2));
	}

	Cursor::State UIClassConfirm::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		for (auto& btit : buttons)
		{
			if (btit.second->is_active() && btit.second->bounds(position).contains(cursorpos))
			{
				if (btit.second->get_state() == Button::State::NORMAL)
				{
					Sound(Sound::Name::BUTTONOVER).play();

					btit.second->set_state(Button::State::MOUSEOVER);
				}
				else if (btit.second->get_state() == Button::State::MOUSEOVER)
				{
					if (clicked)
					{
						Sound(Sound::Name::BUTTONCLICK).play();

						btit.second->set_state(button_pressed(btit.first));
					}
				}
			}
			else if (btit.second->get_state() == Button::State::MOUSEOVER)
			{
				btit.second->set_state(Button::State::NORMAL);
			}
		}

		return Cursor::State::LEAF;
	}

	void UIClassConfirm::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
				deactivate();
			else if (keycode == KeyAction::Id::RETURN)
				button_pressed(Buttons::OK);
		}
	}

	UIElement::Type UIClassConfirm::get_type() const
	{
		return TYPE;
	}

	Button::State UIClassConfirm::button_pressed(uint16_t buttonid)
	{
		deactivate();

		if (buttonid == Buttons::OK)
			okhandler();

		return Button::State::NORMAL;
	}

	UIKeySelect::UIKeySelect(std::function<void(bool)> oh, bool l) : okhandler(oh), login(l)
	{
		// v83 has a single key layout and no KeyType chooser art, so the two
		// OpenStory layouts are offered as text-labelled areas on a plain notice.
		constexpr int16_t ROWS = 4;

		dimension = build_notice_frame(sprites, ROWS);

		title = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Select your key layout", NOTICE_TEXT_W);
		hint = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "Type A is the classic layout, Type B the alternate one.", NOTICE_TEXT_W);
		label_a = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "Type A");
		label_b = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "Type B");

		choice_box = ColorBox(CHOICE_W, CHOICE_H, Color::Name::WHITE, 0.25f);
		choice_box_hover = ColorBox(CHOICE_W, CHOICE_H, Color::Name::WHITE, 0.45f);

		// 12x12 close cross in the frame's top-right corner
		buttons[Buttons::CLOSE] = std::make_unique<MapleButton>(nl::nx::ui["Basic.img"]["BtClose"], Point<int16_t>(dimension.x() - 18, 6));
		buttons[Buttons::TYPEA] = std::make_unique<AreaButton>(Point<int16_t>(CHOICE_A_X, CHOICE_Y), Point<int16_t>(CHOICE_W, CHOICE_H));
		buttons[Buttons::TYPEB] = std::make_unique<AreaButton>(Point<int16_t>(CHOICE_B_X, CHOICE_Y), Point<int16_t>(CHOICE_W, CHOICE_H));

		if (login)
			buttons[Buttons::CLOSE]->set_active(false);

		position = UIScale::at((800 - dimension.x()) / 2, (600 - dimension.y()) / 2);
	}

	void UIKeySelect::draw(float inter) const
	{
		UIElement::draw(inter);

		title.draw(position + Point<int16_t>(NOTICE_CENTER_X, 16));
		hint.draw(position + Point<int16_t>(NOTICE_CENTER_X, 36));

		bool hover_a = buttons.at(Buttons::TYPEA)->get_state() == Button::State::MOUSEOVER;
		bool hover_b = buttons.at(Buttons::TYPEB)->get_state() == Button::State::MOUSEOVER;

		(hover_a ? choice_box_hover : choice_box).draw(DrawArgument(position + Point<int16_t>(CHOICE_A_X, CHOICE_Y)));
		(hover_b ? choice_box_hover : choice_box).draw(DrawArgument(position + Point<int16_t>(CHOICE_B_X, CHOICE_Y)));

		label_a.draw(position + Point<int16_t>(CHOICE_A_X + CHOICE_W / 2, CHOICE_Y + 2));
		label_b.draw(position + Point<int16_t>(CHOICE_B_X + CHOICE_W / 2, CHOICE_Y + 2));
	}

	void UIKeySelect::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && !login)
			if (escape || keycode == KeyAction::Id::RETURN)
				deactivate();
	}

	UIElement::Type UIKeySelect::get_type() const
	{
		return TYPE;
	}

	Button::State UIKeySelect::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
			default:
			case Buttons::CLOSE:
				deactivate();
				break;
			case Buttons::TYPEA:
			case Buttons::TYPEB:
			{
				bool alternate = (buttonid == Buttons::TYPEA) ? false : true;

				if (alternate)
					buttons[Buttons::TYPEA]->set_state(Button::State::DISABLED);
				else
					buttons[Buttons::TYPEB]->set_state(Button::State::DISABLED);

				auto onok = [&, alternate]()
				{
					okhandler(alternate);
					deactivate();
				};

				UI::get().emplace<UIKeyConfirm>(alternate, onok, login);
				break;
			}
		}

		return Button::State::DISABLED;
	}

	UIKeyConfirm::UIKeyConfirm(bool alternate, std::function<void()> oh, bool l) : okhandler(oh), login(l)
	{
		// v83 has no KeyType alert art; confirm the choice on a plain notice
		constexpr int16_t ROWS = 3;

		dimension = build_notice_frame(sprites, ROWS);

		std::string text = std::string("Your key layout has been set to ") + (alternate ? "Type B" : "Type A")
			+ ".\\nYou can change it later in the key settings.";
		message = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, text, NOTICE_TEXT_W);

		buttons[Buttons::OK] = std::make_unique<MapleButton>(nl::nx::ui["Basic.img"]["BtOK2"], Point<int16_t>(NOTICE_CENTER_X - NOTICE_BUTTON_W / 2, notice_button_y(ROWS)));

		position = UIScale::at((800 - dimension.x()) / 2, (600 - dimension.y()) / 2);
	}

	void UIKeyConfirm::draw(float inter) const
	{
		UIElement::draw(inter);

		int16_t blue_h = dimension.y() - 10 - (NOTICE_BOTTOM_H - 11);
		message.draw(position + Point<int16_t>(NOTICE_CENTER_X, 10 + (blue_h - message.height()) / 2));
	}

	void UIKeyConfirm::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (keycode == KeyAction::Id::RETURN)
			{
				confirm();
			}
			else if (!login && escape)
			{
				deactivate();

				UI::get().remove(UIElement::Type::LOGINNOTICE);
			}
		}
	}

	UIElement::Type UIKeyConfirm::get_type() const
	{
		return TYPE;
	}

	Button::State UIKeyConfirm::button_pressed(uint16_t buttonid)
	{
		confirm();

		return Button::State::NORMAL;
	}

	void UIKeyConfirm::confirm()
	{
		okhandler();
		deactivate();

		UI::get().remove(UIElement::Type::LOGINNOTICE);
	}
}