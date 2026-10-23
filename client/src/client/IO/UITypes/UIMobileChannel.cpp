//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileChannel.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/ChannelLoadData.h"
#include "../../Net/Packets/GameplayPackets.h"

#include <algorithm>
#include <string>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 360;
		constexpr int16_t PAD = 16;
		constexpr int16_t TOP = 78;
		constexpr int16_t CELL_H = 70;
		constexpr int16_t GAP = 8;
		constexpr int16_t COLS = 2;

		constexpr int32_t T_CHANNEL = 100;	// + channel
	}

	UIMobileChannel::UIMobileChannel() : UIMobilePanel(Edge::LEFT, W) {}

	uint8_t UIMobileChannel::count() const
	{
		uint8_t n = ChannelLoadData::get().get_count(static_cast<int8_t>(Configuration::get().get_worldid()));

		return n > 0 ? n : 1;
	}

	Rect UIMobileChannel::channel_rect(uint8_t channel) const
	{
		Rect area = scroll_area();
		int16_t w = static_cast<int16_t>((area.width() - GAP * (COLS - 1)) / COLS);
		int16_t col = static_cast<int16_t>(channel % COLS);
		int16_t row = static_cast<int16_t>(channel / COLS);

		return rect(static_cast<int16_t>(area.left() + col * (w + GAP)), static_cast<int16_t>(area.top() + row * (CELL_H + GAP) - scroll), w, CELL_H);
	}

	Rectangle<int16_t> UIMobileChannel::scroll_area() const
	{
		Rect f = frame();

		return rect(static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.top() + TOP), W - PAD * 2,
			static_cast<int16_t>(f.height() - TOP - PAD));
	}

	int16_t UIMobileChannel::scroll_content() const
	{
		int16_t rows = static_cast<int16_t>((count() + COLS - 1) / COLS);

		return static_cast<int16_t>(rows * (CELL_H + GAP));
	}

	int32_t UIMobileChannel::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		if (!scroll_area().contains(pos))
			return NO_TARGET;

		for (uint8_t ch = 0; ch < count(); ch++)
			if (channel_rect(ch).contains(pos))
				return T_CHANNEL + ch;

		return NO_TARGET;
	}

	void UIMobileChannel::activate(int32_t target)
	{
		if (target < T_CHANNEL)
			return;

		uint8_t channel = static_cast<uint8_t>(target - T_CHANNEL);

		if (channel == Configuration::get().get_channelid() || channel >= count())
			return;

		Sound(Sound::Name::BUTTONCLICK).play();

		// The game reloads on the new channel; the lobby keeps it next time.
		Configuration::get().set_channelid(channel);
		deactivate();
		UI::get().disable();
		ChangeChannelPacket(channel).dispatch();
	}

	void UIMobileChannel::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();
		uint8_t current = Configuration::get().get_channelid();
		int8_t world = static_cast<int8_t>(Configuration::get().get_worldid());

		panel(f, "Channels", "You're on Channel " + std::to_string(current + 1) + "  \xC2\xB7  tap one to move");
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		Rect area = scroll_area();
		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		for (uint8_t ch = 0; ch < count(); ch++)
		{
			Rect r = channel_rect(ch);

			if (r.bottom() < area.top() || r.top() > area.bottom())
				continue;

			bool here = ch == current;
			int32_t id = T_CHANNEL + ch;
			fill(r, here ? Theme::ACCENT.alpha(0.22f) : (pressed == id ? Theme::CARD_HI : Theme::CARD), 14);

			if (here)
				fill(rect(r.left(), static_cast<int16_t>(r.top() + 14), 4, static_cast<int16_t>(CELL_H - 28)), Theme::ACCENT, 2);

			int32_t load = ChannelLoadData::get().get_load(world, ch) & 0xFFFF;
			float fraction = std::clamp(load / 1200.0f, 0.0f, 1.0f);
			const char* how = here ? "You're here" : fraction < 0.35f ? "Quiet" : fraction < 0.8f ? "Busy" : "Full";

			text("Channel " + std::to_string(ch + 1), Font::BODY_BOLD, here ? Theme::ACCENT : Theme::TEXT,
				Point<int16_t>(static_cast<int16_t>(r.left() + 14), static_cast<int16_t>(r.top() + 12)));
			text(how, Font::SMALL, here ? Theme::ACCENT : Theme::MUTED, Point<int16_t>(static_cast<int16_t>(r.left() + 14), static_cast<int16_t>(r.top() + 34)));

			Rect track = rect(static_cast<int16_t>(r.left() + 14), static_cast<int16_t>(r.bottom() - 12), static_cast<int16_t>(r.width() - 28), 4);
			fill(track, Theme::TRACK, 2);
			fill(rect(track.left(), track.top(), static_cast<int16_t>(std::max(4.0f, track.width() * fraction)), 4),
				fraction < 0.35f ? Theme::GREEN : fraction < 0.8f ? Theme::GOLD : Theme::RED, 2);
		}

		GraphicsGL::get().end_clip();
	}

	UIElement::Type UIMobileChannel::get_type() const
	{
		return TYPE;
	}
}
