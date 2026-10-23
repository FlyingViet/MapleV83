//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <string>

namespace ms
{
	// Surviving the app going to the background. While away the character
	// stays in the world marked AFK; coming back resumes the same connection,
	// or, if it died, signs back in to the same character on its own.
	namespace Resume
	{
		// The app is about to be suspended / is back in front.
		void backgrounded();
		void foregrounded();
		bool in_background();

		// What a reconnect needs, kept in memory only: the lobby records the
		// sign-in and the character played.
		void remember_login(const std::string& account, const std::string& password);
		void remember_character(int32_t world, uint8_t channel, int32_t cid, const std::string& name);

		// Leave the game for the character list: the connection is dropped
		// on purpose and the lobby signs back in without entering the game.
		void change_character();
		bool changing_character();

		// The connection was lost in game: sign back in to that character
		// (or just to the character list when changing character). Returns
		// whether there is enough to do so.
		bool arm();

		struct Target
		{
			std::string account;
			std::string password;
			int32_t world = 0;
			uint8_t channel = 0;
			int32_t cid = 0;
			std::string name;
			// Enter the game with that character, or stop at the list.
			bool play = true;
		};

		// The armed reconnect, if any; the lobby clears it once done or failed.
		const Target* pending();
		void clear();

		// The server said this account signed in on another device: don't
		// take it back automatically.
		void replaced();
		// A message for the lobby to show once (empty when none).
		std::string take_notice();

		// The server's marker for a replaced session.
		constexpr const char* REPLACED_NOTICE = "[MV83] Signed in on another device.";
	}
}
