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
#include "Notifications.h"

#include "NotificationCenter.h"
#include "UI.h"
#include "UITypes/UIStatusBar.h"
#include "UITypes/UIToastStack.h"

namespace ms
{
	namespace Notifications
	{
		namespace
		{
			void post(std::string title, std::string body,
				std::function<void(bool yes)> resolver, bool invite)
			{
				bool actionable = static_cast<bool>(resolver);

				// Persistent drawer entry — kept until user clicks Accept or
				// Decline in the bell-button popup (the phone's inbox). Returns
				// an id we use to resolve the same entry from the toast.
				int32_t entry_id = NotificationCenter::get().push(
					title, body, resolver, invite);

				// Transient toast — auto-dismisses, or sooner if the user
				// clicks Accept/Decline. Either path routes through
				// NotificationCenter::resolve so the drawer entry is removed
				// in lockstep.
				auto toasts = UI::get().get_element<UIToastStack>();
				if (!toasts)
				{
					UI::get().emplace<UIToastStack>();
					toasts = UI::get().get_element<UIToastStack>();
				}
				if (toasts)
				{
					toasts->push(std::move(title), std::move(body),
						[entry_id](bool yes)
						{
							NotificationCenter::get().resolve(entry_id, yes);
						},
						actionable, entry_id, invite);
				}

				// Bell badge on the status bar.
				if (auto statusbar = UI::get().get_element<UIStatusBar>())
					statusbar->notify();
			}
		}

		void notify(std::string title, std::string body,
			std::function<void(bool yes)> resolver)
		{
			post(std::move(title), std::move(body), std::move(resolver), false);
		}

		void invite(std::string title, std::string body,
			std::function<void(bool yes)> resolver)
		{
			post(std::move(title), std::move(body), std::move(resolver), true);
		}

		void info(std::string title, std::string body)
		{
			notify(std::move(title), std::move(body), nullptr);
		}

		void hint(std::string title, std::string body)
		{
			auto toasts = UI::get().get_element<UIToastStack>();

			if (!toasts)
			{
				UI::get().emplace<UIToastStack>();
				toasts = UI::get().get_element<UIToastStack>();
			}

			if (toasts && !toasts->has(title))
				toasts->push(std::move(title), std::move(body), nullptr, false);
		}
	}
}
