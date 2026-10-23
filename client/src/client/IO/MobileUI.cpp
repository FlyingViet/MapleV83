//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "MobileUI.h"

#include "UI.h"
#include "UITypes/UIChatBar.h"
#include "UITypes/UIMiniMap.h"
#include "UITypes/UIMobileAdmin.h"
#include "UITypes/UIMobileStore.h"

#include "../Net/Packets/MessagingPackets.h"

#include "UITypes/UIMobileEquip.h"
#include "UITypes/UIMobileInventory.h"
#include "UITypes/UIMobileKeyConfig.h"
#include "UITypes/UIMobileMenu.h"
#include "UITypes/UIMobileQuests.h"
#include "UITypes/UIMobileSocial.h"
#include "UITypes/UIMobileWorldMap.h"
#include "UITypes/UIMobileChannel.h"
#include "UITypes/UIMobileInbox.h"
#include "UITypes/UIMobileOptions.h"
#include "UITypes/UIMobileSkills.h"
#include "UITypes/UIMobileStats.h"

#include "../Gameplay/Stage.h"

#include "../Audio/Audio.h"
#include "../Constants.h"
#include "../Graphics/GraphicsGL.h"

#include <algorithm>
#include <iostream>
#include <cmath>
#include <cstdlib>

namespace ms
{
	namespace MobileUI
	{
		namespace
		{
			Text::Font raster(Font font)
			{
				switch (font)
				{
					case Font::SMALL: return Text::Font::H11M;
					case Font::SMALL_BOLD: return Text::Font::H11B;
					case Font::BODY: return Text::Font::H13M;
					case Font::BODY_BOLD: return Text::Font::H13B;
					case Font::TITLE: return Text::Font::H16B;
					case Font::HEADLINE: return Text::Font::H20B;
				}

				return Text::Font::H13M;
			}

			Text make(const std::string& str, Font font, Text::Alignment align, int16_t maxwidth)
			{
				return Text(raster(font), align, Color::Name::WHITE, str,
					static_cast<uint16_t>(std::max<int>(0, maxwidth * 2)), true);
			}

			// How far below the anchor the glyphs of a line start, and how tall
			// capitals are - for centring text in a box.
			// GraphicsGL puts a line's baseline one linespace (1.35 x the
			// raster size + 1) below the anchor, and capitals are ~0.7 of the
			// raster size; both halve on screen. Measured on SMALL: 7 and 8.
			int16_t cap_top(Font font)
			{
				switch (font)
				{
					case Font::SMALL:
					case Font::SMALL_BOLD: return 7;
					case Font::BODY:
					case Font::BODY_BOLD: return 9;
					case Font::TITLE: return 11;
					case Font::HEADLINE: return 13;
				}

				return 9;
			}

			int16_t cap_height(Font font)
			{
				switch (font)
				{
					case Font::SMALL:
					case Font::SMALL_BOLD: return 8;
					case Font::BODY:
					case Font::BODY_BOLD: return 9;
					case Font::TITLE: return 11;
					case Font::HEADLINE: return 14;
				}

				return 9;
			}

			void solid(const Rect& r, RGBA c)
			{
				GraphicsGL::get().drawrectangle(r.left(), r.top(), r.width(), r.height(), c.r, c.g, c.b, c.a);
			}
		}

		Rect rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rect(x, static_cast<int16_t>(x + w), y, static_cast<int16_t>(y + h));
		}

		Rect inset(const Rect& r, int16_t dx, int16_t dy)
		{
			return Rect(static_cast<int16_t>(r.left() + dx), static_cast<int16_t>(r.right() - dx),
				static_cast<int16_t>(r.top() + dy), static_cast<int16_t>(r.bottom() - dy));
		}

		Point<int16_t> centre(const Rect& r)
		{
			return Point<int16_t>(static_cast<int16_t>((r.left() + r.right()) / 2),
				static_cast<int16_t>((r.top() + r.bottom()) / 2));
		}

		void fill(const Rect& r, RGBA c, int16_t radius)
		{
			if (c.a <= 0.0f)
				return;

			GraphicsGL::get().drawroundrect(r.left(), r.top(), r.width(), r.height(), radius, c.r, c.g, c.b, c.a);
		}

		void text(const std::string& str, Font font, RGBA c, Point<int16_t> pos, Text::Alignment align, int16_t maxwidth)
		{
			if (str.empty())
				return;

			Text t = make(str, font, align, maxwidth);
			t.draw(DrawArgument(pos, pos, Point<int16_t>(0, 0), 0.5f, 0.5f, Color(c.r, c.g, c.b, c.a), 0.0f));
		}

		void text_in(const std::string& str, Font font, RGBA c, const Rect& r, Text::Alignment align, int16_t pad)
		{
			int16_t y = static_cast<int16_t>(r.top() + (r.height() - cap_height(font)) / 2 - cap_top(font));
			int16_t x = align == Text::Alignment::LEFT ? static_cast<int16_t>(r.left() + pad)
				: align == Text::Alignment::RIGHT ? static_cast<int16_t>(r.right() - pad)
				: centre(r).x();

			text(str, font, c, Point<int16_t>(x, y), align);
		}

		int16_t text_width(const std::string& str, Font font)
		{
			return static_cast<int16_t>(make(str, font, Text::Alignment::LEFT, 0).width() / 2);
		}

		int16_t text_height(const std::string& str, Font font, int16_t maxwidth)
		{
			return static_cast<int16_t>(make(str, font, Text::Alignment::LEFT, maxwidth).height() / 2);
		}

		int16_t line_height(Font font)
		{
			return static_cast<int16_t>(make("Ag", font, Text::Alignment::LEFT, 0).height() / 2);
		}

		void glyph(const Rect& r, Glyph g, RGBA c, int16_t size)
		{
			Point<int16_t> m = centre(r);
			int16_t t = std::max<int16_t>(2, static_cast<int16_t>(size / 6));
			GraphicsGL& gl = GraphicsGL::get();

			switch (g)
			{
				case Glyph::PLUS:
					fill(rect(m.x() - size / 2, m.y() - t / 2, size, t), c, t / 2);
					fill(rect(m.x() - t / 2, m.y() - size / 2, t, size), c, t / 2);
					break;
				case Glyph::MINUS:
					fill(rect(m.x() - size / 2, m.y() - t / 2, size, t), c, t / 2);
					break;
				case Glyph::CLOSE:
				{
					int16_t len = static_cast<int16_t>(size * 1.25f);
					gl.drawrotatedrect(m.x(), m.y(), len, t, 0.785398f, c.r, c.g, c.b, c.a);
					gl.drawrotatedrect(m.x(), m.y(), len, t, -0.785398f, c.r, c.g, c.b, c.a);
					break;
				}
				case Glyph::UP:
				case Glyph::DOWN:
				{
					int16_t len = static_cast<int16_t>(size * 0.72f);
					int16_t dx = static_cast<int16_t>(size * 0.24f);
					float a = g == Glyph::UP ? 0.785398f : -0.785398f;
					int16_t y = static_cast<int16_t>(m.y() + (g == Glyph::UP ? 0 : 0));
					gl.drawrotatedrect(m.x() - dx, y, len, t, -a, c.r, c.g, c.b, c.a);
					gl.drawrotatedrect(m.x() + dx, y, len, t, a, c.r, c.g, c.b, c.a);
					break;
				}
				case Glyph::LEFT:
				case Glyph::RIGHT:
				{
					// A chevron: two strokes meeting at the point it shows.
					int16_t len = static_cast<int16_t>(size * 0.72f);
					int16_t dy = static_cast<int16_t>(size * 0.24f);
					float a = g == Glyph::LEFT ? -0.785398f : 0.785398f;
					gl.drawrotatedrect(m.x(), m.y() - dy, len, t, a, c.r, c.g, c.b, c.a);
					gl.drawrotatedrect(m.x(), m.y() + dy, len, t, -a, c.r, c.g, c.b, c.a);
					break;
				}
			}
		}

		namespace
		{
			struct Look
			{
				RGBA bg;
				RGBA fg;
			};

			Look look(Style style)
			{
				switch (style)
				{
					case Style::PRIMARY: return { Theme::ACCENT, { 0.13f, 0.08f, 0.03f, 1.0f } };
					case Style::SECONDARY: return { Theme::CARD_HI, Theme::TEXT };
					case Style::DANGER: return { { 0.93f, 0.35f, 0.35f, 0.16f }, { 1.0f, 0.55f, 0.55f, 1.0f } };
					case Style::GHOST: return { { 1.0f, 1.0f, 1.0f, 0.06f }, Theme::MUTED };
					case Style::SELECTED: return { { 0.20f, 0.42f, 0.82f, 1.0f }, Theme::TEXT };
				}

				return { Theme::CARD_HI, Theme::TEXT };
			}
		}

		void button(const Rect& r, const std::string& label, Style style, bool enabled, bool pressed, Font font)
		{
			Look l = look(style);
			float k = enabled ? 1.0f : 0.38f;
			RGBA bg = pressed && enabled ? l.bg.mix({ 1.0f, 1.0f, 1.0f, l.bg.a }, 0.18f) : l.bg;

			fill(r, bg.alpha(k), static_cast<int16_t>(std::min<int>(r.height() / 2, 12)));
			text_in(label, font, l.fg.alpha(k), r);
		}

		void icon_button(const Rect& r, Glyph g, Style style, bool enabled, bool pressed)
		{
			Look l = look(style);
			float k = enabled ? 1.0f : 0.32f;
			RGBA bg = pressed && enabled ? l.bg.mix({ 1.0f, 1.0f, 1.0f, l.bg.a }, 0.22f) : l.bg;

			fill(r, bg.alpha(k), static_cast<int16_t>(std::min(r.width(), r.height()) / 2));
			glyph(r, g, l.fg.alpha(k), static_cast<int16_t>(std::min(r.width(), r.height()) * 0.42f));
		}

		void bar(const Rect& r, float fraction, RGBA c, const std::string& left, const std::string& right)
		{
			int16_t radius = static_cast<int16_t>(r.height() / 2);
			fill(r, Theme::TRACK, radius);

			fraction = std::clamp(fraction, 0.0f, 1.0f);

			if (fraction > 0.0f)
			{
				int16_t w = static_cast<int16_t>(std::max<float>(r.height(), r.width() * fraction));
				fill(rect(r.left(), r.top(), w, r.height()), c, radius);
			}

			if (!left.empty())
				text_in(left, Font::SMALL_BOLD, Theme::TEXT, r, Text::Alignment::LEFT, 8);

			if (!right.empty())
				text_in(right, Font::SMALL_BOLD, Theme::TEXT, r, Text::Alignment::RIGHT, 8);
		}

		void icon(const Texture& texture, const Rect& r, float opacity)
		{
			if (!texture.is_valid())
				return;

			Point<int16_t> p = Point<int16_t>(r.left(), r.top()) + texture.get_origin();
			texture.draw(DrawArgument(p, p, Point<int16_t>(r.width(), r.height()), 1.0f, 1.0f, opacity, 0.0f));
		}

		std::string ellipsize(const std::string& str, Font font, int16_t maxwidth)
		{
			if (text_width(str, font) <= maxwidth)
				return str;

			// Back off whole UTF-8 characters until it fits with the dots.
			std::string cut = str;

			while (!cut.empty())
			{
				do
				{
					cut.pop_back();
				} while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80);

				if (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0x80))
					cut.pop_back();

				std::string candidate = cut + "...";

				if (text_width(candidate, font) <= maxwidth)
					return candidate;
			}

			return "...";
		}

		bool enabled()
		{
#ifdef PLATFORM_IOS
			return true;
#else
			static const bool forced = std::getenv("MV83_MOBILE_UI") != nullptr;

			return forced;
#endif
		}

		void open_stats()
		{
			UI::get().emplace<UIMobileStats>(Stage::get().get_player().get_stats());
		}

		void open_skills()
		{
			Player& player = Stage::get().get_player();
			UI::get().emplace<UIMobileSkills>(player.get_stats(), player.get_skills());
		}

		void open_keys()
		{
			UI::get().emplace<UIMobileKeyConfig>();
		}

		void open_items()
		{
			UI::get().emplace<UIMobileInventory>(Stage::get().get_player().get_inventory());
		}

		void open_equip()
		{
			Player& player = Stage::get().get_player();
			UI::get().emplace<UIMobileEquip>(player.get_inventory(), player.get_stats());
		}

		void open_menu()
		{
			UI::get().emplace<UIMobileMenu>();
		}

		void open_store()
		{
			if (auto store = UI::get().get_element<UIMobileStore>())
			{
				if (store->is_active())
				{
					store->deactivate();
					return;
				}
			}

			GeneralChatPacket("@store", false).dispatch();
		}

		void open_admin()
		{
			UI::get().emplace<UIMobileAdmin>();
		}

		void open_quests()
		{
			if (auto quests = UI::get().get_element<UIMobileQuests>())
			{
				if (!quests->is_active())
				{
					Sound(Sound::Name::MENUUP).play();
					quests->makeactive();
				}

				return;
			}

			UI::get().emplace<UIMobileQuests>(Stage::get().get_player().get_quests());
		}

		void open_world_map()
		{
			// It covers both sides: whatever is docked on the right goes too.
			auto hide = [](UIElement* element)
			{
				if (element && element->is_active())
					element->deactivate();
			};

			hide(UI::get().get_element<UIMobileInventory>().get());
			hide(UI::get().get_element<UIMobileMenu>().get());
			hide(UI::get().get_element<UIMobileStore>().get());

			if (auto chatbar = UI::get().get_element<UIChatBar>())
				if (chatbar->is_chatopen())
					chatbar->toggle_chat();

			if (auto minimap = UI::get().get_element<UIMiniMap>())
				minimap->shrink();

			if (auto map = UI::get().get_element<UIMobileWorldMap>())
			{
				if (!map->is_active())
				{
					Sound(Sound::Name::WORLDMAPOPEN).play();
					map->makeactive();
				}

				map->show_here();
				return;
			}

			Sound(Sound::Name::WORLDMAPOPEN).play();
			UI::get().emplace<UIMobileWorldMap>();
		}

		void open_inbox()
		{
			auto panel = UI::get().get_element<UIMobileInbox>();

			if (!panel || !panel->is_active())
				UI::get().emplace<UIMobileInbox>();
		}

		void open_channels()
		{
			auto panel = UI::get().get_element<UIMobileChannel>();

			if (!panel || !panel->is_active())
				UI::get().emplace<UIMobileChannel>();
		}

		void open_options()
		{
			auto panel = UI::get().get_element<UIMobileOptions>();

			if (!panel || !panel->is_active())
				UI::get().emplace<UIMobileOptions>();
		}

		void open_social(bool party)
		{
			UIMobileSocial::Tab tab = party ? UIMobileSocial::Tab::PARTY : UIMobileSocial::Tab::BUDDIES;

			if (auto social = UI::get().get_element<UIMobileSocial>())
			{
				if (!social->is_active())
					Sound(Sound::Name::MENUUP).play();

				social->show(tab);
				return;
			}

			UI::get().emplace<UIMobileSocial>(tab);
		}

		void toggle_chat()
		{
			if (auto chatbar = UI::get().get_element<UIChatBar>())
			{
				// The large minimap gives the left side to the chat window.
				if (!chatbar->is_chatopen())
					if (auto minimap = UI::get().get_element<UIMiniMap>())
						minimap->shrink();

				chatbar->toggle_chat();
			}
		}

		namespace
		{
			Rect chat_button_rect;
		}

		void set_chat_button(const Rect& r)
		{
			chat_button_rect = r;
		}

		Rect chat_button()
		{
			return chat_button_rect;
		}

		Rect panel_close(const Rect& r)
		{
			return rect(static_cast<int16_t>(r.right() - 48), static_cast<int16_t>(r.top() + 12), 36, 36);
		}

		void panel(const Rect& r, const std::string& title, const std::string& subtitle)
		{
			fill(r, Theme::PANEL, 18);
			solid(rect(r.left(), r.top() + 64, r.width(), 1), Theme::LINE);

			text(title, Font::HEADLINE, Theme::TEXT, Point<int16_t>(r.left() + 18, r.top() + 13));

			if (!subtitle.empty())
				text(subtitle, Font::SMALL, Theme::MUTED, Point<int16_t>(r.left() + 18, r.top() + 39));
		}
	}

	// ---- UIMobilePanel ------------------------------------------------------

	UIMobilePanel::UIMobilePanel(Edge edge, int16_t width) : UIElement(Point<int16_t>(0, 0), Point<int16_t>(width, HEIGHT))
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t x = edge == Edge::LEFT ? 6 : static_cast<int16_t>(vw - 6 - width);

		position = Point<int16_t>(x, 6);
	}

	MobileUI::Rect UIMobilePanel::frame() const
	{
		return MobileUI::rect(position.x(), position.y(), dimension.x(), dimension.y());
	}

	int32_t UIMobilePanel::pressed_target() const
	{
		return tracking && !dragging ? target : NO_TARGET;
	}

	int16_t UIMobilePanel::scroll_max() const
	{
		Rectangle<int16_t> area = scroll_area();

		return static_cast<int16_t>(std::max(0, scroll_content() - area.height()));
	}

	void UIMobilePanel::clamp_scroll()
	{
		scroll = std::clamp<int16_t>(scroll, 0, scroll_max());
	}

	void UIMobilePanel::update()
	{
		if (!tracking)
			return;

		// The finger lifted somewhere else.
		if (!UI::get().is_mouse_held())
		{
			tracking = false;
			dragging = false;
			target = NO_TARGET;
			return;
		}

		// Wall-clock time: updates come once a frame, and frames slow down.
		int32_t held_ms = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - pressed_at).count());

		// Holding a stepper repeats it, faster the longer it is held.
		if (!dragging && target != NO_TARGET && repeats(target) && held_ms >= next_repeat_ms)
		{
			activate(target);
			repeated = true;
			next_repeat_ms = held_ms + (held_ms > 1600 ? 45 : 110);
		}
	}

	Cursor::State UIMobilePanel::send_cursor(bool down, Point<int16_t> pos)
	{
		if (down)
		{
			uint32_t serial = UI::get().get_press_serial();

			if (!tracking || serial != press)
			{
				tracking = true;
				dragging = false;
				repeated = false;
				press = serial;
				start = pos;
				last = pos;
				pressed_at = std::chrono::steady_clock::now();
				next_repeat_ms = 380;
				target = hit(pos);

				return Cursor::State::CLICKING;
			}

			// A vertical move over the list turns the press into a scroll.
			if (!dragging && scroll_max() > 0 && scroll_area().contains(start)
				&& std::abs(pos.y() - start.y()) > 8)
			{
				dragging = true;
			}

			if (dragging)
			{
				scroll = static_cast<int16_t>(scroll - (pos.y() - last.y()));
				clamp_scroll();
			}

			last = pos;

			return Cursor::State::CLICKING;
		}

		if (tracking)
		{
			int32_t t = target;
			bool tap = !dragging && !repeated && t != NO_TARGET && hit(pos) == t;

			tracking = false;
			dragging = false;
			target = NO_TARGET;

			if (tap)
			{
				if (t == CLOSE_TARGET)
				{
					Sound(Sound::Name::MENUDOWN).play();
					deactivate();
				}
				else
				{
					activate(t);
				}
			}
		}

		return Cursor::State::IDLE;
	}

	void UIMobilePanel::send_scroll(double yoffset)
	{
		scroll = static_cast<int16_t>(scroll - yoffset * 48);
		clamp_scroll();
	}

	void UIMobilePanel::send_key(int32_t, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			Sound(Sound::Name::MENUDOWN).play();
			deactivate();
		}
	}

	void UIMobilePanel::remove_cursor()
	{
		if (tracking && !UI::get().is_mouse_held())
		{
			tracking = false;
			dragging = false;
			target = NO_TARGET;
		}
	}
}
