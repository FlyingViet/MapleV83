//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>

namespace ms
{
	// What the MapleV83 server tells this client about the character and world:
	// its GM level and the world's rates. The client asks with the @mv83info
	// command on entering the game; the answer is one yellow line,
	// "[MV83] gm=6 exp=10 drop=10 meso=10", kept out of the chat log.
	namespace ServerInfo
	{
		struct Info
		{
			bool known = false;
			int gm = 0;
			int exp = 1;
			int drop = 1;
			int meso = 1;
		};

		const Info& get();
		bool is_admin();

		void request();

		// True when the line was an info line (now remembered).
		bool consume(const std::string& line);
	}
}
