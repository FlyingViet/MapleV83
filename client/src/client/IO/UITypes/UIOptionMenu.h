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

#include "../UIDragElement.h"

#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"

namespace ms
{
	// Settings window built from the two v83 option panels drawn side by side:
	// UIWindow.img/SysOpt (299x366, sliders / view mode) on the left and
	// UIWindow.img/GameOpt (158x284, social "allow" rows) on the right. Both
	// panels are flat bitmaps with their labels baked in, so every control is
	// a Basic.img slider / checkbox placed at the row it belongs to.
	class UIOptionMenu : public UIDragElement<PosOPTIONMENU>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::OPTIONMENU;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UIOptionMenu();

		void draw(float inter) const override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void load_settings();
		void save_settings();
		void revert_settings();
		void apply_audio_preview(int16_t slider) const;

		enum Buttons : uint16_t
		{
			BT_OK,
			BT_CANCEL
		};

		// Panel geometry
		static constexpr int16_t SYS_W = 299;              // SysOpt/backgrnd
		static constexpr int16_t SYS_H = 366;
		static constexpr int16_t GAME_W = 158;             // GameOpt/backgrnd
		static constexpr int16_t GAME_H = 284;
		static constexpr int16_t PANEL_GAP = 4;
		static constexpr int16_t GAME_X = SYS_W + PANEL_GAP;
		static constexpr int16_t DRAG_H = 20;              // both title bars
		// Basic.img/Slider: 3px caps + 1px mid tile, 8px tall (origin 0,-2); 22x14 thumbs
		static constexpr int16_t TRACK_H = 8;
		static constexpr int16_t TRACK_CAP_W = 3;
		static constexpr int16_t THUMB_W = 22;
		static constexpr int16_t THUMB_H = 14;
		// Basic.img/CheckBox/0 (off) and /1 (on) are 12x12
		static constexpr int16_t CHECK_SIZE = 12;
		static constexpr int16_t HIT_PAD = 2;
		// Basic.img/BtOK2 + BtCancel2 (47x18) centred in the SysOpt bottom strip (y 338..360)
		static constexpr int16_t BT_W = 47;
		static constexpr int16_t BT_GAP = 8;
		static constexpr int16_t BT_Y = 342;
		// GameOpt: twelve 18px "ALLOW" rows from y 33, checkbox in the ALLOW column
		static constexpr int16_t GAME_ROW_Y = 36;
		static constexpr int16_t GAME_ROW_H = 18;
		static constexpr int16_t GAME_CHECK_X = 134;
		// "Simple minimap" lives in GameOpt's otherwise empty bottom strip (y 266..284)
		static constexpr Point<int16_t> MINIMAP_CHECK = Point<int16_t>(GAME_X + 10, 269);
		static constexpr Point<int16_t> MINIMAP_LABEL = Point<int16_t>(GAME_X + 26, 267);

		Texture track_left;
		Texture track_mid;
		Texture track_right;
		Texture thumb_normal;
		Texture thumb_pressed;
		Texture check_off;
		Texture check_on;

		// Sliders. SL_EFFECTS has no SysOpt row and is never drawn; its
		// stored value just round-trips through load/save untouched.
		enum SliderType : int16_t
		{
			SL_NONE = -1,
			SL_GRAPHICS,
			SL_EFFECTS,
			SL_BGM,
			SL_SFX,
			SL_MOUSE_SPEED,
			SL_HP_WARNING,
			SL_MP_WARNING,
			NUM_SLIDERS
		};

		struct SliderLayout
		{
			int16_t x;       // track left, between the row's min / max labels
			int16_t w;       // track length
			int16_t cy;      // row centre line
			bool shown;
		};

		SliderLayout slider_layout[NUM_SLIDERS];
		uint8_t slider_val[NUM_SLIDERS];
		uint8_t slider_orig[NUM_SLIDERS];
		SliderType active_slider = SL_NONE;

		// Checkboxes. WINDOWED / FULLSCREEN are a radio pair. The newer
		// menu's resolution radios, minimap "normal" radio, alliance chat,
		// expedition invite and follow toggles have no v83 row and were dropped.
		enum CheckType : uint16_t
		{
			CHK_MUTE_BGM,
			CHK_MUTE_SFX,
			CHK_SCREEN_SHAKE,
			CHK_WINDOWED,
			CHK_FULLSCREEN,
			CHK_MINIMAP_SIMPLE,
			// GameOpt rows, in strip order
			CHK_WHISPER,
			CHK_FRIEND_CHAT,
			CHK_CHAT_INVITE,
			CHK_TRADE_REQUEST,
			CHK_PARTY_INVITE,
			CHK_GUILD_CHAT,
			CHK_GUILD_INVITE,
			CHK_ALLIANCE_INVITE,
			CHK_FAMILY_INVITE,
			NUM_CHECKS
		};

		Point<int16_t> check_positions[NUM_CHECKS];
		bool check_val[NUM_CHECKS];
		bool check_orig[NUM_CHECKS];

		Text minimap_label;
	};
}
