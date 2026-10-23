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
#include <iostream>
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
	// 800x600 design coordinates of one race's v83 boards (Login.img/NewChar,
	// NewCharKnight, NewCharAran). Insets were measured on the v83 bitmaps.
	struct UIExplorerCreation::Layout
	{
		const char* board;			// Login.img child holding the boards
		const char* female_pool;	// Etc MakeCharInfo.img paths Cosmic validates against
		const char* male_pool;
		Color::Name name_color;		// name text on the board's name box
		// charSet (CHARACTER SETTINGS) top-left; also the uniform-scale anchor
		int16_t sign_x, sign_y;
		// charName (NAME OF CHARACTER) top-left
		int16_t name_x, name_y;
		// name box text area, BtCheck, and BtYes/BtNo on the name sign
		int16_t name_text_x, name_text_y, name_text_r, name_text_b;
		int16_t check_x, check_y;
		int16_t name_ok_x, name_no_x, name_ok_y;
		// BtYes/BtNo on the settings sign
		int16_t set_ok_x, set_no_x, set_ok_y;
		// first avatarSel row (200x17) and the row pitch
		int16_t row_x, row_y0, row_h;
		// character feet; the stat table sits left of the character
		int16_t feet_x, feet_y;

		int16_t row_y(size_t row) const
		{
			return static_cast<int16_t>(row_y0 + row_h * static_cast<int16_t>(row));
		}
	};

	namespace
	{
		using Layout = UIExplorerCreation::Layout;

		// Explorer: wooden signs, 9 rows at a 19px pitch in charSet's cream area
		// (10..211, 100..271); BtYes/BtNo (81x41) cut from the bottom planks.
		constexpr Layout EXPLORER_LAYOUT = {
			"NewChar", "Info/CharFemale", "Info/CharMale", Color::Name::WHITE,
			450, 115, 455, 115,
			455 + 34, 115 + 107, 455 + 124, 115 + 126,
			455 + 126, 115 + 103,
			455 + 27, 455 + 100, 115 + 177,
			450 + 36, 450 + 110, 115 + 330,
			450 + 11, 115 + 102, 19,
			360, 348
		};

		// Cygnus: lavender signs of the same shape; content (10..217, 108..277),
		// name box (29..177, 103..127), BtYes/BtNo 67x38 on the planks.
		constexpr Layout CYGNUS_LAYOUT = {
			"NewCharKnight", "PremiumCharFemale", "PremiumCharMale", Color::Name::WHITE,
			450, 115, 455, 115,
			455 + 35, 115 + 107, 455 + 125, 115 + 127,
			455 + 129, 115 + 105,
			455 + 28, 455 + 106, 115 + 178,
			450 + 40, 450 + 118, 115 + 328,
			450 + 13, 115 + 111, 18,
			394, 339
		};

		// Aran: snowy scrolls, settings content (30..245, 88..245), name box
		// (30..180, 88..116); BtYes/BtNo are fish ornaments (75x125) hanging
		// below each scroll.
		constexpr Layout ARAN_LAYOUT = {
			"NewCharAran", "OrientCharFemale", "OrientCharMale", Color::Name::BLACK,
			440, 100, 471, 100,
			471 + 36, 100 + 92, 471 + 126, 100 + 116,
			471 + 128, 100 + 90,
			471 + 30, 471 + 110, 100 + 133,
			440 + 50, 440 + 150, 100 + 262,
			440 + 37, 100 + 90, 17,
			394, 339
		};

		constexpr size_t ROW_COUNT = 9;
		// BtLeft (15x16) just past the row's label divider (x=57), BtRight
		// flush with the row's right edge; arrows are 1px shorter than the row
		constexpr int16_t ROW_LEFT_DX = 60;
		constexpr int16_t ROW_RIGHT_DX = 184;
		constexpr int16_t ROW_BT_DY = -1;
		// option name centred between the arrows, A11M nudged up as in HeavenClient
		constexpr int16_t ROW_NAME_DX = 130;
		constexpr int16_t ROW_NAME_DY = -3;
		// Login.img/NewChar/statTb (65x77) left of the character: offset from
		// the feet, value column (40..64) centre and the 20px label rows
		constexpr int16_t STAT_DX = -112;
		constexpr int16_t STAT_DY = -86;
		constexpr int16_t STAT_VAL_DX = 52;
		constexpr int16_t STAT_ROW_DY = 2;
		constexpr int16_t STAT_ROW_H = 20;
		// Every new character starts at 4/4/4/4 with 9 AP to place in game
		// (Cosmic USE_AUTOASSIGN_STARTERS_AP off).
		constexpr uint8_t STARTING_STAT = 4;
		// Common/BtStart (back) and the version label keep their places
		constexpr int16_t BACK_X = 0;
		constexpr int16_t BACK_Y = 515;
		constexpr int16_t VERSION_X = 707;
		constexpr int16_t VERSION_Y = 4;

		// Cyclic step through an option list; false when there is nothing to pick
		bool step_index(size_t& index, size_t count, bool forward)
		{
			if (count == 0)
				return false;

			index = forward ? (index + 1) % count : (index + count - 1) % count;

			return true;
		}
	}

	const Layout& UIExplorerCreation::layout_for(Race race)
	{
		switch (race)
		{
			case Race::CYGNUS:
				return CYGNUS_LAYOUT;
			case Race::ARAN:
				return ARAN_LAYOUT;
			default:
				return EXPLORER_LAYOUT;
		}
	}

	uint16_t UIExplorerCreation::job_for(Race race)
	{
		// CreateChar job type: 0 Cygnus (Noblesse), 1 Explorer, 2 Aran (Legend)
		switch (race)
		{
			case Race::CYGNUS:
				return 0;
			case Race::ARAN:
				return 2;
			default:
				return 1;
		}
	}

	UIExplorerCreation::UIExplorerCreation(Race race) : UICreationBase(job_for(race)), race(race), layout(layout_for(race))
	{
		// v83 has no gender board: gender is avatarSel row 8, so the base's
		// gender step is always "done"
		gender = true;
		charSet = false;
		named = false;

		ui_scale = std::min(UIScale::scale_x(), UIScale::scale_y());
		ui_scale_x = UIScale::scale_x();
		ui_scale_y = UIScale::scale_y();

		std::string version_text = Configuration::get().get_version();
		version = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::LEMONGRASS, "Ver. " + version_text);

		nl::node Login = nl::nx::ui["Login.img"];
		nl::node Common = Login["Common"];
		nl::node NewChar = Login[layout.board];
		nl::node back = nl::nx::map["Back"]["login.img"]["back"];

		// Reference v83 composition (HeavenClient): sky strip back/2 tiled,
		// cloud back/27 drifting, the race's scene from the login map and the
		// Common frame on top.
		sky = Texture(back["2"]);
		cloud = back["27"];

		auto scenepc = [&](nl::node src, int16_t x, int16_t y)
		{
			if (src)
				scenery.emplace_back(Texture(src), Point<int16_t>(x, y));
		};

		// Gazebos keep their aspect: uniform scale at their map spot, pieces
		// offset in uniform space so they stay glued together.
		auto gazebo = [&](int16_t ax, int16_t ay, nl::node src, int16_t dx, int16_t dy)
		{
			Point<int16_t> anchor(
				static_cast<int16_t>(std::lround(ax * UIScale::scale_x())),
				static_cast<int16_t>(std::lround(ay * UIScale::scale_y())));

			if (src)
				sprites.emplace_back(src, UIScale::uniform_args(Texture(src), anchor, Point<int16_t>(0, 0), dx, dy, ui_scale));
		};

		switch (race)
		{
			case Race::CYGNUS:
				// Ereve: the Cygnus gazebo (back/30) with its vines (31/32)
				gazebo(265, 300, back["30"], 0, 0);
				gazebo(265, 300, back["31"], 430, -221);
				gazebo(265, 300, back["32"], 376, -150);
				sprites.emplace_back(Common["frame"], UIScale::stretch_args(Texture(Common["frame"]), 400, 300));
				break;
			case Race::ARAN:
				// Rien: the snowy gazebo (back/33) the new Aran stands on
				gazebo(256, 299, back["33"], 0, 0);
				sprites.emplace_back(Common["frame"], UIScale::stretch_args(Texture(Common["frame"]), 400, 300));
				break;
			default:
				// Maple Island: scene pieces 15-18
				scenepc(back["15"], 153, 685);
				scenepc(back["16"], 200, 400);
				scenepc(back["17"], 160, 263);
				scenepc(back["18"], 349, 1220);
				scenepc(Common["frame"], 400, 300);
				break;
		}

		// CHARACTER SETTINGS sign with its nine option rows
		sprites_lookboard.emplace_back(NewChar["charSet"], sign_args(Texture(NewChar["charSet"]), layout.sign_x, layout.sign_y));

		for (size_t i = 0; i < ROW_COUNT; i++)
			sprites_lookboard.emplace_back(NewChar["avatarSel"][i]["normal"], DrawArgument(slay(layout.row_x, layout.row_y(i)), ui_scale, ui_scale));

		// One BtLeft/BtRight pair per row: face, hair style, hair colour, skin,
		// top, bottom, shoes, weapon, gender
		auto arrows = [&](uint16_t leftid, uint16_t rightid, size_t row)
		{
			int16_t y = static_cast<int16_t>(layout.row_y(row) + ROW_BT_DY);
			buttons[leftid] = std::make_unique<MapleButton>(NewChar["BtLeft"], slay(layout.row_x + ROW_LEFT_DX, y));
			buttons[rightid] = std::make_unique<MapleButton>(NewChar["BtRight"], slay(layout.row_x + ROW_RIGHT_DX, y));
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

		buttons[Buttons::BT_CHARC_OK] = std::make_unique<MapleButton>(NewChar["BtYes"], slay(layout.name_ok_x, layout.name_ok_y));
		buttons[Buttons::BT_CHARC_CANCEL] = std::make_unique<MapleButton>(NewChar["BtNo"], slay(layout.name_no_x, layout.name_ok_y));
		buttons[BT_CHARC_CHECK] = std::make_unique<MapleButton>(NewChar["BtCheck"], slay(layout.check_x, layout.check_y));
		buttons[Buttons::BT_BACK] = std::make_unique<MapleButton>(Common["BtStart"], lay(BACK_X, BACK_Y));

		// Uniform-scale every button (draw + hit bounds).
		for (auto& btit : buttons)
			btit.second->set_scale(ui_scale);

		// NAME OF CHARACTER sign; the field's top-left is where the text draws
		nameboard = NewChar["charName"];
		namechar = Textfield(Text::Font::A13M, Text::Alignment::LEFT, layout.name_color, Rectangle<int16_t>(slay(layout.name_text_x, layout.name_text_y), slay(layout.name_text_r, layout.name_text_b)), 12);

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

		// Stat table: the v83 CreateChar packet carries no stats and the server
		// starts everyone at 4/4/4/4 with 9 AP, so there is no dice to roll.
		stattable = NewChar["statTb"];

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

		nl::node mkinfo = nl::nx::etc["MakeCharInfo.img"];

		for (size_t i = 0; i < 2; i++)
		{
			bool f;
			nl::node CharGender;

			if (i == 0)
			{
				f = true;
				CharGender = mkinfo.resolve(layout.female_pool);
			}
			else
			{
				f = false;
				CharGender = mkinfo.resolve(layout.male_pool);
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

		for (auto& stattext : stattexts)
			stattext.change_text(std::to_string(STARTING_STAT));

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
		return alay(Point<int16_t>(layout.sign_x, layout.sign_y), x, y);
	}

	DrawArgument UIExplorerCreation::sign_args(const Texture& t, int16_t x, int16_t y) const
	{
		return aargs(t, Point<int16_t>(layout.sign_x, layout.sign_y), x, y);
	}

	Point<int16_t> UIExplorerCreation::flay(int16_t x, int16_t y) const
	{
		return alay(Point<int16_t>(layout.feet_x, layout.feet_y), x, y);
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
			nameboard.draw(sign_args(nameboard, layout.name_x, layout.name_y));
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
				names[i]->draw(slay(layout.row_x + ROW_NAME_DX, static_cast<int16_t>(layout.row_y(i) + ROW_NAME_DY)));

			int16_t stat_x = static_cast<int16_t>(layout.feet_x + STAT_DX);
			int16_t stat_y = static_cast<int16_t>(layout.feet_y + STAT_DY);
			stattable.draw(aargs(stattable, Point<int16_t>(layout.feet_x, layout.feet_y), stat_x, stat_y));

			for (size_t i = 0; i < stattexts.size(); i++)
				stattexts[i].draw(flay(stat_x + STAT_VAL_DX, static_cast<int16_t>(stat_y + STAT_ROW_DY + STAT_ROW_H * static_cast<int16_t>(i))));
		}

		// feet on the scene's platform, at uniform scale like the boards
		newchar.draw(DrawArgument(lay(layout.feet_x, layout.feet_y), ui_scale, ui_scale), inter);

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
		buttons[Buttons::BT_CHARC_OK]->set_position(slay(layout.name_ok_x, layout.name_ok_y));
		buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::NORMAL);
		buttons[Buttons::BT_CHARC_CANCEL]->set_position(slay(layout.name_no_x, layout.name_ok_y));
		buttons[Buttons::BT_CHARC_CANCEL]->set_state(Button::State::NORMAL);

		namechar.set_state(Textfield::State::FOCUSED);
	}

	void UIExplorerCreation::show_look_sign()
	{
		namechar.set_state(Textfield::State::DISABLED);

		buttons[BT_CHARC_CHECK]->set_active(false);

		set_look_buttons(true);

		// naming_ok_pressed() disabled OK/Cancel while the check was pending
		buttons[Buttons::BT_CHARC_OK]->set_position(slay(layout.set_ok_x, layout.set_ok_y));
		buttons[Buttons::BT_CHARC_OK]->set_state(Button::State::NORMAL);
		buttons[Buttons::BT_CHARC_CANCEL]->set_position(slay(layout.set_no_x, layout.set_ok_y));
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

	const std::string& UIExplorerCreation::hair_color_name(uint8_t color)
	{
		// v83 hair ids end in the colour digit
		static const std::string names[] = { "Black", "Red", "Orange", "Blonde", "Green", "Blue", "Violet", "Brown" };
		static const std::string unknown = "Unknown";

		return color < 8 ? names[color] : unknown;
	}
}
