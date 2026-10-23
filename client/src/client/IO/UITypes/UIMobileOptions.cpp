//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileOptions.h"

#include "UIMobileConfirm.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Resume.h"

#include <algorithm>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t W = 380;
		constexpr int16_t PAD = 16;
		constexpr int16_t TOP = 76;
		constexpr int16_t HEADER_H = 30;
		constexpr int16_t ROW_H = 58;
		constexpr int16_t CHOICE_H = 82;
		constexpr int16_t GAP = 6;

		enum Id : int
		{
			MUSIC, SOUND,
			SHOW_FPS, SHAKE,
			GRAPHICS, EFFECTS,
			HP_WARN, MP_WARN,
			WHISPER, PARTY, BUDDY_CHAT, CHAT_INVITE, GUILD_INVITE,
			KEYS, CHARACTER
		};

		// Targets: row id * 10 + part.
		enum Part : int32_t { BODY = 0, MINUS = 1, PLUS = 2, CHOICE = 3 };

		const char* const QUALITY[] = { "Low", "Medium", "High" };
		constexpr uint8_t QUALITY_VALUES[] = { 20, 50, 100 };
		const char* const WARN[] = { "Off", "25%", "50%", "75%" };
		constexpr uint8_t WARN_VALUES[] = { 0, 25, 50, 75 };

		void save()
		{
			Configuration::get().save();
		}
	}

	UIMobileOptions::UIMobileOptions() : UIMobilePanel(Edge::LEFT, W) {}

	// ---- values -----------------------------------------------------------

	int UIMobileOptions::volume(int id) const
	{
		return id == MUSIC ? Setting<BGMVolume>::get().load() : Setting<SFXVolume>::get().load();
	}

	void UIMobileOptions::set_volume(int id, int value)
	{
		uint8_t v = static_cast<uint8_t>(std::clamp(value, 0, 100));

		if (id == MUSIC)
		{
			Setting<BGMVolume>::get().save(v);
			Music::set_bgmvolume(v);
		}
		else
		{
			Setting<SFXVolume>::get().save(v);
			Sound::set_sfxvolume(v);
			Sound(Sound::Name::BUTTONCLICK).play();
		}

		save();
	}

	bool UIMobileOptions::toggle_value(int id) const
	{
		switch (id)
		{
			case SHOW_FPS: return Setting<ShowFPS>::get().load();
			case SHAKE: return Setting<ScreenShake>::get().load();
			case WHISPER: return Setting<AllowWhisper>::get().load();
			case PARTY: return Setting<AllowPartyInvite>::get().load();
			case BUDDY_CHAT: return Setting<AllowFriendChat>::get().load();
			case CHAT_INVITE: return Setting<AllowChatInvite>::get().load();
			case GUILD_INVITE: return Setting<AllowGuildInvite>::get().load();
			default: return false;
		}
	}

	void UIMobileOptions::set_toggle(int id, bool value)
	{
		switch (id)
		{
			case SHOW_FPS: Setting<ShowFPS>::get().save(value); break;
			case SHAKE: Setting<ScreenShake>::get().save(value); break;
			case WHISPER: Setting<AllowWhisper>::get().save(value); break;
			case PARTY: Setting<AllowPartyInvite>::get().save(value); break;
			case BUDDY_CHAT: Setting<AllowFriendChat>::get().save(value); break;
			case CHAT_INVITE: Setting<AllowChatInvite>::get().save(value); break;
			case GUILD_INVITE: Setting<AllowGuildInvite>::get().save(value); break;
			default: return;
		}

		save();
	}

	int UIMobileOptions::choice_value(int id) const
	{
		uint8_t v = 0;

		switch (id)
		{
			case GRAPHICS: v = Setting<GraphicsQuality>::get().load(); break;
			case EFFECTS: v = Setting<EffectsQuality>::get().load(); break;
			case HP_WARN: v = Setting<HPWarning>::get().load(); break;
			case MP_WARN: v = Setting<MPWarning>::get().load(); break;
			default: return 0;
		}

		// The nearest choice to what is stored.
		const uint8_t* values = id == GRAPHICS || id == EFFECTS ? QUALITY_VALUES : WARN_VALUES;
		int count = id == GRAPHICS || id == EFFECTS ? 3 : 4;
		int best = 0;

		for (int i = 1; i < count; i++)
			if (std::abs(values[i] - v) < std::abs(values[best] - v))
				best = i;

		return best;
	}

	void UIMobileOptions::set_choice(int id, int index)
	{
		switch (id)
		{
			case GRAPHICS: Setting<GraphicsQuality>::get().save(QUALITY_VALUES[index]); break;
			case EFFECTS: Setting<EffectsQuality>::get().save(QUALITY_VALUES[index]); break;
			case HP_WARN: Setting<HPWarning>::get().save(WARN_VALUES[index]); break;
			case MP_WARN: Setting<MPWarning>::get().save(WARN_VALUES[index]); break;
			default: return;
		}

		save();
	}

	// ---- layout -----------------------------------------------------------

	std::vector<UIMobileOptions::Row> UIMobileOptions::rows() const
	{
		std::vector<Row> out;
		int16_t y = 0;

		auto add = [&](Kind kind, int id, const std::string& label, const std::string& detail = "")
		{
			int16_t h = kind == Kind::HEADER ? HEADER_H : kind == Kind::CHOICE ? CHOICE_H : ROW_H;
			out.push_back({ kind, id, label, detail, y, h });
			y = static_cast<int16_t>(y + h + (kind == Kind::HEADER ? 0 : GAP));
		};

		add(Kind::HEADER, -1, "SOUND");
		add(Kind::VOLUME, MUSIC, "Music");
		add(Kind::VOLUME, SOUND, "Sound effects");

		add(Kind::HEADER, -1, "DISPLAY");
		add(Kind::CHOICE, GRAPHICS, "Graphics", "Lower draws less scenery");
		add(Kind::CHOICE, EFFECTS, "Skill effects", "Lower shows fewer at once");
		add(Kind::TOGGLE, SHAKE, "Screen shake");
		add(Kind::TOGGLE, SHOW_FPS, "Show frame rate");

		add(Kind::HEADER, -1, "WARNINGS");
		add(Kind::CHOICE, HP_WARN, "Low HP warning", "Flashes when HP drops below");
		add(Kind::CHOICE, MP_WARN, "Low MP warning", "Flashes when MP drops below");

		add(Kind::HEADER, -1, "WHO CAN REACH YOU");
		add(Kind::TOGGLE, WHISPER, "Whispers");
		add(Kind::TOGGLE, PARTY, "Party invites");
		add(Kind::TOGGLE, BUDDY_CHAT, "Buddy chat");
		add(Kind::TOGGLE, CHAT_INVITE, "Chat room invites");
		add(Kind::TOGGLE, GUILD_INVITE, "Guild invites");

		add(Kind::HEADER, -1, "MORE");
		add(Kind::ACTION, KEYS, "Controls", "Rebind buttons and skills");
		add(Kind::ACTION, CHARACTER, "Change character", "Back to your character list");

		return out;
	}

	Rectangle<int16_t> UIMobileOptions::scroll_area() const
	{
		Rect f = frame();

		return rect(static_cast<int16_t>(f.left() + PAD), static_cast<int16_t>(f.top() + TOP), W - PAD * 2,
			static_cast<int16_t>(f.height() - TOP - 10));
	}

	int16_t UIMobileOptions::scroll_content() const
	{
		std::vector<Row> list = rows();

		return list.empty() ? 0 : static_cast<int16_t>(list.back().y + list.back().h + 10);
	}

	Rect UIMobileOptions::row_rect(const Row& row) const
	{
		Rectangle<int16_t> area = scroll_area();

		return rect(area.left(), static_cast<int16_t>(area.top() + row.y - scroll), area.width(), row.h);
	}

	Rect UIMobileOptions::minus_rect(const Row& row) const
	{
		Rect r = row_rect(row);

		return rect(static_cast<int16_t>(r.right() - 12 - 40 - 8 - 40), static_cast<int16_t>(r.top() + (r.height() - 40) / 2), 40, 40);
	}

	Rect UIMobileOptions::plus_rect(const Row& row) const
	{
		Rect r = row_rect(row);

		return rect(static_cast<int16_t>(r.right() - 12 - 40), static_cast<int16_t>(r.top() + (r.height() - 40) / 2), 40, 40);
	}

	Rect UIMobileOptions::choice_rect(const Row& row, int index, int count) const
	{
		Rect r = row_rect(row);
		int16_t gap = 6;
		int16_t w = static_cast<int16_t>((r.width() - 24 - gap * (count - 1)) / count);

		return rect(static_cast<int16_t>(r.left() + 12 + index * (w + gap)), static_cast<int16_t>(r.bottom() - 10 - 32), w, 32);
	}

	Rect UIMobileOptions::switch_rect(const Row& row) const
	{
		Rect r = row_rect(row);

		return rect(static_cast<int16_t>(r.right() - 14 - 52), static_cast<int16_t>(r.top() + (r.height() - 30) / 2), 52, 30);
	}

	// ---- input ------------------------------------------------------------

	int32_t UIMobileOptions::hit(Point<int16_t> pos) const
	{
		if (inset(panel_close(frame()), -6, -6).contains(pos))
			return CLOSE_TARGET;

		if (!scroll_area().contains(pos))
			return NO_TARGET;

		for (const Row& row : rows())
		{
			if (!row_rect(row).contains(pos))
				continue;

			int32_t base = row.id * 10;

			switch (row.kind)
			{
				case Kind::VOLUME:
					if (inset(minus_rect(row), -4, -4).contains(pos))
						return base + MINUS;

					if (inset(plus_rect(row), -4, -4).contains(pos))
						return base + PLUS;

					return NO_TARGET;
				case Kind::CHOICE:
				{
					int count = row.id == GRAPHICS || row.id == EFFECTS ? 3 : 4;

					for (int i = 0; i < count; i++)
						if (inset(choice_rect(row, i, count), -2, -4).contains(pos))
							return base + CHOICE + i;

					return NO_TARGET;
				}
				case Kind::TOGGLE:
				case Kind::ACTION:
					return base + BODY;
				default:
					return NO_TARGET;
			}
		}

		return NO_TARGET;
	}

	bool UIMobileOptions::repeats(int32_t target) const
	{
		int part = target % 10;

		return target >= 0 && target / 10 <= SOUND && (part == MINUS || part == PLUS);
	}

	void UIMobileOptions::activate(int32_t target)
	{
		if (target < 0)
			return;

		int id = target / 10;
		int part = target % 10;

		switch (id)
		{
			case MUSIC:
			case SOUND:
				set_volume(id, (volume(id) / 10 + (part == PLUS ? 1 : -1)) * 10);
				return;
			case GRAPHICS:
			case EFFECTS:
			case HP_WARN:
			case MP_WARN:
				Sound(Sound::Name::BUTTONCLICK).play();
				set_choice(id, part - CHOICE);
				return;
			case KEYS:
				Sound(Sound::Name::BUTTONCLICK).play();
				deactivate();
				open_keys();
				return;
			case CHARACTER:
				Sound(Sound::Name::BUTTONCLICK).play();
				UI::get().emplace<UIMobileConfirm>("Change character?", "You'll go back to your character list.", "Change", false,
					[]() { Resume::change_character(); });
				return;
			default:
				Sound(Sound::Name::BUTTONCLICK).play();
				set_toggle(id, !toggle_value(id));
				return;
		}
	}

	// ---- drawing ----------------------------------------------------------

	void UIMobileOptions::draw(float) const
	{
		Rect f = frame();
		int32_t pressed = pressed_target();

		panel(f, "Options", "Changes save right away");
		icon_button(panel_close(f), Glyph::CLOSE, Style::SECONDARY, true, pressed == CLOSE_TARGET);

		Rectangle<int16_t> area = scroll_area();
		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		for (const Row& row : rows())
		{
			Rect r = row_rect(row);

			if (r.bottom() < area.top() || r.top() > area.bottom())
				continue;

			int32_t base = row.id * 10;

			if (row.kind == Kind::HEADER)
			{
				text(row.label, Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(r.left() + 2), static_cast<int16_t>(r.top() + 8)));
				continue;
			}

			bool down = row.kind == Kind::TOGGLE || row.kind == Kind::ACTION ? pressed == base + BODY : false;
			fill(r, down ? Theme::CARD_HI : Theme::CARD, 14);

			int16_t lx = static_cast<int16_t>(r.left() + 16);

			switch (row.kind)
			{
				case Kind::VOLUME:
				{
					int v = volume(row.id);
					text(row.label, Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(lx, static_cast<int16_t>(r.top() + 9)));
					Rect minus = minus_rect(row);
					int16_t bw = static_cast<int16_t>(minus.left() - 14 - lx);
					MobileUI::bar(rect(lx, static_cast<int16_t>(r.top() + 34), bw, 10), v / 100.0f, row.id == MUSIC ? Theme::BLUE : Theme::ACCENT);
					text(v == 0 ? "Off" : std::to_string(v) + "%", Font::SMALL_BOLD, v == 0 ? Theme::FAINT : Theme::TEXT,
						Point<int16_t>(static_cast<int16_t>(lx + bw), static_cast<int16_t>(r.top() + 10)), Text::Alignment::RIGHT);
					icon_button(minus, Glyph::MINUS, Style::SECONDARY, v > 0, pressed == base + MINUS);
					icon_button(plus_rect(row), Glyph::PLUS, Style::PRIMARY, v < 100, pressed == base + PLUS);
					break;
				}
				case Kind::CHOICE:
				{
					int count = row.id == GRAPHICS || row.id == EFFECTS ? 3 : 4;
					int on = choice_value(row.id);
					text(row.label, Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(lx, static_cast<int16_t>(r.top() + 9)));
					text(row.detail, Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(r.right() - 14), static_cast<int16_t>(r.top() + 11)),
						Text::Alignment::RIGHT);

					for (int i = 0; i < count; i++)
					{
						const char* label = count == 3 ? QUALITY[i] : WARN[i];
						button(choice_rect(row, i, count), label, i == on ? Style::SELECTED : Style::SECONDARY, true,
							pressed == base + CHOICE + i, Font::SMALL_BOLD);
					}

					break;
				}
				case Kind::TOGGLE:
				{
					bool on = toggle_value(row.id);
					text_in(row.label, Font::BODY_BOLD, Theme::TEXT, rect(lx, r.top(), static_cast<int16_t>(r.width() - 90), r.height()), Text::Alignment::LEFT, 0);

					Rect sw = switch_rect(row);
					fill(sw, on ? Theme::GREEN : Theme::TRACK, 15);
					fill(rect(static_cast<int16_t>(on ? sw.right() - 27 : sw.left() + 3), static_cast<int16_t>(sw.top() + 3), 24, 24), Theme::TEXT, 12);
					break;
				}
				case Kind::ACTION:
				{
					text(row.label, Font::BODY_BOLD, row.id == CHARACTER ? Theme::RED.mix(Theme::TEXT, 0.3f) : Theme::TEXT,
						Point<int16_t>(lx, static_cast<int16_t>(r.top() + 10)));
					text(row.detail, Font::SMALL, Theme::MUTED, Point<int16_t>(lx, static_cast<int16_t>(r.top() + 32)));
					glyph(rect(static_cast<int16_t>(r.right() - 32), static_cast<int16_t>(r.top() + (r.height() - 20) / 2), 20, 20),
						Glyph::RIGHT, Theme::FAINT, 12);
					break;
				}
				default:
					break;
			}
		}

		GraphicsGL::get().end_clip();
	}

	UIElement::Type UIMobileOptions::get_type() const
	{
		return TYPE;
	}
}
