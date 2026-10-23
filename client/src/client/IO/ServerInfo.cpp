//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "ServerInfo.h"

#include "../Net/Packets/MessagingPackets.h"

#include <sstream>

namespace ms
{
	namespace ServerInfo
	{
		namespace
		{
			Info info;
			const std::string PREFIX = "[MV83] ";

			int number(const std::string& value, int fallback)
			{
				try { return std::stoi(value); }
				catch (...) { return fallback; }
			}
		}

		const Info& get()
		{
			return info;
		}

		bool is_admin()
		{
			return info.known && info.gm > 0;
		}

		void request()
		{
			GeneralChatPacket("@mv83info", false).dispatch();
		}

		bool consume(const std::string& line)
		{
			if (line.compare(0, PREFIX.size(), PREFIX) != 0)
				return false;

			std::istringstream words(line.substr(PREFIX.size()));
			std::string word;

			while (words >> word)
			{
				size_t eq = word.find('=');

				if (eq == std::string::npos)
					continue;

				std::string key = word.substr(0, eq);
				std::string value = word.substr(eq + 1);

				if (key == "gm")
					info.gm = number(value, info.gm);
				else if (key == "exp")
					info.exp = number(value, info.exp);
				else if (key == "drop")
					info.drop = number(value, info.drop);
				else if (key == "meso")
					info.meso = number(value, info.meso);
			}

			info.known = true;

			return true;
		}
	}
}
