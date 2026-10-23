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
#include "UIExplorerCreation.h"

#include "../../Graphics/Geometry.h"

#include <cmath>
#include <string>

#include "UICharSelect.h"
#include "UILoginNotice.h"
#include "UIRaceSelect.h"

#include "../UI.h"
#include "../UIScale.h"

#include "../Components/MapleButton.h"

#include "../../Configuration.h"
#include "../../Constants.h"

#include "../../Audio/Audio.h"
#include "../../Data/ItemData.h"

#include "../../Net/Packets/CharCreationPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// 800x600 design coordinates. Sign placement follows the v83
		// HeavenClient reference; insets were measured on the v83 bitmaps.

		// Login.img/NewChar/charSet (225x377) top-left; also the sign anchor
		constexpr int16_t SIGN_X = 450;
		constexpr int16_t SIGN_Y = 115;
		// Login.img/NewChar/charName (201x224) top-left
		constexpr int16_t NAME_X = 455;
		constexpr int16_t NAME_Y = 115;
		// charName's dark name box spans (28..176, 103..126) inside the sign:
		// text starts a few px in, A13M sits centred in the 24px box
		constexpr int16_t NAME_TEXT_X = NAME_X + 34;
		constexpr int16_t NAME_TEXT_Y = NAME_Y + 107;
		// field ends where BtCheck (50x24) fills the right end of the box
		constexpr int16_t NAME_TEXT_R = NAME_X + 124;
		constexpr int16_t NAME_TEXT_B = NAME_Y + 126;
		constexpr int16_t CHECK_X = NAME_X + 126;
		constexpr int16_t CHECK_Y = NAME_Y + 103;
		// BtYes/BtNo (81x41) are cut from the bottom plank: 6px below the plank
		// top, left one 12px in, right one overlapping it by 8px
		constexpr int16_t NAME_OK_X = NAME_X + 27;
		constexpr int16_t NAME_OK_Y = NAME_Y + 177;
		constexpr int16_t NAME_NO_X = NAME_X + 100;
		constexpr int16_t SET_OK_X = SIGN_X + 36;
		constexpr int16_t SET_OK_Y = SIGN_Y + 330;
		constexpr int16_t SET_NO_X = SIGN_X + 110;
		// avatarSel rows (200x17) fill charSet's cream area (10..211, 100..271)
		constexpr size_t ROW_COUNT = 9;
		constexpr int16_t ROW_X = SIGN_X + 11;
		constexpr int16_t ROW_Y = SIGN_Y + 102;
		// row pitch: 17px art plus the 2px cream gap
		constexpr int16_t ROW_H = 19;
		// BtLeft (15x16) just past the row's label divider (x=57), BtRight
		// flush with the row's right edge; arrows are 1px shorter than the row
		constexpr int16_t ROW_LEFT_X = ROW_X + 60;
		constexpr int16_t ROW_RIGHT_X = ROW_X + 184;
		constexpr int16_t ROW_BT_DY = -1;
		// option name centred between the arrows, A11M nudged up as in HeavenClient
		constexpr int16_t ROW_NAME_X = ROW_X + 130;
		constexpr int16_t ROW_NAME_DY = -3;
		// character feet on the platform (existing tuned position) — the stat
		// table and dice are anchored here so they stay beside the preview
		constexpr int16_t FEET_X = 360;
		constexpr int16_t FEET_Y = 348;
		// Login.img/NewChar/statTb (65x77) left of the character
		constexpr int16_t STAT_X = 248;
		constexpr int16_t STAT_Y = 262;
		// value column (40..64) centre and the 20px label rows (first at y=4)
		constexpr int16_t STAT_VAL_X = STAT_X + 52;
		constexpr int16_t STAT_ROW_Y = STAT_Y + 2;
		constexpr int16_t STAT_ROW_H = 20;
		// dice frames share a bottom edge at y+56 and bounce upward; the
		// resting frame 0 (37x26, origin y -30) covers y+30..y+56
		constexpr int16_t DICE_X = 262;
		constexpr int16_t DICE_Y = 320;
		constexpr int16_t DICE_REST_L = DICE_X;
		constexpr int16_t DICE_REST_T = DICE_Y + 30;
		constexpr int16_t DICE_REST_R = DICE_X + 37;
		constexpr int16_t DICE_REST_B = DICE_Y + 56;
		// bounces played per click
		constexpr uint8_t DICE_CYCLES = 2;
		// classic roll: 4..13 per stat, 25 in total
		constexpr uint8_t STAT_MIN = 4;
		constexpr uint8_t STAT_MAX = 13;
		constexpr uint8_t STAT_TOTAL = 25;
		// Common/BtStart (back) and the version label keep their places
		constexpr int16_t BACK_X = 0;
		constexpr int16_t BACK_Y = 515;
		constexpr int16_t VERSION_X = 707;
		constexpr int16_t VERSION_Y = 4;

		constexpr int16_t row_y(size_t row)
		{
			return static_cast<int16_t>(ROW_Y + ROW_H * static_cast<int16_t>(row));
		}

		constexpr int16_t row_button_y(size_t row)
		{
			return static_cast<int16_t>(row_y(row) + ROW_BT_DY);
		}

		constexpr int16_t row_name_y(size_t row)
		{
			return static_cast<int16_t>(row_y(row) + ROW_NAME_DY);
		}

		constexpr int16_t stat_row_y(size_t row)
		{
			return static_cast<int16_t>(STAT_ROW_Y + STAT_ROW_H * static_cast<int16_t>(row));
		}

		// Cyclic step through an option list; false when there is nothing to pick
		bool step_index(size_t& index, size_t count, bool forward)
		{
			if (count == 0)
				return false;

			index = forward ? (index + 1) % count : (index + count - 1) % count;

			return true;
		}
	}

	UIExplorerCreation::UIExplorerCreation() : UICreationBase(1)
	{
		// v83 has no gender board: gender is avatarSel row 8, so the base's
		// gender step is always "done"
		gender = true;
		charSet = false;
		named = false;
		rolling = false;
		roll_cycles = 0;

		ui_scale = std::min(UIScale::scale_x(), UIScale::scale_y());
		ui_scale_x = UIScale::scale_x();
		ui_scale_y = UIScale::scale_y();

		std::string version_text = Configuration::get().get_version();
		version = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::LEMONGRASS, "Ver. " + version_text);

		nl::node Login = nl::nx::ui["Login.img"];
		nl::node Common = Login["Common"];
		nl::node NewChar = Login["NewChar"];
		nl::node back = nl::nx::map["Back"]["login.img"]["back"];

		// Reference v83 composition (HeavenClient): sky strip back/2 tiled,
		// cloud back/27 drifting, scene pieces 15-18 and the Common frame.
		sky = Texture(back["2"]);
		cloud = back["27"];

		auto scenepc = [&](nl::node src, int16_t x, int16_t y)
		{
			if (src)
				scenery.emplace_back(Texture(src), Point<int16_t>(x, y));
		};
		scenepc(back["15"], 153, 685);
		scenepc(back["16"], 200, 400);
		scenepc(back["17"], 160, 263);
		scenepc(back["18"], 349, 1220);
		scenepc(Common["frame"], 400, 300);

		// CHARACTER SETTINGS sign with its nine option rows
		sprites_lookboard.emplace_back(NewChar["charSet"], sign_args(Texture(NewChar["charSet"]), SIGN_X, SIGN_Y));

		for (size_t i = 0; i < ROW_COUNT; i++)
			sprites_lookboard.emplace_back(NewChar["avatarSel"][i]["normal"], DrawArgument(slay(ROW_X, row_y(i)), ui_scale, ui_scale));

		// One BtLeft/BtRight pair per row: face, hair style, hair colour, skin,
		// top, bottom, shoes, weapon, gender
		auto arrows = [&](uint16_t leftid, uint16_t rightid, size_t row)
		{
			buttons[leftid] = std::make_unique<MapleButton>(NewChar["BtLeft"], slay(ROW_LEFT_X, row_button_y(row)));
			buttons[rightid] = std::make_unique<MapleButton>(NewChar["BtRight"], slay(ROW_RIGHT_X, row_button_y(row)));
		};
		arrows(Buttons::BT_CHARC_FACEL, Buttons::BT_CHARC_FACER, 0);
		arrows(Buttons::BT_CHARC_HAIRL, Buttons::BT_CHARC_HAIRR, 1);
		arrows(BT_CHARC_HAIRCL, BT_CHARC_HAIRCR, 2);
		arrows(Buttons::BT_CHARC_SKINL, Buttons::BT_CHARC_SKINR, 3);
		arrows(Buttons::BT_CHARC_TOPL, Buttons::BT_CHARC_TOPR, 4);
		arrows(Buttons::BT_CHARC_BOTL, Buttons::BT_CHARC_BOTR, 5);
		arrows(Buttons::BT_CHARC_SHOESL, Buttons::BT_CHARC_SHOESR, 6);
		arrows(Buttons::BT_CHARC_WEPL, Buttons::BT_CHARC_WEPR, 7);
		arrows(BT_CHARC_GENDERL, BT_CHARC_GENDERR, 8);

		buttons[Buttons::BT_CHARC_OK] = std::make_unique<MapleButton>(NewChar["BtYes"], slay(NAME_OK_X, NAME_OK_Y));
		buttons[Buttons::BT_CHARC_CANCEL] = std::make_unique<MapleButton>(NewChar["BtNo"], slay(NAME_NO_X, NAME_OK_Y));
		buttons[BT_CHARC_CHECK] = std::make_unique<MapleButton>(NewChar["BtCheck"], slay(CHECK_X, CHECK_Y));
		buttons[Buttons::BT_BACK] = std::make_unique<MapleButton>(Common["BtStart"], lay(BACK_X, BACK_Y));

		// Uniform-scale every button (draw + hit bounds).
		for (auto& btit : buttons)
			btit.second->set_scale(ui_scale);

		// NAME OF CHARACTER sign; the field's top-left is where the text draws
		nameboard = NewChar["charName"];
		namechar = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Rectangle<int16_t>(slay(NAME_TEXT_X, NAME_TEXT_Y), slay(NAME_TEXT_R, NAME_TEXT_B)), 12);

		namechar.set_enter_callback(
			[&](std::string)
			{
				button_pressed(Buttons::BT_CHARC_OK);
			}
		);

		namechar.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				button_pressed(Buttons::BT_CHARC_CANCEL);
			}
		);

		// Stat table and dice. The v83 CreateChar packet carries no stats, so
		// the roll is display-only; the dice has no button node and is
		// hit-tested on its resting frame in send_cursor().
		stattable = NewChar["statTb"];
		dice_idle = NewChar["dice"]["0"];
		dice_roll = Animation(NewChar["dice"]);
		dice_bounds = Rectangle<int16_t>(flay(DICE_REST_L, DICE_REST_T), flay(DICE_REST_R, DICE_REST_B));

		facename = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		hairname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		haircname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		bodyname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		topname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		botname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		shoename = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		wepname = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);
		gendername = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);

		for (auto& stattext : stattexts)
			stattext = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::BLACK);

		nl::node mkinfo = nl::nx::etc["MakeCharInfo.img"]["Info"];

		for (size_t i = 0; i < 2; i++)
		{
			bool f;
			nl::node CharGender;

			if (i == 0)
			{
				f = true;
				CharGender = mkinfo["CharFemale"];
			}
			else
			{
				f = false;
				CharGender = mkinfo["CharMale"];
			}

			for (auto node : CharGender)
			{
				int num = stoi(node.name());

				for (auto idnode : node)
				{
					int32_t value = idnode;

					switch (num)
					{
						case 0:
							faces[f].push_back(value);
							break;
						case 1:
							hairs[f].push_back(value);
							break;
						case 2:
							haircolors[f].push_back(static_cast<uint8_t>(value));
							break;
						case 3:
							skins[f].push_back(static_cast<uint8_t>(value));
							break;
						case 4:
							tops[f].push_back(value);
							break;
						case 5:
							bots[f].push_back(value);
							break;
						case 6:
							shoes[f].push_back(value);
							break;
						case 7:
							weapons[f].push_back(value);
							break;
					}
				}
			}
		}

		female = false;
		randomize_look();
		roll_stats();

		newchar.set_direction(true);

		show_name_sign();

		cloudfx = 200.0f;
	}

	Point<int16_t> UIExplorerCreation::lay(int16_t x, int16_t y) const
	{
		return Point<int16_t>(
			static_cast<int16_t>(std::lround(x * ui_scale_x)),
			static_cast<int16_t>(std::lround(y * ui_scale_y)));
	}

	Point<int16_t> UIExplorerCreation::alay(Point<int16_t> anchor, int16_t x, int16_t y) const
	{
		return UIScale::uniform_point(lay(anchor.x(), anchor.y()), anchor, x, y, ui_scale);
	}

	DrawArgument UIExplorerCreation::aargs(const Texture& t, Point<int16_t> anchor, int16_t x, int16_t y) const
	{
		return UIScale::uniform_args(t, lay(anchor.x(), anchor.y()), anchor, x, y, ui_scale);
	}

	Point<int16_t> UIExplorerCreation::slay(int16_t x, int16_t y) const
	{
		return alay(Point<int16_t>(SIGN_X, SIGN_Y), x, y);
	}

	DrawArgument UIExplorerCreation::sign_args(const Texture& t, int16_t x, int16_t y) const
	{
		return aargs(t, Point<int16_t>(SIGN_X, SIGN_Y), x, y);
	}

	Point<int16_t> UIExplorerCreation::flay(int16_t x, int16_t y) const
	{
		return alay(Point<int16_t>(FEET_X, FEET_Y), x, y);
	}

	void UIExplorerCreation::draw(float inter) const
	{
		// One gradient across the whole screen — the 20px strip is horizontally
		// uniform, so a single stretched quad covers it seamlessly.
		Point<int16_t> skyo = sky.get_origin();
		sky.draw(DrawArgument(skyo, Point<int16_t>(UIScale::view_width(), UIScale::view_height())));

		int16_t cloudx = static_cast<int16_t>(cloudfx) % 800;
		cloud.draw(UIScale::stretch_args(cloud, static_cast<int16_t>(cloudx - 800), 300));
		cloud.draw(UIScale::stretch_args(cloud, cloudx, 300));
		cloud.draw(UIScale::stretch_args(cloud, static_cast<int16_t>(cloudx + 800), 300));

		// scenic stage (tree / house / greenery / bridge) sits on top of the sky
		for (const auto& pc : scenery)
			pc.first.draw(UIScale::stretch_args(pc.first, pc.second.x(), pc.second.y()));

		UIElement::draw_sprites(inter);

		if (!named)
		{
			nameboard.draw(sign_args(nameboard, NAME_X, NAME_Y));
			namechar.draw(Point<int16_t>(0, 0));
		}
		else
		{
			for (const auto& sprite : sprites_lookboard)
				sprite.draw(Point<int16_t>(0, 0), inter);

			const Text* const names[ROW_COUNT] = {
				&facename, &hairname, &haircname, &bodyname, &topname, &botname, &shoename, &wepname, &gendername
			};

			for (size_t i = 0; i < ROW_COUNT; i++)
				names[i]->draw(slay(ROW_NAME_X, row_name_y(i)));

			stattable.draw(aargs(stattable, Point<int16_t>(FEET_X, FEET_Y), STAT_X, STAT_Y));

			for (size_t i = 0; i < stattexts.size(); i++)
				stattexts[i].draw(flay(STAT_VAL_X, stat_row_y(i)));

			DrawArgument diceargs(flay(DICE_X, DICE_Y), ui_scale, ui_scale);

			if (rolling)
				dice_roll.draw(diceargs, inter);
			else
				dice_idle.draw(diceargs);
		}

		// feet on the mushroom platform, reference position
		newchar.draw(DrawArgument(lay(FEET_X, FEET_Y), ui_scale_x, ui_scale_y), inter);

		UIElement::draw_buttons(inter);

		version.draw(lay(VERSION_X, VERSION_Y));
	}

	void UIExplorerCreation::update()
	{
		if (!named)
		{
			namechar.update(get_draw_position());
		}
		else
		{
			for (auto& sprite : sprites_lookboard)
				sprite.update();

			// Animation::update() reports true when the frame loop wraps, i.e.
			// the die is back on its resting frame; roll once the last bounce lands
			if (rolling && dice_roll.update())
			{
				roll_cycles--;

				if (roll_cycles == 0)
				{
					rolling = false;
					roll_stats();
				}
			}
		}

		// BtCheck mirrors BtYes: the base name helpers only know OK/Cancel, so
		// the check button follows OK's disabled state while a name request
		// or notice is pending and is released with it
		bool waiting = buttons[Buttons::BT_CHARC_OK]->get_state() == Button::State::DISABLED;
		bool frozen = buttons[BT_CHARC_CHECK]->get_state() == Button::State::DISABLED;

		if (waiting != frozen)
			buttons[BT_CHARC_CHECK]->set_state(waiting ? Button::State::DISABLED : Button::State::NORMAL);

		newchar.update(Constants::TIMESTEP);

		UIElement::update();

		cloudfx += 0.25f;
	}

	Cursor::State UIExplorerCreation::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// The dice has no button node in v83: hit-test its resting frame while
		// the look sign is up and no creation is pending
		if (named && !charSet && dice_bounds.contains(cursorpos))
		{
			if (clicked && !rolling)
			{
				Sound(Sound::Name::BUTTONCLICK).play();

				rolling = true;
				roll_cycles = DICE_CYCLES;
				dice_roll.reset();
			}

			return clicked ? Cursor::State::CLICKING : Cursor::State::CANCLICK;
		}

		return UICreationBase::send_cursor(clicked, cursorpos);
	}

	void UIExplorerCreation::send_naming_result(bool nameused)
	{
		// Unlike the base, a free name only unlocks the look sign; the
		// character is created from BtYes there
		if (named)
			return;

		if (nameused)
		{
			reject_name();

			return;
		}

		named = true;

		show_look_sign();
	}

	Button::State UIExplorerCreation::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
			case BT_CHARC_CHECK:
			{
				// Sends NameCharPacket (IDENTITY while the answer is pending) or
				// raises the illegal-name notice; update() keeps BtCheck in step
				return naming_ok_pressed();
			}
			case Buttons::BT_CHARC_OK:
			{
				if (!named)
					return naming_ok_pressed();

				if (charSet)
					return Button::State::NORMAL;

				charSet = true;

				set_look_buttons(false);

				buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::DISABLED);
				buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::DISABLED);

				create_character();

				return Button::State::IDENTITY;
			}
			case Buttons::BT_BACK:
			{
				Sound(Sound::Name::SCROLLUP).play();

				UI::get().remove(UIElement::Type::CLASSCREATION);
				UI::get().emplace<UIRaceSelect>();

				return Button::State::NORMAL;
			}
			case Buttons::BT_CHARC_CANCEL:
			{
				if (charSet)
					return Button::State::NORMAL;

				if (named)
				{
					show_name_sign();

					return Button::State::NORMAL;
				}

				return button_pressed(Buttons::BT_BACK);
			}
			case Buttons::BT_CHARC_FACEL:
			case Buttons::BT_CHARC_FACER:
			{
				if (step_index(face, faces[female].size(), buttonid == Buttons::BT_CHARC_FACER))
				{
					newchar.set_face(faces[female][face]);
					facename.change_text(newchar.get_face()->get_name());
				}

				return Button::State::NORMAL;
			}
			case Buttons::BT_CHARC_HAIRL:
			case Buttons::BT_CHARC_HAIRR:
			{
				if (step_index(hair, hairs[female].size(), buttonid == Buttons::BT_CHARC_HAIRR))
					apply_hair();

				return Button::State::NORMAL;
			}
			case BT_CHARC_HAIRCL:
			case BT_CHARC_HAIRCR:
			{
				if (step_index(haircolor, haircolors[female].size(), buttonid == BT_CHARC_HAIRCR))
					apply_hair();

				return Button::State::NORMAL;
			}
			case Buttons::BT_CHARC_SKINL:
			case Buttons::BT_CHARC_SKINR:
			{
				if (step_index(skin, skins[female].size(), buttonid == Buttons::BT_CHARC_SKINR))
				{
					newchar.set_body(skins[female][skin]);
					bodyname.change_text(newchar.get_body()->get_name());
				}

				return Button::State::NORMAL;
			}
			case Buttons::BT_CHARC_TOPL:
			case Buttons::BT_CHARC_TOPR:
			{
				if (step_index(top, tops[female].size(), buttonid == Buttons::BT_CHARC_TOPR))
				{
					newchar.add_equip(tops[female][top]);
					topname.change_text(get_equipname(EquipSlot::Id::TOP));
				}

				return Button::State::NORMAL;
			}
			case Buttons::BT_CHARC_BOTL:
			case Buttons::BT_CHARC_BOTR:
			{
				if (step_index(bot, bots[female].size(), buttonid == Buttons::BT_CHARC_BOTR))
				{
					newchar.add_equip(bots[female][bot]);
					botname.change_text(get_equipname(EquipSlot::Id::BOTTOM));
				}

				return Button::State::NORMAL;
			}
			case Buttons::BT_CHARC_SHOESL:
			case Buttons::BT_CHARC_SHOESR:
			{
				if (step_index(shoe, shoes[female].size(), buttonid == Buttons::BT_CHARC_SHOESR))
				{
					newchar.add_equip(shoes[female][shoe]);
					shoename.change_text(get_equipname(EquipSlot::Id::SHOES));
				}

				return Button::State::NORMAL;
			}
			case Buttons::BT_CHARC_WEPL:
			case Buttons::BT_CHARC_WEPR:
			{
				if (step_index(weapon, weapons[female].size(), buttonid == Buttons::BT_CHARC_WEPR))
				{
					newchar.add_equip(weapons[female][weapon]);
					wepname.change_text(get_equipname(EquipSlot::Id::WEAPON));
				}

				return Button::State::NORMAL;
			}
			case BT_CHARC_GENDERL:
			case BT_CHARC_GENDERR:
			{
				// Two genders, so either arrow flips; the option lists differ
				// per gender, hence a fresh random look
				female = !female;
				randomize_look();

				return Button::State::NORMAL;
			}
		}

		return Button::State::PRESSED;
	}

	void UIExplorerCreation::show_name_sign()
	{
		named = false;
		charSet = false;

		set_look_buttons(false);

		buttons[BT_CHARC_CHECK]->set_active(true);
		buttons[BT_CHARC_CHECK]->set_state(Button::State::NORMAL);
		buttons[Buttons::BT_CHARC_OK]->set_position(slay(NAME_OK_X, NAME_OK_Y));
		buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::NORMAL);
		buttons[Buttons::BT_CHARC_CANCEL]->set_position(slay(NAME_NO_X, NAME_OK_Y));
		buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::NORMAL);

		namechar.set_state(Textfield::State::FOCUSED);
	}

	void UIExplorerCreation::show_look_sign()
	{
		namechar.set_state(Textfield::State::DISABLED);

		buttons[BT_CHARC_CHECK]->set_active(false);

		set_look_buttons(true);

		// naming_ok_pressed() disabled OK/Cancel while the check was pending
		buttons[Buttons::BT_CHARC_OK]->set_position(slay(SET_OK_X, SET_OK_Y));
		buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::NORMAL);
		buttons[Buttons::BT_CHARC_CANCEL]->set_position(slay(SET_NO_X, SET_OK_Y));
		buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::NORMAL);
	}

	void UIExplorerCreation::set_look_buttons(bool active)
	{
		static constexpr uint16_t ids[] = {
			Buttons::BT_CHARC_FACEL, Buttons::BT_CHARC_FACER,
			Buttons::BT_CHARC_HAIRL, Buttons::BT_CHARC_HAIRR,
			BT_CHARC_HAIRCL, BT_CHARC_HAIRCR,
			Buttons::BT_CHARC_SKINL, Buttons::BT_CHARC_SKINR,
			Buttons::BT_CHARC_TOPL, Buttons::BT_CHARC_TOPR,
			Buttons::BT_CHARC_BOTL, Buttons::BT_CHARC_BOTR,
			Buttons::BT_CHARC_SHOESL, Buttons::BT_CHARC_SHOESR,
			Buttons::BT_CHARC_WEPL, Buttons::BT_CHARC_WEPR,
			BT_CHARC_GENDERL, BT_CHARC_GENDERR
		};

		for (uint16_t id : ids)
			buttons[id]->set_active(active);
	}

	void UIExplorerCreation::apply_hair()
	{
		newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
		hairname.change_text(newchar.get_hair()->get_name());
		haircname.change_text(hair_color_name(haircolors[female][haircolor]));
	}

	void UIExplorerCreation::refresh_names()
	{
		bodyname.change_text(newchar.get_body()->get_name());
		facename.change_text(newchar.get_face()->get_name());
		hairname.change_text(newchar.get_hair()->get_name());
		haircname.change_text(hair_color_name(haircolors[female][haircolor]));
		topname.change_text(get_equipname(EquipSlot::Id::TOP));
		botname.change_text(get_equipname(EquipSlot::Id::BOTTOM));
		shoename.change_text(get_equipname(EquipSlot::Id::SHOES));
		wepname.change_text(get_equipname(EquipSlot::Id::WEAPON));
		gendername.change_text(female ? "Female" : "Male");
	}

	void UIExplorerCreation::randomize_look()
	{
		hair = randomizer.next_int(hairs[female].size());
		face = randomizer.next_int(faces[female].size());
		skin = randomizer.next_int(skins[female].size());
		haircolor = randomizer.next_int(haircolors[female].size());
		top = randomizer.next_int(tops[female].size());
		bot = randomizer.next_int(bots[female].size());
		shoe = randomizer.next_int(shoes[female].size());
		weapon = randomizer.next_int(weapons[female].size());

		newchar.set_body(skins[female][skin]);
		newchar.set_face(faces[female][face]);
		newchar.set_hair(hairs[female][hair] + haircolors[female][haircolor]);
		newchar.add_equip(tops[female][top]);
		newchar.add_equip(bots[female][bot]);
		newchar.add_equip(shoes[female][shoe]);
		newchar.add_equip(weapons[female][weapon]);

		refresh_names();
	}

	void UIExplorerCreation::roll_stats()
	{
		// Every point above the floor goes one at a time to a random stat
		// that still has room, so any 4..13 split summing to 25 can come up
		stats.fill(STAT_MIN);

		uint8_t left = static_cast<uint8_t>(STAT_TOTAL - STAT_MIN * stats.size());

		while (left > 0)
		{
			size_t i = randomizer.next_int(stats.size());

			if (stats[i] < STAT_MAX)
			{
				stats[i]++;
				left--;
			}
		}

		for (size_t i = 0; i < stats.size(); i++)
			stattexts[i].change_text(std::to_string(stats[i]));
	}

	const std::string& UIExplorerCreation::hair_color_name(uint8_t color)
	{
		// v83 hair ids end in the colour digit
		static const std::string names[] = { "Black", "Red", "Orange", "Blonde", "Green", "Blue", "Violet", "Brown" };
		static const std::string unknown = "Unknown";

		return color < 8 ? names[color] : unknown;
	}
}
