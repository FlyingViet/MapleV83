//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../MobileUI.h"
#include "../Components/Textfield.h"

#include "../../Character/Look/CharLook.h"
#include "../../Net/Login.h"

#include <chrono>
#include <string>
#include <vector>

namespace ms
{
	// The phone's way from opening the app to playing: one full-screen lobby
	// in the mobile style instead of v83's login, world select, character
	// select and creation windows. Its pages are sign in, choose a channel,
	// characters, and a new character (looks only: v83's dice are gone, the
	// MapleV83 server never read them). The login packet handlers send their
	// answers here while it is showing.
	class UIMobileLobby : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILELOBBY;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIMobileLobby();

		void draw(float alpha) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> pos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		void send_scroll(double yoffset) override;
		bool is_in_range(Point<int16_t>) const override { return true; }
		bool captures_hold() const override { return true; }

		UIElement::Type get_type() const override;

		// The server's answers.
		void login_failed(int32_t reason);
		void logged_in(const Account& account);
		void add_world(const World& world);
		void worlds_ready();
		void set_characters(std::vector<CharEntry> characters, int32_t slots, int8_t pic);
		void name_checked(bool used);
		void character_added(CharEntry character);
		void delete_result(int32_t id, uint8_t state);
		void show_error(const std::string& message);

	private:
		enum class Page { LOGIN, WORLDS, CHARACTERS, CREATE };

		struct Spot
		{
			MobileUI::Rect rect;
			int32_t target;
		};

		// A character on the list, with its look kept animating.
		struct Slot
		{
			CharEntry entry;
			CharLook look;
		};

		// The appearances a class offers (Etc MakeCharInfo.img, which the
		// server checks a new character against), per gender.
		struct Pool
		{
			std::vector<int32_t> faces, hairs, tops, bottoms, shoes, weapons;
			std::vector<uint8_t> hair_colors, skins;
		};

		// ---- pages ----
		void go(Page next);
		std::vector<Spot> spots() const;
		int32_t hit(Point<int16_t> pos) const;
		void activate(int32_t target);

		void draw_backdrop() const;
		void draw_banner(const MobileUI::Rect& r) const;
		void draw_login() const;
		void draw_worlds() const;
		void draw_characters() const;
		void draw_create() const;
		void draw_confirm() const;
		void draw_resume() const;

		// ---- layout ----
		MobileUI::Rect login_card() const;
		MobileUI::Rect world_card() const;
		MobileUI::Rect side_card() const;
		MobileUI::Rect stage() const;
		MobileUI::Rect account_field() const;
		MobileUI::Rect password_field() const;
		MobileUI::Rect name_field() const;
		MobileUI::Rect channel_rect(size_t world, size_t channel) const;
		MobileUI::Rect list_area() const;
		MobileUI::Rect slot_rect(size_t index) const;
		MobileUI::Rect option_rect(int option) const;
		MobileUI::Rect confirm_card() const;
		int16_t list_content() const;
		void clamp_list();

		// ---- actions ----
		void log_in();
		void log_out();
		void enter_channel();
		void play();
		void start_create();
		void submit_create();
		void step_option(int option, int direction);
		void randomize();
		void refresh_preview();
		const Pool& pool() const;
		std::string option_value(int option) const;
		void set_busy(const std::string& text);
		void clear_busy();
		void fail(const std::string& message);

		// ---- state ----
		Page page = Page::LOGIN;
		// Signing back in on its own after the connection was lost in game.
		bool resuming = false;
		// ...only to the character list (Change Character in the menu).
		bool to_list = false;
		std::string resume_name;
		std::string busy;
		std::chrono::steady_clock::time_point busy_since;
		std::string error;
		int32_t error_ticks = 0;
		std::string notice;
		int32_t notice_ticks = 0;
		uint32_t tick = 0;

		Textfield account;
		Textfield password;
		Textfield name;
		bool remember = true;
		std::string account_name;

		std::vector<World> worlds;
		bool worlds_loaded = false;
		int8_t world_id = 0;
		uint8_t channel_id = 0;

		std::vector<Slot> slots;
		int32_t slot_count = 0;
		int8_t require_pic = 2;
		size_t selected = 0;
		int16_t list_scroll = 0;
		bool confirm_delete = false;

		// The character being made.
		int creation_class = 1;	// 0 Cygnus, 1 Explorer, 2 Aran
		bool female = false;
		size_t choice[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		Pool pools[3][2];
		CharLook preview;
		std::string pending_name;

		Texture sky;
		Texture backdrop;
		Texture logo;

		// Press tracking: a target acts when the finger lifts on it.
		bool tracking = false;
		bool dragging = false;
		uint32_t press = 0;
		int32_t target = -1;
		Point<int16_t> press_start;
		Point<int16_t> press_last;
	};
}
