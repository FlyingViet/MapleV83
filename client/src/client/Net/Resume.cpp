//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "Resume.h"

#include "Session.h"

#include "Packets/MessagingPackets.h"

#include "../Configuration.h"
#include "../IO/UI.h"

#include <iostream>

namespace ms
{
	namespace Resume
	{
		namespace
		{
			// How long a resumed connection has to show it is still alive.
			constexpr int32_t REPLY_MS = 6000;

			bool background = false;
			bool afk_sent = false;

			std::string account;
			std::string password;
			int32_t played_cid = 0;
			std::string played_name;

			Target target;
			bool armed = false;
			bool taken_over = false;
			bool switching = false;
			std::string notice;

			bool in_game()
			{
				return UI::get().get_state_id() == UI::State::GAME && Session::get().is_connected();
			}
		}

		void backgrounded()
		{
			if (background)
				return;

			background = true;

			// The server keeps an AFK character in the world while the app
			// can't answer it.
			if (in_game())
			{
				GeneralChatPacket("@mv83afk on", false).dispatch();
				afk_sent = true;
				std::cout << "[Net] In the background: AFK" << std::endl;
			}
		}

		void foregrounded()
		{
			if (!background)
				return;

			background = false;

			if (!afk_sent)
				return;

			afk_sent = false;

			// What arrived while away first (it may say the session was taken).
			Session::get().read();

			if (!in_game())
				return;

			// The server answers this; silence means the connection died
			// while the app was suspended, and the game signs back in.
			std::cout << "[Net] Back in front: checking the connection" << std::endl;
			GeneralChatPacket("@mv83afk off", false).dispatch();
			Session::get().expect_reply(REPLY_MS);
		}

		bool in_background()
		{
			return background;
		}

		void remember_login(const std::string& acc, const std::string& pass)
		{
			account = acc;
			password = pass;
		}

		void remember_character(int32_t, uint8_t, int32_t cid, const std::string& name)
		{
			played_cid = cid;
			played_name = name;
		}

		void change_character()
		{
			if (!in_game())
				return;

			std::cout << "[Net] Changing character" << std::endl;
			switching = true;
			Session::get().drop();
		}

		bool changing_character()
		{
			return switching;
		}

		bool arm()
		{
			bool to_list = switching;
			switching = false;
			armed = false;
			afk_sent = false;

			if (taken_over)
			{
				taken_over = false;
				notice = "You signed in on another device.";
				return false;
			}

			if (account.empty() || password.empty() || played_cid == 0)
				return false;

			if (UI::get().get_state_id() == UI::State::LOGIN)
				return false;

			// The channel the player was on last, which may have changed in game.
			target.account = account;
			target.password = password;
			target.world = Configuration::get().get_worldid();
			target.channel = Configuration::get().get_channelid();
			target.cid = played_cid;
			target.name = played_name;
			target.play = !to_list;
			armed = true;

			std::cout << (to_list ? "[Net] Back to the character list" : "[Net] Will sign back in as " + played_name) << std::endl;
			return true;
		}

		const Target* pending()
		{
			return armed ? &target : nullptr;
		}

		void clear()
		{
			armed = false;
		}

		void replaced()
		{
			taken_over = true;
		}

		std::string take_notice()
		{
			std::string out = notice;
			notice.clear();
			return out;
		}
	}
}
