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
#pragma once

#include "../UIElement.h"

#include "../Components/Charset.h"
#include "../Components/Icon.h"
#include "../Components/MobGage.h"
#include "../KeyConfig.h"
#include "../KeyType.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../Character/CharStats.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Animation.h"
#include "../../Graphics/Geometry.h"

namespace ms
{
	// In-game status bar rendered from the v83 UI.nx assets
	// (StatusBar.img + Basic.img). The v83 bar is a single 800x71 strip
	// authored for an 800x600 client: chat input line on top, then
	// LV / name / HP / MP / EXP on the bottom row, hot-key buttons and the
	// Shop / MTS / Menu / Shortcut buttons on the right.
	class UIStatusBar : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::STATUSBAR;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		// Size of StatusBar.img/base/backgrnd (and the backgrnd2 layout).
		static constexpr int16_t BAR_WIDTH = 800;
		static constexpr int16_t BAR_HEIGHT = 71;

		// Screen-space top-left of the 800-wide bar content: flush with the
		// bottom of the view and centred horizontally (the plain backgrnd
		// strip is stretched across the whole view width behind it). Shared
		// with UIChatBar so the chat input lines up with the bar art.
		static Point<int16_t> bar_origin();

		UIStatusBar(const CharStats& stats);

		void draw(float alpha) const override;
		void update() override;

		bool is_in_range(Point<int16_t> cursorpos) const override;
		bool send_icon(const Icon& icon, Point<int16_t> cursorpos) override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void remove_cursor() override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

		// Show/refresh the boss HP gauge with a boss's name + HP percent (0..100).
		// Auto-hides shortly after the last update (or on 0%).
		void update_boss_hp(const std::string& name, int8_t percent);

		void toggle_qs();
		void toggle_menu();
		void remove_menus();
		bool is_menu_active();
		void notify();
		void clear_notification();

		// Screen-space top-left of the notification (memo) icon slot in the
		// base/box tray. Used as the anchor for both the drawer popup and
		// the floating toast so they line up over it.
		Point<int16_t> get_notice_anchor() const;

		// Opens the notification drawer (v83's memo slot).
		void open_notices();

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		float getexppercent() const;
		float gethppercent() const;
		float getmppercent() const;

		enum Buttons : uint16_t
		{
			// Main bar (StatusBar.img/Bt*) - MapleButtons
			BT_WHISPER,     // BtWhisper
			BT_CALLGM,      // BtClaim (report)
			BT_CASHSHOP,    // BtShop
			BT_TRADE,       // BtNPT (MTS)
			BT_MENU,        // BtMenu
			BT_KEYSETTING,  // BtShort (shortcut / key config)
			// Hot-key row (StatusBar.img/*Key) - MapleButtons
			BT_EQUIPS,      // EquipKey
			BT_INVENTORY,   // InvenKey
			BT_STATS,       // StatKey
			BT_SKILL,       // SkillKey
			BT_KEYSET,      // KeySet
			// Quick slot toggle (StatusBar.img/QuickSlot, QuickSlotD)
			BT_QS_OPEN,
			BT_QS_CLOSE,
			// Notification slot in base/box - hit-rect, not a MapleButton
			BT_NOTICE,
			// Menu popup rows - text rows, not MapleButtons (v83 ships no
			// per-entry menu button art)
			BT_MENU_STAT,
			BT_MENU_SKILL,
			BT_MENU_QUEST,
			BT_MENU_ITEM,
			BT_MENU_EQUIP,
			BT_MENU_COMMUNITY,
			BT_MENU_EVENT,
			BT_MENU_RANK,
			BT_MENU_EPISODBOOK,
			BT_MENU_MSN,
			BT_MENU_MONSTERBATTLE,
			BT_MENU_MONSTERLIFE,
			BT_SYS_CHANNEL,
			BT_SYS_KEYSETTING,
			BT_SYS_OPTION,
			BT_SYS_JOYPAD,
			BT_SYS_GAMEQUIT
		};

		const CharStats& stats;

		// Boss HP gauge (top-centre) - shown while a boss reports its HP.
		MobGage boss_gage;
		float boss_hp_percent = 0.0f;
		int32_t boss_hp_ticks = 0; // ms remaining before the gauge auto-hides

		// UIWindow.img/EnergyBar
		Texture energy_bar_c;
		Texture energy_bar_e;
		Texture energy_fill;
		Animation energy_full_effect;

		// StatusBar.img/base
		Texture bar_backgrnd;   // 800x71 plain strip, stretched to the view width
		Texture bar_backgrnd2;  // 570x71 overlay: input line + LV/name + gauge panels
		Texture hotkey_box;     // 42x19 two-slot tray (report button + memo icon)
		Texture icon_memo;      // 14x11 memo icon, reused as the notification badge

		// StatusBar.img/gauge. v83 draws the fully coloured `bar`, covers the
		// missing part of each gauge with the 1x16 `gray` strip and lays the
		// `graduation` ticks on top.
		Texture gauge_bar;
		Texture gauge_graduation;
		Texture gauge_gray;
		Animation hp_flash;
		Animation mp_flash;

		// Hot-key blink frames (StatusBar.img/StatKey|SkillKey/ani), used
		// as the unspent AP / SP reminder.
		Animation stat_blink;
		Animation skill_blink;

		// StatusBar.img/number (digits + Lbracket/Rbracket/percent/slash)
		Charset statset;
		Texture num_lbracket;
		Texture num_rbracket;
		// Basic.img/LevelNo (orange 11x13 digits)
		Charset levelset;

		Text namelabel;
		Text joblabel;

		// Draws a stat string with the bitmap font: digits, 's' (slash),
		// 'p' (percent), 'L'/'R' (brackets) and '.' (drawn as a dot box).
		void draw_stat_text(const std::string& text, Point<int16_t> pos) const;

		// Displayed gauge fill, eased towards the real percentage each tick.
		float hp_shown;
		float mp_shown;
		float exp_shown;
		static float ease_towards(float current, float target);

		// Gauge-change flash state: start a short timer whenever HP/MP/EXP
		// drops, and play the hpFlash/mpFlash animation while it is alive,
		// not only when the resource is below the warning threshold.
		float prev_hp_pct;
		float prev_mp_pct;
		float prev_exp_pct;
		uint16_t hp_flash_ticks;
		uint16_t mp_flash_ticks;
		uint16_t exp_flash_ticks;
		static constexpr uint16_t FLASH_DURATION_TICKS = 20;

		// Menu popup (BtMenu). v83 has no sub-menu button art, so the popup
		// is a translucent panel of text rows drawn above the button.
		struct MenuRow
		{
			uint16_t id;
			std::string label;
		};
		std::vector<MenuRow> menu_rows;
		bool show_menu;
		// Fade alpha for the popup (0 hidden .. 1 fully visible).
		float menu_fade = 0.0f;
		int16_t menu_hover = -1;
		// mutable: draw() is const but re-texts the shared label per row.
		mutable Text menu_label;
		int16_t menu_panel_height() const;
		Rectangle<int16_t> menu_panel_rect() const;
		Rectangle<int16_t> menu_row_rect(int16_t row) const;
		int16_t menu_row_at(Point<int16_t> cursorpos) const;

		// Notification slot hit-rect (right slot of base/box).
		Rectangle<int16_t> notice_rect() const;

		// Quick slot (StatusBar.img/base/quickSlot, 151x80, 4x2 cubes)
		Texture quickslot_bg;
		bool show_quickslot;
		Texture qs_key_sprites[8];

		// Quickslot drop support.
		// The v83 panel shows 8 key bindings laid out in 2 rows of 4;
		// the layout comes from Keyboard::get_quickslot_keys(), which is
		// seeded with the default (SHIFT/INS/HOME/PGUP, CTRL/DEL/END/PGDN)
		// and overwritten by the server's QUICKSLOT_INIT at login.
		// Returns the quickslot slot index (0..7) under the cursor, or -1 if outside.
		int16_t quickslot_slot_at(Point<int16_t> cursorpos) const;
		// Returns the top-left screen position of the given slot (0..7).
		Point<int16_t> quickslot_slot_pos(int16_t slot) const;
		// Returns the top-left screen position of the quickslot panel sprite.
		Point<int16_t> quickslot_panel_pos() const;
		// Applies a mapping to a quickslot key and dispatches the server packet.
		void assign_quickslot(int16_t slot, KeyType::Id type, int32_t action);

		// Reusable label drawn on each quickslot cube showing its bound key name
		// (change_text per slot). Keeps the key indicator correct after rebinding.
		// mutable: draw() is const but we re-text it per slot.
		mutable Text qs_key_label;

		// Drawn icons for bound quickslot entries (keyed by maple keycode).
		mutable std::map<int32_t, Texture> qs_icon_cache;
		Texture get_quickslot_icon(KeyType::Id type, int32_t action) const;

		// Clears the binding on a quickslot key locally + on the server.
		void clear_quickslot(uint8_t key);

		// Owns draggable Icon objects created when the user picks a binding
		// out of a quickslot cube. Keyed by maple keycode so repeat drags of
		// the same slot reuse the same Icon.
		std::map<uint8_t, std::unique_ptr<Icon>> qs_drag_icons;

		// Type/action pickup helper class that writes a cleared binding on
		// drop_on_stage so dropping outside the quickslot removes it.
		class QuickslotDragType : public Icon::Type
		{
		public:
			QuickslotDragType(uint8_t key, KeyType::Id type, int32_t action);

			void drop_on_stage() const override;
			void drop_on_equips(EquipSlot::Id) const override {}
			bool drop_on_items(InventoryType::Id, EquipSlot::Id, int16_t, bool) const override { return true; }
			void drop_on_bindings(Point<int16_t>, bool) const override {}
			void set_count(int16_t) override {}
			Icon::IconType get_type() override;
			int32_t get_action_id() const override { return action; }

		private:
			uint8_t key;
			KeyType::Id type;
			int32_t action;
		};

		// Notice/notification state
		bool has_notification;
		uint32_t notice_pulse_tick;

		// ---- The phone's HUD (UIStatusBarHud.cpp) ----
		// v83's 800-pixel bar does not suit a phone. There it becomes a
		// see-through HUD in the top-left corner: level and name, HP, MP and
		// EXP, plus chips for unspent AP/SP and waiting notifications. The
		// rest of the bar moved to the touch controls and the menu.
		enum HudTarget : int32_t
		{
			HUD_NONE = -1,
			HUD_AP,
			HUD_SP,
			HUD_NOTICE
		};

		Rectangle<int16_t> hud_chip(int32_t target) const;
		int32_t hud_hit(Point<int16_t> pos) const;
		void draw_hud(float alpha) const;
		Cursor::State hud_cursor(bool clicked, Point<int16_t> pos);

		uint32_t hud_press = 0;
		uint32_t hud_tick = 0;
	};
}
