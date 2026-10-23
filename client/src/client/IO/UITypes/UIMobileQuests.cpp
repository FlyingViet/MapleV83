//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileQuests.h"

#include "UIMiniMap.h"
#include "UIMobileConfirm.h"
#include "UIQuestHelper.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Character/QuestLog.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Gameplay/MapleMap/Npc.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/QuestPackets.h"
#include "../../Util/Misc.h"

#include <nlnx/nx.hpp>

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 420;
		constexpr int16_t PAD = 14;
		constexpr int16_t TABS_Y = 72;
		constexpr int16_t LIST_Y = 120;
		constexpr int16_t ROW_H = 62;
		constexpr int16_t ROW_GAP = 6;
		constexpr int16_t ACTIONS_H = 46;

		enum Target : int32_t
		{
			T_TAB = 0,			// + tab
			T_BACK = 10,
			T_FORFEIT = 11,
			T_TRACK = 12,
			T_FIND = 13,
			T_ROW = 1000		// + index
		};

		const char* const TAB_NAMES[3] = { "In progress", "Available", "Done" };

		int32_t integer(nl::node n)
		{
			if (n.data_type() == nl::node::type::integer)
				return static_cast<int32_t>(n.get_integer());

			if (n.data_type() == nl::node::type::string)
			{
				try { return std::stoi(n.get_string()); }
				catch (...) { return 0; }
			}

			return 0;
		}

		std::string npc_name(int32_t id)
		{
			std::string name = nl::nx::string["Npc.img"][std::to_string(id)]["name"];

			return name.empty() ? "NPC " + std::to_string(id) : name;
		}

		std::string mob_name(int32_t id)
		{
			std::string name = nl::nx::string["Mob.img"][std::to_string(id)]["name"];

			return name.empty() ? "Monster " + std::to_string(id) : name;
		}

		bool has_cjk(const std::string& s)
		{
			for (size_t i = 0; i + 2 < s.size(); i++)
			{
				unsigned char c = static_cast<unsigned char>(s[i]);

				if (c >= 0xE3 && c <= 0xED)
					return true;
			}

			return false;
		}

		// Some descriptions in the v83 data are scrambled bytes.
		bool corrupt(const std::string& s)
		{
			for (char ch : s)
			{
				unsigned char c = static_cast<unsigned char>(ch);

				if (c < 0x20 && c != '\n' && c != '\r' && c != '\t')
					return true;
			}

			return false;
		}

		// MapleStory's text codes, resolved into words: #p/#@ NPC, #m map,
		// #o monster, #t/#z item, #c item count, #h the player; colours,
		// pictures and list markers dropped.
		std::string readable(const std::string& src)
		{
			std::string out;
			out.reserve(src.size());

			for (size_t i = 0; i < src.size(); i++)
			{
				char c = src[i];

				if (c == '\r')
					continue;

				if (c == '\n')
				{
					out += "\\n";
					continue;
				}

				if (c != '#' || i + 1 >= src.size())
				{
					out += c;
					continue;
				}

				char code = src[i + 1];

				// #h # is the player's name.
				if (code == 'h')
				{
					size_t end = src.find('#', i + 2);

					if (end != std::string::npos && end - i <= 4)
					{
						out += Stage::get().get_player().get_stats().get_name();
						i = end;
						continue;
					}
				}

				// #<code><id>[:]# carries an id; anything else is a lone switch.
				size_t j = i + 2;

				while (j < src.size() && (std::isdigit(static_cast<unsigned char>(src[j])) || src[j] == ':'))
					j++;

				if (j == i + 2 || j >= src.size() || src[j] != '#')
				{
					i++;
					continue;
				}

				int32_t id = 0;

				try { id = std::stoi(src.substr(i + 2, j - i - 2)); }
				catch (...) { id = 0; }

				switch (code)
				{
					case 'p':
					case '@': out += npc_name(id); break;
					case 'm': out += NxHelper::Map::get_map_name(id); break;
					case 'o': out += mob_name(id); break;
					case 't':
					case 'z': out += ItemData::get(id).get_name(); break;
					case 'c': out += std::to_string(Stage::get().get_player().get_inventory().get_total_item_count(id)); break;
					default: break;
				}

				i = j;
			}

			// Tidy what dropped codes leave behind.
			std::string tidy;

			for (char ch : out)
				if (!(ch == ' ' && !tidy.empty() && tidy.back() == ' '))
					tidy += ch;

			while (tidy.size() >= 2 && tidy.compare(tidy.size() - 2, 2, "\\n") == 0)
				tidy.resize(tidy.size() - 2);

			return tidy;
		}

		// Which map an NPC stands on: the current continent first, every
		// other continent only if needed. Kept for the session.
		std::string npc_location(int32_t npc)
		{
			static std::unordered_map<int32_t, int32_t> found;
			static std::unordered_map<int32_t, bool> scanned;

			auto known = found.find(npc);

			if (known != found.end())
				return known->second > 0 ? NxHelper::Map::get_map_name(known->second) : "";

			int32_t here = Stage::get().get_mapid() / 100000000;
			std::vector<int32_t> order = { here };

			for (int32_t c = 0; c <= 9; c++)
				if (c != here)
					order.push_back(c);

			for (int32_t continent : order)
			{
				if (scanned[continent])
					continue;

				scanned[continent] = true;

				for (nl::node map_img : nl::nx::map["Map"]["Map" + std::to_string(continent)])
				{
					nl::node life = map_img["life"];

					if (!life)
						continue;

					std::string file = map_img.name();
					int32_t map_id = 0;

					try { map_id = std::stoi(file.substr(0, file.find('.'))); }
					catch (...) { continue; }

					for (nl::node l : life)
					{
						nl::node type = l["type"];
						bool is_npc = type.data_type() == nl::node::type::string ? type.get_string() == "n" : integer(type) == 'n';

						if (!is_npc)
							continue;

						int32_t id = integer(l["id"]);

						if (id > 0 && !found.count(id))
							found[id] = map_id;
					}
				}

				if (found.count(npc))
					break;
			}

			auto hit = found.find(npc);

			if (hit == found.end())
			{
				found[npc] = 0;
				return "";
			}

			return NxHelper::Map::get_map_name(hit->second);
		}

		bool npc_on_map(int32_t npc)
		{
			MapObjects* npcs = Stage::get().get_npcs().get_npcs();

			for (auto it = npcs->begin(); it != npcs->end(); ++it)
			{
				auto* n = static_cast<Npc*>(it->second.get());

				if (n && n->get_npcid() == npc)
					return true;
			}

			return false;
		}

		// Kills recorded in a started quest's progress string: three digits per
		// required monster, in order.
		int32_t kills(const std::string& progress, int index)
		{
			size_t off = static_cast<size_t>(index) * 3;

			if (off + 3 > progress.size())
				return 0;

			try { return std::stoi(progress.substr(off, 3)); }
			catch (...) { return 0; }
		}
	}

	UIMobileQuests::UIMobileQuests(const QuestLog& ql) : UIMobilePanel(Edge::LEFT, W), questlog(ql)
	{
		rebuild();
	}

	// ---- data -------------------------------------------------------------

	void UIMobileQuests::rebuild()
	{
		for (auto& list : lists)
			list.clear();

		nl::node info = nl::nx::quest["QuestInfo.img"];
		nl::node check = nl::nx::quest["Check.img"];
		const Player& player = Stage::get().get_player();
		const Inventory& inventory = player.get_inventory();
		int16_t level = static_cast<int16_t>(player.get_stats().get_stat(MapleStat::Id::LEVEL));
		int16_t job = static_cast<int16_t>(player.get_stats().get_stat(MapleStat::Id::JOB));

		const auto& started = questlog.get_started();
		const auto& completed = questlog.get_completed();

		seen_started = started.size();
		seen_completed = completed.size();
		seen_level = level;
		seen_progress.clear();

		for (const auto& s : started)
			seen_progress += s.second + "|";

		auto title = [&](int16_t id) -> std::string
		{
			nl::node q = info[std::to_string(id)];

			if (!q || !q["name"])
				return "";

			std::string name = readable(q["name"].get_string());

			return has_cjk(name) ? "" : name;
		};

		// In progress: what is left, or that it is done.
		for (const auto& s : started)
		{
			std::string name = title(s.first);

			if (name.empty())
				continue;

			nl::node end = check[std::to_string(s.first)]["1"];
			bool ready = questlog.is_completable(s.first, inventory);
			std::string detail;

			if (!ready)
			{
				int index = 0;

				for (nl::node mob : end["mob"])
				{
					int32_t need = integer(mob["count"]);

					if (need > 0 && kills(s.second, index) < need)
					{
						detail = mob_name(integer(mob["id"])) + "  " + std::to_string(kills(s.second, index)) + " / " + std::to_string(need);
						break;
					}

					index++;
				}

				if (detail.empty())
				{
					for (nl::node item : end["item"])
					{
						int32_t id = integer(item["id"]);
						int32_t need = integer(item["count"]);
						int32_t have = inventory.get_total_item_count(id);

						if (id > 0 && need > 0 && have < need)
						{
							detail = ItemData::get(id).get_name() + "  " + std::to_string(have) + " / " + std::to_string(need);
							break;
						}
					}
				}
			}

			int32_t npc = integer(end["npc"]);

			if (detail.empty())
				detail = npc > 0 ? (ready ? "Ready \xC2\xB7 see " : "Talk to ") + npc_name(npc) : (ready ? "Ready to finish" : "In progress");

			lists[ACTIVE].push_back({ s.first, name, detail, 0, ready });
		}

		// Done.
		for (const auto& c : completed)
		{
			std::string name = title(c.first);

			if (!name.empty())
				lists[DONE].push_back({ c.first, name, "Completed", 0, false });
		}

		// Available now: level, job and earlier quests allow it, and an NPC
		// hands it out.
		for (nl::node q : info)
		{
			std::string key = q.name();
			int32_t id = 0;

			try { id = std::stoi(key); }
			catch (...) { continue; }

			if (id <= 0 || id > 32767)
				continue;

			int16_t qid = static_cast<int16_t>(id);

			if (started.count(qid) || completed.count(qid) || integer(q["autoStart"]) != 0)
				continue;

			nl::node start = check[key]["0"];

			if (!start || integer(start["normalAutoStart"]) != 0 || !start["end"].get_string().empty())
				continue;

			int32_t npc = integer(start["npc"]);

			if (npc <= 0)
				continue;

			int32_t lvmin = integer(start["lvmin"]);
			int32_t lvmax = integer(start["lvmax"]);

			if ((lvmin > 0 && level < lvmin) || (lvmax > 0 && level > lvmax))
				continue;

			nl::node jobs = start["job"];

			if (jobs && jobs.size() > 0)
			{
				bool ok = false;

				for (nl::node j : jobs)
				{
					int32_t need = integer(j);

					if (need == 0 || need == job || (need > 0 && job / 100 == need / 100 && job >= need))
					{
						ok = true;
						break;
					}
				}

				if (!ok)
					continue;
			}

			bool prereqs = true;

			for (nl::node p : start["quest"])
			{
				int16_t pid = static_cast<int16_t>(integer(p["id"]));
				int32_t state = integer(p["state"]);

				if ((state == 2 && !completed.count(pid)) || (state == 1 && !started.count(pid)))
				{
					prereqs = false;
					break;
				}
			}

			if (!prereqs)
				continue;

			std::string name = title(qid);

			if (name.empty())
				continue;

			std::string detail = (lvmin > 0 ? "Lv " + std::to_string(lvmin) + "+  \xC2\xB7  " : std::string()) + npc_name(npc);
			lists[AVAILABLE].push_back({ qid, name, detail, lvmin, false });
		}

		// Ready ones first; available ones closest to the player's level.
		std::stable_sort(lists[ACTIVE].begin(), lists[ACTIVE].end(), [](const Entry& a, const Entry& b) { return a.ready && !b.ready; });
		std::stable_sort(lists[AVAILABLE].begin(), lists[AVAILABLE].end(), [](const Entry& a, const Entry& b) { return a.level > b.level; });
		std::reverse(lists[DONE].begin(), lists[DONE].end());

		clamp_scroll();
	}

	void UIMobileQuests::open_detail(int16_t id, Tab from)
	{
		detail = {};
		detail.id = id;
		detail.tab = from;
		detail_open = true;
		scroll = 0;
		refresh_detail();
	}

	void UIMobileQuests::refresh_detail()
	{
		int16_t id = detail.id;
		std::string key = std::to_string(id);
		nl::node q = nl::nx::quest["QuestInfo.img"][key];
		nl::node check = nl::nx::quest["Check.img"][key];
		nl::node act = nl::nx::quest["Act.img"][key]["1"];
		const Inventory& inventory = Stage::get().get_player().get_inventory();

		const auto& started = questlog.get_started();
		auto progress = started.find(id);
		bool active = progress != started.end();
		bool done = questlog.get_completed().count(id) > 0;

		detail.tab = active ? ACTIVE : done ? DONE : AVAILABLE;
		detail.name = readable(q["name"].get_string());
		detail.needs.clear();
		detail.rewards.clear();

		// QuestInfo has the offer (0), the task (1) and the ending (2).
		const char* which = done ? "2" : active ? "1" : "0";
		std::string text = q[which].get_string();

		if (text.empty() || corrupt(text))
			text = q["0"].get_string();

		if (corrupt(text))
			text.clear();

		detail.text = readable(text);

		nl::node side = check[active || done ? "1" : "0"];
		detail.npc = integer(side["npc"]);

		if (!active && !done && detail.npc <= 0)
			detail.npc = integer(check["0"]["npc"]);

		if (detail.npc > 0)
		{
			detail.npc_name = npc_name(detail.npc);
			detail.npc_where = npc_location(detail.npc);
			detail.npc_here = npc_on_map(detail.npc);
		}

		if (active)
		{
			nl::node end = check["1"];
			int index = 0;

			for (nl::node mob : end["mob"])
			{
				int32_t need = integer(mob["count"]);

				if (need > 0)
					detail.needs.push_back({ "Defeat " + mob_name(integer(mob["id"])), kills(progress->second, index), need, 0 });

				index++;
			}

			for (nl::node item : end["item"])
			{
				int32_t iid = integer(item["id"]);
				int32_t need = integer(item["count"]);

				if (iid > 0 && need > 0)
					detail.needs.push_back({ ItemData::get(iid).get_name(), inventory.get_total_item_count(iid), need, iid });
			}

			detail.ready = questlog.is_completable(id, inventory);
		}
		else if (!done)
		{
			int32_t lvmin = integer(check["0"]["lvmin"]);

			if (lvmin > 0)
				detail.needs.push_back({ "Level " + std::to_string(lvmin), Stage::get().get_player().get_stats().get_stat(MapleStat::Id::LEVEL), lvmin, 0 });
		}

		// Rewards: experience, mesos, fame and items given (not taken).
		if (int32_t exp = integer(act["exp"]))
			detail.rewards.push_back({ std::to_string(exp) + " EXP", 0 });

		if (int32_t meso = integer(act["money"]); meso > 0)
			detail.rewards.push_back({ std::to_string(meso) + " mesos", 0 });

		if (int32_t fame = integer(act["pop"]); fame > 0)
			detail.rewards.push_back({ std::to_string(fame) + " fame", 0 });

		bool female = Stage::get().get_player().get_stats().get_female();

		for (nl::node item : act["item"])
		{
			int32_t iid = integer(item["id"]);
			int32_t count = integer(item["count"]);

			if (iid <= 0 || count < 0)
				continue;

			// Some rewards are for one gender only.
			nl::node gender = item["gender"];

			if (gender && integer(gender) != 2 && (integer(gender) == 1) != female)
				continue;

			std::string label = ItemData::get(iid).get_name();

			if (count > 1)
				label += "  \xC3\x97" + std::to_string(count);

			if (integer(item["prop"]) > 0)
				label += "  (chance)";

			detail.rewards.push_back({ label, iid });
		}
	}

	const std::vector<UIMobileQuests::Entry>& UIMobileQuests::shown() const
	{
		return lists[tab];
	}

	void UIMobileQuests::update()
	{
		UIMobilePanel::update();

		// Quests start, finish and progress while the journal is open.
		if (++refresh_ticks < 60)
			return;

		refresh_ticks = 0;

		const auto& started = questlog.get_started();
		std::string progress;

		for (const auto& s : started)
			progress += s.second + "|";

		int16_t level = static_cast<int16_t>(Stage::get().get_player().get_stats().get_stat(MapleStat::Id::LEVEL));

		if (started.size() != seen_started || questlog.get_completed().size() != seen_completed
			|| progress != seen_progress || level != seen_level)
		{
			rebuild();

			if (detail_open)
				refresh_detail();
		}
	}

	// ---- layout -----------------------------------------------------------

	Rect UIMobileQuests::tab_rect(int index) const
	{
		int16_t w = static_cast<int16_t>((W - PAD * 2 - 12) / 3);

		return rect(static_cast<int16_t>(position.x() + PAD + index * (w + 6)), static_cast<int16_t>(position.y() + TABS_Y), w, 36);
	}

	Rectangle<int16_t> UIMobileQuests::scroll_area() const
	{
		Rect f = frame();
		int16_t top = static_cast<int16_t>(f.top() + (detail_open ? 72 : LIST_Y));
		int16_t bottom = static_cast<int16_t>(f.bottom() - (detail_open ? 14 + ACTIONS_H + 10 : 12));

		return rect(static_cast<int16_t>(f.left() + PAD), top, W - PAD * 2, static_cast<int16_t>(bottom - top));
	}

	int16_t UIMobileQuests::scroll_content() const
	{
		if (detail_open)
			return detail_height();

		return static_cast<int16_t>(shown().size() * (ROW_H + ROW_GAP));
	}

	Rect UIMobileQuests::row_rect(size_t index) const
	{
		Rectangle<int16_t> area = scroll_area();

		return rect(area.left(), static_cast<int16_t>(area.top() + index * (ROW_H + ROW_GAP) - scroll), area.width(), ROW_H);
	}

	Rect UIMobileQuests::back_rect() const
	{
		Rect f = frame();

		return rect(static_cast<int16_t>(f.left() + 12), static_cast<int16_t>(f.top() + 14), 40, 40);
	}

	std::vector<int32_t> UIMobileQuests::actions() const
	{
		std::vector<int32_t> out;

		if (detail.npc > 0 && detail.tab != DONE)
			out.push_back(T_FIND);

		if (detail.tab == ACTIVE)
		{
			out.push_back(T_TRACK);
			out.push_back(T_FORFEIT);
		}

		return out;
	}

	Rect UIMobileQuests::action_rect(int index, int count) const
	{
		Rect f = frame();
		int16_t gap = 8;
		int16_t w = static_cast<int16_t>((W - PAD * 2 - gap * (count - 1)) / std::max(1, count));

		return rect(static_cast<int16_t>(f.left() + PAD + index * (w + gap)), static_cast<int16_t>(f.bottom() - 14 - ACTIONS_H), w, ACTIONS_H);
	}

	// ---- input ------------------------------------------------------------

	int32_t UIMobileQuests::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		if (detail_open)
		{
			if (inset(back_rect(), -6, -6).contains(pos))
				return T_BACK;

			std::vector<int32_t> list = actions();

			for (size_t i = 0; i < list.size(); i++)
				if (action_rect(static_cast<int>(i), static_cast<int>(list.size())).contains(pos))
					return list[i];

			return NO_TARGET;
		}

		for (int t = 0; t < NUM_TABS; t++)
			if (inset(tab_rect(t), -2, -4).contains(pos))
				return T_TAB + t;

		Rectangle<int16_t> area = scroll_area();

		if (!area.contains(pos))
			return NO_TARGET;

		for (size_t i = 0; i < shown().size(); i++)
			if (row_rect(i).contains(pos))
				return T_ROW + static_cast<int32_t>(i);

		return NO_TARGET;
	}

	void UIMobileQuests::activate(int32_t target)
	{
		if (target >= T_ROW)
		{
			size_t i = static_cast<size_t>(target - T_ROW);

			if (i < shown().size())
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				open_detail(shown()[i].id, tab);
			}

			return;
		}

		if (target >= T_TAB && target < T_TAB + NUM_TABS)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			tab = static_cast<Tab>(target - T_TAB);
			scroll = 0;
			return;
		}

		switch (target)
		{
			case T_BACK:
				Sound(Sound::Name::BUTTONCLICK).play();
				detail_open = false;
				scroll = 0;
				clamp_scroll();
				break;
			case T_FIND:
			{
				Sound(Sound::Name::BUTTONCLICK).play();

				// Flag the NPC on the minimap when it is on this map.
				if (detail.npc_here)
				{
					if (auto minimap = UI::get().get_element<UIMiniMap>())
						minimap->set_quest_npc(detail.npc);

					deactivate();
				}

				break;
			}
			case T_TRACK:
			{
				Sound(Sound::Name::BUTTONCLICK).play();

				if (auto helper = UI::get().get_element<UIQuestHelper>())
				{
					if (helper->is_tracked(detail.id))
						helper->untrack_quest(detail.id);
					else
						helper->track_quest(detail.id);
				}

				break;
			}
			case T_FORFEIT:
			{
				int16_t id = detail.id;
				UI::get().emplace<UIMobileConfirm>("Give up this quest?",
					"\"" + detail.name + "\" will be dropped. You can usually take it again from " +
					(detail.npc_name.empty() ? std::string("its NPC") : detail.npc_name) + ".",
					"Give up", true, [id]()
					{
						ForfeitQuestPacket(id).dispatch();
					});

				detail_open = false;
				scroll = 0;
				break;
			}
		}
	}

	// ---- drawing ----------------------------------------------------------

	int16_t UIMobileQuests::detail_height() const
	{
		int16_t w = static_cast<int16_t>(W - PAD * 2);
		int16_t h = 0;

		if (detail.npc > 0)
			h = static_cast<int16_t>(h + 70);

		h = static_cast<int16_t>(h + text_height(detail.text, Font::BODY, static_cast<int16_t>(w - 4)) + 18);

		if (!detail.needs.empty())
			h = static_cast<int16_t>(h + 28 + detail.needs.size() * 48);

		if (!detail.rewards.empty())
			h = static_cast<int16_t>(h + 28 + detail.rewards.size() * 34);

		return static_cast<int16_t>(h + 8);
	}

	void UIMobileQuests::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();

		if (detail_open)
		{
			fill(f, Theme::PANEL, 18);

			Rect back = back_rect();
			icon_button(back, Glyph::LEFT, Style::SECONDARY, true, pressed == T_BACK);
			icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

			int16_t tx = static_cast<int16_t>(back.right() + 12);
			int16_t tw = static_cast<int16_t>(panel_close(f).left() - 10 - tx);
			text(ellipsize(detail.name, Font::TITLE, tw), Font::TITLE, Theme::TEXT, Point<int16_t>(tx, static_cast<int16_t>(f.top() + 16)));

			std::string state = detail.tab == DONE ? "Completed" : detail.tab == AVAILABLE ? "Available" : detail.ready ? "Ready to finish" : "In progress";
			RGBA tint = detail.tab == DONE ? Theme::MUTED : detail.tab == AVAILABLE ? Theme::GOLD : detail.ready ? Theme::GREEN : Theme::BLUE;
			text(state, Font::SMALL_BOLD, tint, Point<int16_t>(tx, static_cast<int16_t>(f.top() + 40)));

			Rectangle<int16_t> area = scroll_area();
			GraphicsGL::get().begin_clip(area.top(), area.bottom());

			int16_t x = area.left();
			int16_t w = area.width();
			int16_t y = static_cast<int16_t>(area.top() - scroll);

			// Who to see, and where.
			if (detail.npc > 0)
			{
				Rect card = rect(x, y, w, 60);
				fill(card, Theme::CARD, 14);
				Rect dot = rect(static_cast<int16_t>(card.left() + 12), static_cast<int16_t>(card.top() + 12), 36, 36);
				fill(dot, Theme::GREEN.alpha(0.2f), 18);
				text_in("NPC", Font::SMALL_BOLD, Theme::GREEN, dot);

				std::string who = (detail.tab == AVAILABLE ? "Start with " : detail.tab == DONE ? "Finished with " : "Turn in to ") + detail.npc_name;
				text(ellipsize(who, Font::BODY_BOLD, static_cast<int16_t>(w - 70)), Font::BODY_BOLD, Theme::TEXT,
					Point<int16_t>(static_cast<int16_t>(card.left() + 60), static_cast<int16_t>(card.top() + 12)));

				std::string where = detail.npc_here ? "On this map" : detail.npc_where.empty() ? "Location unknown" : detail.npc_where;
				text(ellipsize(where, Font::SMALL, static_cast<int16_t>(w - 70)), Font::SMALL, detail.npc_here ? Theme::GREEN : Theme::MUTED,
					Point<int16_t>(static_cast<int16_t>(card.left() + 60), static_cast<int16_t>(card.top() + 34)));

				y = static_cast<int16_t>(y + 70);
			}

			text(detail.text, Font::BODY, Theme::TEXT.alpha(0.9f), Point<int16_t>(static_cast<int16_t>(x + 2), y), Text::Alignment::LEFT,
				static_cast<int16_t>(w - 4));
			y = static_cast<int16_t>(y + text_height(detail.text, Font::BODY, static_cast<int16_t>(w - 4)) + 18);

			if (!detail.needs.empty())
			{
				text(detail.tab == AVAILABLE ? "REQUIRES" : "TO DO", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(x + 2), y));
				y = static_cast<int16_t>(y + 28);

				for (const Need& n : detail.needs)
				{
					bool met = n.have >= n.need;
					Rect row = rect(x, y, w, 42);
					fill(row, Theme::CARD, 12);

					int16_t lx = static_cast<int16_t>(row.left() + 12);

					if (n.icon_item > 0)
					{
						MobileUI::icon(ItemData::get(n.icon_item).get_icon(false), rect(lx, static_cast<int16_t>(row.top() + 5), 32, 32));
						lx = static_cast<int16_t>(lx + 40);
					}

					std::string count = std::to_string(std::min(n.have, n.need)) + " / " + std::to_string(n.need);
					int16_t cw = text_width(count, Font::SMALL_BOLD);
					text(ellipsize(n.label, Font::SMALL_BOLD, static_cast<int16_t>(row.right() - 12 - cw - 12 - lx)), Font::SMALL_BOLD, Theme::TEXT,
						Point<int16_t>(lx, static_cast<int16_t>(row.top() + 8)));
					text(count, Font::SMALL_BOLD, met ? Theme::GREEN : Theme::GOLD,
						Point<int16_t>(static_cast<int16_t>(row.right() - 12), static_cast<int16_t>(row.top() + 8)), Text::Alignment::RIGHT);

					float fraction = n.need > 0 ? std::min(1.0f, static_cast<float>(n.have) / n.need) : 1.0f;
					Rect track = rect(lx, static_cast<int16_t>(row.bottom() - 12), static_cast<int16_t>(row.right() - 12 - lx), 5);
					fill(track, Theme::TRACK, 2);

					if (fraction > 0.0f)
						fill(rect(track.left(), track.top(), static_cast<int16_t>(std::max(5.0f, track.width() * fraction)), 5), met ? Theme::GREEN : Theme::ACCENT, 2);

					y = static_cast<int16_t>(y + 48);
				}
			}

			if (!detail.rewards.empty())
			{
				text("REWARDS", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(x + 2), y));
				y = static_cast<int16_t>(y + 28);

				for (const Reward& r : detail.rewards)
				{
					Rect row = rect(x, y, w, 30);
					int16_t lx = static_cast<int16_t>(row.left() + 4);

					if (r.item > 0)
					{
						MobileUI::icon(ItemData::get(r.item).get_icon(false), rect(lx, row.top(), 30, 30));
						lx = static_cast<int16_t>(lx + 38);
					}
					else
					{
						fill(rect(static_cast<int16_t>(lx + 9), static_cast<int16_t>(row.top() + 9), 12, 12), Theme::GOLD, 6);
						lx = static_cast<int16_t>(lx + 38);
					}

					text_in(ellipsize(r.label, Font::BODY, static_cast<int16_t>(row.right() - lx)), Font::BODY, Theme::TEXT,
						rect(lx, row.top(), static_cast<int16_t>(row.right() - lx), row.height()), Text::Alignment::LEFT, 0);
					y = static_cast<int16_t>(y + 34);
				}
			}

			GraphicsGL::get().end_clip();

			// Actions.
			std::vector<int32_t> list = actions();
			bool tracked = false;

			if (auto helper = UI::get().get_element<UIQuestHelper>())
				tracked = helper->is_tracked(detail.id);

			for (size_t i = 0; i < list.size(); i++)
			{
				Rect r = action_rect(static_cast<int>(i), static_cast<int>(list.size()));

				switch (list[i])
				{
					case T_FIND:
						button(r, detail.npc_here ? "Show on map" : "Not on this map", Style::PRIMARY, detail.npc_here, pressed == T_FIND, Font::SMALL_BOLD);
						break;
					case T_TRACK:
						button(r, tracked ? "Untrack" : "Track", tracked ? Style::SELECTED : Style::SECONDARY, true, pressed == T_TRACK, Font::SMALL_BOLD);
						break;
					case T_FORFEIT:
						button(r, "Give up", Style::DANGER, true, pressed == T_FORFEIT, Font::SMALL_BOLD);
						break;
				}
			}

			return;
		}

		size_t ready = 0;

		for (const Entry& e : lists[ACTIVE])
			if (e.ready)
				ready++;

		std::string subtitle = std::to_string(lists[ACTIVE].size()) + " in progress" + (ready > 0 ? "  \xC2\xB7  " + std::to_string(ready) + " ready" : "");
		panel(f, "Quests", subtitle);
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		for (int t = 0; t < NUM_TABS; t++)
		{
			std::string label = std::string(TAB_NAMES[t]) + "  " + std::to_string(lists[t].size());
			button(tab_rect(t), label, t == tab ? Style::SELECTED : Style::SECONDARY, true, pressed == T_TAB + t, Font::SMALL_BOLD);
		}

		Rectangle<int16_t> area = scroll_area();

		if (shown().empty())
		{
			const char* empty = tab == ACTIVE ? "No quests in progress. Pick one up from Available."
				: tab == AVAILABLE ? "Nothing new right now. Level up or finish quests to unlock more."
				: "Finished quests show here.";
			text(empty, Font::BODY, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(area.left() + 4), static_cast<int16_t>(area.top() + 10)),
				Text::Alignment::LEFT, static_cast<int16_t>(area.width() - 8));
			return;
		}

		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		for (size_t i = 0; i < shown().size(); i++)
		{
			Rect r = row_rect(i);

			if (r.bottom() < area.top() || r.top() > area.bottom())
				continue;

			const Entry& e = shown()[i];
			int32_t id = T_ROW + static_cast<int32_t>(i);
			fill(r, pressed == id ? Theme::CARD_HI : Theme::CARD, 14);

			RGBA tint = tab == DONE ? Theme::FAINT : tab == AVAILABLE ? Theme::GOLD : e.ready ? Theme::GREEN : Theme::BLUE;
			Rect mark = rect(static_cast<int16_t>(r.left() + 12), static_cast<int16_t>(r.top() + 15), 32, 32);
			fill(mark, tint.alpha(0.18f), 16);
			text_in(tab == DONE ? "\xE2\x9C\x93" : tab == AVAILABLE ? "!" : e.ready ? "?" : "\xE2\x80\xA6", Font::TITLE, tint, mark);

			int16_t tx = static_cast<int16_t>(r.left() + 56);
			int16_t tw = static_cast<int16_t>(r.right() - 30 - tx);
			text(ellipsize(e.name, Font::BODY_BOLD, tw), Font::BODY_BOLD, tab == DONE ? Theme::MUTED : Theme::TEXT,
				Point<int16_t>(tx, static_cast<int16_t>(r.top() + 12)));
			text(ellipsize(e.detail, Font::SMALL, tw), Font::SMALL, e.ready ? Theme::GREEN : Theme::MUTED,
				Point<int16_t>(tx, static_cast<int16_t>(r.top() + 34)));
			glyph(rect(static_cast<int16_t>(r.right() - 28), static_cast<int16_t>(r.top() + 21), 20, 20), Glyph::RIGHT, Theme::FAINT, 12);
		}

		GraphicsGL::get().end_clip();
	}

	UIElement::Type UIMobileQuests::get_type() const
	{
		return TYPE;
	}
}
