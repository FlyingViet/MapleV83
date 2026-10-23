//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileSocial.h"

#include "UIMobileConfirm.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Character/BuddyList.h"
#include "../../Character/Job.h"
#include "../../Character/Party.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/BuddyPackets.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Util/Misc.h"

#include <algorithm>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 420;
		constexpr int16_t PAD = 14;
		constexpr int16_t TABS_Y = 72;
		constexpr int16_t FIELD_Y = 120;
		constexpr int16_t FIELD_H = 44;
		constexpr int16_t LIST_Y = 178;
		constexpr int16_t BUDDY_H = 60;
		constexpr int16_t MEMBER_H = 84;
		constexpr int16_t ACTIONS_H = 50;
		constexpr int16_t GAP = 6;
		constexpr int16_t LEAVE_H = 46;

		enum Target : int32_t
		{
			T_TAB = 0,			// + tab
			T_SUBMIT = 10,
			T_CREATE = 11,
			T_LEAVE = 12,
			T_ROW = 1000,		// + index
			T_ACTION = 2000		// + index * 4 + action
		};

		Party& party()
		{
			return Stage::get().get_player().get_party();
		}

		const std::vector<PartyMember>& members()
		{
			return party().get_members();
		}

		std::vector<BuddyEntry> buddies()
		{
			std::vector<BuddyEntry> list;

			for (const auto& b : Stage::get().get_player().get_buddylist().get_entries())
				list.push_back(b.second);

			// Online first, then by name.
			std::stable_sort(list.begin(), list.end(), [](const BuddyEntry& a, const BuddyEntry& b)
			{
				if (a.online() != b.online())
					return a.online();

				return a.name < b.name;
			});

			return list;
		}

		int32_t me()
		{
			return Stage::get().get_player().get_oid();
		}
	}

	UIMobileSocial::UIMobileSocial(Tab t) : UIMobilePanel(Edge::LEFT, W), tab(t)
	{
		field = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::WHITE, 1.0f,
			Rectangle<int16_t>(Point<int16_t>(14, 13), Point<int16_t>(250, 31)), 12);
		field.set_enter_callback([&](std::string) { submit(); });
	}

	UIMobileSocial::~UIMobileSocial()
	{
		unfocus();
	}

	void UIMobileSocial::show(Tab t)
	{
		if (tab != t)
		{
			tab = t;
			selected = 0;
			scroll = 0;
		}

		makeactive();
	}

	void UIMobileSocial::deactivate()
	{
		unfocus();
		UIMobilePanel::deactivate();
	}

	void UIMobileSocial::unfocus()
	{
		if (field.get_state() == Textfield::State::FOCUSED)
		{
			field.set_state(Textfield::State::NORMAL);
			UI::get().remove_textfield();
		}
	}

	bool UIMobileSocial::in_party() const
	{
		return party().is_in_party() && !members().empty();
	}

	bool UIMobileSocial::leading() const
	{
		return in_party() && party().get_leader() == me();
	}

	void UIMobileSocial::submit()
	{
		std::string name = field.get_text();
		name.erase(std::remove(name.begin(), name.end(), ' '), name.end());

		if (name.empty())
			return;

		Sound(Sound::Name::BUTTONCLICK).play();

		if (tab == Tab::BUDDIES)
			AddBuddyPacket(name).dispatch();
		else
			InviteToPartyPacket(name).dispatch();

		field.change_text("");
		unfocus();
	}

	// ---- layout -----------------------------------------------------------

	Rect UIMobileSocial::tab_rect(int index) const
	{
		int16_t w = static_cast<int16_t>((W - PAD * 2 - 6) / 2);

		return rect(static_cast<int16_t>(position.x() + PAD + index * (w + 6)), static_cast<int16_t>(position.y() + TABS_Y), w, 36);
	}

	Rect UIMobileSocial::field_rect() const
	{
		return rect(static_cast<int16_t>(position.x() + PAD), static_cast<int16_t>(position.y() + FIELD_Y), W - PAD * 2 - 98, FIELD_H);
	}

	Rect UIMobileSocial::submit_rect() const
	{
		Rect f = field_rect();

		return rect(static_cast<int16_t>(f.right() + 8), f.top(), 90, FIELD_H);
	}

	Rect UIMobileSocial::leave_rect() const
	{
		Rect f = frame();

		return rect(static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.bottom() - 14 - LEAVE_H), W - PAD * 2, LEAVE_H);
	}

	Rect UIMobileSocial::create_rect() const
	{
		Rectangle<int16_t> area = scroll_area();

		return rect(static_cast<int16_t>(area.left() + 16), static_cast<int16_t>(area.top() + 92), static_cast<int16_t>(area.width() - 32), 46);
	}

	Rectangle<int16_t> UIMobileSocial::scroll_area() const
	{
		Rect f = frame();
		int16_t top = static_cast<int16_t>(f.top() + LIST_Y);
		int16_t bottom = static_cast<int16_t>(tab == Tab::PARTY && in_party() ? leave_rect().top() - 10 : f.bottom() - 12);

		return rect(static_cast<int16_t>(f.left() + PAD), top, W - PAD * 2, static_cast<int16_t>(bottom - top));
	}

	size_t UIMobileSocial::row_count() const
	{
		if (tab == Tab::BUDDIES)
			return Stage::get().get_player().get_buddylist().get_entries().size();

		return in_party() ? members().size() : 0;
	}

	int16_t UIMobileSocial::row_height(size_t index) const
	{
		int32_t id = 0;

		if (tab == Tab::BUDDIES)
		{
			std::vector<BuddyEntry> list = buddies();

			if (index < list.size())
				id = list[index].cid;

			return static_cast<int16_t>(BUDDY_H + (id != 0 && id == selected ? ACTIONS_H : 0));
		}

		const auto& list = members();

		if (index < list.size())
			id = list[index].cid;

		bool actions = id != 0 && id == selected && leading() && id != me();

		return static_cast<int16_t>(MEMBER_H + (actions ? ACTIONS_H : 0));
	}

	int16_t UIMobileSocial::scroll_content() const
	{
		int16_t h = 0;

		for (size_t i = 0; i < row_count(); i++)
			h = static_cast<int16_t>(h + row_height(i) + GAP);

		return h;
	}

	Rect UIMobileSocial::row_rect(size_t index) const
	{
		Rectangle<int16_t> area = scroll_area();
		int16_t y = static_cast<int16_t>(area.top() - scroll);

		for (size_t i = 0; i < index; i++)
			y = static_cast<int16_t>(y + row_height(i) + GAP);

		return rect(area.left(), y, area.width(), row_height(index));
	}

	Rect UIMobileSocial::row_action(size_t index, int action, int count) const
	{
		Rect r = row_rect(index);
		int16_t gap = 8;
		int16_t w = static_cast<int16_t>((r.width() - 24 - gap * (count - 1)) / std::max(1, count));

		return rect(static_cast<int16_t>(r.left() + 12 + action * (w + gap)), static_cast<int16_t>(r.bottom() - 10 - 38), w, 38);
	}

	// ---- input ------------------------------------------------------------

	Cursor::State UIMobileSocial::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// The name box takes the keyboard as soon as it is touched.
		if (clicked && field_rect().contains(cursorpos) && field.get_state() != Textfield::State::FOCUSED)
		{
			field.set_state(Textfield::State::FOCUSED);
			return Cursor::State::CLICKING;
		}

		return UIMobilePanel::send_cursor(clicked, cursorpos);
	}

	int32_t UIMobileSocial::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		for (int t = 0; t < 2; t++)
			if (inset(tab_rect(t), -2, -4).contains(pos))
				return T_TAB + t;

		if (submit_rect().contains(pos))
			return T_SUBMIT;

		if (tab == Tab::PARTY && in_party() && leave_rect().contains(pos))
			return T_LEAVE;

		Rectangle<int16_t> area = scroll_area();

		if (!area.contains(pos))
			return NO_TARGET;

		if (tab == Tab::PARTY && !in_party())
			return create_rect().contains(pos) ? T_CREATE : NO_TARGET;

		for (size_t i = 0; i < row_count(); i++)
		{
			Rect r = row_rect(i);

			if (!r.contains(pos))
				continue;

			if (row_height(i) > (tab == Tab::BUDDIES ? BUDDY_H : MEMBER_H))
				for (int a = 0; a < 2; a++)
					if (row_action(i, a, 2).contains(pos))
						return T_ACTION + static_cast<int32_t>(i) * 4 + a;

			return T_ROW + static_cast<int32_t>(i);
		}

		return NO_TARGET;
	}

	void UIMobileSocial::activate(int32_t target)
	{
		if (target >= T_ACTION)
		{
			size_t index = static_cast<size_t>((target - T_ACTION) / 4);
			int action = (target - T_ACTION) % 4;
			Sound(Sound::Name::BUTTONCLICK).play();

			if (tab == Tab::BUDDIES)
			{
				std::vector<BuddyEntry> list = buddies();

				if (index >= list.size())
					return;

				BuddyEntry b = list[index];

				if (action == 0)
				{
					if (b.online())
						InviteToPartyPacket(b.name).dispatch();
				}
				else
				{
					int32_t cid = b.cid;
					UI::get().emplace<UIMobileConfirm>("Remove buddy?", b.name + " will be removed from your buddy list.", "Remove", true,
						[cid]() { DeleteBuddyPacket(cid).dispatch(); });
				}
			}
			else
			{
				const auto& list = members();

				if (index >= list.size())
					return;

				PartyMember m = list[index];
				int32_t cid = m.cid;

				if (action == 0)
					UI::get().emplace<UIMobileConfirm>("Make leader?", m.name + " will lead the party.", "Make leader", false,
						[cid]() { ChangePartyLeaderPacket(cid).dispatch(); });
				else
					UI::get().emplace<UIMobileConfirm>("Remove from party?", m.name + " will be kicked from the party.", "Kick", true,
						[cid]() { ExpelFromPartyPacket(cid).dispatch(); });
			}

			selected = 0;
			clamp_scroll();
			return;
		}

		if (target >= T_ROW)
		{
			size_t index = static_cast<size_t>(target - T_ROW);
			int32_t id = 0;

			if (tab == Tab::BUDDIES)
			{
				std::vector<BuddyEntry> list = buddies();

				if (index < list.size())
					id = list[index].cid;
			}
			else if (index < members().size())
			{
				id = members()[index].cid;
			}

			Sound(Sound::Name::BUTTONCLICK).play();
			selected = selected == id ? 0 : id;
			clamp_scroll();
			return;
		}

		switch (target)
		{
			case T_TAB + 0:
			case T_TAB + 1:
				Sound(Sound::Name::BUTTONCLICK).play();
				show(target == T_TAB ? Tab::BUDDIES : Tab::PARTY);
				break;
			case T_SUBMIT:
				submit();
				break;
			case T_CREATE:
				Sound(Sound::Name::BUTTONCLICK).play();
				CreatePartyPacket().dispatch();
				break;
			case T_LEAVE:
			{
				bool lead = leading();
				UI::get().emplace<UIMobileConfirm>(lead ? "Disband the party?" : "Leave the party?",
					lead ? "You lead this party, so leaving ends it for everyone." : "You can be invited back later.",
					lead ? "Disband" : "Leave", true, []() { LeavePartyPacket().dispatch(); });
				break;
			}
		}
	}

	void UIMobileSocial::update()
	{
		UIMobilePanel::update();

		field.update(field_rect().get_left_top());

		// Members leave and buddies vanish while the panel is open.
		if (selected != 0)
		{
			bool found = false;

			if (tab == Tab::BUDDIES)
				found = Stage::get().get_player().get_buddylist().get_entries().count(selected) > 0;
			else
				for (const auto& m : members())
					found = found || m.cid == selected;

			if (!found)
				selected = 0;
		}

		clamp_scroll();
	}

	// ---- drawing ----------------------------------------------------------

	void UIMobileSocial::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();
		std::vector<BuddyEntry> list = buddies();
		size_t online = std::count_if(list.begin(), list.end(), [](const BuddyEntry& b) { return b.online(); });
		bool partied = in_party();

		std::string subtitle = std::to_string(online) + " of " + std::to_string(list.size()) + " buddies online"
			+ (partied ? "  \xC2\xB7  party of " + std::to_string(members().size()) : "");
		panel(f, "Friends", subtitle);
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		button(tab_rect(0), "Buddies  " + std::to_string(list.size()), tab == Tab::BUDDIES ? Style::SELECTED : Style::SECONDARY, true,
			pressed == T_TAB, Font::SMALL_BOLD);
		button(tab_rect(1), partied ? "Party  " + std::to_string(members().size()) : "Party", tab == Tab::PARTY ? Style::SELECTED : Style::SECONDARY, true,
			pressed == T_TAB + 1, Font::SMALL_BOLD);

		// Name box: add a buddy, or invite to the party.
		Rect fr = field_rect();
		bool focused = field.get_state() == Textfield::State::FOCUSED;
		fill(fr, focused ? Theme::CARD_HI : Theme::CARD, 12);

		if (focused)
			fill(rect(static_cast<int16_t>(fr.left() + 12), static_cast<int16_t>(fr.bottom() - 3), static_cast<int16_t>(fr.width() - 24), 2), Theme::ACCENT, 1);

		if (field.empty() && !focused)
			text_in(tab == Tab::BUDDIES ? "Character name" : "Invite by name", Font::BODY, Theme::FAINT, fr, Text::Alignment::LEFT, 14);

		field.draw(fr.get_left_top());
		button(submit_rect(), tab == Tab::BUDDIES ? "Add" : "Invite", Style::PRIMARY, !field.empty(), pressed == T_SUBMIT, Font::SMALL_BOLD);

		Rectangle<int16_t> area = scroll_area();

		if (tab == Tab::PARTY && !partied)
		{
			Rect card = rect(area.left(), area.top(), area.width(), 154);
			fill(card, Theme::CARD, 16);
			text_in("You're not in a party", Font::BODY_BOLD, Theme::TEXT, rect(card.left(), static_cast<int16_t>(card.top() + 16), card.width(), 24));
			text("Make one, or invite someone above and a party starts with them.", Font::SMALL, Theme::MUTED,
				Point<int16_t>(static_cast<int16_t>(card.left() + card.width() / 2), static_cast<int16_t>(card.top() + 46)), Text::Alignment::CENTER,
				static_cast<int16_t>(card.width() - 40));
			button(create_rect(), "Create party", Style::PRIMARY, true, pressed == T_CREATE);
			return;
		}

		if (tab == Tab::BUDDIES && list.empty())
		{
			text("No buddies yet. Type a character's name above and tap Add; they get a request to accept.", Font::BODY, Theme::MUTED,
				Point<int16_t>(static_cast<int16_t>(area.left() + 4), static_cast<int16_t>(area.top() + 8)), Text::Alignment::LEFT,
				static_cast<int16_t>(area.width() - 8));
			return;
		}

		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		if (tab == Tab::BUDDIES)
		{
			for (size_t i = 0; i < list.size(); i++)
			{
				Rect r = row_rect(i);

				if (r.bottom() < area.top() || r.top() > area.bottom())
					continue;

				const BuddyEntry& b = list[i];
				bool open = b.cid == selected;
				fill(r, pressed == T_ROW + static_cast<int32_t>(i) || open ? Theme::CARD_HI : Theme::CARD, 14);

				Rect avatar = rect(static_cast<int16_t>(r.left() + 12), static_cast<int16_t>(r.top() + 12), 36, 36);
				fill(avatar, (b.online() ? Theme::GREEN : Theme::FAINT).alpha(0.2f), 18);
				text_in(b.name.substr(0, 1), Font::BODY_BOLD, b.online() ? Theme::GREEN : Theme::MUTED, avatar);
				fill(rect(static_cast<int16_t>(avatar.right() - 10), static_cast<int16_t>(avatar.bottom() - 10), 10, 10),
					b.online() ? Theme::GREEN : Theme::FAINT, 5);

				int16_t tx = static_cast<int16_t>(avatar.right() + 12);
				int16_t tw = static_cast<int16_t>(r.right() - 34 - tx);
				text(ellipsize(b.name, Font::BODY_BOLD, tw), Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(tx, static_cast<int16_t>(r.top() + 10)));

				std::string where = b.online() ? "Online  \xC2\xB7  Channel " + std::to_string(b.channel + 1)
					: b.status != 0 ? "Waiting for them to accept" : "Offline";
				text(ellipsize(where, Font::SMALL, tw), Font::SMALL, b.online() ? Theme::GREEN : Theme::MUTED,
					Point<int16_t>(tx, static_cast<int16_t>(r.top() + 33)));

				glyph(rect(static_cast<int16_t>(r.right() - 28), static_cast<int16_t>(r.top() + 20), 20, 20), open ? Glyph::UP : Glyph::DOWN, Theme::FAINT, 12);

				if (open)
				{
					int32_t base = T_ACTION + static_cast<int32_t>(i) * 4;
					button(row_action(i, 0, 2), "Invite to party", Style::SECONDARY, b.online(), pressed == base, Font::SMALL_BOLD);
					button(row_action(i, 1, 2), "Remove", Style::DANGER, true, pressed == base + 1, Font::SMALL_BOLD);
				}
			}
		}
		else
		{
			int32_t leader = party().get_leader();
			const auto& list_m = members();

			for (size_t i = 0; i < list_m.size(); i++)
			{
				Rect r = row_rect(i);

				if (r.bottom() < area.top() || r.top() > area.bottom())
					continue;

				const PartyMember& m = list_m[i];
				bool on = m.online;
				bool self = m.cid == me();
				bool open = m.cid == selected && leading() && !self;
				fill(r, pressed == T_ROW + static_cast<int32_t>(i) || open ? Theme::CARD_HI : Theme::CARD, 14);

				Rect avatar = rect(static_cast<int16_t>(r.left() + 12), static_cast<int16_t>(r.top() + 14), 40, 40);
				fill(avatar, (on ? Theme::BLUE : Theme::FAINT).alpha(0.2f), 20);
				text_in(std::to_string(m.level), Font::BODY_BOLD, on ? Theme::BLUE : Theme::MUTED, avatar);

				int16_t tx = static_cast<int16_t>(avatar.right() + 12);
				int16_t right = static_cast<int16_t>(r.right() - 12);
				std::string tag = m.cid == leader ? "LEADER" : self ? "YOU" : "";
				int16_t tag_w = tag.empty() ? 0 : static_cast<int16_t>(text_width(tag, Font::SMALL_BOLD) + 16);

				if (!tag.empty())
				{
					Rect pill = rect(static_cast<int16_t>(right - tag_w), static_cast<int16_t>(r.top() + 10), tag_w, 22);
					RGBA tint = m.cid == leader ? Theme::GOLD : Theme::BLUE;
					fill(pill, tint.alpha(0.18f), 11);
					text_in(tag, Font::SMALL_BOLD, tint, pill);
				}

				std::string name = m.name + (self && m.cid == leader ? "  (you)" : "");
				text(ellipsize(name, Font::BODY_BOLD, static_cast<int16_t>(right - tag_w - 8 - tx)), Font::BODY_BOLD, Theme::TEXT,
					Point<int16_t>(tx, static_cast<int16_t>(r.top() + 9)));

				std::string line = "Lv " + std::to_string(m.level) + "  \xC2\xB7  " + Job(static_cast<uint16_t>(m.job)).get_name();

				if (on)
					line += "  \xC2\xB7  Ch " + std::to_string(m.channel + 1);

				text(ellipsize(line, Font::SMALL, static_cast<int16_t>(right - tx)), Font::SMALL, Theme::MUTED,
					Point<int16_t>(tx, static_cast<int16_t>(r.top() + 32)));

				std::string place = on ? NxHelper::Map::get_map_name(m.mapid) : "Offline";
				int16_t hp_w = 110;
				text(ellipsize(place, Font::SMALL, static_cast<int16_t>(right - hp_w - 10 - tx)), Font::SMALL, on ? Theme::TEXT.alpha(0.8f) : Theme::FAINT,
					Point<int16_t>(tx, static_cast<int16_t>(r.top() + 55)));

				// HP, when the server shares it (same map).
				if (on && m.maxhp > 0)
				{
					float hp = std::clamp(static_cast<float>(m.hp) / m.maxhp, 0.0f, 1.0f);
					Rect track = rect(static_cast<int16_t>(right - hp_w), static_cast<int16_t>(r.top() + 61), hp_w, 6);
					fill(track, Theme::TRACK, 3);
					fill(rect(track.left(), track.top(), static_cast<int16_t>(std::max(6.0f, hp_w * hp)), 6), Theme::HP, 3);
				}

				if (open)
				{
					int32_t base = T_ACTION + static_cast<int32_t>(i) * 4;
					button(row_action(i, 0, 2), "Make leader", Style::SECONDARY, on, pressed == base, Font::SMALL_BOLD);
					button(row_action(i, 1, 2), "Kick", Style::DANGER, true, pressed == base + 1, Font::SMALL_BOLD);
				}
			}
		}

		GraphicsGL::get().end_clip();

		if (tab == Tab::PARTY && partied)
			button(leave_rect(), leading() ? "Disband party" : "Leave party", Style::DANGER, true, pressed == T_LEAVE, Font::SMALL_BOLD);
	}

	UIElement::Type UIMobileSocial::get_type() const
	{
		return TYPE;
	}
}
