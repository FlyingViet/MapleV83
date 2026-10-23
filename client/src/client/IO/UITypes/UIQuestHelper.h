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
#pragma once

#include "../UIDragElement.h"

#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"
#include "../../Character/QuestLog.h"

namespace ms
{
	// Shared drag state for quest drag-and-drop between UIQuestLog and UIQuestHelper
	struct QuestDragState
	{
		bool active;
		int16_t questid;
		std::string quest_name;
		Point<int16_t> cursor_pos;

		QuestDragState() : active(false), questid(-1) {}

		void start(int16_t qid, const std::string& name)
		{
			active = true;
			questid = qid;
			quest_name = name;
		}

		void end()
		{
			active = false;
			questid = -1;
			quest_name.clear();
		}
	};

	// Pinned-quest tracker drawn with the v83 UIWindow.img/QuestAlarm bars:
	// backgrndmin (223x20) is the collapsed title bar, backgrndmax (223x25)
	// the expanded header, backgrndcenter (223x18) the body tile and
	// backgrndbottom (223x5) the rounded foot. BtQ / BtAuto come from the
	// same node; minimise / maximise / per-quest close use Basic.img glyphs.
	class UIQuestHelper : public UIDragElement<PosQUESTHELPER>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::QUESTHELPER;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;
		static constexpr int MAX_TRACKED = 5;

		// Global quest drag state — shared between quest log and helper
		static QuestDragState quest_drag;

		UIQuestHelper(const QuestLog& questLog);

		void draw(float inter) const override;
		void update() override;
		void toggle_active() override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		void track_quest(int16_t questid);
		void untrack_quest(int16_t questid);
		void auto_track();
		void refresh_all();

		// Whether the given quest id is currently pinned in the helper.
		// Used by UIQuestLog to show the "Quest tracked" icon.
		bool is_tracked(int16_t questid) const;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		struct TrackedQuest
		{
			int16_t questid;
			Text name;
			Text progress;
			std::vector<Text> requirements;
			std::vector<bool> requirements_complete;
			bool all_complete;
			bool collapsed;

			TrackedQuest() : questid(-1), all_complete(false), collapsed(false) {}
		};

		void refresh_quest_info(TrackedQuest& tq);
		void recalc_dimension();
		void layout_buttons();
		bool is_compact() const;
		void save_tracked();
		int16_t draw_quest_entry(const TrackedQuest& tq, Point<int16_t> pos) const;
		int16_t get_quest_entry_height(const TrackedQuest& tq) const;
		int16_t get_content_height() const;

		enum Buttons : uint16_t
		{
			BT_Q,
			BT_AUTO,
			BT_MIN,
			BT_MAX
		};

		static constexpr int16_t WIDTH = 223;          // every QuestAlarm bar is 223 wide
		static constexpr int16_t MIN_H = 20;           // backgrndmin
		static constexpr int16_t HEADER_H = 25;        // backgrndmax
		static constexpr int16_t TILE_H = 18;          // backgrndcenter
		static constexpr int16_t BOTTOM_H = 5;         // backgrndbottom
		static constexpr int16_t TITLE_X = 8;
		static constexpr int16_t TITLE_Y_MIN = 3;      // title text inside the 20px bar
		static constexpr int16_t TITLE_Y_MAX = 6;      // title text inside the 25px header
		// Header buttons, right to left: min/max (Basic 12x12), AUTO (21x12), Q (12x12)
		static constexpr int16_t BTN_H = 12;
		static constexpr int16_t BT_MINMAX_X = WIDTH - 18;
		static constexpr int16_t BT_AUTO_X = WIDTH - 43;
		static constexpr int16_t BT_Q_X = WIDTH - 59;
		// Quest entries
		static constexpr int16_t ENTRY_X = 8;
		static constexpr int16_t ENTRY_TEXT_W = 180;   // wrap width leaving room for the close glyph
		static constexpr int16_t REQ_INDENT = 6;
		static constexpr int16_t LINE_GAP = 1;
		static constexpr int16_t ENTRY_GAP = 6;
		static constexpr int16_t CONTENT_PAD = 4;
		static constexpr int16_t CLOSE_GLYPH = 12;     // Basic.img/BtClose 12x12
		static constexpr int16_t CLOSE_GAP = 4;
		static constexpr int16_t DRAG_THRESHOLD = 5;

		const QuestLog& questlog;

		Texture bg_min;
		Texture bg_max;
		Texture bg_center;
		Texture bg_bottom;

		// Per-quest close glyph (Basic.img/BtClose normal / mouseOver)
		Texture close_btn_normal;
		Texture close_btn_mouseover;

		std::vector<TrackedQuest> tracked_quests;
		bool minimized;

		// Hit test positions for per-quest X buttons and collapse toggles (set during draw)
		struct QuestHitArea
		{
			int16_t questid;
			Rectangle<int16_t> close_btn;
			Rectangle<int16_t> header_area;
		};
		mutable std::vector<QuestHitArea> quest_hit_areas;
		int16_t hovered_close_questid;

		// Internal reorder drag
		int16_t reorder_drag_index;
		Point<int16_t> reorder_start_pos;
		Point<int16_t> reorder_cursor_pos;

		mutable Text title_text;
		mutable Text drag_text;
	};
}
