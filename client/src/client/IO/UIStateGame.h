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

#include "UIState.h"

#include "Components/EquipTooltip.h"
#include "Components/ItemTooltip.h"
#include "Components/MapTooltip.h"
#include "Components/SkillTooltip.h"
#include "Components/TextTooltip.h"

#include "../Character/CharStats.h"

#include <vector>

namespace ms
{
	class UIStateGame : public UIState
	{
	public:
		UIStateGame();

		void draw(float inter, Point<int16_t> cursor) const override;
		void update() override;

		void doubleclick(Point<int16_t> pos) override;
		void rightclick(Point<int16_t> pos) override;
		void send_key(KeyType::Id type, int32_t action, bool pressed, bool escape) override;
		Cursor::State send_cursor(Cursor::State cursorstate, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;
		void send_close() override;

		void drag_icon(Icon* icon) override;
		void purge_icon(const Icon* icon) override;
		void clear_tooltip(Tooltip::Parent parent) override;
		void show_equip(Tooltip::Parent parent, int16_t slot) override;
		void show_item(Tooltip::Parent parent, int32_t itemid) override;
		void show_skill(Tooltip::Parent parent, int32_t skill_id, int32_t level, int32_t masterlevel, int64_t expiration) override;
		void show_text(Tooltip::Parent parent, std::string text) override;
		void show_map(Tooltip::Parent parent, std::string name, std::string description, int32_t mapid, bool bolded) override;

		Iterator pre_add(UIElement::Type type, bool toggled, bool focused);
		void remove(UIElement::Type type) override;
		UIElement* get(UIElement::Type type) override;
		UIElement* get_front(std::list<UIElement::Type> types) override;
		UIElement* get_front(Point<int16_t> pos) override;

		bool side_panel_open() const override;
		bool holds_press() const override;
		Range<int16_t> free_span() const override;
		bool npc_talk_open() const override;

	private:
		// On the iPhone the big windows open as side panels: docked to the
		// left or right edge, drawn larger for fingers, sliding in over a
		// dimmed game; a tap on the dimmed game closes them.
		enum class Side
		{
			LEFT,
			RIGHT,
			// Enlarged in place in the middle (NPC conversations): no dimming,
			// and a tap outside does not close it.
			CENTER
		};

		struct Dock
		{
			UIElement::Type type;
			Side side;
			float progress = 0.0f;
		};

		struct DockGeometry
		{
			Point<int16_t> from;
			Point<int16_t> to;
			float scale = 1.0f;
		};

		bool docking() const;
		const Dock* dock_for(UIElement::Type type) const;
		bool dock_geometry(const UIElement& element, const Dock& dock, DockGeometry& geometry) const;
		Point<int16_t> to_local(const UIElement* element, Point<int16_t> screen) const;
		// The first open side panel in draw order, or end() when none is open.
		std::list<UIElement::Type>::const_iterator first_open_dock() const;
		bool scrim_covers(const UIElement* front) const;
		void close_docks();
		void update_docks();

		std::vector<Dock> docks;

		const CharStats& stats;

		bool drop_icon(const Icon& icon, Point<int16_t> pos);
		void remove_icon();
		void remove_cursors();
		void remove_cursor(UIElement::Type type);

		template <class T, typename...Args>
		void emplace(Args&& ...args);

		EnumMap<UIElement::Type, UIElement::UPtr, UIElement::Type::NUM_TYPES> elements;
		std::list<UIElement::Type> elementorder;
		UIElement::Type focused;
		UIElement* dragged;

		// Elements pulled out by remove() are parked here and destroyed at
		// the start of the next update(), so an element that removes itself
		// from inside its own handler isn't freed mid-call.
		std::vector<UIElement::UPtr> graveyard;

		EquipTooltip eqtooltip;
		ItemTooltip ittooltip;
		SkillTooltip sktooltip;
		TextTooltip tetooltip;
		MapTooltip matooltip;
		Optional<Tooltip> tooltip;
		Tooltip::Parent tooltipparent;

		Optional<Icon> draggedicon;
		UIElement::Type icon_owner(Icon::IconType type);

		std::map<Icon::IconType, UIElement::Type> icon_map =
		{
			{ Icon::IconType::NONE,			UIElement::Type::NONE			},
			{ Icon::IconType::SKILL,		UIElement::Type::SKILLBOOK		},
			{ Icon::IconType::EQUIP,		UIElement::Type::EQUIPINVENTORY	},
			{ Icon::IconType::ITEM,			UIElement::Type::ITEMINVENTORY	},
			{ Icon::IconType::KEY,			UIElement::Type::KEYCONFIG		},
			{ Icon::IconType::NUM_TYPES,	UIElement::Type::NUM_TYPES		}
		};

		int16_t VWIDTH;
		int16_t VHEIGHT;
	};
}