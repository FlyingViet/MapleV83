//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileKeyConfig.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Data/ItemData.h"
#include "../../Data/SkillData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/PlayerPackets.h"

#include "../../../platform/shared/KeyCodes.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ms
{
	using namespace MobileUI;

	const std::array<UIMobileKeyConfig::Slot, UIMobileKeyConfig::NUM_SLOTS> UIMobileKeyConfig::SLOTS = { {
		{ GLFW_KEY_LEFT_CONTROL, KeyConfig::Key::LEFT_CONTROL, "ATTACK", KeyType::Id::ACTION, KeyAction::Id::ATTACK },
		{ GLFW_KEY_LEFT_ALT, KeyConfig::Key::LEFT_ALT, "JUMP", KeyType::Id::ACTION, KeyAction::Id::JUMP },
		{ GLFW_KEY_Z, KeyConfig::Key::Z, "LOOT", KeyType::Id::ACTION, KeyAction::Id::PICKUP },
		{ GLFW_KEY_LEFT_SHIFT, KeyConfig::Key::LEFT_SHIFT, "1", KeyType::Id::NONE, 0 },
		{ GLFW_KEY_INSERT, KeyConfig::Key::INSERT, "2", KeyType::Id::NONE, 0 },
		{ GLFW_KEY_HOME, KeyConfig::Key::HOME, "3", KeyType::Id::NONE, 0 },
		{ GLFW_KEY_PAGE_UP, KeyConfig::Key::PAGE_UP, "4", KeyType::Id::NONE, 0 },
		{ GLFW_KEY_DELETE, KeyConfig::Key::DELETE, "5", KeyType::Id::NONE, 0 },
		{ GLFW_KEY_END, KeyConfig::Key::END, "6", KeyType::Id::NONE, 0 },
		{ GLFW_KEY_PAGE_DOWN, KeyConfig::Key::PAGE_DOWN, "7", KeyType::Id::NONE, 0 },
	} };

	namespace
	{
		constexpr int16_t W = 400;
		constexpr int16_t PAD = 16;
		constexpr int16_t TOP = 76;
		constexpr int16_t INFO_W = 196;
		constexpr int16_t CLUSTER_H = 200;
		constexpr int16_t TABS_Y = TOP + CLUSTER_H + 10;
		constexpr int16_t LIST_Y = TABS_Y + 44;
		constexpr int16_t ENTRY_H = 48;
		constexpr int16_t ENTRY_STEP = 54;
		constexpr int16_t UNIT = 20;

		struct Action
		{
			int32_t id;
			const char* label;
			const char* name;
		};

		constexpr Action ACTIONS[] = {
			{ KeyAction::Id::ATTACK, "ATTACK", "Attack" },
			{ KeyAction::Id::JUMP, "JUMP", "Jump" },
			{ KeyAction::Id::PICKUP, "LOOT", "Pick up" },
			{ KeyAction::Id::SIT, "SIT", "Sit" },
			{ KeyAction::Id::INTERACT_HARVEST, "TALK", "Talk to NPC" },
		};

		const Action* find_action(int32_t id)
		{
			for (const Action& a : ACTIONS)
				if (a.id == id)
					return &a;

			return nullptr;
		}

		const char* TAB_NAMES[3] = { "Skills", "Items", "Actions" };
	}

	const char* UIMobileKeyConfig::action_label(int32_t action)
	{
		const Action* a = find_action(action);

		return a ? a->label : nullptr;
	}

	void UIMobileKeyConfig::assign(KeyType::Id type, int32_t action)
	{
		auto panel = UI::get().get_element<UIMobileKeyConfig>();

		if (!panel || !panel->is_active())
			UI::get().emplace<UIMobileKeyConfig>();

		if (auto opened = UI::get().get_element<UIMobileKeyConfig>())
			opened->begin_assign(type, action);
	}

	UIMobileKeyConfig::UIMobileKeyConfig() : UIMobilePanel(Edge::LEFT, W)
	{
		refresh();
	}

	void UIMobileKeyConfig::toggle_active()
	{
		UIMobilePanel::toggle_active();

		if (active)
		{
			assigning = false;
			refresh();
		}
	}

	void UIMobileKeyConfig::begin_assign(KeyType::Id type, int32_t action)
	{
		assigning = true;
		assign_type = type;
		assign_action = action;
		tab = type == KeyType::Id::SKILL ? Tab::SKILLS : Tab::ITEMS;
		refresh();
	}

	// ---- data ---------------------------------------------------------------

	std::string UIMobileKeyConfig::binding_name(KeyType::Id type, int32_t action) const
	{
		if (type == KeyType::Id::SKILL && action > 0)
			return SkillData::get(action).get_name();

		if ((type == KeyType::Id::ITEM || type == KeyType::Id::CASH) && action > 0)
			return ItemData::get(action).get_name();

		if (type == KeyType::Id::ACTION)
			if (const Action* a = find_action(action))
				return a->name;

		if (type == KeyType::Id::NONE)
			return "Nothing yet";

		return "A game shortcut";
	}

	Texture UIMobileKeyConfig::binding_icon(KeyType::Id type, int32_t action) const
	{
		if (type == KeyType::Id::SKILL && action > 0)
			return SkillData::get(action).get_icon(SkillData::Icon::NORMAL);

		if ((type == KeyType::Id::ITEM || type == KeyType::Id::CASH) && action > 0)
			return ItemData::get(action).get_icon(false);

		return Texture();
	}

	void UIMobileKeyConfig::refresh()
	{
		entries.clear();
		Player& player = Stage::get().get_player();

		if (tab == Tab::SKILLS)
		{
			std::set<int32_t> ids;

			for (const auto& [id, entry] : player.get_skills().get_entries())
			{
				const SkillData& data = SkillData::get(id);

				if (entry.level > 0 && !data.is_passive() && !data.is_invisible())
					ids.insert(id);
			}

			for (int32_t id : ids)
				entries.push_back({ KeyType::Id::SKILL, id, SkillData::get(id).get_name() });
		}
		else if (tab == Tab::ITEMS)
		{
			const Inventory& inventory = player.get_inventory();
			uint8_t slots = inventory.get_slotmax(InventoryType::Id::USE);
			std::set<int32_t> seen;

			for (int16_t slot = 1; slot <= slots; slot++)
			{
				int32_t id = inventory.get_item_id(InventoryType::Id::USE, slot);

				if (id > 0 && seen.insert(id).second)
					entries.push_back({ KeyType::Id::ITEM, id, ItemData::get(id).get_name() });
			}
		}
		else
		{
			for (const Action& a : ACTIONS)
				entries.push_back({ KeyType::Id::ACTION, a.id, a.name });
		}

		scroll = 0;
		clamp_scroll();
	}

	void UIMobileKeyConfig::bind(size_t slot, KeyType::Id type, int32_t action)
	{
		KeyConfig::Key key = SLOTS[slot].maple_key;

		ChangeKeyMapPacket({ std::make_tuple(key, type, action) }).dispatch();

		Keyboard& keyboard = UI::get().get_keyboard();

		if (type == KeyType::Id::NONE)
			keyboard.remove(key);
		else
			keyboard.assign(key, type, action);

		Sound(Sound::Name::BUTTONCLICK).play();
	}

	// ---- layout -------------------------------------------------------------

	// The buttons as they sit on screen (TouchControls::layout), in units of
	// the JUMP button's radius.
	Point<int16_t> UIMobileKeyConfig::slot_centre(size_t slot) const
	{
		float r = UNIT;
		float right = position.x() + W - PAD - 2.0f;
		float bottom = position.y() + TOP + CLUSTER_H - 4.0f;
		float outer = right - r * 1.55f;
		float inner = outer - r * 2.35f;
		float third = inner - r * 2.1f;
		float jump_y = bottom - r * 1.05f;
		float attack_y = jump_y - r * 1.2f;
		float row = r * 2.2f;

		float pos[NUM_SLOTS][2] = {
			{ outer, attack_y }, { inner, jump_y }, { third, jump_y + r * 0.1f },
			{ outer, attack_y - row * 1.05f }, { inner, jump_y - row * 1.0f },
			{ outer, attack_y - row * 1.95f }, { inner, jump_y - row * 1.9f },
			{ third, jump_y - row * 1.0f }, { third, jump_y - row * 1.9f },
			{ outer, attack_y - row * 2.85f }
		};

		return Point<int16_t>(static_cast<int16_t>(std::lround(pos[slot][0])), static_cast<int16_t>(std::lround(pos[slot][1])));
	}

	int16_t UIMobileKeyConfig::slot_radius(size_t slot) const
	{
		return static_cast<int16_t>(std::lround(UNIT * (slot == 0 ? 1.25f : slot == 1 ? 1.0f : 0.82f)));
	}

	Rect UIMobileKeyConfig::info_rect() const
	{
		return rect(position.x() + PAD, position.y() + TOP, INFO_W, CLUSTER_H);
	}

	Rect UIMobileKeyConfig::clear_rect() const
	{
		Rect info = info_rect();
		int16_t w = static_cast<int16_t>((info.width() - 24 - 8) / 2);

		return rect(static_cast<int16_t>(info.left() + 12), static_cast<int16_t>(info.bottom() - 12 - 38), w, 38);
	}

	Rect UIMobileKeyConfig::default_rect() const
	{
		Rect c = clear_rect();

		return rect(static_cast<int16_t>(c.right() + 8), c.top(), c.width(), c.height());
	}

	Rect UIMobileKeyConfig::tab_rect(int index) const
	{
		int16_t w = static_cast<int16_t>((W - PAD * 2 - 12) / 3);

		return rect(static_cast<int16_t>(position.x() + PAD + index * (w + 6)), position.y() + TABS_Y, w, 34);
	}

	Rectangle<int16_t> UIMobileKeyConfig::scroll_area() const
	{
		return Rectangle<int16_t>(static_cast<int16_t>(position.x() + PAD), static_cast<int16_t>(position.x() + W - PAD),
			static_cast<int16_t>(position.y() + LIST_Y), static_cast<int16_t>(position.y() + HEIGHT - 12));
	}

	int16_t UIMobileKeyConfig::scroll_content() const
	{
		size_t rows = (entries.size() + 1) / 2;

		return static_cast<int16_t>(rows * ENTRY_STEP);
	}

	Rect UIMobileKeyConfig::entry_rect(size_t index) const
	{
		int16_t w = static_cast<int16_t>((W - PAD * 2 - 8) / 2);
		int16_t col = static_cast<int16_t>(index % 2);
		int16_t row = static_cast<int16_t>(index / 2);

		return rect(static_cast<int16_t>(position.x() + PAD + col * (w + 8)),
			static_cast<int16_t>(position.y() + LIST_Y + row * ENTRY_STEP - scroll), w, ENTRY_H);
	}

	// ---- input --------------------------------------------------------------

	int32_t UIMobileKeyConfig::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		for (size_t i = 0; i < NUM_SLOTS; i++)
		{
			Point<int16_t> c = slot_centre(i);
			int32_t dx = pos.x() - c.x();
			int32_t dy = pos.y() - c.y();
			int32_t reach = slot_radius(i) + 4;

			if (dx * dx + dy * dy <= reach * reach)
				return SLOT + static_cast<int32_t>(i);
		}

		if (assigning)
		{
			if (inset(clear_rect(), 0, 0).contains(pos) || default_rect().contains(pos))
				return CANCEL_ASSIGN;
		}
		else
		{
			if (clear_rect().contains(pos))
				return CLEAR;

			if (default_rect().contains(pos))
				return DEFAULT;
		}

		for (int i = 0; i < 3; i++)
			if (tab_rect(i).contains(pos))
				return TAB + i;

		if (scroll_area().contains(pos))
			for (size_t i = 0; i < entries.size(); i++)
				if (entry_rect(i).contains(pos))
					return ENTRY + static_cast<int32_t>(i);

		return NO_TARGET;
	}

	void UIMobileKeyConfig::activate(int32_t target)
	{
		if (target >= ENTRY)
		{
			size_t i = static_cast<size_t>(target - ENTRY);

			if (i >= entries.size())
				return;

			bind(selected, entries[i].type, entries[i].id);

			// The numbered buttons fill in order: a run of taps sets them all.
			if (selected >= 3 && selected + 1 < NUM_SLOTS)
				selected++;

			return;
		}

		if (target >= TAB)
		{
			Tab t = static_cast<Tab>(target - TAB);

			if (t != tab)
			{
				Sound(Sound::Name::TAB).play();
				tab = t;
				refresh();
			}

			return;
		}

		if (target >= SLOT)
		{
			selected = static_cast<size_t>(target - SLOT);

			if (assigning)
			{
				bind(selected, assign_type, assign_action);
				assigning = false;
			}
			else
			{
				Sound(Sound::Name::BUTTONCLICK).play();
			}

			return;
		}

		switch (target)
		{
			case CLEAR:
				bind(selected, KeyType::Id::NONE, 0);
				break;
			case DEFAULT:
				bind(selected, SLOTS[selected].default_type, SLOTS[selected].default_action);
				break;
			case CANCEL_ASSIGN:
				Sound(Sound::Name::BUTTONCLICK).play();
				assigning = false;
				break;
		}
	}

	// ---- drawing ------------------------------------------------------------

	void UIMobileKeyConfig::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();
		const Keyboard& keyboard = UI::get().get_keyboard();

		panel(f, "Touch Controls", assigning ? "Tap the button it should go on" : "Pick a button, then what it does");
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		// What the selected button does.
		{
			Rect info = info_rect();
			fill(info, Theme::CARD, 14);

			if (assigning)
			{
				text("PUT ON A BUTTON", Font::SMALL_BOLD, Theme::BLUE, Point<int16_t>(info.left() + 12, info.top() + 10));

				Rect box = rect(static_cast<int16_t>(info.left() + 12), static_cast<int16_t>(info.top() + 32), 52, 52);
				fill(box, Theme::TRACK, 12);
				icon(binding_icon(assign_type, assign_action), inset(box, 6, 6));

				text(binding_name(assign_type, assign_action), Font::BODY_BOLD, Theme::TEXT,
					Point<int16_t>(info.left() + 12, info.top() + 94), Text::Alignment::LEFT, info.width() - 24);
				text("Tap one of the buttons on the right.", Font::SMALL, Theme::MUTED,
					Point<int16_t>(info.left() + 12, info.top() + 122), Text::Alignment::LEFT, info.width() - 24);

				Rect cancel = rect(clear_rect().left(), clear_rect().top(), static_cast<int16_t>(default_rect().right() - clear_rect().left()), clear_rect().height());
				button(cancel, "Cancel", Style::SECONDARY, true, pressed == CANCEL_ASSIGN);
			}
			else
			{
				const Slot& slot = SLOTS[selected];
				Keyboard::Mapping m = keyboard.get_mapping(slot.glfw_key);
				bool numbered = selected >= 3;

				text(numbered ? std::string("BUTTON ") + slot.label : std::string(slot.label) + " BUTTON",
					Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(info.left() + 12, info.top() + 10));

				Rect box = rect(static_cast<int16_t>(info.left() + 12), static_cast<int16_t>(info.top() + 32), 52, 52);
				fill(box, Theme::TRACK, 12);
				Texture tex = binding_icon(m.type, m.action);

				if (tex.is_valid())
					icon(tex, inset(box, 6, 6));
				else if (m.type == KeyType::Id::ACTION && action_label(m.action))
					text_in(action_label(m.action), Font::SMALL_BOLD, Theme::TEXT, box);

				text(binding_name(m.type, m.action), Font::BODY_BOLD, m.type == KeyType::Id::NONE ? Theme::FAINT : Theme::TEXT,
					Point<int16_t>(info.left() + 12, info.top() + 94), Text::Alignment::LEFT, info.width() - 24);

				bool is_default = m.type == slot.default_type && (slot.default_type == KeyType::Id::NONE || m.action == slot.default_action);
				button(clear_rect(), "Clear", Style::GHOST, m.type != KeyType::Id::NONE, pressed == CLEAR);
				button(default_rect(), "Default", Style::GHOST, !is_default, pressed == DEFAULT);
			}
		}

		// The buttons, laid out like the real ones.
		for (size_t i = 0; i < NUM_SLOTS; i++)
		{
			Point<int16_t> c = slot_centre(i);
			int16_t r = slot_radius(i);
			Rect disc = rect(static_cast<int16_t>(c.x() - r), static_cast<int16_t>(c.y() - r), static_cast<int16_t>(r * 2), static_cast<int16_t>(r * 2));
			bool is_selected = i == selected && !assigning;

			if (is_selected || assigning)
			{
				RGBA ring = is_selected ? Theme::ACCENT : Theme::BLUE.alpha(0.85f);
				fill(inset(disc, -3, -3), ring, static_cast<int16_t>(r + 3));
			}

			fill(disc, pressed == SLOT + static_cast<int32_t>(i) ? Theme::TRACK.mix(Theme::TEXT, 0.15f) : Theme::TRACK, r);

			Keyboard::Mapping m = keyboard.get_mapping(SLOTS[i].glfw_key);
			Texture tex = binding_icon(m.type, m.action);

			if (tex.is_valid())
			{
				int16_t s = static_cast<int16_t>(r * 1.25f);
				icon(tex, rect(static_cast<int16_t>(c.x() - s / 2), static_cast<int16_t>(c.y() - s / 2), s, s));
			}
			else
			{
				const char* label = m.type == KeyType::Id::ACTION && action_label(m.action) ? action_label(m.action) : SLOTS[i].label;
				text_in(label, i == 0 ? Font::SMALL_BOLD : Font::SMALL_BOLD, m.type == KeyType::Id::NONE ? Theme::FAINT : Theme::TEXT, disc);
			}
		}

		// Skills / Items / Actions.
		for (int i = 0; i < 3; i++)
			button(tab_rect(i), TAB_NAMES[i], static_cast<int>(tab) == i ? Style::SELECTED : Style::GHOST, true,
				pressed == TAB + i, Font::SMALL_BOLD);

		Rectangle<int16_t> area = scroll_area();
		Keyboard::Mapping current = keyboard.get_mapping(SLOTS[selected].glfw_key);

		if (entries.empty())
		{
			text_in(tab == Tab::SKILLS ? "Learn an active skill in Skills first." : "Potions and scrolls you carry show up here.",
				Font::SMALL, Theme::FAINT, rect(area.left(), area.top(), area.width(), 50));
		}

		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		for (size_t i = 0; i < entries.size(); i++)
		{
			Rect e = entry_rect(i);

			if (e.bottom() < area.top() || e.top() > area.bottom())
				continue;

			const Entry& entry = entries[i];
			bool bound = !assigning && current.type == entry.type && current.action == entry.id;
			bool is_pressed = pressed == ENTRY + static_cast<int32_t>(i);

			if (bound)
				fill(inset(e, -2, -2), Theme::ACCENT, 13);

			fill(e, is_pressed ? Theme::CARD_HI : Theme::CARD, 12);

			Rect box = rect(static_cast<int16_t>(e.left() + 6), static_cast<int16_t>(e.top() + 6), 36, 36);
			Texture tex = binding_icon(entry.type, entry.id);
			fill(box, Theme::TRACK, 9);

			if (tex.is_valid())
				icon(tex, inset(box, 2, 2));
			else if (const char* label = action_label(entry.id))
				text_in(std::string(label).substr(0, 1), Font::BODY_BOLD, Theme::ACCENT, box);

			int16_t name_w = static_cast<int16_t>(e.right() - box.right() - 14);
			text_in(ellipsize(entry.name, Font::SMALL_BOLD, name_w), Font::SMALL_BOLD, Theme::TEXT,
				rect(static_cast<int16_t>(box.right() + 8), e.top(), name_w, e.height()), Text::Alignment::LEFT, 0);
		}

		GraphicsGL::get().end_clip();
	}

	UIElement::Type UIMobileKeyConfig::get_type() const
	{
		return TYPE;
	}
}
