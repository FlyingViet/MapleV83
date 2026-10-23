//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
//
// The iPhone's own panels (stats, skills, touch bindings, menu): flat dark
// cards, rounded corners, finger-sized targets and sharp text, drawn with
// GraphicsGL primitives instead of v83's window art.
#pragma once

#include "UIElement.h"

#include "../Graphics/Text.h"
#include "../Graphics/Texture.h"

#include <chrono>
#include <string>

namespace ms
{
	namespace MobileUI
	{
		struct RGBA
		{
			float r, g, b, a;

			constexpr RGBA alpha(float k) const { return { r, g, b, a * k }; }
			constexpr RGBA mix(const RGBA& o, float t) const
			{
				return { r + (o.r - r) * t, g + (o.g - g) * t, b + (o.b - b) * t, a + (o.a - a) * t };
			}
		};

		namespace Theme
		{
			constexpr RGBA PANEL = { 0.070f, 0.082f, 0.112f, 0.97f };
			constexpr RGBA CARD = { 0.125f, 0.145f, 0.195f, 1.0f };
			constexpr RGBA CARD_HI = { 0.180f, 0.210f, 0.275f, 1.0f };
			constexpr RGBA TRACK = { 0.215f, 0.240f, 0.300f, 1.0f };
			constexpr RGBA LINE = { 1.0f, 1.0f, 1.0f, 0.07f };
			constexpr RGBA ACCENT = { 1.000f, 0.600f, 0.200f, 1.0f };
			constexpr RGBA BLUE = { 0.330f, 0.600f, 1.000f, 1.0f };
			constexpr RGBA GREEN = { 0.300f, 0.790f, 0.470f, 1.0f };
			constexpr RGBA RED = { 0.930f, 0.350f, 0.350f, 1.0f };
			constexpr RGBA GOLD = { 1.000f, 0.820f, 0.300f, 1.0f };
			constexpr RGBA PURPLE = { 0.690f, 0.520f, 1.000f, 1.0f };
			constexpr RGBA TEXT = { 0.960f, 0.970f, 1.000f, 1.0f };
			constexpr RGBA MUTED = { 0.640f, 0.680f, 0.770f, 1.0f };
			constexpr RGBA FAINT = { 0.430f, 0.470f, 0.560f, 1.0f };
			constexpr RGBA HP = { 0.940f, 0.330f, 0.360f, 1.0f };
			constexpr RGBA MP = { 0.300f, 0.560f, 1.000f, 1.0f };
			constexpr RGBA EXP = { 0.980f, 0.780f, 0.250f, 1.0f };
		}

		// Text sizes, in logical pixels as drawn.
		enum class Font
		{
			SMALL,		// 11 regular
			SMALL_BOLD,	// 11 bold
			BODY,		// 13 regular
			BODY_BOLD,	// 13 bold
			TITLE,		// 16 bold
			HEADLINE	// 20 bold
		};

		using Rect = Rectangle<int16_t>;

		Rect rect(int16_t x, int16_t y, int16_t w, int16_t h);
		Rect inset(const Rect& r, int16_t dx, int16_t dy);
		Point<int16_t> centre(const Rect& r);

		void fill(const Rect& r, RGBA color, int16_t radius = 0);

		// pos is the top-left (LEFT), top-centre (CENTER) or top-right (RIGHT)
		// of the first line; maxwidth wraps.
		void text(const std::string& str, Font font, RGBA color, Point<int16_t> pos,
			Text::Alignment align = Text::Alignment::LEFT, int16_t maxwidth = 0);
		// Vertically centred in r.
		void text_in(const std::string& str, Font font, RGBA color, const Rect& r,
			Text::Alignment align = Text::Alignment::CENTER, int16_t pad = 0);
		int16_t text_width(const std::string& str, Font font);
		int16_t text_height(const std::string& str, Font font, int16_t maxwidth);
		int16_t line_height(Font font);

		enum class Style { PRIMARY, SECONDARY, DANGER, GHOST, SELECTED };

		void button(const Rect& r, const std::string& label, Style style, bool enabled, bool pressed,
			Font font = Font::BODY_BOLD);

		enum class Glyph { PLUS, MINUS, CLOSE, UP, DOWN, LEFT, RIGHT };

		void glyph(const Rect& r, Glyph g, RGBA color, int16_t size);
		void icon_button(const Rect& r, Glyph g, Style style, bool enabled, bool pressed);

		// A track with a filled part and an optional caption on top.
		void bar(const Rect& r, float fraction, RGBA color, const std::string& left = "",
			const std::string& right = "");

		// An item or skill icon stretched into r.
		void icon(const Texture& texture, const Rect& r, float opacity = 1.0f);

		// str cut to fit maxwidth, with an ellipsis.
		std::string ellipsize(const std::string& str, Font font, int16_t maxwidth);

		// True on the iPhone (or with MV83_MOBILE_UI set): these panels stand
		// in for v83's stat window, skill book, key config and menu.
		bool enabled();

		void open_stats();
		void open_skills();
		void open_keys();
		void open_items();
		void open_equip();
		void open_menu();

		// Opens or closes the chat window (it replaces v83's chat line).
		void toggle_chat();

		// The store (the server opens it as a shop: @store) and, for GMs, the
		// admin settings.
		void open_store();
		void open_admin();

		// Always open (never toggle shut): the quest journal and the friends
		// screen on its buddies (false) or party (true) tab.
		void open_quests();
		void open_social(bool party);
		// The world map, on the player's region; takes the whole screen.
		void open_world_map();
		void open_channels();
		void open_options();
		void open_inbox();

		// v83's status bar becomes a see-through HUD in the top-left corner:
		// level, HP, MP and EXP, below the strip of the server's scrolling
		// notice (23 px). The minimap and the chat window start below it.
		constexpr int16_t HUD_LEFT = 8;
		constexpr int16_t HUD_TOP = 29;
		constexpr int16_t HUD_WIDTH = 214;
		constexpr int16_t HUD_BOTTOM = 108;

		// Where the touch controls put the chat button, in game pixels. The
		// chat window it opens reaches down to it (the button hides while the
		// window shows). Empty until the controls lay out.
		void set_chat_button(const Rect& r);
		Rect chat_button();

		// The dimmed card a panel sits on, with its title and close button.
		void panel(const Rect& r, const std::string& title, const std::string& subtitle);
		Rect panel_close(const Rect& r);
	}

	// A docked mobile panel: every tap, hold and drag is resolved here into
	// targets the subclass names, so the panels only lay out and react.
	class UIMobilePanel : public UIElement
	{
	public:
		void update() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		void remove_cursor() override;
		bool captures_hold() const override { return true; }

		// Every mobile panel is the full height minus a margin, so it docks
		// at scale 1 and its text stays sharp.
		static constexpr int16_t HEIGHT = 588;

	protected:
		enum class Edge { LEFT, RIGHT };

		UIMobilePanel(Edge edge, int16_t width);

		static constexpr int32_t NO_TARGET = -1;
		static constexpr int32_t CLOSE_TARGET = -2;

		// Targets are subclass codes; CLOSE_TARGET is the panel's close button.
		virtual int32_t hit(Point<int16_t> pos) const = 0;
		virtual void activate(int32_t target) = 0;
		virtual bool repeats(int32_t target) const { return false; }

		// The list that drags scroll, if any.
		virtual Rectangle<int16_t> scroll_area() const { return {}; }
		virtual int16_t scroll_content() const { return 0; }

		int32_t pressed_target() const;
		int16_t scroll_max() const;
		void clamp_scroll();

		MobileUI::Rect frame() const;

		int16_t scroll = 0;

	private:
		bool tracking = false;
		bool dragging = false;
		bool repeated = false;
		int32_t target = NO_TARGET;
		Point<int16_t> start;
		Point<int16_t> last;
		std::chrono::steady_clock::time_point pressed_at;
		int32_t next_repeat_ms = 0;
		uint32_t press = 0;
	};
}
