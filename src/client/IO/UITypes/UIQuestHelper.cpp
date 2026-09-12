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
#include "UIQuestHelper.h"
#include "UIQuestLog.h"

#include "../Components/MapleButton.h"
#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"

#include <algorithm>
#include <sstream>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	QuestDragState UIQuestHelper::quest_drag;

	UIQuestHelper::UIQuestHelper(const QuestLog& ql) :
		UIDragElement<PosQUESTHELPER>(Point<int16_t>(WIDTH, MIN_H)), questlog(ql),
		minimized(false), hovered_close_questid(-1), reorder_drag_index(-1)
	{
		nl::node alarm = nl::nx::ui["UIWindow.img"]["QuestAlarm"];
		nl::node basic = nl::nx::ui["Basic.img"];

		bg_min = alarm["backgrndmin"];
		bg_max = alarm["backgrndmax"];
		bg_center = alarm["backgrndcenter"];
		bg_bottom = alarm["backgrndbottom"];

		buttons[Buttons::BT_Q] = std::make_unique<MapleButton>(alarm["BtQ"]);
		buttons[Buttons::BT_AUTO] = std::make_unique<MapleButton>(alarm["BtAuto"]);
		// v83 QuestAlarm has no min / max / delete buttons of its own
		buttons[Buttons::BT_MIN] = std::make_unique<MapleButton>(basic["BtMin"]);
		buttons[Buttons::BT_MAX] = std::make_unique<MapleButton>(basic["BtMax"]);

		nl::node close = basic["BtClose"];
		close_btn_normal = close["normal"]["0"];
		close_btn_mouseover = close["mouseOver"]["0"];

		title_text = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::BLACK, "", 0, false);
		drag_text = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "", 0, false);

		auto_track();
		recalc_dimension();
	}

	bool UIQuestHelper::is_compact() const
	{
		// With nothing pinned only the title bar shows, so the helper never
		// sits on screen as an empty box
		return minimized || tracked_quests.empty();
	}

	int16_t UIQuestHelper::get_quest_entry_height(const TrackedQuest& tq) const
	{
		int16_t h = tq.name.height() + LINE_GAP;

		if (!tq.collapsed)
		{
			if (!tq.requirements.empty())
			{
				for (const auto& req : tq.requirements)
					h += req.height() + LINE_GAP;
			}
			else
			{
				h += tq.progress.height() + LINE_GAP;
			}
		}

		return h;
	}

	int16_t UIQuestHelper::get_content_height() const
	{
		int16_t h = CONTENT_PAD;

		for (size_t i = 0; i < tracked_quests.size(); i++)
		{
			if (i > 0)
				h += ENTRY_GAP;

			h += get_quest_entry_height(tracked_quests[i]);
		}

		return h + CONTENT_PAD;
	}

	int16_t UIQuestHelper::draw_quest_entry(const TrackedQuest& tq, Point<int16_t> pos) const
	{
		int16_t y = 0;

		// Quest name: click to collapse / expand, drag to reorder
		tq.name.draw(pos);

		// Close glyph right after the name, centred on its first line
		bool hovered = (tq.questid == hovered_close_questid);
		const Texture& glyph = hovered ? close_btn_mouseover : close_btn_normal;
		int16_t glyph_y = (tq.name.height() - CLOSE_GLYPH) / 2;

		if (glyph_y < 0)
			glyph_y = 0;

		glyph.draw(DrawArgument(pos + Point<int16_t>(tq.name.width() + CLOSE_GAP, glyph_y)));

		y += tq.name.height() + LINE_GAP;

		if (!tq.collapsed)
		{
			if (!tq.requirements.empty())
			{
				for (const auto& req : tq.requirements)
				{
					req.draw(pos + Point<int16_t>(REQ_INDENT, y));
					y += req.height() + LINE_GAP;
				}
			}
			else
			{
				tq.progress.draw(pos + Point<int16_t>(REQ_INDENT, y));
				y += tq.progress.height() + LINE_GAP;
			}
		}

		return y;
	}

	void UIQuestHelper::draw(float alpha) const
	{
		int16_t count = static_cast<int16_t>(tracked_quests.size());
		title_text.change_text("Quest Helper (" + std::to_string(count) + "/" + std::to_string(MAX_TRACKED) + ")");

		if (is_compact())
		{
			bg_min.draw(DrawArgument(position));
			title_text.draw(position + Point<int16_t>(TITLE_X, TITLE_Y_MIN));
			UIElement::draw_buttons(alpha);
			return;
		}

		// Header, enough body tiles for the entries, rounded foot
		int16_t content_h = get_content_height();
		int16_t tiles = (content_h + TILE_H - 1) / TILE_H;

		bg_max.draw(DrawArgument(position));

		for (int16_t i = 0; i < tiles; i++)
			bg_center.draw(DrawArgument(position + Point<int16_t>(0, HEADER_H + i * TILE_H)));

		bg_bottom.draw(DrawArgument(position + Point<int16_t>(0, HEADER_H + tiles * TILE_H)));

		title_text.draw(position + Point<int16_t>(TITLE_X, TITLE_Y_MAX));

		quest_hit_areas.clear();

		int16_t y = HEADER_H + CONTENT_PAD;

		for (auto& tq : tracked_quests)
		{
			Point<int16_t> entry_pos = position + Point<int16_t>(ENTRY_X, y);
			int16_t entry_h = draw_quest_entry(tq, entry_pos);

			QuestHitArea hit;
			hit.questid = tq.questid;

			int16_t name_w = tq.name.width();
			int16_t name_h = tq.name.height();
			int16_t glyph_x = name_w + CLOSE_GAP;
			int16_t glyph_y = std::max<int16_t>(0, (name_h - CLOSE_GLYPH) / 2);

			hit.close_btn = Rectangle<int16_t>(
				entry_pos + Point<int16_t>(glyph_x, glyph_y),
				entry_pos + Point<int16_t>(glyph_x + CLOSE_GLYPH, glyph_y + CLOSE_GLYPH)
			);
			hit.header_area = Rectangle<int16_t>(
				entry_pos,
				entry_pos + Point<int16_t>(name_w, name_h)
			);
			quest_hit_areas.push_back(hit);

			y += entry_h + ENTRY_GAP;
		}

		UIElement::draw_buttons(alpha);

		// Drag overlay: quest being dragged from the quest log or reordered here
		if (quest_drag.active)
		{
			drag_text.change_text(quest_drag.quest_name);
			drag_text.draw(DrawArgument(quest_drag.cursor_pos + Point<int16_t>(12, -8)));
		}
		else if (reorder_drag_index >= 0 && reorder_drag_index < static_cast<int16_t>(tracked_quests.size()))
		{
			int16_t dx = reorder_cursor_pos.x() - reorder_start_pos.x();
			int16_t dy = reorder_cursor_pos.y() - reorder_start_pos.y();

			if ((dx * dx + dy * dy) > (DRAG_THRESHOLD * DRAG_THRESHOLD))
			{
				drag_text.change_text(tracked_quests[reorder_drag_index].name.get_text());
				drag_text.draw(DrawArgument(reorder_cursor_pos + Point<int16_t>(12, -8)));
			}
		}
	}

	void UIQuestHelper::layout_buttons()
	{
		bool compact = is_compact();
		int16_t header_h = compact ? MIN_H : HEADER_H;
		int16_t btn_y = (header_h - BTN_H) / 2;

		buttons[Buttons::BT_Q]->set_position(Point<int16_t>(BT_Q_X, btn_y));
		buttons[Buttons::BT_AUTO]->set_position(Point<int16_t>(BT_AUTO_X, btn_y));
		buttons[Buttons::BT_MIN]->set_position(Point<int16_t>(BT_MINMAX_X, btn_y));
		buttons[Buttons::BT_MAX]->set_position(Point<int16_t>(BT_MINMAX_X, btn_y));

		// Nothing to fold when nothing is pinned
		bool has_quests = !tracked_quests.empty();
		buttons[Buttons::BT_MIN]->set_active(has_quests && !minimized);
		buttons[Buttons::BT_MAX]->set_active(has_quests && minimized);

		dragarea = Point<int16_t>(WIDTH, header_h);
	}

	void UIQuestHelper::recalc_dimension()
	{
		if (is_compact())
		{
			dimension = Point<int16_t>(WIDTH, MIN_H);
		}
		else
		{
			int16_t content_h = get_content_height();
			int16_t tiles = (content_h + TILE_H - 1) / TILE_H;

			dimension = Point<int16_t>(WIDTH, HEADER_H + tiles * TILE_H + BOTTOM_H);
		}

		layout_buttons();
	}

	void UIQuestHelper::toggle_active()
	{
		UIElement::toggle_active();

		if (active)
		{
			minimized = false;

			if (tracked_quests.empty())
				auto_track();

			recalc_dimension();
		}
	}

	void UIQuestHelper::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	void UIQuestHelper::update()
	{
		UIElement::update();
	}

	Cursor::State UIQuestHelper::send_cursor(bool clicking, Point<int16_t> cursorpos)
	{
		// While a quest is being dragged from the quest log, show the grabbing cursor
		if (quest_drag.active)
		{
			quest_drag.cursor_pos = cursorpos;
			return Cursor::State::GRABBING;
		}

		// Internal reorder drag
		if (reorder_drag_index >= 0)
		{
			reorder_cursor_pos = cursorpos;
			int16_t dx = cursorpos.x() - reorder_start_pos.x();
			int16_t dy = cursorpos.y() - reorder_start_pos.y();
			bool moved = (dx * dx + dy * dy) > (DRAG_THRESHOLD * DRAG_THRESHOLD);

			if (!clicking)
			{
				int16_t idx = reorder_drag_index;
				reorder_drag_index = -1;

				if (!moved)
				{
					// Short click: collapse / expand
					if (idx >= 0 && idx < static_cast<int16_t>(tracked_quests.size()))
					{
						tracked_quests[idx].collapsed = !tracked_quests[idx].collapsed;
						recalc_dimension();
					}

					return Cursor::State::IDLE;
				}

				// Drop target by cursor position
				int16_t drop_index = -1;

				for (size_t i = 0; i < quest_hit_areas.size(); i++)
				{
					if (quest_hit_areas[i].header_area.contains(cursorpos) ||
						quest_hit_areas[i].close_btn.contains(cursorpos))
					{
						drop_index = static_cast<int16_t>(i);
						break;
					}
				}

				if (drop_index >= 0 && drop_index != idx &&
					drop_index < static_cast<int16_t>(tracked_quests.size()))
				{
					TrackedQuest moved_q = std::move(tracked_quests[idx]);
					tracked_quests.erase(tracked_quests.begin() + idx);
					tracked_quests.insert(tracked_quests.begin() + drop_index, std::move(moved_q));
					recalc_dimension();
					save_tracked();
				}

				return Cursor::State::IDLE;
			}

			return moved ? Cursor::State::GRABBING : Cursor::State::CANCLICK;
		}

		// Per-quest hit areas (close glyph, collapse / reorder header)
		hovered_close_questid = -1;

		if (!is_compact())
		{
			for (size_t i = 0; i < quest_hit_areas.size(); i++)
			{
				auto& hit = quest_hit_areas[i];

				if (hit.close_btn.contains(cursorpos))
				{
					hovered_close_questid = hit.questid;

					if (clicking)
					{
						Sound(Sound::Name::BUTTONCLICK).play();
						untrack_quest(hit.questid);
						return Cursor::State::IDLE;
					}

					return Cursor::State::CANCLICK;
				}

				if (hit.header_area.contains(cursorpos))
				{
					if (clicking)
					{
						reorder_drag_index = static_cast<int16_t>(i);
						reorder_start_pos = cursorpos;
						reorder_cursor_pos = cursorpos;
						return Cursor::State::CLICKING;
					}

					return Cursor::State::CANCLICK;
				}
			}
		}

		// Title bar buttons and window dragging
		return UIDragElement::send_cursor(clicking, cursorpos);
	}

	UIElement::Type UIQuestHelper::get_type() const
	{
		return TYPE;
	}

	// Persist the current tracked quest ids to Configuration as a
	// comma-separated list so the pin state survives client restarts.
	// Writes the whole config file immediately — ~Configuration() only
	// runs on a clean shutdown, so flushing on every pin change avoids
	// losing the state if the client is killed.
	void UIQuestHelper::save_tracked()
	{
		std::ostringstream oss;

		for (size_t i = 0; i < tracked_quests.size(); ++i)
		{
			if (i > 0)
				oss << ',';

			oss << tracked_quests[i].questid;
		}

		Setting<TrackedQuests>::get().save(oss.str());
		Configuration::get().save();
	}

	void UIQuestHelper::track_quest(int16_t questid)
	{
		if (questid <= 0)
			return;

		// Cosmic stores internal state in quest slots; those have no QuestInfo
		// entry and are not player-visible quests
		nl::node qinfo = nl::nx::quest["QuestInfo.img"][std::to_string(questid)];

		if (!qinfo || qinfo["name"].get_string().empty())
			return;

		for (auto& tq : tracked_quests)
			if (tq.questid == questid)
				return;

		if (static_cast<int>(tracked_quests.size()) >= MAX_TRACKED)
			return;

		TrackedQuest tq;
		tq.questid = questid;
		tq.collapsed = false;
		refresh_quest_info(tq);
		tracked_quests.push_back(std::move(tq));
		recalc_dimension();
		save_tracked();
	}

	void UIQuestHelper::untrack_quest(int16_t questid)
	{
		tracked_quests.erase(
			std::remove_if(tracked_quests.begin(), tracked_quests.end(),
				[questid](const TrackedQuest& tq) { return tq.questid == questid; }),
			tracked_quests.end()
		);
		recalc_dimension();
		save_tracked();
	}

	void UIQuestHelper::auto_track()
	{
		const auto& started = questlog.get_started();

		// On first call this session, restore whatever the player had
		// pinned in the previous client session from Configuration.
		static bool restored_once = false;

		if (!restored_once)
		{
			restored_once = true;
			std::string saved = Setting<TrackedQuests>::get().load();

			if (!saved.empty())
			{
				std::stringstream ss(saved);
				std::string tok;

				while (std::getline(ss, tok, ','))
				{
					if (tok.empty())
						continue;

					int16_t qid = 0;

					try { qid = static_cast<int16_t>(std::stoi(tok)); }
					catch (...) { continue; }

					if (qid <= 0)
						continue;

					if (started.find(qid) == started.end())
						continue; // finished or abandoned since

					if (static_cast<int>(tracked_quests.size()) >= MAX_TRACKED)
						break;

					bool dup = false;

					for (const auto& t : tracked_quests)
						if (t.questid == qid) { dup = true; break; }

					if (dup)
						continue;

					TrackedQuest tq;
					tq.questid = qid;
					tq.collapsed = false;
					refresh_quest_info(tq);
					tracked_quests.push_back(std::move(tq));
				}

				recalc_dimension();
			}
		}

		// Drop any tracked entries whose quest is no longer started
		// (e.g. the player turned it in or abandoned it).
		size_t before = tracked_quests.size();
		tracked_quests.erase(
			std::remove_if(tracked_quests.begin(), tracked_quests.end(),
				[&](const TrackedQuest& tq) {
					return started.find(tq.questid) == started.end();
				}),
			tracked_quests.end());

		if (before != tracked_quests.size())
		{
			recalc_dimension();
			save_tracked();
		}

		// Do NOT auto-pin every started quest: if we did, every in-progress
		// row in the quest log would show the "tracked" icon and suppress
		// the "in progress" icon entirely. Players pin quests manually
		// (drag-and-drop from quest log / track button / AUTO).
	}

	void UIQuestHelper::refresh_quest_info(TrackedQuest& tq)
	{
		tq.requirements.clear();
		tq.requirements_complete.clear();
		tq.all_complete = false;

		if (tq.questid <= 0)
			return;

		nl::node quest_info = nl::nx::quest["QuestInfo.img"];
		nl::node quest_check = nl::nx::quest["Check.img"];

		std::string qid_str = std::to_string(tq.questid);

		nl::node qnode = quest_info[qid_str];
		std::string name;

		if (qnode)
			name = qnode["name"].get_string();

		// Reached only for ids restored from saved settings; track_quest filters
		// state-flag records out up front
		if (name.empty())
			name = "Quest " + qid_str;

		// Dark text on the grey QuestAlarm tiles
		tq.name = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::BLACK, name, ENTRY_TEXT_W, false);

		nl::node check = quest_check[qid_str];

		if (!check)
		{
			tq.progress = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR, "In progress...", ENTRY_TEXT_W, false);
			return;
		}

		nl::node end_check = check["1"];

		if (!end_check)
		{
			tq.progress = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR, "In progress...", ENTRY_TEXT_W, false);
			return;
		}

		const Inventory& inventory = Stage::get().get_player().get_inventory();

		// Item requirements
		for (auto item_node : end_check["item"])
		{
			int32_t itemid = item_node["id"].get_integer();
			int32_t count = item_node["count"].get_integer();

			if (itemid <= 0)
				continue;

			int16_t have = inventory.get_total_item_count(itemid);
			const ItemData& idata = ItemData::get(itemid);
			std::string item_name = idata.get_name();

			if (item_name.empty())
				item_name = "Item " + std::to_string(itemid);

			std::string line = item_name + " " + std::to_string(have) + "/" + std::to_string(count);
			bool done = have >= count;
			Color::Name color = done ? Color::Name::DUSTYGRAY : Color::Name::EMPEROR;

			tq.requirements.push_back(
				Text(Text::Font::A11M, Text::Alignment::LEFT, color, line, ENTRY_TEXT_W, false)
			);
			tq.requirements_complete.push_back(done);
		}

		// Mob requirements: parse progress from the quest data string
		// (v83 format: a 3-digit zero-padded kill count per mob)
		std::string qdata;
		{
			auto it = questlog.get_started().find(tq.questid);

			if (it != questlog.get_started().end())
				qdata = it->second;
		}

		int mob_idx = 0;

		for (auto mob_node : end_check["mob"])
		{
			int32_t mobid = mob_node["id"].get_integer();
			int32_t count = mob_node["count"].get_integer();

			if (mobid <= 0)
				continue;

			int32_t killed = 0;
			size_t offset = static_cast<size_t>(mob_idx) * 3;

			if (offset + 3 <= qdata.size())
			{
				try { killed = std::stoi(qdata.substr(offset, 3)); }
				catch (...) { killed = 0; }
			}

			std::string mob_name = nl::nx::string["Mob.img"][std::to_string(mobid)]["name"].get_string();

			if (mob_name.empty())
				mob_name = "Monster " + std::to_string(mobid);

			std::string line = mob_name + " " + std::to_string(killed) + "/" + std::to_string(count);
			bool done = killed >= count;
			Color::Name color = done ? Color::Name::DUSTYGRAY : Color::Name::EMPEROR;

			tq.requirements.push_back(
				Text(Text::Font::A11M, Text::Alignment::LEFT, color, line, ENTRY_TEXT_W, false)
			);
			tq.requirements_complete.push_back(done);
			mob_idx++;
		}

		// NPC requirement
		int32_t end_npc = end_check["npc"].get_integer();

		if (end_npc > 0)
		{
			std::string npc_name = nl::nx::string["Npc.img"][std::to_string(end_npc)]["name"].get_string();

			if (!npc_name.empty())
			{
				tq.requirements.push_back(
					Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR, "Talk to " + npc_name, ENTRY_TEXT_W, false)
				);
				// Can't track NPC-talked state client-side without a server flag
				tq.requirements_complete.push_back(false);
			}
		}

		if (!tq.requirements_complete.empty())
		{
			tq.all_complete = true;

			for (bool c : tq.requirements_complete)
				if (!c) { tq.all_complete = false; break; }
		}

		if (tq.requirements.empty())
			tq.progress = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR, "In progress...", ENTRY_TEXT_W, false);
		else
			tq.progress = Text();
	}

	void UIQuestHelper::refresh_all()
	{
		// Drop entries whose quest is no longer in the started map (completed
		// or forfeited on the server). Without this, completed quests would
		// remain visibly pinned with stale data after the server confirms
		// turn-in.
		const auto& started = questlog.get_started();
		size_t before = tracked_quests.size();

		tracked_quests.erase(
			std::remove_if(tracked_quests.begin(), tracked_quests.end(),
				[&](const TrackedQuest& tq) {
					return started.find(tq.questid) == started.end();
				}),
			tracked_quests.end());

		for (auto& tq : tracked_quests)
			refresh_quest_info(tq);

		recalc_dimension();

		if (before != tracked_quests.size())
			save_tracked();
	}

	bool UIQuestHelper::is_tracked(int16_t questid) const
	{
		for (const auto& tq : tracked_quests)
			if (tq.questid == questid)
				return true;

		return false;
	}

	Button::State UIQuestHelper::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BT_Q:
		{
			UI::get().emplace<UIQuestLog>(
				Stage::get().get_player().get_quests()
			);
			break;
		}
		case Buttons::BT_AUTO:
		{
			// Auto-pin: fill the tracker with the newest in-progress
			// quests (highest quest ids first) up to the cap
			tracked_quests.clear();

			const auto& started = questlog.get_started();
			nl::node quest_info = nl::nx::quest["QuestInfo.img"];

			for (auto it = started.rbegin(); it != started.rend(); ++it)
			{
				if (static_cast<int>(tracked_quests.size()) >= MAX_TRACKED)
					break;

				// Skip the server's internal tracker quests — they have
				// no QuestInfo entry (no name/description to show)
				std::string name = quest_info[std::to_string(it->first)]["name"];

				if (name.empty())
					continue;

				TrackedQuest tq;
				tq.questid = it->first;
				tq.collapsed = false;
				refresh_quest_info(tq);
				tracked_quests.push_back(std::move(tq));
			}

			minimized = false;
			recalc_dimension();
			save_tracked();
			break;
		}
		case Buttons::BT_MAX:
			minimized = false;
			recalc_dimension();
			break;
		case Buttons::BT_MIN:
			minimized = true;
			recalc_dimension();
			break;
		default:
			break;
		}

		return Button::State::NORMAL;
	}
}
