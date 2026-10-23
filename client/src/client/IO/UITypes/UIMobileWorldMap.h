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
#include "../Components/Textfield.h"

#include "../../Graphics/Texture.h"

#include <string>
#include <vector>

namespace ms
{
	// The iPhone's world map: the region's picture with every place on it as
	// a dot, the same places as a list beside it, a search over the whole
	// world, and a free teleport to any map.
	class UIMobileWorldMap : public UIMobilePanel
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEWORLDMAP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIMobileWorldMap();
		~UIMobileWorldMap();

		void draw(float alpha) const override;
		void update() override;
		void deactivate() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		// Back to the region the player is in, with their place picked.
		void show_here();

	protected:
		int32_t hit(Point<int16_t> pos) const override;
		void activate(int32_t target) override;
		Rectangle<int16_t> scroll_area() const override;
		int16_t scroll_content() const override;

	private:
		struct Spot
		{
			Point<int16_t> pos;
			int32_t type;
			std::string title;
			std::string subtitle;
			// The maps here that exist in this client's data.
			std::vector<int32_t> maps;
		};

		struct Link
		{
			std::string image;
			std::string name;
			Texture picture;
			MobileUI::Rect area;
		};

		struct Result
		{
			int32_t map;
			std::string name;
			std::string where;
			std::string image;
		};

		struct Row
		{
			enum Kind { HEADER, LINK, SPOT, MAP, RESULT, NOTE } kind;
			size_t index;
			int16_t y;
			int16_t h;
			std::string text;
		};

		void load(const std::string& image);
		void select(int32_t spot);
		void teleport(int32_t map);
		void search(const std::string& query);
		void unfocus();
		int32_t spot_of(int32_t map) const;

		MobileUI::Rect canvas() const;
		MobileUI::Rect picture() const;
		Point<int16_t> on_screen(Point<int16_t> image_pos) const;
		MobileUI::Rect list() const;
		MobileUI::Rect field_rect() const;
		MobileUI::Rect back_rect() const;
		MobileUI::Rect go_rect(const Row& row) const;
		std::vector<Row> rows() const;

		int16_t width;
		std::string image;
		std::string parent;
		std::string region;
		Texture base;
		std::vector<Spot> spots;
		std::vector<Link> links;
		int32_t selected = -1;
		int32_t here = 0;

		Textfield field;
		std::string query;
		std::vector<Result> results;
	};
}
