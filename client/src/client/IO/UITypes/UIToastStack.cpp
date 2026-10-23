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
#include "UIToastStack.h"

#include "../MobileUI.h"
#include "../NotificationCenter.h"
#include "../UI.h"
#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "UIStatusBar.h"

#include <algorithm>
#include <cctype>

namespace ms
{
	using Kind = MobileNoticeCard::Kind;
	using MobileUI::Style;

	namespace
	{
		// How long a toast shows: alerts 10 s, invites 30 s.
		constexpr int32_t LIFETIME_MS = 10000;
		constexpr int32_t INVITE_MS = 30000;
		// Fade-in: short ramp so the toast appears responsive.
		constexpr int32_t FADE_IN_MS = 100;
		// Fade-out: longer so it has visual weight as it leaves.
		constexpr int32_t FADE_OUT_MS = 250;
		// X nudge so the toast sits just inside the BT_NOTICE button
		// instead of hanging far to its left. Mirrors UINotificationList.
		constexpr int16_t POCKET_X_NUDGE = 20;
	}

	UIToastStack::UIToastStack()
		: UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0), true)
	{
		mobile = MobileUI::enabled();

		row.load();
		dimension = row.dims;

		buttons[Buttons::BT_ACCEPT]  = row.accept_button();
		buttons[Buttons::BT_DECLINE] = row.decline_button();
		update_button_visibility();
		place();
	}

	void UIToastStack::push(std::string title, std::string body,
		std::function<void(bool yes)> resolver, bool actionable, int32_t center_id, bool invite)
	{
		Entry e;
		e.title_str = std::move(title);
		e.body_str  = std::move(body);
		e.resolver  = std::move(resolver);
		e.actionable = actionable;
		e.center_id = center_id;
		e.invite = invite;
		e.serial = next_serial++;

		if (invite)
		{
			// Ahead of every plain alert; one showing now starts over later.
			auto at = std::find_if(queue.begin(), queue.end(), [](const Entry& q) { return !q.invite; });

			if (at != queue.end())
				at->age = 0;

			queue.insert(at, std::move(e));
		}
		else
		{
			queue.push_back(std::move(e));
		}

		update_button_visibility();
	}

	bool UIToastStack::has(const std::string& title) const
	{
		return std::any_of(queue.begin(), queue.end(), [&title](const Entry& e) { return e.title_str == title; });
	}

	bool UIToastStack::paused() const
	{
		// A panel, a conversation or a notice card has the screen: the banner
		// waits (the Inbox chip still counts it) instead of running out.
		return mobile && (UI::get().side_panel_open() || UI::get().npc_talk_open() || UI::get().popover_open());
	}

	int32_t UIToastStack::lifetime(const Entry& e) const
	{
		return e.invite ? INVITE_MS : LIFETIME_MS;
	}

	void UIToastStack::resolve_front(bool yes)
	{
		if (queue.empty())
			return;

		auto resolver = std::move(queue.front().resolver);
		queue.pop_front();
		if (resolver)
			resolver(yes);

		update_button_visibility();
		if (!queue.empty())
			queue.front().age = 0;
	}

	void UIToastStack::expire_front()
	{
		if (queue.empty())
			return;

		// Drop the toast WITHOUT calling its resolver — the drawer
		// entry stays in NotificationCenter so the user can still act
		// on it from the bell button. NotificationCenter::tick()
		// auto-declines if it's never resolved before TTL_MS.
		queue.pop_front();
		update_button_visibility();
		if (!queue.empty())
			queue.front().age = 0;
	}

	void UIToastStack::update_button_visibility()
	{
		// The phone's banner draws its own buttons.
		bool have = !queue.empty() && !mobile;
		buttons[Buttons::BT_ACCEPT]->set_active(have);
		buttons[Buttons::BT_DECLINE]->set_active(have);
	}

	void UIToastStack::refresh_card()
	{
		const Entry& e = queue.front();
		card_serial = e.serial;

		Kind kind = MobileNoticeCard::title_kind(e.title_str);
		std::string eyebrow = e.title_str;
		std::transform(eyebrow.begin(), eyebrow.end(), eyebrow.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

		card.setup(MobileNoticeCard::Layout::BANNER, kind, eyebrow, e.body_str);

		if (!e.actionable)
		{
			card.add(Buttons::BT_DECLINE, "OK", Style::SECONDARY);
		}
		else if (e.invite)
		{
			card.add(Buttons::BT_DECLINE, "Decline", Style::SECONDARY);
			card.add(Buttons::BT_ACCEPT, "Accept", Style::PRIMARY);
		}
		else
		{
			card.add(Buttons::BT_DECLINE, "Dismiss", Style::SECONDARY);
			card.add(Buttons::BT_ACCEPT, "View", Style::PRIMARY);
		}
	}

	void UIToastStack::place()
	{
		Point<int16_t> anchor;
		if (auto sb = UI::get().get_element<UIStatusBar>())
		{
			anchor = sb->get_notice_anchor();
		}
		else
		{
			int16_t vw = Constants::Constants::get().get_viewwidth();
			int16_t vh = Constants::Constants::get().get_viewheight();
			anchor = Point<int16_t>(vw - 8, vh - 36);
		}
		position = anchor - Point<int16_t>(row.dims.x() - POCKET_X_NUDGE, row.dims.y());
	}

	void UIToastStack::update()
	{
		UIElement::update();

		// Answered or expired elsewhere (the drawer, the inbox): go away
		// without answering again.
		size_t before = queue.size();
		queue.erase(std::remove_if(queue.begin(), queue.end(),
			[](const Entry& e) { return e.center_id != 0 && !NotificationCenter::get().has(e.center_id); }), queue.end());

		if (queue.size() != before)
			update_button_visibility();

		if (queue.empty() || paused())
			return;

		queue.front().age += Constants::TIMESTEP;
		if (queue.front().age >= lifetime(queue.front()))
			expire_front();

		if (mobile)
		{
			card.update();

			if (!queue.empty())
			{
				if (queue.front().serial != card_serial)
					refresh_card();

				card.set_progress(1.0f - static_cast<float>(queue.front().age) / lifetime(queue.front()));
			}
		}
	}

	void UIToastStack::draw(float inter) const
	{
		if (queue.empty())
			return;

		if (mobile)
		{
			if (queue.front().serial == card_serial && !paused())
				card.draw();

			return;
		}

		const_cast<UIToastStack*>(this)->place();

		const Entry& e = queue.front();
		int32_t life = lifetime(e);

		// Linear fade in / hold / fade out.
		float opacity = 1.0f;
		if (e.age < FADE_IN_MS)
			opacity = static_cast<float>(e.age) / FADE_IN_MS;
		else if (e.age > life - FADE_OUT_MS)
			opacity = static_cast<float>(life - e.age) / FADE_OUT_MS;

		row.draw(position, e.title_str, e.body_str, opacity);
		UIElement::draw(opacity);
	}

	bool UIToastStack::is_in_range(Point<int16_t> cursorpos) const
	{
		if (mobile)
			return !queue.empty() && queue.front().serial == card_serial && !paused() && card.contains(cursorpos);

		return UIElement::is_in_range(cursorpos);
	}

	Cursor::State UIToastStack::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (queue.empty())
			return Cursor::State::IDLE;

		if (mobile)
		{
			MobileNoticeCard::Touch touch = card.touch(clicked, cursorpos);

			if (touch.tap == MobileNoticeCard::Tap::ACTION)
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				resolve_front(touch.id == Buttons::BT_ACCEPT);
			}

			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;
		}

		Cursor::State btn_state = UIElement::send_cursor(clicked, cursorpos);
		if (btn_state != Cursor::State::IDLE)
			return btn_state;

		return Cursor::State::IDLE;
	}

	Button::State UIToastStack::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::BT_ACCEPT:
			resolve_front(true);
			return Button::State::NORMAL;
		case Buttons::BT_DECLINE:
			resolve_front(false);
			return Button::State::NORMAL;
		}
		return Button::State::DISABLED;
	}

	UIElement::Type UIToastStack::get_type() const
	{
		return TYPE;
	}
}
