//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileMenu.h"

#include <cmath>
#include <cstdio>

#include "UIChannel.h"
#include "UIMobileAdmin.h"
#include "UIMobileEquip.h"
#include "UIMobileInventory.h"
#include "UIMobileKeyConfig.h"
#include "UIMobileSkills.h"
#include "UIMobileStats.h"
#include "UIMobileStore.h"
#include "UIOptionMenu.h"
#include "UIMobileConfirm.h"
#include "UIWorldMap.h"

#include "../ServerInfo.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Resume.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/MessagingPackets.h"

#include <string>
#include <vector>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		template <class T>
		bool showing()
		{
			auto element = UI::get().get_element<T>();

			return element && element->is_active();
		}

		constexpr int16_t W = 360;
		constexpr int16_t PAD = 16;
		constexpr int16_t GRID_Y = 78;
		constexpr int16_t TILE_H = 72;
		constexpr int16_t GAP = 8;
		constexpr int16_t ROWS = 5;

		enum MenuTile : int32_t
		{
			STATS, SKILLS, ITEMS,
			EQUIP, QUESTS, STORE,
			MAP, CONTROLS, BUDDIES,
			PARTY, CHANNEL, OPTIONS,
			MAPLE_ADMIN, ADMIN, CHARACTER,
			NUM_TILES
		};

		// Admin (the GM panel) shows for GM characters only; Maple Admin is the NPC, for everyone.
		std::vector<int32_t> shown_tiles()
		{
			std::vector<int32_t> tiles;

			for (int32_t t = 0; t < NUM_TILES; t++)
				if (t != ADMIN || ServerInfo::is_admin())
					tiles.push_back(t);

			return tiles;
		}

		// The Cash Shop button in the wallet card under the tiles.
		constexpr int32_t CASH_SHOP = 100;
		constexpr int16_t CARD_H = 64;

		struct TileInfo
		{
			const char* label;
			const char* mark;
			RGBA color;
		};

		const TileInfo TILES[NUM_TILES] = {
			{ "Stats", "S", Theme::GOLD },
			{ "Skills", "K", Theme::BLUE },
			{ "Items", "I", Theme::ACCENT },
			{ "Equipment", "E", Theme::PURPLE },
			{ "Quests", "Q", Theme::GREEN },
			{ "Store", "$", Theme::GOLD },
			{ "World Map", "W", { 0.30f, 0.80f, 0.85f, 1.0f } },
			{ "Controls", "T", Theme::MUTED },
			{ "Buddies", "B", { 1.0f, 0.55f, 0.75f, 1.0f } },
			{ "Party", "P", { 0.55f, 0.85f, 0.35f, 1.0f } },
			{ "Change Channel", "C", Theme::MUTED },
			{ "Options", "O", Theme::MUTED },
			{ "Maple Admin", "M", { 1.0f, 0.60f, 0.20f, 1.0f } },
			{ "Admin", "A", Theme::RED },
			{ "Change Character", "R", Theme::RED }
		};
	}

	UIMobileMenu::UIMobileMenu() : UIMobilePanel(Edge::RIGHT, W) {}

	Rect UIMobileMenu::tile_rect(size_t index) const
	{
		int16_t w = static_cast<int16_t>((W - PAD * 2 - GAP * 2) / 3);
		int16_t col = static_cast<int16_t>(index % 3);
		int16_t row = static_cast<int16_t>(index / 3);

		return rect(static_cast<int16_t>(position.x() + PAD + col * (w + GAP)),
			static_cast<int16_t>(position.y() + GRID_Y + row * (TILE_H + GAP)), w, TILE_H);
	}

	int32_t UIMobileMenu::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		std::vector<int32_t> tiles = shown_tiles();

		for (size_t i = 0; i < tiles.size(); i++)
			if (tile_rect(i).contains(pos))
				return tiles[i];

		if (cash_shop_rect().contains(pos))
			return CASH_SHOP;

		return NO_TARGET;
	}

	Rect UIMobileMenu::wallet_rect() const
	{
		Rect last = tile_rect(ROWS * 3 - 1);

		return rect(static_cast<int16_t>(position.x() + PAD), static_cast<int16_t>(last.bottom() + 14), W - PAD * 2, CARD_H);
	}

	Rect UIMobileMenu::cash_shop_rect() const
	{
		Rect card = wallet_rect();

		return rect(static_cast<int16_t>(card.right() - 12 - 138), static_cast<int16_t>(card.top() + 12), 138,
			static_cast<int16_t>(CARD_H - 24));
	}

	void UIMobileMenu::activate(int32_t target)
	{
		if (target == CASH_SHOP)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			deactivate();
			EnterCashShopPacket().dispatch();
			return;
		}

		if (target < 0 || target >= NUM_TILES)
			return;

		Sound(Sound::Name::BUTTONCLICK).play();

		// The menu gives way to what it opens.
		deactivate();

		switch (target)
		{
			// Tiles always open their screen, never close one already up.
			case MenuTile::STATS: if (!showing<UIMobileStats>()) open_stats(); break;
			case MenuTile::SKILLS: if (!showing<UIMobileSkills>()) open_skills(); break;
			case MenuTile::CONTROLS: if (!showing<UIMobileKeyConfig>()) open_keys(); break;
			case MenuTile::ITEMS: if (!showing<UIMobileInventory>()) open_items(); break;
			case MenuTile::STORE: if (!showing<UIMobileStore>()) open_store(); break;
			case MenuTile::ADMIN: if (!showing<UIMobileAdmin>()) open_admin(); break;
			case MenuTile::EQUIP: if (!showing<UIMobileEquip>()) open_equip(); break;
			case MenuTile::QUESTS: open_quests(); break;
			case MenuTile::MAP: open_world_map(); break;
			case MenuTile::BUDDIES: open_social(false); break;
			case MenuTile::PARTY: open_social(true); break;
			case MenuTile::CHANNEL: open_channels(); break;
			case MenuTile::OPTIONS: open_options(); break;
			// Job advancement, starter pack and AP reset (NPC 9010000), from anywhere.
			case MenuTile::MAPLE_ADMIN: GeneralChatPacket("@mapleadmin", false).dispatch(); break;
			case MenuTile::CHARACTER:
				UI::get().emplace<UIMobileConfirm>("Change character?", "You'll go back to your character list.", "Change", false,
					[]() { Resume::change_character(); });
				break;
		}
	}

	void UIMobileMenu::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();
		const CharStats& stats = Stage::get().get_player().get_stats();

		panel(f, "Menu", stats.get_name() + "  \xC2\xB7  Lv " + std::to_string(stats.get_stat(MapleStat::Id::LEVEL))
			+ "  \xC2\xB7  " + stats.get_jobname() + "  \xC2\xB7  Ch " + std::to_string(Configuration::get().get_channelid() + 1));
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		int32_t ap = stats.get_stat(MapleStat::Id::AP);
		int32_t sp = stats.get_stat(MapleStat::Id::SP);

		std::vector<int32_t> tiles = shown_tiles();

		for (size_t i = 0; i < tiles.size(); i++)
		{
			int32_t tile = tiles[i];
			Rect t = tile_rect(i);
			const TileInfo& info = TILES[tile];
			fill(t, pressed == tile ? Theme::CARD_HI : Theme::CARD, 16);

			Point<int16_t> c = centre(t);
			Rect badge = rect(static_cast<int16_t>(c.x() - 18), static_cast<int16_t>(t.top() + 9), 36, 36);
			fill(badge, info.color.alpha(0.18f), 18);
			text_in(info.mark, Font::TITLE, info.color, badge);

			text_in(info.label, Font::SMALL_BOLD, Theme::TEXT, rect(t.left(), static_cast<int16_t>(t.bottom() - 25), t.width(), 20));

			// Points waiting to be placed.
			int32_t count = tile == MenuTile::STATS ? ap : tile == MenuTile::SKILLS ? sp : 0;

			if (count > 0)
			{
				std::string n = count > 9999 ? "9999+" : std::to_string(count);
				int16_t w = static_cast<int16_t>(std::max<int16_t>(22, text_width(n, Font::SMALL_BOLD) + 12));
				Rect dot = rect(static_cast<int16_t>(t.right() - w - 8), static_cast<int16_t>(t.top() + 8), w, 22);
				fill(dot, Theme::ACCENT, 11);
				text_in(n, Font::SMALL_BOLD, { 0.13f, 0.08f, 0.03f, 1.0f }, dot);
			}
		}

		// Mesos, and the Cash Shop (once a button on v83's status bar).
		{
			const Player& player = Stage::get().get_player();
			Rect card = wallet_rect();
			fill(card, Theme::CARD, 16);

			int64_t meso = player.get_inventory().get_meso();
			std::string digits = std::to_string(meso);
			std::string mesos;

			for (size_t i = 0; i < digits.size(); i++)
			{
				if (i > 0 && (digits.size() - i) % 3 == 0)
					mesos += ',';

				mesos += digits[i];
			}

			Rect shop = cash_shop_rect();
			text("MESOS", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(card.left() + 16, card.top() + 12));
			// Big 64-bit wallets read as words ("9 quintillion"); the Inventory
			// footer still shows every digit.
			int16_t room = static_cast<int16_t>(shop.left() - card.left() - 28);

			if (text_width(mesos, Font::TITLE) > room)
			{
				static const char* const NAMES[] = { "million", "billion", "trillion", "quadrillion", "quintillion" };
				double value = static_cast<double>(meso) / 1e6;
				size_t unit = 0;

				while (value >= 1000.0 && unit + 1 < 5)
				{
					value /= 1000.0;
					unit++;
				}

				char buf[48];
				std::snprintf(buf, sizeof(buf), value >= 100.0 || value == static_cast<int64_t>(value) ? "%.0f %s" : "%.2f %s",
					std::floor(value * 100.0) / 100.0, NAMES[unit]);
				mesos = buf;
			}

			text(ellipsize(mesos, Font::TITLE, room), Font::TITLE, Theme::GOLD,
				Point<int16_t>(card.left() + 16, card.top() + 28));
			button(shop, "Cash Shop", Style::PRIMARY, true, pressed == CASH_SHOP);
		}
	}

	UIElement::Type UIMobileMenu::get_type() const
	{
		return TYPE;
	}
}
