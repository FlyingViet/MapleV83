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
#include "UISkillBook.h"

#include "UIKeyConfig.h"

#include "../UI.h"

#include "../Components/MapleButton.h"
#include "../Components/TextButton.h"
#include "UINotice.h"
#include "../Components/TwoSpriteButton.h"

#include "../../Character/SkillId.h"
#include "../../Data/JobData.h"
#include "../../Data/SkillData.h"
#include "../../Gameplay/Stage.h"

#include "../../Net/Packets/PlayerPackets.h"
#include "../../Net/Packets/MessagingPackets.h"

#include <unordered_set>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// ---- UIWindow.img/Skill/backgrnd (175x289), measured from the v83 bitmap ----

		// Title bar text box spans y 5..18; the 12x12 Basic.img/BtClose sits at its right end.
		constexpr Point<int16_t> CLOSE_POS = Point<int16_t>(159, 6);

		// Tab strip: light grey band y 23..41 (red underline y 42..44), x 5..169.
		// The Tab/enabled|disabled glyphs (dot, I, II, III, IV; 7-16 px wide, 11-12 px
		// tall) carry no origin, so each one is centred in a fifth of the strip
		// (centres x = 21, 54, 87, 121, 154) at y 27 (4 px above, 3 px below).
		constexpr int16_t TAB_Y = 27;
		constexpr int16_t TAB_X[5] = { 16, 51, 81, 113, 146 };

		// Job icon frame (the "SKILL BOOK" placeholder) spans x 7..42, y 50..90.
		constexpr Point<int16_t> BOOK_ICON_CENTER = Point<int16_t>(25, 70);
		// Dark job-name band x 42..166, y 62..80: centred white text.
		constexpr Point<int16_t> BOOK_TEXT_POS = Point<int16_t>(104, 64);

		// White list area x 3..171, y 95..256. skill0/skill1 rows are 141x35 and
		// repeat every 40 px from (11, 95); the last row ends at y 250 and the
		// strip x 152..171 is left for the 12 px wide Basic.img/VScr scrollbar.
		constexpr Point<int16_t> SKILL_OFFSET = Point<int16_t>(11, 95);
		constexpr int16_t ROW_WIDTH = 141;
		constexpr int16_t ROW_HEIGHT = 40;
		constexpr int16_t ROW_ART_HEIGHT = 35;
		// Row art: 36 px icon cell on the left (pink divider at x 36), name line
		// y 0..16 and level line y 17..34 (white divider at y 16).
		constexpr Point<int16_t> ICON_OFFSET = Point<int16_t>(2, 2);     // 32x32 icon inside the icon cell
		constexpr Point<int16_t> NAME_OFFSET = Point<int16_t>(40, 1);    // relative to the row
		constexpr Point<int16_t> LEVEL_OFFSET = Point<int16_t>(40, 19);
		constexpr Point<int16_t> LINE_OFFSET = Point<int16_t>(0, 37);    // 141x1 separator under the row
		constexpr Point<int16_t> SPUP_OFFSET = Point<int16_t>(124, 20);  // 12x12 "+" at the right end of the level line
		constexpr int16_t SLIDER_X = 158;
		constexpr int16_t SLIDER_TOP = 97;
		constexpr int16_t SLIDER_BOTTOM = 254;

		// "SKILL POINT" box x 83..111, y 266..280: right-aligned number.
		constexpr Point<int16_t> SP_LABEL_POS = Point<int16_t>(109, 267);
		// v83 Skill has no macro button; Basic.img/BtMacro (48x18) goes in the
		// bottom band right of the SP box.
		constexpr Point<int16_t> MACRO_BT_POS = Point<int16_t>(121, 264);
		// MapleV83's SP RESET: the free strip between the red tab underline
		// (y 44) and the job band (y 50) is too thin, so it sits on the right
		// end of the dark job-name band.
		constexpr Point<int16_t> RESET_POS = Point<int16_t>(110, 81);
		constexpr Point<int16_t> RESET_DIM = Point<int16_t>(56, 13);

		// ---- UIWindow.img/SkillMacro (207x289), docked at (175, 0) ----
		// Title bar y 5..18, empty band y 20..38, white area x 14..203 / y 46..260,
		// bottom band y 265..285.

		// Basic.img/BtOK2 (47x18) in the empty band under the title.
		constexpr Point<int16_t> MACRO_OK_POS = Point<int16_t>(152, 21);
		// One macroslot2 band (168x42) per macro, centred in the 190 px white area.
		constexpr int16_t MACRO_ROW_X = 25;
		constexpr int16_t MACRO_ROW_Y = 58;
		constexpr int16_t MACRO_ROW_PITCH = 46;                                     // 42 px band + 4 px gap; rows end at y 190
		constexpr Point<int16_t> MACRO_HANDLE_FRAME_OFFSET = Point<int16_t>(3, 2);  // macroslot1 (38x37) inside the band
		constexpr Point<int16_t> MACRO_HANDLE_OFFSET = Point<int16_t>(6, 5);        // 32x32 Macroicon inside macroslot1
		constexpr Point<int16_t> MACRO_SLOTS_OFFSET = Point<int16_t>(41, 2);        // macroslot (124x38) inside the band
		constexpr int16_t MACRO_SLOT_X = 3;                                         // numbered boxes at x 4..33, 41..70, 78..107
		constexpr int16_t MACRO_SLOT_Y = 3;
		constexpr int16_t MACRO_SLOT_PITCH = 37;
		constexpr int16_t MACRO_SLOT_SIZE = 32;
		// macroname (164x57): white name box y 0..41 and a transparent "SHOUT SKILL
		// NAME" strip y 42..56 that is meant to overhang the bottom band, so the
		// frame is placed 42 px above the band.
		constexpr Point<int16_t> MACRO_NAME_POS = Point<int16_t>(27, 218);
		constexpr Point<int16_t> MACRO_NAME_ICON_OFFSET = Point<int16_t>(5, 5);     // preview box x 3..38, y 3..37
		constexpr Point<int16_t> MACRO_NAME_FIELD_LT = Point<int16_t>(42, 5);       // typed name, right of the preview
		constexpr Point<int16_t> MACRO_NAME_FIELD_RB = Point<int16_t>(160, 19);
		constexpr Point<int16_t> MACRO_SHOUT_CHECK_OFFSET = Point<int16_t>(27, 45); // 12x12 CheckBox left of the "SHOUT" text
		constexpr int16_t MACRO_SHOUT_CHECK_HIT = 14;
		constexpr size_t MACRO_NAME_LIMIT = 13;
	}

	UISkillBook::SkillIcon::SkillIcon(int32_t id) : skill_id(id) {}

	void UISkillBook::SkillIcon::drop_on_bindings(Point<int16_t> cursorposition, bool remove) const
	{
		auto keyconfig = UI::get().get_element<UIKeyConfig>();
		if (!keyconfig) return;
		Keyboard::Mapping mapping = Keyboard::Mapping(KeyType::SKILL, skill_id);

		if (remove)
			keyconfig->unstage_mapping(mapping);
		else
			keyconfig->stage_mapping(cursorposition, mapping);
	}

	Icon::IconType UISkillBook::SkillIcon::get_type()
	{
		return Icon::IconType::SKILL;
	}

	UISkillBook::MacroIcon::MacroIcon(int32_t i) : macro_index(i) {}

	void UISkillBook::MacroIcon::drop_on_bindings(Point<int16_t> cursorposition, bool remove) const
	{
		auto keyconfig = UI::get().get_element<UIKeyConfig>();
		if (!keyconfig) return;
		Keyboard::Mapping mapping = Keyboard::Mapping(KeyType::MACRO, macro_index);

		if (remove)
			keyconfig->unstage_mapping(mapping);
		else
			keyconfig->stage_mapping(cursorposition, mapping);
	}

	UISkillBook::SkillDisplayMeta::SkillDisplayMeta(int32_t i, int32_t l) : id(i), level(l)
	{
		const SkillData& data = SkillData::get(id);

		Texture ntx = data.get_icon(SkillData::Icon::NORMAL);
		Texture dtx = data.get_icon(SkillData::Icon::DISABLED);
		Texture motx = data.get_icon(SkillData::Icon::MOUSEOVER);
		icon = std::make_unique<StatefulIcon>(std::make_unique<SkillIcon>(id), ntx, dtx, motx);

		std::string namestr = data.get_name();
		std::string levelstr = std::to_string(level);

		name_text = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR, namestr);
		level_text = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR, levelstr);

		// Name line runs from x 40 to the row's right edge (141) minus the "+" button
		constexpr uint16_t MAX_NAME_WIDTH = 97;
		size_t overhang = 3;

		while (name_text.width() > MAX_NAME_WIDTH)
		{
			namestr.replace(namestr.end() - overhang, namestr.end(), "..");
			overhang += 1;

			name_text.change_text(namestr);
		}
	}

	void UISkillBook::SkillDisplayMeta::draw(const DrawArgument& args) const
	{
		// args is the icon position (row + ICON_OFFSET); text offsets are row-relative
		icon->draw(args.getpos());
		name_text.draw(args + (NAME_OFFSET - ICON_OFFSET));
		level_text.draw(args + (LEVEL_OFFSET - ICON_OFFSET));
	}

	int32_t UISkillBook::SkillDisplayMeta::get_id() const
	{
		return id;
	}

	int32_t UISkillBook::SkillDisplayMeta::get_level() const
	{
		return level;
	}

	StatefulIcon* UISkillBook::SkillDisplayMeta::get_icon() const
	{
		return icon.get();
	}

	UISkillBook::UISkillBook(const CharStats& in_stats, const SkillBook& in_skillbook) : UIDragElement<PosSKILL>(Point<int16_t>(175, 20)), stats(in_stats), skillbook(in_skillbook), sp(0), beginner_sp(0), tab(0), skillcount(0), offset(0), grabbing(false), macro_enabled(false), selected_macro(0)
	{
		nl::node Skill = nl::nx::ui["UIWindow.img"]["Skill"];
		nl::node ui_backgrnd = Skill["backgrnd"];

		bg_dimensions = Texture(ui_backgrnd).get_dimensions();

		sprites.emplace_back(ui_backgrnd);

		// v83 has skill0 (locked, grey) and skill1 (available, blue) only; empty
		// rows simply show the white list area (there is no skillBlank).
		skilld = Skill["skill0"];
		skille = Skill["skill1"];
		line = Skill["line"];

		buttons[Buttons::BT_CLOSE] = std::make_unique<MapleButton>(nl::nx::ui["Basic.img"]["BtClose"], CLOSE_POS);
		buttons[Buttons::BT_MACRO] = std::make_unique<MapleButton>(nl::nx::ui["Basic.img"]["BtMacro"], MACRO_BT_POS);
		buttons[Buttons::BT_RESET] = std::make_unique<TextButton>(RESET_POS, RESET_DIM, "RESET SP");

		nl::node Tab = Skill["Tab"];
		nl::node enabled = Tab["enabled"];
		nl::node disabled = Tab["disabled"];

		for (uint16_t i = Buttons::BT_TAB0; i <= Buttons::BT_TAB4; ++i)
		{
			uint16_t tabid = i - Buttons::BT_TAB0;
			buttons[i] = std::make_unique<TwoSpriteButton>(disabled[tabid], enabled[tabid], Point<int16_t>(TAB_X[tabid], TAB_Y));
		}

		for (uint16_t i = Buttons::BT_SPUP0; i <= Buttons::BT_SPUP3; ++i)
		{
			uint16_t row = i - Buttons::BT_SPUP0;
			Point<int16_t> spup_position = SKILL_OFFSET + SPUP_OFFSET + Point<int16_t>(0, row * ROW_HEIGHT);
			buttons[i] = std::make_unique<MapleButton>(Skill["BtSpUp"], spup_position);
		}

		booktext = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, "", 120);
		splabel = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK);

		// Type LINE_CYAN maps to Basic.img/VScr, which exists in v83
		slider = Slider(
			Slider::Type::LINE_CYAN, Range<int16_t>(SLIDER_TOP, SLIDER_BOTTOM), SLIDER_X, ROWS, 1,
			[&](bool upwards)
			{
				int16_t shift = upwards ? -1 : 1;
				bool above = offset + shift >= 0;
				bool below = offset + ROWS + shift <= skillcount;

				if (above && below)
					change_offset(offset + shift);
			}
		);

		// ---- Macro panel (UIWindow.img/SkillMacro) ----
		nl::node SkillMacro = nl::nx::ui["UIWindow.img"]["SkillMacro"];

		macro_backgrnd = SkillMacro["backgrnd"];
		macro_dimensions = macro_backgrnd.get_dimensions();
		macro_band = SkillMacro["macroslot2"];
		macro_handle_frame = SkillMacro["macroslot1"];
		macro_slots = SkillMacro["macroslot"];
		macro_name_frame = SkillMacro["macroname"];

		nl::node CheckBox = nl::nx::ui["Basic.img"]["CheckBox"];
		macro_check_off = CheckBox["0"];
		macro_check_on = CheckBox["1"];

		nl::node Macroicon = SkillMacro["Macroicon"];

		for (int16_t i = 0; i < MACRO_COUNT; i++)
		{
			macro_names[i] = "";
			macro_shouts[i] = false;
			macro_skills[i][0] = 0;
			macro_skills[i][1] = 0;
			macro_skills[i][2] = 0;

			nl::node icon_src = Macroicon[std::to_string(i)]["icon"];

			// Icon normalises the (0, 32) origin itself, so it gets the raw texture;
			// the copy used for direct drawing is normalised here instead.
			macro_row_icons[i] = std::make_unique<Icon>(std::make_unique<MacroIcon>(i), Texture(icon_src), -1);
			macro_handle_sprites[i] = icon_src;
			macro_handle_sprites[i].shift(macro_handle_sprites[i].get_origin());
		}

		buttons[Buttons::BT_MACRO_OK] = std::make_unique<MapleButton>(nl::nx::ui["Basic.img"]["BtOK2"], Point<int16_t>(bg_dimensions.x(), 0) + MACRO_OK_POS);

		{
			// Textfield bounds are relative to the element position
			Point<int16_t> name_lt = Point<int16_t>(bg_dimensions.x(), 0) + MACRO_NAME_POS;
			macro_name_field = Textfield(
				Text::A11M, Text::LEFT, Color::Name::BLACK,
				Rectangle<int16_t>(name_lt + MACRO_NAME_FIELD_LT, name_lt + MACRO_NAME_FIELD_RB), MACRO_NAME_LIMIT
			);
		}

		change_job(stats.get_stat(MapleStat::Id::JOB));

		set_macro(false);

		dimension = bg_dimensions;
		dragarea = Point<int16_t>(dimension.x(), 20);
	}

	void UISkillBook::draw(float alpha) const
	{
		UIElement::draw_sprites(alpha);

		// bookicon is origin-normalised in change_tab, so centre it by its size
		if (bookicon.is_valid())
			bookicon.draw(position + BOOK_ICON_CENTER - bookicon.get_dimensions() / 2);

		booktext.draw(position + BOOK_TEXT_POS);
		splabel.draw(position + SP_LABEL_POS);

		Point<int16_t> skill_position = position + SKILL_OFFSET;

		for (size_t i = 0; i < ROWS; i++)
		{
			size_t idx = offset + i;

			if (idx < skills.size())
			{
				if (check_required(skills[idx].get_id()))
				{
					skille.draw(skill_position);
				}
				else
				{
					skilld.draw(skill_position);
					skills[idx].get_icon()->set_state(StatefulIcon::State::DISABLED);
				}

				skills[idx].draw(skill_position + ICON_OFFSET);

				if (i < ROWS - 1)
					line.draw(skill_position + LINE_OFFSET);
			}

			skill_position.shift_y(ROW_HEIGHT);
		}

		slider.draw(position);

		if (macro_enabled)
		{
			Point<int16_t> macro_pos = macro_panel_pos();

			macro_backgrnd.draw(macro_pos);

			for (int16_t i = 0; i < MACRO_COUNT; i++)
			{
				Point<int16_t> row_pos = macro_row_pos(i);

				macro_band.draw(row_pos);
				macro_handle_frame.draw(row_pos + MACRO_HANDLE_FRAME_OFFSET);
				macro_slots.draw(row_pos + MACRO_SLOTS_OFFSET);

				// Drag handle: the row's own Macroicon style
				if (macro_handle_sprites[i].is_valid())
					macro_handle_sprites[i].draw(macro_handle_pos(i));

				for (int16_t s = 0; s < 3; s++)
				{
					int32_t skillid = macro_skills[i][s];

					if (skillid <= 0)
						continue;

					Texture icon = SkillData::get(skillid).get_icon(SkillData::Icon::NORMAL);

					if (!icon.is_valid())
						continue;

					// Skill icons carry a bottom-left origin; draw top-left on the slot box
					icon.shift(icon.get_origin());
					icon.draw(macro_slot_pos(i, s));
				}
			}

			// Name box: preview icon of the selected row, the typed name and the shout checkbox
			Point<int16_t> name_pos = macro_name_pos();

			macro_name_frame.draw(name_pos);

			if (macro_handle_sprites[selected_macro].is_valid())
				macro_handle_sprites[selected_macro].draw(name_pos + MACRO_NAME_ICON_OFFSET);

			macro_name_field.draw(position);

			const Texture& check = macro_shouts[selected_macro] ? macro_check_on : macro_check_off;
			check.draw(name_pos + MACRO_SHOUT_CHECK_OFFSET);
		}

		UIElement::draw_buttons(alpha);
	}

	void UISkillBook::update()
	{
		UIElement::update();

		if (macro_enabled)
			macro_name_field.update(position);

		// Drain queued macro skills one per tick when the player is
		// ready to attack. `combat.use_move` is a no-op while the
		// previous skill's animation / cooldown is still running, so
		// we can't just fire all three back to back.
		if (!pending_macro_skills.empty())
		{
			auto& stage = Stage::get();
			if (stage.get_player().can_attack())
			{
				int32_t next_id = pending_macro_skills.front();
				pending_macro_skills.erase(pending_macro_skills.begin());
				stage.get_combat().use_move(next_id);
			}
		}
	}

	Button::State UISkillBook::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::BT_CLOSE:
			close();
			break;
		case Buttons::BT_MACRO:
			set_macro(!macro_enabled);
			break;
		case Buttons::BT_RESET:
			// Cosmic's @resetsp (MapleV83): SP spent on job skills comes back.
			UI::get().emplace<UIYesNo>("Take back the skill points spent on your job skills so you can place them again?",
				[](bool yes)
				{
					if (yes)
						GeneralChatPacket("@resetsp", false).dispatch();
				});
			break;
		case Buttons::BT_TAB0:
		case Buttons::BT_TAB1:
		case Buttons::BT_TAB2:
		case Buttons::BT_TAB3:
		case Buttons::BT_TAB4:
			change_tab(id - Buttons::BT_TAB0);

			return Button::State::PRESSED;
		case Buttons::BT_SPUP0:
		case Buttons::BT_SPUP1:
		case Buttons::BT_SPUP2:
		case Buttons::BT_SPUP3:
			send_spup(id - Buttons::BT_SPUP0 + offset);
			break;
		case Buttons::BT_MACRO_OK:
			save_macros();
			set_macro(false);
			break;
		default:
			break;
		}

		return Button::State::NORMAL;
	}

	void UISkillBook::toggle_active()
	{
		UIElement::toggle_active();
		clear_tooltip();
	}

	void UISkillBook::doubleclick(Point<int16_t> cursorpos)
	{
		const SkillDisplayMeta* skill = skill_by_position(cursorpos - position);

		if (skill)
		{
			int32_t skill_id = skill->get_id();
			int32_t skill_level = skillbook.get_level(skill_id);

			if (skill_level > 0)
				Stage::get().get_combat().use_move(skill_id);
		}
	}

	void UISkillBook::remove_cursor()
	{
		UIDragElement::remove_cursor();

		slider.remove_cursor();
	}

	Cursor::State UISkillBook::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(clicked, cursorpos);

		// The base hover loop resets un-hovered buttons to NORMAL; keep the
		// currently-selected tab visually pressed regardless of the cursor.
		uint16_t active_tab = Buttons::BT_TAB0 + tab;
		if (buttons.count(active_tab) && buttons[active_tab]->is_active())
			buttons[active_tab]->set_state(Button::State::PRESSED);

		if (dragged)
			return dstate;

		if (macro_enabled)
		{
			// Shout checkbox next to the "SHOUT SKILL NAME" strip
			if (clicked)
			{
				Point<int16_t> cb_lt = macro_name_pos() + MACRO_SHOUT_CHECK_OFFSET - Point<int16_t>(1, 1);
				Rectangle<int16_t> cb_rect(cb_lt, cb_lt + Point<int16_t>(MACRO_SHOUT_CHECK_HIT, MACRO_SHOUT_CHECK_HIT));

				if (cb_rect.contains(cursorpos))
				{
					macro_shouts[selected_macro] = !macro_shouts[selected_macro];
					return Cursor::State::CLICKING;
				}
			}

			// Bottom name field (single, edits selected row)
			Cursor::State tstate = macro_name_field.send_cursor(cursorpos, clicked);
			if (tstate != Cursor::State::IDLE)
				return tstate;

			for (int16_t i = 0; i < MACRO_COUNT; i++)
			{
				// Draggable macro handle at the left end of the row
				Point<int16_t> h_lt = macro_handle_pos(i);
				Rectangle<int16_t> h_rect(h_lt, h_lt + Point<int16_t>(MACRO_SLOT_SIZE, MACRO_SLOT_SIZE));

				if (h_rect.contains(cursorpos))
				{
					if (clicked)
					{
						macro_select_row(i);

						if (macro_row_icons[i])
						{
							macro_row_icons[i]->start_drag(cursorpos - h_lt);
							UI::get().drag_icon(macro_row_icons[i].get());
						}

						return Cursor::State::GRABBING;
					}

					return Cursor::State::CANGRAB;
				}

				// Anywhere else on the band selects the row for name editing
				Point<int16_t> row_lt = macro_row_pos(i);
				Rectangle<int16_t> row_rect(row_lt, row_lt + macro_band.get_dimensions());

				if (clicked && row_rect.contains(cursorpos))
				{
					macro_select_row(i);
					return Cursor::State::CLICKING;
				}
			}
		}

		Point<int16_t> cursor_relative = cursorpos - position;

		if (slider.isenabled())
		{
			if (Cursor::State new_state = slider.send_cursor(cursor_relative, clicked))
			{
				clear_tooltip();

				return new_state;
			}
		}

		// UIDragElement::send_cursor already called UIElement::send_cursor
		// which handles button clicks. Check if a button was hit and return early.
		if (clicked)
		{
			for (uint16_t i = Buttons::BT_SPUP0; i <= Buttons::BT_SPUP3; i++)
			{
				if (buttons[i]->is_active() && buttons[i]->bounds(position).contains(cursorpos))
					return dstate;
			}
			if (buttons[Buttons::BT_CLOSE]->is_active() && buttons[Buttons::BT_CLOSE]->bounds(position).contains(cursorpos))
				return dstate;
			if (buttons[Buttons::BT_MACRO]->is_active() && buttons[Buttons::BT_MACRO]->bounds(position).contains(cursorpos))
				return dstate;
			if (buttons[Buttons::BT_RESET]->is_active() && buttons[Buttons::BT_RESET]->bounds(position).contains(cursorpos))
				return dstate;
		}

		Point<int16_t> skill_position = position + SKILL_OFFSET;

		if (!grabbing)
		{
			for (size_t i = 0; i < ROWS; i++)
			{
				size_t idx = offset + i;

				if (idx >= skills.size())
					break;

				constexpr Rectangle<int16_t> bounds = Rectangle<int16_t>(0, ROW_WIDTH, 0, ROW_ART_HEIGHT);
				Point<int16_t> icon_pos = skill_position + ICON_OFFSET;
				bool inrange = bounds.contains(cursorpos - skill_position);

				if (inrange)
				{
					if (clicked)
					{
						clear_tooltip();
						grabbing = true;

						int32_t skill_id = skills[idx].get_id();
						int32_t skill_level = skillbook.get_level(skill_id);

						if (skill_level > 0 && !SkillData::get(skill_id).is_passive())
						{
							skills[idx].get_icon()->start_drag(cursorpos - icon_pos);
							UI::get().drag_icon(skills[idx].get_icon());

							return Cursor::State::GRABBING;
						}
						else
						{
							return Cursor::State::IDLE;
						}
					}
					else
					{
						skills[idx].get_icon()->set_state(StatefulIcon::State::MOUSEOVER);
						show_skill(skills[idx].get_id());

						return Cursor::State::IDLE;
					}
				}

				skill_position.shift_y(ROW_HEIGHT);
			}

			for (size_t i = 0; i < skills.size(); i++)
			{
				skills[i].get_icon()->set_state(StatefulIcon::State::NORMAL);
			}
			clear_tooltip();
		}
		else
		{
			grabbing = false;
		}

		return dstate;
	}

	void UISkillBook::send_scroll(double yoffset)
	{
		if (!slider.isenabled())
			return;

		int16_t new_offset = offset;

		if (yoffset < 0)
			new_offset = std::min<int16_t>(offset + 1, skillcount > ROWS ? skillcount - ROWS : 0);
		else if (yoffset > 0)
			new_offset = std::max<int16_t>(offset - 1, 0);

		if (new_offset != offset)
		{
			change_offset(new_offset);
			slider.setrows(new_offset, ROWS, skillcount);
		}
	}

	void UISkillBook::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				close();
			}
			else if (keycode == KeyAction::Id::TAB)
			{
				clear_tooltip();

				Job::Level level = job.get_level();
				uint16_t id = tab + 1;
				uint16_t new_tab = tab + Buttons::BT_TAB0;

				if (new_tab < Buttons::BT_TAB4 && id <= level)
					new_tab++;
				else
					new_tab = Buttons::BT_TAB0;

				change_tab(new_tab - Buttons::BT_TAB0);
			}
		}
	}

	UIElement::Type UISkillBook::get_type() const
	{
		return TYPE;
	}

	void UISkillBook::update_stat(MapleStat::Id stat, int32_t value)
	{
		switch (stat)
		{
		case MapleStat::Id::JOB:
			change_job(value);
			break;
		case MapleStat::Id::SP:
			change_sp();
			break;
		}
	}

	void UISkillBook::update_skills(int32_t skill_id)
	{
		change_tab(tab);
	}

	void UISkillBook::change_job(uint16_t id)
	{
		job.change_job(id);

		Job::Level level = job.get_level();

		for (uint16_t i = 0; i <= Job::Level::FOURTH; i++)
			buttons[Buttons::BT_TAB0 + i]->set_active(i <= level);

		change_tab(level - Job::Level::BEGINNER);
	}

	void UISkillBook::change_sp()
	{
		Job::Level joblevel = joblevel_by_tab(tab);
		uint16_t level = stats.get_stat(MapleStat::Id::LEVEL);
		if (joblevel == Job::Level::BEGINNER)
		{
			int16_t remaining_beginner_sp = 0;

			if (level >= 7)
				remaining_beginner_sp = 6;
			else
				remaining_beginner_sp = level - 1;

			for (size_t i = 0; i < skills.size(); i++)
			{
				int32_t skillid = skills[i].get_id();
				int32_t base = skillid % 10000;

				// Three Snails/Recovery/Nimble Feet (works for Explorer, Noblesse, Aran)
				if (base == 1000 || base == 1001 || base == 1002)
					remaining_beginner_sp -= skills[i].get_level();
			}

			beginner_sp = remaining_beginner_sp;
			splabel.change_text(std::to_string(beginner_sp));
		}
		else
		{
			sp = stats.get_stat(MapleStat::Id::SP);
			splabel.change_text(std::to_string(sp));
		}

		change_offset(offset);
	}

	void UISkillBook::change_tab(uint16_t new_tab)
	{
		buttons[Buttons::BT_TAB0 + tab]->set_state(Button::NORMAL);
		buttons[Buttons::BT_TAB0 + new_tab]->set_state(Button::PRESSED);
		tab = new_tab;

		skills.clear();
		skillcount = 0;

		Job::Level joblevel = joblevel_by_tab(tab);
		uint16_t subid = job.get_subjob(joblevel);

		const JobData& data = JobData::get(subid);
		// Normalise the origin so draw() can centre the icon in the frame by size
		bookicon = data.get_icon();
		bookicon.shift(bookicon.get_origin());
		booktext.change_text(data.get_name());

		// Union of two sources, both filtered to prefix == subid:
		//  (a) NX Skill.wz/<subid>.img - full job tree, so unlearned
		//      skills appear as grey 0-level rows.
		//  (b) skillbook entries the server has granted whose prefix
		//      (sid / 10000) matches subid - covers anything the server
		//      assigned that NX doesn't list.
		// Both lists are filtered by prefix so unrelated branches
		// (e.g. Aran skills on an Explorer beginner) never appear.
		std::unordered_set<int32_t> seen;
		auto push = [&](int32_t sid)
		{
			// Common prefix-0 skills (000.img, e.g. Monster Riding 1004)
			// belong on the beginner tab of every tree - GM (900) included.
			bool common_on_beginner = sid / 10000 == 0 && joblevel == Job::Level::BEGINNER;

			if (sid / 10000 != static_cast<int32_t>(subid) && !common_on_beginner)
				return;
			// NX flags mount/item-tied skills (Balrog, spaceship, custom
			// riding) and many GM/utility skills with invisible = 1. Hide those
			// only while UNLEARNED - a learned invisible skill (e.g. a GM's Hide
			// or a granted skill) should still show, matching the vanilla client.
			if (SkillData::get(sid).is_invisible() && skillbook.get_level(sid) == 0)
				return;
			if (!seen.insert(sid).second)
				return;
			skills.emplace_back(sid, skillbook.get_level(sid));
			skillcount++;
		};

		for (int32_t sid : data.get_skills())
			push(sid);

		for (auto& entry : skillbook.get_entries())
			push(entry.first);

		slider.setrows(ROWS, skillcount);
		offset = 0;
		change_sp();
	}

	void UISkillBook::change_offset(uint16_t new_offset)
	{
		offset = new_offset;

		for (int16_t i = 0; i < ROWS; i++)
		{
			uint16_t index = Buttons::BT_SPUP0 + i;
			uint16_t row = offset + i;
			buttons[index]->set_active(row < skillcount);

			if (row < skills.size())
			{
				int32_t skill_id = skills[row].get_id();
				bool canraise = can_raise(skill_id);
				buttons[index]->set_state(canraise ? Button::State::NORMAL : Button::State::DISABLED);
			}
		}
	}

	void UISkillBook::show_skill(int32_t id)
	{
		int32_t skill_id = id;
		int32_t level = skillbook.get_level(id);
		int32_t masterlevel = skillbook.get_masterlevel(id);
		int64_t expiration = skillbook.get_expiration(id);

		UI::get().show_skill(Tooltip::Parent::SKILLBOOK, skill_id, level, masterlevel, expiration);
	}

	void UISkillBook::clear_tooltip()
	{
		UI::get().clear_tooltip(Tooltip::Parent::SKILLBOOK);
	}

	bool UISkillBook::can_raise(int32_t skill_id) const
	{
		Job::Level joblevel = joblevel_by_tab(tab);

		if (joblevel == Job::Level::BEGINNER && beginner_sp <= 0)
			return false;

		if (tab + Buttons::BT_TAB0 != Buttons::BT_TAB0 && sp <= 0)
			return false;

		int32_t level = skillbook.get_level(skill_id);
		int32_t masterlevel = skillbook.get_masterlevel(skill_id);

		if (masterlevel == 0)
			masterlevel = SkillData::get(skill_id).get_masterlevel();

		if (level >= masterlevel)
			return false;

		int32_t base_id = skill_id % 10000;

		switch (base_id)
		{
		case SkillId::Id::ANGEL_BLESSING:
			return false;
		default:
			return check_required(skill_id);
		}
	}

	void UISkillBook::send_spup(uint16_t row)
	{
		if (row >= skills.size())
			return;

		int32_t id = skills[row].get_id();

		std::string sp_txt = splabel.get_text();
		int16_t cur_sp = sp_txt.empty() ? 0 : std::stoi(sp_txt);

		if (cur_sp <= 0)
			return;

		spend_sp(id);
		change_sp();
	}

	void UISkillBook::spend_sp(int32_t skill_id)
	{
		SpendSpPacket(skill_id).dispatch();
	}

	Job::Level UISkillBook::joblevel_by_tab(uint16_t t) const
	{
		switch (t)
		{
		case 1:
			return Job::Level::FIRST;
		case 2:
			return Job::Level::SECOND;
		case 3:
			return Job::Level::THIRD;
		case 4:
			return Job::Level::FOURTH;
		default:
			return Job::Level::BEGINNER;
		}
	}

	const UISkillBook::SkillDisplayMeta* UISkillBook::skill_by_position(Point<int16_t> cursorpos) const
	{
		int16_t x = cursorpos.x();

		if (x < SKILL_OFFSET.x() || x > SKILL_OFFSET.x() + ROW_WIDTH)
			return nullptr;

		int16_t y = cursorpos.y();

		if (y < SKILL_OFFSET.y())
			return nullptr;

		uint16_t row = (y - SKILL_OFFSET.y()) / ROW_HEIGHT;

		if (row >= ROWS)
			return nullptr;

		uint16_t skill_idx = offset + row;

		if (skill_idx >= skills.size())
			return nullptr;

		return skills.data() + skill_idx;
	}

	void UISkillBook::close()
	{
		clear_tooltip();
		deactivate();
	}

	bool UISkillBook::check_required(int32_t id) const
	{
		std::unordered_map<int32_t, int32_t> required = skillbook.collect_required(id);

		if (required.size() <= 0)
			required = SkillData::get(id).get_reqskills();

		for (auto reqskill : required)
		{
			int32_t reqskill_level = skillbook.get_level(reqskill.first);
			int32_t req_level = reqskill.second;

			if (reqskill_level < req_level)
				return false;
		}

		return true;
	}

	/// Macro panel geometry
	Point<int16_t> UISkillBook::macro_panel_pos() const
	{
		return position + Point<int16_t>(bg_dimensions.x(), 0);
	}

	Point<int16_t> UISkillBook::macro_row_pos(int16_t row) const
	{
		return macro_panel_pos() + Point<int16_t>(MACRO_ROW_X, MACRO_ROW_Y + row * MACRO_ROW_PITCH);
	}

	Point<int16_t> UISkillBook::macro_handle_pos(int16_t row) const
	{
		return macro_row_pos(row) + MACRO_HANDLE_OFFSET;
	}

	Point<int16_t> UISkillBook::macro_slot_pos(int16_t row, int16_t slot) const
	{
		return macro_row_pos(row) + MACRO_SLOTS_OFFSET + Point<int16_t>(MACRO_SLOT_X + slot * MACRO_SLOT_PITCH, MACRO_SLOT_Y);
	}

	Point<int16_t> UISkillBook::macro_name_pos() const
	{
		return macro_panel_pos() + MACRO_NAME_POS;
	}

	void UISkillBook::set_macro(bool enabled)
	{
		macro_enabled = enabled;

		if (macro_enabled)
		{
			// Extend dimension to cover the docked macro panel so drops over it hit this UI.
			dimension = Point<int16_t>(
				bg_dimensions.x() + macro_dimensions.x(),
				std::max<int16_t>(bg_dimensions.y(), macro_dimensions.y())
			);
		}
		else
		{
			dimension = bg_dimensions;
		}

		buttons[Buttons::BT_MACRO_OK]->set_active(macro_enabled);
		buttons[Buttons::BT_MACRO_OK]->set_state(macro_enabled ? Button::State::NORMAL : Button::State::DISABLED);

		if (macro_enabled)
		{
			macro_name_field.change_text(macro_names[selected_macro]);
			// Focus immediately so typing starts working without a
			// second click.
			macro_name_field.set_state(Textfield::State::FOCUSED);
		}
		else
		{
			macro_name_field.set_state(Textfield::State::DISABLED);
		}
	}

	void UISkillBook::macro_select_row(int16_t row)
	{
		if (row < 0 || row >= MACRO_COUNT)
			return;

		// Persist current field text into the previously selected macro
		macro_names[selected_macro] = macro_name_field.get_text();

		selected_macro = row;
		macro_name_field.change_text(macro_names[selected_macro]);
	}

	void UISkillBook::load_macro(uint8_t index, const std::string& name, bool shout, int32_t s1, int32_t s2, int32_t s3)
	{
		if (index >= MACRO_COUNT)
			return;

		macro_names[index] = name;
		macro_shouts[index] = shout;
		macro_skills[index][0] = s1;
		macro_skills[index][1] = s2;
		macro_skills[index][2] = s3;

		if (index == selected_macro)
			macro_name_field.change_text(name);
	}

	void UISkillBook::trigger_macro(int32_t index)
	{
		if (index < 0 || index >= MACRO_COUNT)
			return;

		// Queue all 3 skills - update() fires them one at a time as
		// the player becomes ready to attack again, so the second and
		// third don't get swallowed by can_attack() == false.
		pending_macro_skills.clear();
		for (int32_t s = 0; s < 3; s++)
		{
			int32_t skill_id = macro_skills[index][s];
			if (skill_id > 0)
				pending_macro_skills.push_back(skill_id);
		}

		// Shout the macro name so nearby players see it in chat.
		if (macro_shouts[index] && !macro_names[index].empty())
			GeneralChatPacket(macro_names[index], true).dispatch();
	}

	void UISkillBook::save_macros()
	{
		// Persist the currently edited name into the selected row
		macro_names[selected_macro] = macro_name_field.get_text();

		SkillMacroModifiedPacket::MacroData data[MACRO_COUNT];

		for (int16_t i = 0; i < MACRO_COUNT; i++)
		{
			data[i].name = macro_names[i];
			data[i].shout = macro_shouts[i];
			data[i].skill1 = macro_skills[i][0];
			data[i].skill2 = macro_skills[i][1];
			data[i].skill3 = macro_skills[i][2];
		}

		SkillMacroModifiedPacket(data, MACRO_COUNT).dispatch();
	}

	bool UISkillBook::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		if (!macro_enabled)
			return true;

		if (const_cast<Icon&>(icon).get_type() != Icon::IconType::SKILL)
			return true;

		int32_t skill_id = icon.get_action_id();
		if (skill_id == 0)
			return true;

		for (int16_t i = 0; i < MACRO_COUNT; i++)
		{
			for (int16_t s = 0; s < 3; s++)
			{
				Point<int16_t> slot_lt = macro_slot_pos(i, s);
				Rectangle<int16_t> slot_rect(slot_lt, slot_lt + Point<int16_t>(MACRO_SLOT_SIZE, MACRO_SLOT_SIZE));

				if (slot_rect.contains(cursorpos))
				{
					macro_skills[i][s] = skill_id;
					return true;
				}
			}
		}

		return true;
	}
}
