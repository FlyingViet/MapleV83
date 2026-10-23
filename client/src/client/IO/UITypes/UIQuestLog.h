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

#include "../Components/Slider.h"
#include "../Components/Textfield.h"
#include "../../Graphics/Geometry.h"
#include "../../Graphics/Animation.h"

#include "../../Character/QuestLog.h"

namespace ms
{
	class UIQuestLog : public UIDragElement<PosQUEST>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::QUESTLOG;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIQuestLog(const QuestLog& questLog);

		void draw(float inter) const override;
		void update() override;
		void toggle_active() override;

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;
		void doubleclick(Point<int16_t> cursorpos) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;

		UIElement::Type get_type() const override;

		void load_quests();

		// Open the Completed tab and select the entry for `qid` (if it
		// appears in the completed list). Used by the BT_NOTICE drawer
		// when the user clicks a "Quest Completed" notification.
		void focus_completed_quest(int16_t qid);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void change_tab(uint16_t tabid);
		void select_quest(int16_t index, bool from_recommended = false);

		// Safe button accessors (many buttons don't exist in v83)
		void set_btn_active(uint16_t id, bool active);
		void set_btn_state(uint16_t id, Button::State state);
		void set_btn_position(uint16_t id, Point<int16_t> pos);

		enum Buttons : uint16_t
		{
			TAB0,
			TAB1,
			TAB2,
			CLOSE,
			ALL_LEVEL,
			MY_LEVEL,
			BT_SEARCH,
			BT_NEXT,
			BT_PREV,
			BT_ALLLOCN,
			BT_MYLOCATION,
			BT_ICONINFO,
			GIVEUP,
			DETAIL,
			MARK_NPC,
			BT_ACCEPT,
			BT_FINISH,
			BT_DETAIL_CLOSE,
			BT_ARLIM,
			BT_DELIVERY_ACCEPT,
			BT_DELIVERY_COMPLETE,
			BT_NO,
			BT_ALERT,
			TAB3
		};

		/// v83 list panel layout (UIWindow.img/Quest/backgrnd, 245x396)
		static constexpr int16_t ROWS = 11;
		static constexpr int16_t ROW_HEIGHT = 24;
		// Rows start under the search box; the white list area is y 45..363
		static constexpr int16_t LIST_Y = 70;
		// Row hit-box right edge (inside the 3 px frame)
		static constexpr int16_t LIST_RIGHT = 232;
		// Basic.img VScr column hugging the list's right edge; its 12 px
		// prev/next buttons cap the scroll range
		static constexpr int16_t SLIDER_X = 226;
		static constexpr int16_t SLIDER_BTN_H = 12;
		// Tab band is y 24..41; text bitmaps are 45x11, 51x13, 45x13
		static constexpr Point<int16_t> TAB0_POS = Point<int16_t>(8, 27);
		static constexpr Point<int16_t> TAB1_POS = Point<int16_t>(57, 26);
		static constexpr Point<int16_t> TAB2_POS = Point<int16_t>(112, 26);
		// Hand-drawn 4th tab fills the rest of the band
		static constexpr Point<int16_t> TAB3_POS = Point<int16_t>(162, 24);
		static constexpr int16_t TAB3_W = 76;
		static constexpr int16_t TAB3_H = 18;
		// Basic.img BtClose (12x12) in the title box (y 5..18) right corner
		static constexpr int16_t CLOSE_X = 227;
		static constexpr int16_t CLOSE_Y = 6;
		static constexpr int16_t CLOSE_INSET_X = 18;
		// Search field box at the top of the white list area
		static constexpr Point<int16_t> SEARCH_POS = Point<int16_t>(12, 49);
		static constexpr Point<int16_t> SEARCH_DIM = Point<int16_t>(220, 17);
		// Empty-list notice and the countdown clock (foot band y 364..391)
		static constexpr int16_t NOTICE_Y = 130;
		static constexpr int16_t CLOCK_Y = 366;

		/// v83 detail panel layout (UIWindow.img/Quest/backgrnd2, 305x396)
		// Blue summary box spans y 25..119 with a divider at x 189
		static constexpr int16_t DETAIL_HEADER_H = 122;
		static constexpr Point<int16_t> DETAIL_TITLE_POS = Point<int16_t>(16, 3);
		static constexpr int16_t DETAIL_SUMMARY_X = 22;
		static constexpr int16_t DETAIL_SUMMARY_Y = 32;
		static constexpr Point<int16_t> DETAIL_NPC_FEET = Point<int16_t>(243, 98);
		static constexpr Point<int16_t> DETAIL_NPC_NAME_POS = Point<int16_t>(243, 104);
		// Foot band buttons (17 px tall): 57 | 57 | 81 | 86 wide with 4 px gaps
		static constexpr int16_t DETAIL_BTN_Y = 368;
		static constexpr int16_t DETAIL_BTN_X0 = 5;
		static constexpr int16_t DETAIL_BTN_X1 = 66;
		static constexpr int16_t DETAIL_BTN_X2 = 127;
		static constexpr int16_t DETAIL_BTN_X3 = 212;

		const QuestLog& questlog;

		uint16_t tab;
		uint16_t offset;
		// Empty-list notices per tab (notice0..2)
		std::vector<Texture> notice_textures;
		Textfield search;
		Text placeholder;
		Slider slider;
		Animation quest_icon_anim;

		// Quest root icons
		Texture icon0;
		Texture icon1;
		Texture icon4;
		Texture icon10;
		Animation icon2_anim;
		Animation icon5_anim;
		Animation icon6_anim;
		Animation icon7_anim;
		Animation icon8_anim;
		Animation icon9_anim;
		Animation iconQM0_anim;
		Animation iconQM1_anim;

		// Gauge2 - quest progress bar (root level)
		Texture gauge2_frame;
		Texture gauge2_bar;

		// TimeQuest
		Animation time_alarm_clock;
		Animation time_bar;

		// icon_info panel
		Texture icon_info_backgrnd;
		Texture icon_info_backgrnd2;
		bool show_icon_info;


		struct QuestEntry
		{
			int16_t id;
			Text name;
			int32_t area = 0;
			// Which icon from the ? legend panel to draw next to this entry.
			// Maps directly to icon0..icon10 + QM0/QM1 (0-12).
			uint8_t icon_type = 0;
		};

		// Pick an appropriate legend icon (0..12) for a quest, given its tab/state.
		uint8_t pick_quest_icon(int16_t qid, uint16_t tab_id, bool is_recommended) const;
		// Draw the chosen legend icon at pos for a given quest entry.
		void draw_entry_icon(uint8_t icon_type, Point<int16_t> pos, float alpha) const;

		// Returns the visual height (in px) needed to render a quest entry's
		// name. If the name's wrapped layout exceeds ROW_HEIGHT (i.e. it spans
		// more than one visual line), the row grows to fit; otherwise returns
		// ROW_HEIGHT. A small padding is added so successive rows don't touch.
		int16_t row_height_for(const QuestEntry& e) const;

		std::vector<QuestEntry> available_entries;
		std::vector<QuestEntry> recommended_entries;
		std::vector<QuestEntry> active_entries;
		std::vector<QuestEntry> completed_entries;
		std::vector<QuestEntry> weekly_entries;

		// Virtual row layouts with area-group dividers.
		// Each element: entry index (>=0) or -1 (divider row).
		std::vector<int16_t> active_row_layout;
		std::vector<int16_t> completed_row_layout;
		std::vector<int16_t> weekly_row_layout;
		bool filter_my_level;
		bool filter_my_location;
		bool show_recommended;
		std::string search_text;

		int16_t selected_entry;
		int16_t hover_entry;
		Point<int16_t> last_cursor_pos;
		bool selected_from_recommended;

		// Cached revision of NpcResponseTracker so we can rebuild the
		// "Not Available" tab only when the unavailable set actually changes.
		uint32_t cached_unavail_rev = 0;

		// Quest drag-to-helper state
		int16_t drag_quest_id;
		std::string drag_quest_name;
		Point<int16_t> drag_start_pos;
		bool drag_started;
		static constexpr int16_t QUEST_DRAG_THRESHOLD = 8;

		// Returns total virtual row count for TAB0 (headers + entries)
		uint16_t get_available_row_count() const;
		// Maps a virtual row index to what it represents
		enum class RowType { RECOMMEND_HEADER, RECOMMEND_ENTRY, AVAILABLE_HEADER, AVAILABLE_ENTRY };
		RowType get_row_type(int16_t row, int16_t& entry_index) const;
		// Select quest from a virtual row
		void select_row(int16_t row);

		// Detail panel
		bool show_detail;
		// Slide animation [0..1]: 0 hidden, 1 fully shown.
		// Eased toward show_detail in update().
		float detail_anim_t;
		int16_t detail_scroll;
		mutable int16_t detail_content_height;
		Texture detail_backgrnd;
		Text detail_quest_name;
		Text detail_quest_desc;
		Text detail_quest_summary;
		Text detail_level_req;

		// Detail panel gauge (quest progress)
		Texture detail_gauge_frame;
		Texture detail_gauge_bar;
		Texture detail_gauge_spot;
		float detail_progress;

		// NPC sprite
		Texture detail_npc_sprite;
		Text detail_npc_name;
		int32_t detail_npcid;

		// Reward/requirement items
		struct QuestItem
		{
			Texture icon;
			Text name;
			Text count;
		};
		std::vector<QuestItem> detail_rewards;          // unconditional items + exp/meso/fame/skill
		std::vector<QuestItem> detail_rewards_select;   // "Obtain selectively" (player picks one)
		std::vector<QuestItem> detail_rewards_prob;     // "Obtain randomly" (server rolls from pool)
		std::vector<QuestItem> detail_requirements;

		// Quest job requirement text
		Text detail_job_req;

		// Prerequisite quest names
		std::vector<Text> detail_prereq_quests;

		// Say.img NPC dialog lines
		struct SayEntry
		{
			std::string text;
			bool has_yes_no;
		};
		std::vector<SayEntry> detail_say_lines;

		// PQuest.img data
		std::string detail_pquest_name;
		std::string detail_pquest_mark;

		// Exclusive.img medal
		std::string detail_medal;

		// QuestInfo.img extras
		int32_t detail_area;
		std::string detail_parent;
		int32_t detail_order;
		bool detail_auto_start;
		bool detail_auto_complete;
		int32_t detail_time_limit;

		// v83 root textures from UIWindow.img/Quest
		Texture basic_texture;
		Texture prob_texture;
		Texture reward_texture;
		Texture summary_texture;
		Texture obtain_select_texture;   // UIWindow.img/Quest/select: "Obtain selectively" banner

		// List panel background (UIWindow.img/Quest/backgrnd)
		Texture list_backgrnd;


		// QuestAlarm (UIWindow.img/QuestAlarm)
		Texture quest_alarm_bg_bottom;
		Texture quest_alarm_bg_center;
		Texture quest_alarm_bg_max;
		Texture quest_alarm_bg_min;
		Animation quest_alarm_anim;
		bool show_quest_alarm;

		// QuestIcon (UIWindow.img/QuestIcon) — animated status icons 0-11
		std::vector<Animation> quest_status_icons;

		// Pre-allocated draw objects (avoid per-frame GPU allocations)
		ColorBox tab_bg_active;
		ColorBox tab_bg_inactive;
		mutable Text tab_label_text;
		ColorBox hover_bg_recommend;
		ColorBox sel_bg_recommend;
		ColorBox hover_bg_normal;
		ColorBox sel_bg_normal;
		ColorBox sep_line_box;
		ColorBox header_sep_box;
		ColorBox search_box;
		mutable Text say_preview_text;
		mutable Text medal_text;
		mutable Text time_text;
	};
}
