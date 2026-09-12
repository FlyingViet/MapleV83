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
#include "UIOptionMenu.h"

#include "../KeyAction.h"

#include "../Components/MapleButton.h"
#include "../UI.h"
#include "../Window.h"
#include "UIMiniMap.h"

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIOptionMenu::UIOptionMenu() : UIDragElement<PosOPTIONMENU>(Point<int16_t>(GAME_X + GAME_W, DRAG_H)), active_slider(SL_NONE)
	{
		dimension = Point<int16_t>(GAME_X + GAME_W, SYS_H);
		dragarea = Point<int16_t>(dimension.x(), DRAG_H);

		// Always open centred on the screen
		int16_t screen_w = Constants::Constants::get().get_viewwidth();
		int16_t screen_h = Constants::Constants::get().get_viewheight();
		position = Point<int16_t>((screen_w - dimension.x()) / 2, (screen_h - dimension.y()) / 2);

		nl::node basic = nl::nx::ui["Basic.img"];
		nl::node sysopt = nl::nx::ui["UIWindow.img"]["SysOpt"];
		nl::node gameopt = nl::nx::ui["UIWindow.img"]["GameOpt"];

		sprites.emplace_back(sysopt["backgrnd"], DrawArgument(Point<int16_t>(0, 0)));
		sprites.emplace_back(gameopt["backgrnd"], DrawArgument(Point<int16_t>(GAME_X, 0)));

		int16_t pair_x = (SYS_W - (BT_W + BT_GAP + BT_W)) / 2;
		buttons[BT_OK] = std::make_unique<MapleButton>(basic["BtOK2"], Point<int16_t>(pair_x, BT_Y));
		buttons[BT_CANCEL] = std::make_unique<MapleButton>(basic["BtCancel2"], Point<int16_t>(pair_x + BT_W + BT_GAP, BT_Y));

		nl::node slider = basic["Slider"];
		track_left = slider["left"];
		track_mid = slider["mid"];
		track_right = slider["right"];
		thumb_normal = slider["thumbNormal"];
		thumb_pressed = slider["thumbPressed"];

		nl::node checkbox = basic["CheckBox"];
		check_off = checkbox["0"];
		check_on = checkbox["1"];

		// Slider tracks sit between the min / max words baked into each
		// SysOpt row (rows are 30px apart from y 33; the centre line of each
		// label box is the track's centre line).
		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_layout[i] = { 0, 1, 0, false };

		slider_layout[SL_GRAPHICS]    = { 110, 135, 47, true };   // NORMAL (FAST) .. BEST (SLOW)
		slider_layout[SL_BGM]         = { 100, 105, 76, true };   // SOFT .. LOUD
		slider_layout[SL_SFX]         = { 100, 105, 106, true };  // SOFT .. LOUD
		slider_layout[SL_MOUSE_SPEED] = { 100, 145, 166, true };  // SLOW .. FAST
		slider_layout[SL_HP_WARNING]  = { 105, 140, 196, true };  // DO NOT ALERT .. 100%
		slider_layout[SL_MP_WARNING]  = { 105, 140, 226, true };  // DO NOT ALERT .. 100%

		// Checkboxes: left of the MUTE words, the SHAKE UP sentence and the
		// two VIEWING MODE lines; then the GameOpt ALLOW column.
		check_positions[CHK_MUTE_BGM]     = Point<int16_t>(246, 70);
		check_positions[CHK_MUTE_SFX]     = Point<int16_t>(246, 100);
		check_positions[CHK_SCREEN_SHAKE] = Point<int16_t>(64, 250);
		check_positions[CHK_WINDOWED]     = Point<int16_t>(64, 306);
		check_positions[CHK_FULLSCREEN]   = Point<int16_t>(64, 319);
		check_positions[CHK_MINIMAP_SIMPLE] = MINIMAP_CHECK;

		// GameOpt strip order: WHISPER, FRIENDS, CHAT INVITE, TRADE REQUEST,
		// PARTY INVITE, PARTY SEARCH, GAME INVITE, GUILD CHAT, GUILD INVITE,
		// ALLIANCE TOURNAMENT, ALLIANCE INVITE, FAMILY INVITE. Rows without a
		// matching setting (5, 6, 9) stay empty.
		auto game_row = [](int16_t row) {
			return Point<int16_t>(GAME_X + GAME_CHECK_X, GAME_ROW_Y + row * GAME_ROW_H);
		};

		check_positions[CHK_WHISPER]         = game_row(0);
		check_positions[CHK_FRIEND_CHAT]     = game_row(1);
		check_positions[CHK_CHAT_INVITE]     = game_row(2);
		check_positions[CHK_TRADE_REQUEST]   = game_row(3);
		check_positions[CHK_PARTY_INVITE]    = game_row(4);
		check_positions[CHK_GUILD_CHAT]      = game_row(7);
		check_positions[CHK_GUILD_INVITE]    = game_row(8);
		check_positions[CHK_ALLIANCE_INVITE] = game_row(10);
		check_positions[CHK_FAMILY_INVITE]   = game_row(11);

		for (int i = 0; i < NUM_CHECKS; i++)
		{
			check_val[i] = false;
			check_orig[i] = false;
		}

		minimap_label = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK, "Simple minimap");

		load_settings();
	}

	void UIOptionMenu::draw(float inter) const
	{
		UIElement::draw_sprites(inter);

		for (int i = 0; i < NUM_SLIDERS; i++)
		{
			const SliderLayout& sl = slider_layout[i];

			if (!sl.shown)
				continue;

			// Track pieces carry an origin of (0,-2), so add it back to land
			// the 8px strip centred on the row line
			int16_t track_y = sl.cy - TRACK_H / 2;
			Point<int16_t> left_pos = position + Point<int16_t>(sl.x - TRACK_CAP_W, track_y);
			Point<int16_t> mid_pos = position + Point<int16_t>(sl.x, track_y);
			Point<int16_t> right_pos = position + Point<int16_t>(sl.x + sl.w, track_y);

			track_left.draw(DrawArgument(left_pos + track_left.get_origin()));
			track_mid.draw(DrawArgument(mid_pos + track_mid.get_origin(), Point<int16_t>(sl.w, TRACK_H)));
			track_right.draw(DrawArgument(right_pos + track_right.get_origin()));

			int16_t thumb_x = sl.x + static_cast<int16_t>(slider_val[i] * sl.w / 100) - THUMB_W / 2;
			Point<int16_t> thumb_pos = position + Point<int16_t>(thumb_x, sl.cy - THUMB_H / 2);

			const Texture& thumb = (active_slider == i) ? thumb_pressed : thumb_normal;
			thumb.draw(DrawArgument(thumb_pos + thumb.get_origin()));
		}

		for (int i = 0; i < NUM_CHECKS; i++)
		{
			const Texture& box = check_val[i] ? check_on : check_off;
			box.draw(DrawArgument(position + check_positions[i]));
		}

		minimap_label.draw(position + MINIMAP_LABEL);

		UIElement::draw_buttons(inter);
	}

	Button::State UIOptionMenu::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case BT_CANCEL:
			revert_settings();
			deactivate();
			return Button::State::NORMAL;
		case BT_OK:
			save_settings();
			deactivate();
			return Button::State::NORMAL;
		default:
			return Button::State::DISABLED;
		}
	}

	void UIOptionMenu::apply_audio_preview(int16_t slider) const
	{
		if (slider == SL_BGM && !check_val[CHK_MUTE_BGM])
			Music::set_bgmvolume(slider_val[SL_BGM]);
		else if (slider == SL_SFX && !check_val[CHK_MUTE_SFX])
			Sound::set_sfxvolume(slider_val[SL_SFX]);
	}

	Cursor::State UIOptionMenu::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Continue an active slider drag
		if (clicked && active_slider != SL_NONE)
		{
			const SliderLayout& sl = slider_layout[active_slider];
			int16_t relative_x = cursorpos.x() - position.x() - sl.x;

			if (relative_x < 0)
				relative_x = 0;

			if (relative_x > sl.w)
				relative_x = sl.w;

			slider_val[active_slider] = static_cast<uint8_t>(relative_x * 100 / sl.w);
			apply_audio_preview(active_slider);

			return Cursor::State::CLICKING;
		}
		else if (!clicked && active_slider != SL_NONE)
		{
			active_slider = SL_NONE;
		}

		if (clicked)
		{
			int16_t rel_x = cursorpos.x() - position.x();
			int16_t rel_y = cursorpos.y() - position.y();

			for (int i = 0; i < NUM_SLIDERS; i++)
			{
				const SliderLayout& sl = slider_layout[i];

				if (!sl.shown)
					continue;

				if (rel_x >= sl.x - THUMB_W / 2 && rel_x <= sl.x + sl.w + THUMB_W / 2 &&
					rel_y >= sl.cy - THUMB_H / 2 && rel_y <= sl.cy + THUMB_H / 2)
				{
					active_slider = static_cast<SliderType>(i);

					int16_t track_x = rel_x - sl.x;

					if (track_x < 0)
						track_x = 0;

					if (track_x > sl.w)
						track_x = sl.w;

					slider_val[i] = static_cast<uint8_t>(track_x * 100 / sl.w);
					apply_audio_preview(i);

					return Cursor::State::CLICKING;
				}
			}

			for (int i = 0; i < NUM_CHECKS; i++)
			{
				Point<int16_t> chk = check_positions[i];

				if (rel_x >= chk.x() - HIT_PAD && rel_x <= chk.x() + CHECK_SIZE + HIT_PAD &&
					rel_y >= chk.y() - HIT_PAD && rel_y <= chk.y() + CHECK_SIZE + HIT_PAD)
				{
					if (i == CHK_WINDOWED || i == CHK_FULLSCREEN)
					{
						// Radio pair
						check_val[CHK_WINDOWED] = (i == CHK_WINDOWED);
						check_val[CHK_FULLSCREEN] = (i == CHK_FULLSCREEN);
					}
					else
					{
						check_val[i] = !check_val[i];

						// Apply mute immediately
						if (i == CHK_MUTE_BGM)
							Music::set_bgmvolume(check_val[CHK_MUTE_BGM] ? 0 : slider_val[SL_BGM]);
						else if (i == CHK_MUTE_SFX)
							Sound::set_sfxvolume(check_val[CHK_MUTE_SFX] ? 0 : slider_val[SL_SFX]);
					}

					return Cursor::State::CANCLICK;
				}
			}
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIOptionMenu::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				revert_settings();
				deactivate();
			}
			else if (keycode == KeyAction::Id::RETURN)
			{
				button_pressed(BT_OK);
			}
		}
	}

	UIElement::Type UIOptionMenu::get_type() const
	{
		return TYPE;
	}

	void UIOptionMenu::load_settings()
	{
		slider_val[SL_BGM] = Setting<BGMVolume>::get().load();
		slider_val[SL_SFX] = Setting<SFXVolume>::get().load();
		slider_val[SL_GRAPHICS] = Setting<GraphicsQuality>::get().load();
		slider_val[SL_EFFECTS] = Setting<EffectsQuality>::get().load();
		slider_val[SL_MOUSE_SPEED] = Setting<MouseSpeed>::get().load();
		slider_val[SL_HP_WARNING] = Setting<HPWarning>::get().load();
		slider_val[SL_MP_WARNING] = Setting<MPWarning>::get().load();

		// Viewing mode radio pair
		bool fullscreen = Setting<Fullscreen>::get().load();
		check_val[CHK_WINDOWED] = !fullscreen;
		check_val[CHK_FULLSCREEN] = fullscreen;

		check_val[CHK_SCREEN_SHAKE] = Setting<ScreenShake>::get().load();
		check_val[CHK_MINIMAP_SIMPLE] = Setting<MiniMapSimpleMode>::get().load();

		// Social filters
		check_val[CHK_WHISPER]         = Setting<AllowWhisper>::get().load();
		check_val[CHK_FRIEND_CHAT]     = Setting<AllowFriendChat>::get().load();
		check_val[CHK_CHAT_INVITE]     = Setting<AllowChatInvite>::get().load();
		check_val[CHK_TRADE_REQUEST]   = Setting<AllowTradeRequest>::get().load();
		check_val[CHK_PARTY_INVITE]    = Setting<AllowPartyInvite>::get().load();
		check_val[CHK_GUILD_CHAT]      = Setting<AllowGuildChat>::get().load();
		check_val[CHK_GUILD_INVITE]    = Setting<AllowGuildInvite>::get().load();
		check_val[CHK_ALLIANCE_INVITE] = Setting<AllowAllianceInvite>::get().load();
		check_val[CHK_FAMILY_INVITE]   = Setting<AllowFamilyInvite>::get().load();

		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_orig[i] = slider_val[i];

		for (int i = 0; i < NUM_CHECKS; i++)
			check_orig[i] = check_val[i];
	}

	void UIOptionMenu::save_settings()
	{
		Setting<BGMVolume>::get().save(slider_val[SL_BGM]);
		Setting<SFXVolume>::get().save(slider_val[SL_SFX]);

		Setting<GraphicsQuality>::get().save(slider_val[SL_GRAPHICS]);
		Setting<EffectsQuality>::get().save(slider_val[SL_EFFECTS]);
		Setting<MouseSpeed>::get().save(slider_val[SL_MOUSE_SPEED]);
		Window::get().apply_mouse_speed(slider_val[SL_MOUSE_SPEED]);
		Setting<HPWarning>::get().save(slider_val[SL_HP_WARNING]);
		Setting<MPWarning>::get().save(slider_val[SL_MP_WARNING]);

		bool want_fullscreen = check_val[CHK_FULLSCREEN];
		bool was_fullscreen = Setting<Fullscreen>::get().load();

		if (want_fullscreen != was_fullscreen)
		{
			Setting<Fullscreen>::get().save(want_fullscreen);
			Window::get().toggle_fullscreen();
		}

		Setting<ScreenShake>::get().save(check_val[CHK_SCREEN_SHAKE]);

		// Minimap mode: recreate the minimap if it changed
		bool old_simple = Setting<MiniMapSimpleMode>::get().load();
		Setting<MiniMapSimpleMode>::get().save(check_val[CHK_MINIMAP_SIMPLE]);

		if (check_val[CHK_MINIMAP_SIMPLE] != old_simple)
		{
			UI::get().remove(UIElement::Type::MINIMAP);
			UI::get().emplace<UIMiniMap>(Stage::get().get_player().get_stats());
		}

		// Social filters
		Setting<AllowWhisper>::get().save(check_val[CHK_WHISPER]);
		Setting<AllowFriendChat>::get().save(check_val[CHK_FRIEND_CHAT]);
		Setting<AllowChatInvite>::get().save(check_val[CHK_CHAT_INVITE]);
		Setting<AllowTradeRequest>::get().save(check_val[CHK_TRADE_REQUEST]);
		Setting<AllowPartyInvite>::get().save(check_val[CHK_PARTY_INVITE]);
		Setting<AllowGuildChat>::get().save(check_val[CHK_GUILD_CHAT]);
		Setting<AllowGuildInvite>::get().save(check_val[CHK_GUILD_INVITE]);
		Setting<AllowAllianceInvite>::get().save(check_val[CHK_ALLIANCE_INVITE]);
		Setting<AllowFamilyInvite>::get().save(check_val[CHK_FAMILY_INVITE]);

		// Apply final audio
		Music::set_bgmvolume(check_val[CHK_MUTE_BGM] ? 0 : slider_val[SL_BGM]);
		Sound::set_sfxvolume(check_val[CHK_MUTE_SFX] ? 0 : slider_val[SL_SFX]);

		Configuration::get().save();

		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_orig[i] = slider_val[i];

		for (int i = 0; i < NUM_CHECKS; i++)
			check_orig[i] = check_val[i];
	}

	void UIOptionMenu::revert_settings()
	{
		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_val[i] = slider_orig[i];

		for (int i = 0; i < NUM_CHECKS; i++)
			check_val[i] = check_orig[i];

		Music::set_bgmvolume(check_val[CHK_MUTE_BGM] ? 0 : slider_val[SL_BGM]);
		Sound::set_sfxvolume(check_val[CHK_MUTE_SFX] ? 0 : slider_val[SL_SFX]);
	}
}
