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

#include "../MobileUI.h"

#include "../../Character/Look/CharLook.h"
#include "../../Graphics/Texture.h"

#include <chrono>
#include <string>
#include <vector>

namespace ms
{
	// The Cash Shop, drawn in the mobile style at the game's own view size:
	// a try-on preview and the selected item on the left, categories and the
	// catalog (Etc/Commodity.img, the list Cosmic sells from) on the right,
	// and the Locker where purchases wait until they go to the inventory.
	class UICashShop : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CASHSHOP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UICashShop();

		void draw(float inter) const override;
		void update() override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t>) const override { return true; }

		UIElement::Type get_type() const override;

		// The server has handed us back to the channel.
		void exit_cashshop();

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		enum Tab : uint8_t { FEATURED, EQUIP, USE, ETC, PET, LOCKER, NUM_TABS };

		struct Commodity
		{
			int32_t sn;
			int32_t item_id;
			int32_t price;
			int16_t count;
			int16_t period;
			Tab tab;
			int8_t sub;
			std::string name;
			std::string price_text;
			Texture icon;
		};

		struct SubFilter
		{
			const char* label;
			int8_t sub;
		};

		const std::vector<SubFilter>& sub_filters() const;
		void select_tab(uint8_t tab);
		void select_sub(size_t index);
		void rebuild();
		void select_item(int32_t index);
		void buy_selected();
		void leave();
		// Currency code that can pay for it (1 credit, 4 prepaid, 2 points), or 0
		int32_t pay_with(int32_t price) const;

		MobileUI::Rect exit_rect() const;
		MobileUI::Rect preview_rect() const;
		MobileUI::Rect reset_rect() const;
		MobileUI::Rect detail_rect() const;
		MobileUI::Rect buy_rect() const;
		MobileUI::Rect tab_rect(uint8_t tab) const;
		MobileUI::Rect card_rect(size_t index) const;
		MobileUI::Rect locker_row(size_t index) const;
		MobileUI::Rect locker_button(size_t index) const;
		MobileUI::Rect move_all_rect() const;
		int16_t grid_top() const;

		void draw_top_bar(int32_t pressed) const;
		void draw_preview(float inter, int32_t pressed) const;
		void draw_detail(int32_t pressed) const;
		void draw_catalog(int32_t pressed) const;
		void draw_locker(int32_t pressed) const;
		void draw_notice() const;

		std::vector<Commodity> catalog;
		std::vector<int32_t> shown;
		std::vector<MobileUI::Rect> sub_rects;

		uint8_t tab = FEATURED;
		size_t sub_index = 0;
		int32_t selected = -1;
		int16_t cols = 4;
		int16_t card_w = 150;

		CharLook preview_look;
		bool trying_on = false;
		bool moving_all = false;
		bool leaving = false;

		uint32_t notice_serial = 0;
		std::chrono::steady_clock::time_point notice_at;
	};
}
