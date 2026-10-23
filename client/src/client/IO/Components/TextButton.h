//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Button.h"

#include "../../Graphics/Text.h"

namespace ms
{
	// A small drawn button with a text label, for actions the v83 artwork has
	// no button bitmap for (MapleV83's AP/SP RESET).
	class TextButton : public Button
	{
	public:
		TextButton(Point<int16_t> position, Point<int16_t> dimensions, const std::string& label);

		void draw(Point<int16_t> parentpos) const override;
		void update() override {}
		Rectangle<int16_t> bounds(Point<int16_t> parentpos) const override;
		int16_t width() const override;
		Point<int16_t> origin() const override;
		Cursor::State send_cursor(bool, Point<int16_t>) override { return Cursor::State::IDLE; }

	private:
		Point<int16_t> dimension;
		Text label;
	};
}
