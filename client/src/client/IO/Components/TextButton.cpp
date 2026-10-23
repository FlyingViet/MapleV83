//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "TextButton.h"

#include "../../Graphics/GraphicsGL.h"

namespace ms
{
	TextButton::TextButton(Point<int16_t> pos, Point<int16_t> dim, const std::string& text)
	{
		position = pos;
		dimension = dim;
		state = Button::State::NORMAL;
		active = true;
		label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, text);
	}

	void TextButton::draw(Point<int16_t> parentpos) const
	{
		if (!active)
			return;

		Point<int16_t> p = position + parentpos;
		int16_t w = dimension.x();
		int16_t h = dimension.y();

		// v83-style orange plate with a darker rim; brighter while hovered/held
		bool lit = state == Button::State::MOUSEOVER || state == Button::State::PRESSED;
		bool off = state == Button::State::DISABLED;
		float r = off ? 0.55f : (lit ? 0.98f : 0.90f);
		float g = off ? 0.55f : (lit ? 0.58f : 0.45f);
		float b = off ? 0.55f : (lit ? 0.22f : 0.12f);

		GraphicsGL::get().drawrectangle(p.x(), p.y(), w, h, 0.45f, 0.20f, 0.05f, 1.0f);
		GraphicsGL::get().drawrectangle(p.x() + 1, p.y() + 1, w - 2, h - 2, r, g, b, 1.0f);
		GraphicsGL::get().drawrectangle(p.x() + 1, p.y() + 1, w - 2, h / 2 - 1, 1.0f, 1.0f, 1.0f, 0.18f);

		label.draw(Point<int16_t>(p.x() + w / 2, p.y() + h / 2 - 8));
	}

	Rectangle<int16_t> TextButton::bounds(Point<int16_t> parentpos) const
	{
		Point<int16_t> absp = position + parentpos;

		return Rectangle<int16_t>(absp, absp + dimension);
	}

	int16_t TextButton::width() const
	{
		return dimension.x();
	}

	Point<int16_t> TextButton::origin() const
	{
		return Point<int16_t>();
	}
}
