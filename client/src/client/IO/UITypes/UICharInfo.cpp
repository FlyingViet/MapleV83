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
#include "UICharInfo.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "UIChatBar.h"

#include "../../Gameplay/Stage.h"

#include "../../Net/Packets/PlayerInteractionPackets.h"
#include "../../Net/Packets/TradePackets.h"
#include "../../Net/Packets/SocialPackets.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/BotInventoryPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UICharInfo::UICharInfo(int32_t cid) : UIDragElement<PosCHARINFO>(Point<int16_t>(WIN_W, DRAG_H)),
		target_char_id(cid), bot_tab_active(false), bot_sub_tab(4), bot_scroll_row(0)
	{
		nl::node basic = nl::nx::ui["Basic.img"];
		nl::node UserInfo = nl::nx::ui["UIWindow.img"]["UserInfo"];

		main_bg = UserInfo["backgrnd"];
		pet_bg = UserInfo["backgrnd2"];
		taming_bg = UserInfo["backgrnd5"];
		wish_bg = UserInfo["backgrnd3"];

		buttons[Buttons::BtClose] = std::make_unique<MapleButton>(basic["BtClose"], CLOSE_POS);
		buttons[Buttons::BtParty] = std::make_unique<MapleButton>(UserInfo["BtParty"], PARTY_POS);
		buttons[Buttons::BtTrade] = std::make_unique<MapleButton>(UserInfo["BtTrade"], TRADE_POS);
		buttons[Buttons::BtFamily] = std::make_unique<MapleButton>(UserInfo["BtFamily"], FAMILY_POS);

		// v83 UserInfo has no fame arrows; Basic.img's 12x12 up / down glyphs
		// sit at the right end of the FAME field instead
		buttons[Buttons::BtPopUp] = std::make_unique<MapleButton>(basic["BtUP"], FAME_UP_POS);
		buttons[Buttons::BtPopDown] = std::make_unique<MapleButton>(basic["BtDown"], FAME_DOWN_POS);

		// Bottom strip; set_panel() moves these to the current background's strip
		buttons[Buttons::BtItem] = std::make_unique<MapleButton>(UserInfo["BtItem"]);
		buttons[Buttons::BtWish] = std::make_unique<MapleButton>(UserInfo["BtWish"]);
		buttons[Buttons::BtPetShow] = std::make_unique<MapleButton>(UserInfo["BtPetShow"]);
		buttons[Buttons::BtPetHide] = std::make_unique<MapleButton>(UserInfo["BtPetHide"]);
		buttons[Buttons::BtTamingShow] = std::make_unique<MapleButton>(UserInfo["BtTamingShow"]);
		buttons[Buttons::BtTamingHide] = std::make_unique<MapleButton>(UserInfo["BtTamingHide"]);

		// Pet selector icons (30x27, origin 15,13) in the three boxes of the pet section
		for (uint16_t i = 0; i < 3; i++)
		{
			std::string key = "BtPet" + std::to_string(i + 1);
			buttons[Buttons::BtPet1 + i] = std::make_unique<MapleButton>(UserInfo[key],
				Point<int16_t>(PET_SLOT_X + i * PET_SLOT_PITCH, PET_SLOT_Y));
		}

		// Nothing behind the family button yet, and the item view only opens
		// once a bot inventory has arrived
		buttons[Buttons::BtFamily]->set_state(Button::State::DISABLED);
		buttons[Buttons::BtItem]->set_state(Button::State::DISABLED);
		buttons[Buttons::BtPetShow]->set_state(Button::State::DISABLED);
		buttons[Buttons::BtTamingShow]->set_state(Button::State::DISABLED);

		name = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::WHITE);
		job = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK);
		level = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK);
		fame = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK);
		guild = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK);
		alliance = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK);
		panel_label = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK);

		// Bot inventory view: v83 UIWindow.img/Item window and its tab glyphs
		nl::node Item = nl::nx::ui["UIWindow.img"]["Item"];
		nl::node Tab = Item["Tab"];
		nl::node taben = Tab["enabled"];
		nl::node tabdis = Tab["disabled"];

		bot_backgrnd = Item["backgrnd"];
		bot_slot_disabled = Item["disabled"];

		// The tab strip is baked into backgrnd; Tab/enabled|disabled/0-4 are
		// label glyphs (origin 0,0) in the order Equip, Use, Set-up, Etc, Cash.
		// Our fifth tab shows the bot's equipped gear under the Cash glyph slot.
		const uint16_t tab_buttons[5] = {
			Buttons::BtBotEquip, Buttons::BtBotUse, Buttons::BtBotSetup, Buttons::BtBotEtc, Buttons::BtBotEquipped
		};

		for (int16_t i = 0; i < 5; i++)
		{
			std::string key = std::to_string(i);
			Point<int16_t> cell = Point<int16_t>(BOT_TAB_X + BOT_TAB_PITCH * i, BOT_TAB_Y);
			Point<int16_t> ndim = Texture(tabdis[key]).get_dimensions();
			Point<int16_t> sdim = Texture(taben[key]).get_dimensions();
			Point<int16_t> npos = cell + Point<int16_t>((BOT_TAB_PITCH - ndim.x()) / 2, (BOT_TAB_H - ndim.y()) / 2);
			Point<int16_t> spos = cell + Point<int16_t>((BOT_TAB_PITCH - sdim.x()) / 2, (BOT_TAB_H - sdim.y()) / 2);

			buttons[tab_buttons[i]] = std::make_unique<TwoSpriteButton>(tabdis[key], taben[key], npos, spos);
			buttons[tab_buttons[i]]->set_active(false);
		}

		bot_name_label = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK);
		bot_level_label = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK);
		bot_meso_label = Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::BLACK);

		set_panel(PANEL_NONE);

		CharInfoRequestPacket(cid).dispatch();
	}

	void UICharInfo::set_panel(PanelMode mode)
	{
		panel_mode = mode;

		int16_t height = (mode == PANEL_PET) ? PET_WIN_H : (mode == PANEL_TAMING) ? TAMING_WIN_H : WIN_H;
		int16_t strip_y = height - STRIP_BTN_FROM_BOTTOM;

		dimension = Point<int16_t>(WIN_W, height);
		dragarea = Point<int16_t>(WIN_W, DRAG_H);

		buttons[Buttons::BtItem]->set_position(Point<int16_t>(ITEM_X, strip_y));
		buttons[Buttons::BtWish]->set_position(Point<int16_t>(WISH_X, strip_y));
		buttons[Buttons::BtPetShow]->set_position(Point<int16_t>(PET_X, strip_y));
		buttons[Buttons::BtPetHide]->set_position(Point<int16_t>(PET_X, strip_y));
		buttons[Buttons::BtTamingShow]->set_position(Point<int16_t>(TAMING_X, strip_y));
		buttons[Buttons::BtTamingHide]->set_position(Point<int16_t>(TAMING_X, strip_y));

		buttons[Buttons::BtPetShow]->set_active(mode != PANEL_PET);
		buttons[Buttons::BtPetHide]->set_active(mode == PANEL_PET);
		buttons[Buttons::BtTamingShow]->set_active(mode != PANEL_TAMING);
		buttons[Buttons::BtTamingHide]->set_active(mode == PANEL_TAMING);

		for (uint16_t i = 0; i < 3; i++)
		{
			bool shown = mode == PANEL_PET && i < pets_info.size();
			buttons[Buttons::BtPet1 + i]->set_active(shown);

			if (shown)
				buttons[Buttons::BtPet1 + i]->set_state(i == selected_pet ? Button::State::PRESSED : Button::State::NORMAL);
		}
	}

	void UICharInfo::set_bot_view(bool enable)
	{
		bot_tab_active = enable;

		for (uint16_t i = Buttons::BtBotEquip; i <= Buttons::BtBotEquipped; i++)
			buttons[i]->set_active(enable);

		if (enable)
		{
			dimension = Point<int16_t>(BOT_W, BOT_H);
			dragarea = Point<int16_t>(BOT_W, DRAG_H);
			buttons[Buttons::BtClose]->set_position(BOT_CLOSE_POS);
		}
		else
		{
			buttons[Buttons::BtClose]->set_position(CLOSE_POS);
			set_panel(panel_mode);
		}
	}

	void UICharInfo::draw(float inter) const
	{
		if (bot_tab_active && bot_inventory.is_valid())
		{
			draw_bot_inventory(inter);
			return;
		}

		draw_main(inter);
	}

	void UICharInfo::draw_main(float inter) const
	{
		const Texture& bg = (panel_mode == PANEL_PET) ? pet_bg : (panel_mode == PANEL_TAMING) ? taming_bg : main_bg;
		bg.draw(DrawArgument(position));

		// Drawn from the snapshot, not the live Char, so the portrait survives the
		// target walking out of view or the map changing while this stays open.
		if (has_preview)
			preview_look.draw(position + PORTRAIT_FEET, false, Stance::Id::STAND1, Expression::Id::DEFAULT);

		name.draw(position + NAME_PLATE);
		level.draw(position + Point<int16_t>(STAT_X, STAT_Y + STAT_PITCH * 0));
		job.draw(position + Point<int16_t>(STAT_X, STAT_Y + STAT_PITCH * 1));
		fame.draw(position + Point<int16_t>(STAT_X, STAT_Y + STAT_PITCH * 2));
		guild.draw(position + Point<int16_t>(STAT_X, STAT_Y + STAT_PITCH * 3));
		alliance.draw(position + Point<int16_t>(STAT_X, STAT_Y + STAT_PITCH * 4));

		if (panel_mode == PANEL_PET)
			draw_pet_panel();
		else if (panel_mode == PANEL_TAMING)
			draw_taming_panel();

		if (wish_open)
			draw_wish_panel();

		for (auto& btn : buttons)
			if (btn.first < Buttons::BtBotEquip && btn.second && btn.second->is_active())
				btn.second->draw(position);
	}

	void UICharInfo::draw_pet_panel() const
	{
		size_t shown = std::min<size_t>(pets_info.size(), 3);

		// Pet list on the right: icon + name per pet
		for (size_t i = 0; i < shown; i++)
		{
			const CharPetInfo& pi = pets_info[i];
			const ItemData& pd = ItemData::get(pi.itemid);
			int16_t row_y = PET_ROW_Y + static_cast<int16_t>(i) * PET_ROW_PITCH;

			// Item icons have their origin at the bottom-left corner
			if (pd.is_valid())
				pd.get_icon(false).draw(DrawArgument(position + Point<int16_t>(PET_ROW_X, row_y + 32)));

			panel_label.change_text(pi.name);
			panel_label.draw(position + Point<int16_t>(PET_ROW_NAME_X, row_y + 10));
		}

		if (shown == 0)
			return;

		// Portrait and the TYPE / LEVEL / CLOSENESS / FULLNESS fields describe the selected pet
		size_t sel = std::min<size_t>(static_cast<size_t>(selected_pet), shown - 1);
		const CharPetInfo& pi = pets_info[sel];
		const ItemData& pd = ItemData::get(pi.itemid);

		Texture stand = nl::nx::item["Pet"][std::to_string(pi.itemid) + ".img"]["stand0"]["0"];

		if (stand.is_valid())
			stand.draw(DrawArgument(position + PET_FEET));
		else if (pd.is_valid())
			pd.get_icon(false).draw(DrawArgument(position + PET_FEET + Point<int16_t>(-16, 0)));

		std::string type = pd.is_valid() ? pd.get_name() : "";

		if (type.size() > 12)
			type = type.substr(0, 11) + "..";

		panel_label.change_text(pi.name);
		panel_label.draw(position + PET_NAME_PLATE + Point<int16_t>(-panel_label.width() / 2, 0));
		panel_label.change_text(type);
		panel_label.draw(position + PET_TYPE_POS);
		panel_label.change_text(std::to_string(pi.level));
		panel_label.draw(position + PET_LEVEL_POS);
		panel_label.change_text(std::to_string(pi.closeness));
		panel_label.draw(position + PET_CLOSE_POS);
		panel_label.change_text(std::to_string(pi.fullness));
		panel_label.draw(position + PET_FULL_POS);
	}

	void UICharInfo::draw_taming_panel() const
	{
		// The server only tells us level / exp / tiredness, not which mount
		panel_label.change_text("Mount");
		panel_label.draw(position + MOUNT_NAME_PLATE + Point<int16_t>(-panel_label.width() / 2, 0));
		panel_label.change_text("-");
		panel_label.draw(position + MOUNT_TYPE_POS);
		panel_label.change_text(std::to_string(mount_level));
		panel_label.draw(position + MOUNT_LEVEL_POS);
		panel_label.change_text(std::to_string(mount_exp));
		panel_label.draw(position + MOUNT_EXP_POS);
		panel_label.change_text(std::to_string(mount_tired));
		panel_label.draw(position + MOUNT_TIRED_POS);
	}

	void UICharInfo::draw_wish_panel() const
	{
		Point<int16_t> wpos = position + Point<int16_t>(WIN_W + WISH_PANEL_GAP, 0);
		wish_bg.draw(DrawArgument(wpos));

		int16_t shown = 0;

		for (size_t i = 0; i < wishlist.size() && shown < WISH_MAX_ROWS; i++, shown++)
		{
			const ItemData& wd = ItemData::get(wishlist[i]);
			int16_t wy = WISH_ROW_Y + shown * WISH_ROW_PITCH;

			if (wd.is_valid())
			{
				wd.get_icon(false).draw(DrawArgument(wpos + Point<int16_t>(WISH_ICON_X, wy + 34)));

				std::string wn = wd.get_name();

				if (wn.size() > 22)
					wn = wn.substr(0, 21) + "..";

				panel_label.change_text(wn);
				panel_label.draw(wpos + Point<int16_t>(WISH_TEXT_X, wy + 6));
			}
		}

		if (wishlist.empty())
		{
			panel_label.change_text("No wish list items.");
			panel_label.draw(wpos + Point<int16_t>(WISH_TEXT_X, WISH_ROW_Y + 6));
		}
		else if (wishlist.size() > WISH_MAX_ROWS)
		{
			panel_label.change_text("+" + std::to_string(wishlist.size() - WISH_MAX_ROWS) + " more");
			panel_label.draw(wpos + Point<int16_t>(WISH_TEXT_X, WISH_ROW_Y + (WISH_MAX_ROWS - 1) * WISH_ROW_PITCH + 20));
		}
	}

	void UICharInfo::update()
	{
		// Keep a copy of the look while the target is still on screen. Once they
		// despawn their Char is gone, and this snapshot is all the portrait has.
		if (Char* c = target())
		{
			preview_look = c->get_look_preview();
			has_preview = true;
		}
	}

	Button::State UICharInfo::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BtClose:
			if (bot_tab_active)
				set_bot_view(false);
			else
				deactivate();
			return Button::State::NORMAL;
		case Buttons::BtFamily:
			return Button::State::DISABLED;
		case Buttons::BtParty:
			if (target())
				InviteToPartyPacket(target()->get_name()).dispatch();
			deactivate();
			return Button::State::NORMAL;
		case Buttons::BtTrade:
			if (target())
			{
				TradeCreatePacket().dispatch();
				TradeInvitePacket(target()->get_oid()).dispatch();
			}
			deactivate();
			return Button::State::NORMAL;
		case Buttons::BtItem:
			if (bot_inventory.is_valid())
			{
				set_bot_view(true);
				load_bot_icons();
			}
			return Button::State::NORMAL;
		case Buttons::BtWish:
			wish_open = !wish_open;
			return Button::State::NORMAL;
		case Buttons::BtPetShow:
			set_panel(PANEL_PET);
			return Button::State::NORMAL;
		case Buttons::BtPetHide:
			set_panel(PANEL_NONE);
			return Button::State::NORMAL;
		case Buttons::BtTamingShow:
			set_panel(PANEL_TAMING);
			return Button::State::NORMAL;
		case Buttons::BtTamingHide:
			set_panel(PANEL_NONE);
			return Button::State::NORMAL;
		case Buttons::BtPet1:
		case Buttons::BtPet2:
		case Buttons::BtPet3:
			selected_pet = static_cast<int8_t>(buttonid - Buttons::BtPet1);
			set_panel(panel_mode);
			return Button::State::PRESSED;
		case Buttons::BtPopUp:
		case Buttons::BtPopDown:
			// Cosmic silently drops fame packets from characters below
			// level 15 (and self-fames) — tell the player instead of
			// looking like a dead button.
			if (Stage::get().get_player().get_level() < 15)
			{
				chat::log("You must be at least Level 15 to raise or drop someone's fame.", chat::LineType::RED);
			}
			else if (target())
			{
				GiveFamePacket(target()->get_oid(), buttonid == Buttons::BtPopUp).dispatch();
			}
			else
			{
				chat::log("That character is no longer nearby.", chat::LineType::RED);
			}
			return Button::State::NORMAL;
		case Buttons::BtBotEquip:
		case Buttons::BtBotUse:
		case Buttons::BtBotSetup:
		case Buttons::BtBotEtc:
		case Buttons::BtBotEquipped:
			bot_sub_tab = static_cast<int8_t>(buttonid - Buttons::BtBotEquip);
			bot_scroll_row = 0;
			load_bot_icons();

			for (uint16_t i = Buttons::BtBotEquip; i <= Buttons::BtBotEquipped; i++)
				buttons[i]->set_state(i == buttonid ? Button::State::PRESSED : Button::State::NORMAL);

			return Button::State::PRESSED;
		default:
			break;
		}

		return Button::State::DISABLED;
	}

	bool UICharInfo::is_in_range(Point<int16_t> cursorpos) const
	{
		Point<int16_t> extent = dimension;

		// The wish list hangs off the right edge of the window
		if (wish_open && !bot_tab_active)
			extent = Point<int16_t>(WIN_W + WISH_PANEL_GAP + WISH_PANEL_W, std::max<int16_t>(dimension.y(), WISH_PANEL_H));

		Rectangle<int16_t> bounds = Rectangle<int16_t>(position, position + extent);
		return bounds.contains(cursorpos);
	}

	void UICharInfo::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			if (bot_tab_active)
				set_bot_view(false);
			else
				deactivate();
		}
	}

	UIElement::Type UICharInfo::get_type() const
	{
		return TYPE;
	}

	void UICharInfo::update_stats(int32_t character_id, int16_t job_id, int16_t lv, int16_t f, std::string g, std::string a)
	{
		int32_t player_id = Stage::get().get_player().get_oid();

		if (character_id == player_id)
		{
			buttons[Buttons::BtParty]->set_state(Button::State::DISABLED);
			buttons[Buttons::BtTrade]->set_state(Button::State::DISABLED);
			buttons[Buttons::BtPopUp]->set_state(Button::State::DISABLED);
			buttons[Buttons::BtPopDown]->set_state(Button::State::DISABLED);
		}

		Job character_job = Job(job_id);

		if (target())
			name.change_text(target()->get_name());

		job.change_text(character_job.get_name());
		level.change_text(std::to_string(lv));
		fame.change_text(std::to_string(f));
		guild.change_text((g == "" ? "-" : g));
		alliance.change_text((a == "" ? "-" : a));

		bool pet = (target() && target()->has_pet()) || !pets_info.empty();
		bool mount = (target() && target()->has_mount()) || has_mount;

		buttons[Buttons::BtPetShow]->set_state(pet ? Button::State::NORMAL : Button::State::DISABLED);
		buttons[Buttons::BtTamingShow]->set_state(mount ? Button::State::NORMAL : Button::State::DISABLED);
	}

	void UICharInfo::update_fame(int16_t f)
	{
		fame.change_text(std::to_string(f));
	}

	void UICharInfo::set_extra_info(std::vector<CharPetInfo> pets, bool mount,
		int32_t m_level, int32_t m_exp, int32_t m_tired, std::vector<int32_t> wish)
	{
		pets_info = std::move(pets);
		has_mount = mount;
		mount_level = m_level;
		mount_exp = m_exp;
		mount_tired = m_tired;
		wishlist = std::move(wish);
		selected_pet = 0;

		buttons[Buttons::BtPetShow]->set_state(pets_info.empty()
			? Button::State::DISABLED : Button::State::NORMAL);
		buttons[Buttons::BtTamingShow]->set_state(has_mount
			? Button::State::NORMAL : Button::State::DISABLED);

		// A panel that lost its data folds back up
		if ((panel_mode == PANEL_PET && pets_info.empty()) || (panel_mode == PANEL_TAMING && !has_mount))
			panel_mode = PANEL_NONE;

		if (!bot_tab_active)
			set_panel(panel_mode);
	}

	Char* UICharInfo::target() const
	{
		return Stage::get().get_character(target_char_id).get();
	}

	int32_t UICharInfo::get_char_id() const
	{
		return target_char_id;
	}

	void UICharInfo::set_bot_inventory(BotInventoryData data)
	{
		bot_inventory = std::move(data);
		bot_scroll_row = 0;
		bot_sub_tab = 4;

		buttons[Buttons::BtItem]->set_state(Button::State::NORMAL);

		for (uint16_t i = Buttons::BtBotEquip; i <= Buttons::BtBotEquipped; i++)
			buttons[i]->set_state(i == Buttons::BtBotEquipped ? Button::State::PRESSED : Button::State::NORMAL);

		bot_name_label.change_text(bot_inventory.name);
		bot_level_label.change_text("Lv. " + std::to_string(bot_inventory.level));

		std::string meso_str = std::to_string(bot_inventory.meso);
		string_format::split_number(meso_str);
		bot_meso_label.change_text(meso_str);

		set_bot_view(true);
		load_bot_icons();
	}

	void UICharInfo::draw_bot_inventory(float inter) const
	{
		bot_backgrnd.draw(DrawArgument(position));

		const auto& items = bot_inventory.get_tab(bot_sub_tab);
		draw_bot_item_grid(items);

		bot_name_label.draw(position + BOT_NAME_POS);
		bot_level_label.draw(position + BOT_LEVEL_POS);
		bot_meso_label.draw(position + BOT_MESO_POS);

		for (uint16_t i = Buttons::BtBotEquip; i <= Buttons::BtBotEquipped; i++)
		{
			auto it = buttons.find(i);

			if (it != buttons.end() && it->second)
				it->second->draw(position);
		}

		auto close = buttons.find(static_cast<uint16_t>(Buttons::BtClose));

		if (close != buttons.end() && close->second)
			close->second->draw(position);
	}

	void UICharInfo::draw_bot_item_grid(const std::vector<BotItem>& items) const
	{
		size_t first = static_cast<size_t>(bot_scroll_row) * BOT_COLS;

		for (int16_t i = 0; i < BOT_MAX_SLOTS; i++)
		{
			Point<int16_t> slotpos = position + get_bot_slotpos(i);
			size_t item_index = first + i;

			if (item_index >= items.size())
			{
				bot_slot_disabled.draw(DrawArgument(slotpos));
				continue;
			}

			auto it = bot_icons.find(i);

			if (it != bot_icons.end() && it->second)
				it->second->draw(slotpos);
			else
				bot_slot_disabled.draw(DrawArgument(slotpos));
		}
	}

	Cursor::State UICharInfo::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(clicked, cursorpos);

		if (dragged)
			return dstate;

		if (bot_tab_active && bot_inventory.is_valid())
		{
			Point<int16_t> cursor_rel = cursorpos - position;
			int16_t slot_index = bot_slot_by_position(cursor_rel);
			Icon* icon = get_bot_icon(slot_index);

			if (icon)
			{
				if (clicked)
				{
					Point<int16_t> slotpos = get_bot_slotpos(slot_index);
					icon->start_drag(cursor_rel - slotpos);
					UI::get().drag_icon(icon);

					return Cursor::State::GRABBING;
				}
				else
				{
					const auto& items = bot_inventory.get_tab(bot_sub_tab);
					size_t item_index = static_cast<size_t>(bot_scroll_row) * BOT_COLS + slot_index;

					if (item_index < items.size())
						UI::get().show_item(Tooltip::Parent::CHARINFO, items[item_index].item_id);

					return Cursor::State::CANGRAB;
				}
			}
		}

		return dstate;
	}

	void UICharInfo::send_scroll(double yoffset)
	{
		if (!bot_tab_active || !bot_inventory.is_valid())
			return;

		// One grid row per wheel notch; the 6-row window shows 24 slots at a time
		const auto& items = bot_inventory.get_tab(bot_sub_tab);
		int16_t rows = static_cast<int16_t>((items.size() + BOT_COLS - 1) / BOT_COLS);
		int16_t max_row = std::max<int16_t>(0, rows - BOT_ROWS);
		int16_t row = bot_scroll_row - static_cast<int16_t>(yoffset);

		if (row < 0)
			row = 0;
		else if (row > max_row)
			row = max_row;

		if (row != bot_scroll_row)
		{
			bot_scroll_row = row;
			load_bot_icons();
		}
	}

	bool UICharInfo::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		if (!bot_tab_active || !bot_inventory.is_valid())
			return false;

		InventoryType::Id src_tab = icon.get_source_tab();
		int16_t src_slot = icon.get_source_slot();

		if (src_tab == InventoryType::Id::NONE || src_slot < 0)
			return false;

		int8_t inv_type = static_cast<int8_t>(src_tab);
		BotInvGivePacket(bot_inventory.char_id, inv_type, src_slot).dispatch();

		return true;
	}

	void UICharInfo::load_bot_icons()
	{
		bot_icons.clear();

		const auto& items = bot_inventory.get_tab(bot_sub_tab);
		int8_t inv_type = bot_inventory.get_inv_type(bot_sub_tab);
		size_t first = static_cast<size_t>(bot_scroll_row) * BOT_COLS;

		// Icons are keyed by visible grid slot, not by item index
		for (int16_t i = 0; i < BOT_MAX_SLOTS; i++)
		{
			size_t item_index = first + i;

			if (item_index >= items.size())
				break;

			const auto& bi = items[item_index];
			int32_t item_prefix = bi.item_id / 10000;
			bool is_card = (item_prefix == 238 || item_prefix == 239);
			const Texture& texture = ItemData::get(bi.item_id).get_icon(is_card);
			int16_t count = (bot_sub_tab == 0 || bot_sub_tab == 4) ? -1 : bi.count;

			bot_icons[i] = std::make_unique<Icon>(
				std::make_unique<BotItemIcon>(bot_inventory.char_id, inv_type, bi.slot, bi.item_id, bi.count, bot_sub_tab),
				texture, count
			);
		}
	}

	Point<int16_t> UICharInfo::get_bot_slotpos(int16_t slot_index) const
	{
		div_t d = std::div(slot_index, BOT_COLS);
		return Point<int16_t>(BOT_GRID_X + BOT_ICON_W * d.rem, BOT_GRID_Y + BOT_ICON_H * d.quot);
	}

	int16_t UICharInfo::bot_slot_by_position(Point<int16_t> cursor_rel) const
	{
		int16_t rx = cursor_rel.x() - BOT_GRID_X;
		int16_t ry = cursor_rel.y() - BOT_GRID_Y;

		if (rx < 0 || ry < 0)
			return -1;

		int16_t col = rx / BOT_ICON_W;
		int16_t row = ry / BOT_ICON_H;

		if (col >= BOT_COLS || row >= BOT_ROWS)
			return -1;

		return row * BOT_COLS + col;
	}

	Icon* UICharInfo::get_bot_icon(int16_t slot_index)
	{
		if (slot_index < 0)
			return nullptr;

		auto it = bot_icons.find(slot_index);

		if (it != bot_icons.end())
			return it->second.get();

		return nullptr;
	}

	void UICharInfo::remove_cursor()
	{
		UIDragElement::remove_cursor();
		UI::get().clear_tooltip(Tooltip::Parent::CHARINFO);
	}

	// BotItemIcon implementation
	UICharInfo::BotItemIcon::BotItemIcon(int32_t bid, int8_t it, int16_t s, int32_t iid, int16_t c, int8_t st)
		: bot_id(bid), inv_type(it), slot(s), item_id(iid), count(c), sub_tab(st) {}

	void UICharInfo::BotItemIcon::drop_on_stage() const
	{
		if (sub_tab == 4)
			BotInvEquipPacket(bot_id, slot, static_cast<int16_t>(-slot)).dispatch();
		else
			BotInvTakePacket(bot_id, inv_type, slot).dispatch();
	}

	void UICharInfo::BotItemIcon::drop_on_equips(EquipSlot::Id) const {}

	bool UICharInfo::BotItemIcon::drop_on_items(InventoryType::Id, EquipSlot::Id, int16_t, bool) const
	{
		if (sub_tab == 4)
			BotInvEquipPacket(bot_id, slot, static_cast<int16_t>(-slot)).dispatch();
		else
			BotInvTakePacket(bot_id, inv_type, slot).dispatch();

		return true;
	}

	void UICharInfo::BotItemIcon::drop_on_bindings(Point<int16_t>, bool) const {}
	void UICharInfo::BotItemIcon::set_count(int16_t c) { count = c; }
	Icon::IconType UICharInfo::BotItemIcon::get_type() { return Icon::IconType::ITEM; }
}
