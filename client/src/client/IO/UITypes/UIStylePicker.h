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

#include "../../Character/Look/CharLook.h"

#include <string>
#include <vector>

namespace ms
{
	// A salon NPC's "pick a style" question (v83 NPC talk type 7, the
	// scripts' sendStyle): the player's character wearing each offered hair,
	// face, lens colour or skin tone, stepped through with arrows. Choosing
	// answers the NPC with the style's index; Cancel ends the conversation.
	class UIStylePicker : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::STYLEPICKER;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIStylePicker(int32_t npcid, const std::string& message, std::vector<int32_t> styles);

		void draw(float alpha) const override;
		void update() override;
		void update_screen(int16_t new_width, int16_t new_height) override;

		bool is_in_range(Point<int16_t> cursorpos) const override;
		bool captures_hold() const override { return true; }
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	private:
		enum Target : int32_t { NONE = -1, CANCEL = -2, CHOOSE = -3, PREV = -4, NEXT = -5, FIRST_DOT = 0 };
		enum class Kind { HAIR, HAIR_COLOR, FACE, LENS, SKIN };

		MobileUI::Rect card() const;
		MobileUI::Rect preview_rect() const;
		MobileUI::Rect prev_rect() const;
		MobileUI::Rect next_rect() const;
		MobileUI::Rect dot_rect(size_t index) const;
		MobileUI::Rect cancel_rect() const;
		MobileUI::Rect choose_rect() const;
		bool shows_dots() const;
		int32_t hit(Point<int16_t> pos) const;

		void show(size_t index);
		std::string style_name(size_t index) const;
		void answer(bool chosen);

		std::vector<int32_t> styles;
		Kind kind = Kind::HAIR;
		std::string npc_name;
		std::string message;
		size_t current = 0;
		CharLook look;

		int32_t pressed = NONE;
		uint32_t press = 0;
		bool answered = false;
	};
}
