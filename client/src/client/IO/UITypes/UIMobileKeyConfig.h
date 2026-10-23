//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../MobileUI.h"

#include "../KeyConfig.h"
#include "../KeyType.h"

#include <array>
#include <string>
#include <vector>

namespace ms
{
	// Touch-controls binding for the iPhone, in place of the keyboard layout of
	// UIKeyConfig. Every action button on screen - ATTACK, JUMP, LOOT and the
	// seven numbered ones - presses a key, and this panel decides what that
	// key does: a skill, a usable item or a basic action. Bindings go to the
	// server's keymap like the keyboard ones.
	class UIMobileKeyConfig : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEKEYCONFIG;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		// One on-screen button: the key it presses and what that key does
		// out of the box (TouchControls.cpp places them).
		struct Slot
		{
			int32_t glfw_key;
			KeyConfig::Key maple_key;
			const char* label;
			KeyType::Id default_type;
			int32_t default_action;
		};

		static constexpr size_t NUM_SLOTS = 10;
		static const std::array<Slot, NUM_SLOTS> SLOTS;

		// What a button shows when its key does a basic action ("JUMP"), or
		// nullptr when it is not one.
		static const char* action_label(int32_t action);

		// Opens the panel waiting for the player to pick a button for this
		// skill or item (from the skill book's "Put on a touch button").
		static void assign(KeyType::Id type, int32_t action);

		UIMobileKeyConfig();

		void draw(float alpha) const override;
		void toggle_active() override;

		UIElement::Type get_type() const override;

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;

		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		enum class Tab { SKILLS, ITEMS, ACTIONS };

		enum Target : int32_t
		{
			CLEAR = 1,
			DEFAULT,
			CANCEL_ASSIGN,
			SLOT = 100,
			TAB = 200,
			ENTRY = 1000
		};

		struct Entry
		{
			KeyType::Id type;
			int32_t id;
			std::string name;
		};

		void refresh();
		void bind(size_t slot, KeyType::Id type, int32_t action);
		void begin_assign(KeyType::Id type, int32_t action);

		std::string binding_name(KeyType::Id type, int32_t action) const;
		Texture binding_icon(KeyType::Id type, int32_t action) const;

		Point<int16_t> slot_centre(size_t slot) const;
		int16_t slot_radius(size_t slot) const;
		MobileUI::Rect info_rect() const;
		MobileUI::Rect clear_rect() const;
		MobileUI::Rect default_rect() const;
		MobileUI::Rect tab_rect(int index) const;
		MobileUI::Rect entry_rect(size_t index) const;

		Tab tab = Tab::SKILLS;
		size_t selected = 0;
		std::vector<Entry> entries;

		bool assigning = false;
		KeyType::Id assign_type = KeyType::Id::NONE;
		int32_t assign_action = 0;
	};
}
