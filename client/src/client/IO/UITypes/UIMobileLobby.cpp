//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileLobby.h"

#include "../UI.h"

#include "../../Audio/Audio.h"
#include "../../Character/Job.h"
#include "../../Configuration.h"
#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/CharCreationPackets.h"
#include "../../Net/Resume.h"
#include "../../Net/Packets/LoginPackets.h"
#include "../../Net/Packets/SelectCharPackets.h"
#include "../../Net/Session.h"
#include "../../Util/Misc.h"
#include "../../Util/Randomizer.h"

#include <nlnx/nx.hpp>

#include <algorithm>
#include <limits>
#include <cctype>
#include <cmath>

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		// Targets.
		enum : int32_t
		{
			T_NONE = -1,
			T_ACCOUNT = 1, T_PASSWORD, T_REMEMBER, T_LOGIN,
			T_LOGOUT = 10, T_ENTER,
			T_BACK = 20, T_PLAY, T_DELETE, T_NEW, T_CONFIRM_YES, T_CONFIRM_NO,
			T_CANCEL = 30, T_NAME, T_CREATE, T_RANDOM,
			T_CLASS = 40,		// + class
			T_GENDER = 45,		// + 0 male, 1 female
			T_PREV = 50,		// + option
			T_NEXT = 60,		// + option
			T_CHANNEL = 1000,	// + world * 100 + channel
			T_SLOT = 5000		// + index (the list's last row creates)
		};

		constexpr int NUM_OPTIONS = 8;
		const char* const OPTION_LABELS[NUM_OPTIONS] = { "FACE", "HAIR", "HAIR COLOR", "SKIN", "TOP", "BOTTOM", "SHOES", "WEAPON" };

		// Class order on screen; the value is CreateCharPacket's job type.
		constexpr int CLASS_ORDER[3] = { 1, 0, 2 };
		const char* const CLASS_NAMES[3] = { "Cygnus", "Explorer", "Aran" };	// by job type
		const char* const CLASS_BLURBS[3] = {
			"A Noblesse of the Knights of Cygnus, sworn to the young Empress.",
			"Start on Maple Island and become a warrior, magician, bowman, thief or pirate.",
			"The polearm hero, waking from ice in Rien to fight the Black Mage again."
		};
		const char* const POOL_FEMALE[3] = { "PremiumCharFemale", "Info/CharFemale", "OrientCharFemale" };
		const char* const POOL_MALE[3] = { "PremiumCharMale", "Info/CharMale", "OrientCharMale" };

		constexpr int16_t SIDE_W = 420;
		constexpr int16_t CREATE_W = 520;
		constexpr int16_t ROW_H = 84;
		constexpr int16_t ROW_GAP = 8;
		constexpr int32_t TOAST_TICKS = 300;

		constexpr RGBA GLASS = { 0.065f, 0.075f, 0.105f, 0.88f };
		constexpr RGBA SHADOW = { 0.0f, 0.0f, 0.0f, 0.72f };
		constexpr RGBA DARK_TEXT = { 0.13f, 0.08f, 0.03f, 1.0f };

		void shadowed(const std::string& str, Font font, RGBA color, Point<int16_t> pos, Text::Alignment align = Text::Alignment::LEFT)
		{
			text(str, font, SHADOW, Point<int16_t>(static_cast<int16_t>(pos.x() + 1), static_cast<int16_t>(pos.y() + 1)), align);
			text(str, font, color, pos, align);
		}

		std::string hair_color_name(uint8_t color)
		{
			static const char* const NAMES[8] = { "Black", "Red", "Orange", "Blonde", "Green", "Blue", "Purple", "Brown" };

			return color < 8 ? NAMES[color] : "Color " + std::to_string(color);
		}

		std::string skin_name(uint8_t skin)
		{
			switch (skin)
			{
				case 0: return "Light";
				case 1: return "Tanned";
				case 2: return "Dark";
				case 3: return "Pale";
				case 4: return "Ashen";
				case 5: return "Green";
				case 9: return "Ghostly";
				case 10: return "Rosy";
				default: return "Skin " + std::to_string(skin);
			}
		}

		// Face and hair names live in String.nx, not under the item tables.
		std::string look_name(const char* kind, int32_t id)
		{
			std::string name = nl::nx::string["Eqp.img"]["Eqp"][kind][std::to_string(id)]["name"];

			// Faces and hairs carry their colour in brackets: the steppers
			// already say it.
			size_t bracket = name.find(" (");

			if (bracket != std::string::npos)
				name = name.substr(0, bracket);

			return name.empty() ? std::string(kind) + " " + std::to_string(id % 1000) : name;
		}

		std::string login_message(int32_t reason)
		{
			switch (reason)
			{
				case 2:
				case 3: return "This account is blocked.";
				case 4: return "That password is wrong.";
				case 5: return "No account has that username.";
				case 7: return "This account is still logged in. Wait a moment and try again.";
				case 10: return "The server is busy. Try again in a moment.";
				case 13:
				case 14: return "This connection can't log in.";
				case MV83_UPDATE_REQUIRED: return "Update required: install the newest app.";
				default: return "Couldn't log in (code " + std::to_string(reason) + ").";
			}
		}

		bool forbidden(const std::string& name)
		{
			std::string lower = name;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			for (std::string word : nl::nx::etc["ForbiddenName.img"])
			{
				std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

				if (!word.empty() && lower.find(word) != std::string::npos)
					return true;
			}

			return false;
		}
	}

	UIMobileLobby::UIMobileLobby() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 600))
	{
		LoginStartPacket().dispatch();
		Music(Configuration::get().get_login_music()).play();

		backdrop = Texture(nl::nx::map["Back"]["login.img"]["back"]["11"]);
		sky = Texture(nl::nx::map["Back"]["login.img"]["back"]["29"]);
		logo = Texture(nl::nx::map.resolve("Obj/login.img/Title/logo/0/0"));

		// Fields: bounds are relative to their input boxes.
		account = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::WHITE, 1.0f,
			Rectangle<int16_t>(Point<int16_t>(14, 14), Point<int16_t>(300, 32)), 13);
		password = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::WHITE, 1.0f,
			Rectangle<int16_t>(Point<int16_t>(14, 14), Point<int16_t>(300, 32)), 16);
		name = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, Color::Name::WHITE, 1.0f,
			Rectangle<int16_t>(Point<int16_t>(14, 13), Point<int16_t>(440, 31)), 12);

		password.set_cryptchar('*');

		account.set_enter_callback([&](std::string) { password.set_state(Textfield::State::FOCUSED); });
		password.set_enter_callback([&](std::string) { log_in(); });
		name.set_enter_callback([&](std::string) { submit_create(); });

		std::string saved = Setting<DefaultAccount>::get().load();
		remember = Setting<SaveLogin>::get().load() || saved.empty();

		if (Setting<SaveLogin>::get().load())
		{
			account.change_text(saved);
			password.change_text(Setting<DefaultPassword>::get().load());
		}

		std::string why = Resume::take_notice();

		// Stays up until the next sign-in.
		if (!why.empty())
		{
			show_error(why);
			error_ticks = std::numeric_limits<int32_t>::max();
		}

		// The connection was lost in game: straight back to that character.
		if (const Resume::Target* target = Resume::pending())
		{
			resuming = true;
			to_list = !target->play;
			resume_name = target->name;
			account_name = target->account;
			set_busy("Signing in...");
			LoginPacket(target->account, target->password).dispatch();
		}

		// Every class's choices, both genders.
		nl::node info = nl::nx::etc["MakeCharInfo.img"];

		for (int job = 0; job < 3; job++)
		{
			for (int f = 0; f < 2; f++)
			{
				Pool& p = pools[job][f];

				for (nl::node category : info.resolve(f ? POOL_FEMALE[job] : POOL_MALE[job]))
				{
					int index = 0;

					try { index = std::stoi(category.name()); }
					catch (...) { continue; }

					for (nl::node id : category)
					{
						int32_t value = id;

						switch (index)
						{
							case 0: p.faces.push_back(value); break;
							case 1: p.hairs.push_back(value); break;
							case 2: p.hair_colors.push_back(static_cast<uint8_t>(value)); break;
							case 3: p.skins.push_back(static_cast<uint8_t>(value)); break;
							case 4: p.tops.push_back(value); break;
							case 5: p.bottoms.push_back(value); break;
							case 6: p.shoes.push_back(value); break;
							case 7: p.weapons.push_back(value); break;
						}
					}
				}
			}
		}

		go(Page::LOGIN);
	}

	// ---- pages ------------------------------------------------------------

	void UIMobileLobby::go(Page next)
	{
		page = next;
		confirm_delete = false;
		target = T_NONE;
		tracking = false;

		auto enable = [](Textfield& field, bool on)
		{
			if (!on)
				field.set_state(Textfield::State::DISABLED);
			else if (field.get_state() == Textfield::State::DISABLED)
				field.set_state(Textfield::State::NORMAL);
		};

		enable(account, page == Page::LOGIN);
		enable(password, page == Page::LOGIN);
		enable(name, page == Page::CREATE);
	}

	void UIMobileLobby::set_busy(const std::string& text)
	{
		busy = text;
		busy_since = std::chrono::steady_clock::now();
		error_ticks = 0;
	}

	void UIMobileLobby::clear_busy()
	{
		busy.clear();
	}

	void UIMobileLobby::fail(const std::string& message)
	{
		clear_busy();

		// Anything going wrong while signing back in hands over to the player.
		if (resuming)
		{
			resuming = false;
			Resume::clear();
		}

		error = message;
		error_ticks = TOAST_TICKS;
		notice_ticks = 0;
		Sound(Sound::Name::MENUDOWN).play();
	}

	void UIMobileLobby::show_error(const std::string& message)
	{
		fail(message);
	}

	// ---- layout -----------------------------------------------------------

	Rect UIMobileLobby::login_card() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t w = 400;
		int16_t h = 414;
		int16_t margin = static_cast<int16_t>(std::max(32, vw / 12));

		return rect(static_cast<int16_t>(vw - margin - w), static_cast<int16_t>((vh - h) / 2), w, h);
	}

	Rect UIMobileLobby::account_field() const
	{
		Rect c = login_card();

		return rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 102), static_cast<int16_t>(c.width() - 48), 46);
	}

	Rect UIMobileLobby::password_field() const
	{
		Rect c = login_card();

		return rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 178), static_cast<int16_t>(c.width() - 48), 46);
	}

	Rect UIMobileLobby::world_card() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t w = static_cast<int16_t>(std::min(640, vw - 64));
		int16_t h = 24 + 60 + 24 + 50 + 24;

		for (const World& world : worlds)
		{
			int cols = world.channelcount <= 4 ? std::max<int>(1, world.channelcount) : 5;
			int rows = (world.channelcount + cols - 1) / cols;
			h = static_cast<int16_t>(h + 46 + rows * 74);
		}

		if (worlds.empty())
			h = static_cast<int16_t>(h + 60);

		h = std::min<int16_t>(h, static_cast<int16_t>(vh - 32));

		return rect(static_cast<int16_t>((vw - w) / 2), static_cast<int16_t>((vh - h) / 2), w, h);
	}

	Rect UIMobileLobby::channel_rect(size_t world_index, size_t channel) const
	{
		Rect c = world_card();
		int16_t y = static_cast<int16_t>(c.top() + 24 + 60);

		for (size_t i = 0; i < world_index && i < worlds.size(); i++)
		{
			int cols = worlds[i].channelcount <= 4 ? std::max<int>(1, worlds[i].channelcount) : 5;
			int rows = (worlds[i].channelcount + cols - 1) / cols;
			y = static_cast<int16_t>(y + 46 + rows * 74);
		}

		const World& world = worlds[world_index];
		int cols = world.channelcount <= 4 ? std::max<int>(1, world.channelcount) : 5;
		int16_t gap = 10;
		int16_t cw = static_cast<int16_t>((c.width() - 48 - gap * (cols - 1)) / cols);
		int col = static_cast<int>(channel) % cols;
		int row = static_cast<int>(channel) / cols;

		return rect(static_cast<int16_t>(c.left() + 24 + col * (cw + gap)), static_cast<int16_t>(y + 46 + row * 74), cw, 64);
	}

	Rect UIMobileLobby::side_card() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t w = page == Page::CREATE ? CREATE_W : SIDE_W;

		return rect(static_cast<int16_t>(vw - 20 - w), 20, w, static_cast<int16_t>(vh - 40));
	}

	Rect UIMobileLobby::stage() const
	{
		int16_t vh = Constants::Constants::get().get_viewheight();

		return rect(0, 0, side_card().left(), vh);
	}

	Rect UIMobileLobby::list_area() const
	{
		Rect c = side_card();

		return rect(static_cast<int16_t>(c.left() + 14), static_cast<int16_t>(c.top() + 72), static_cast<int16_t>(c.width() - 28),
			static_cast<int16_t>(c.height() - 72 - 16 - 50 - 12));
	}

	Rect UIMobileLobby::slot_rect(size_t index) const
	{
		Rect area = list_area();

		return rect(area.left(), static_cast<int16_t>(area.top() + index * (ROW_H + ROW_GAP) - list_scroll), area.width(), ROW_H);
	}

	int16_t UIMobileLobby::list_content() const
	{
		size_t rows = slots.size() + (static_cast<int32_t>(slots.size()) < slot_count ? 1 : 0);

		return static_cast<int16_t>(rows * (ROW_H + ROW_GAP));
	}

	void UIMobileLobby::clamp_list()
	{
		int16_t most = std::max<int16_t>(0, static_cast<int16_t>(list_content() - list_area().height()));
		list_scroll = std::clamp<int16_t>(list_scroll, 0, most);
	}

	Rect UIMobileLobby::name_field() const
	{
		Rect c = side_card();

		return rect(static_cast<int16_t>(c.left() + 14), static_cast<int16_t>(c.top() + 82), static_cast<int16_t>(c.width() - 28), 44);
	}

	Rect UIMobileLobby::option_rect(int option) const
	{
		Rect c = side_card();
		int16_t gap = 12;
		int16_t cw = static_cast<int16_t>((c.width() - 28 - gap) / 2);
		int col = option % 2;
		int row = option / 2;

		return rect(static_cast<int16_t>(c.left() + 14 + col * (cw + gap)), static_cast<int16_t>(c.top() + 224 + row * 64), cw, 56);
	}

	Rect UIMobileLobby::confirm_card() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		return rect(static_cast<int16_t>((vw - 400) / 2), static_cast<int16_t>((vh - 196) / 2), 400, 196);
	}

	std::vector<UIMobileLobby::Spot> UIMobileLobby::spots() const
	{
		std::vector<Spot> out;

		if (confirm_delete)
		{
			Rect c = confirm_card();
			int16_t bw = static_cast<int16_t>((c.width() - 48 - 10) / 2);
			out.push_back({ rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.bottom() - 24 - 46), bw, 46), T_CONFIRM_NO });
			out.push_back({ rect(static_cast<int16_t>(c.left() + 24 + bw + 10), static_cast<int16_t>(c.bottom() - 24 - 46), bw, 46), T_CONFIRM_YES });
			return out;
		}

		switch (page)
		{
			case Page::LOGIN:
			{
				Rect c = login_card();
				out.push_back({ account_field(), T_ACCOUNT });
				out.push_back({ password_field(), T_PASSWORD });
				out.push_back({ rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 236), 180, 30), T_REMEMBER });
				out.push_back({ rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 322), static_cast<int16_t>(c.width() - 48), 48), T_LOGIN });
				break;
			}
			case Page::WORLDS:
			{
				Rect c = world_card();
				out.push_back({ rect(static_cast<int16_t>(c.right() - 24 - 100), static_cast<int16_t>(c.top() + 22), 100, 36), T_LOGOUT });

				for (size_t w = 0; w < worlds.size(); w++)
					for (size_t ch = 0; ch < worlds[w].channelcount; ch++)
						out.push_back({ channel_rect(w, ch), T_CHANNEL + static_cast<int32_t>(w * 100 + ch) });

				out.push_back({ rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.bottom() - 24 - 50), static_cast<int16_t>(c.width() - 48), 50), T_ENTER });
				break;
			}
			case Page::CHARACTERS:
			{
				Rect c = side_card();
				Rect area = list_area();
				out.push_back({ rect(static_cast<int16_t>(c.right() - 16 - 104), static_cast<int16_t>(c.top() + 16), 104, 36), T_BACK });

				size_t rows = slots.size() + (static_cast<int32_t>(slots.size()) < slot_count ? 1 : 0);

				for (size_t i = 0; i < rows; i++)
				{
					Rect r = slot_rect(i);

					if (r.bottom() > area.top() && r.top() < area.bottom())
						out.push_back({ r, T_SLOT + static_cast<int32_t>(i) });
				}

				if (slots.empty())
				{
					Rect s = stage();
					Point<int16_t> m = centre(s);
					out.push_back({ rect(static_cast<int16_t>(m.x() - 120), static_cast<int16_t>(m.y() + 30), 240, 50), T_NEW });
				}
				else
				{
					out.push_back({ rect(static_cast<int16_t>(c.left() + 14), static_cast<int16_t>(c.bottom() - 16 - 50), 120, 50), T_DELETE });
					out.push_back({ rect(static_cast<int16_t>(c.left() + 14 + 120 + 10), static_cast<int16_t>(c.bottom() - 16 - 50),
						static_cast<int16_t>(c.width() - 28 - 130), 50), T_PLAY });
				}

				break;
			}
			case Page::CREATE:
			{
				Rect c = side_card();
				out.push_back({ rect(static_cast<int16_t>(c.right() - 16 - 96), static_cast<int16_t>(c.top() + 16), 96, 36), T_CANCEL });
				out.push_back({ name_field(), T_NAME });

				int16_t inner = static_cast<int16_t>(c.width() - 28);
				int16_t class_w = static_cast<int16_t>(inner * 0.62f);
				int16_t seg = static_cast<int16_t>((class_w - 12) / 3);

				for (int i = 0; i < 3; i++)
					out.push_back({ rect(static_cast<int16_t>(c.left() + 14 + i * (seg + 6)), static_cast<int16_t>(c.top() + 172), seg, 38),
						T_CLASS + CLASS_ORDER[i] });

				int16_t gx = static_cast<int16_t>(c.left() + 14 + class_w + 12);
				int16_t gseg = static_cast<int16_t>((c.right() - 14 - gx - 6) / 2);

				for (int g = 0; g < 2; g++)
					out.push_back({ rect(static_cast<int16_t>(gx + g * (gseg + 6)), static_cast<int16_t>(c.top() + 172), gseg, 38), T_GENDER + g });

				for (int o = 0; o < NUM_OPTIONS; o++)
				{
					Rect cell = option_rect(o);
					out.push_back({ rect(cell.left(), static_cast<int16_t>(cell.top() + 18), 44, 38), T_PREV + o });
					out.push_back({ rect(static_cast<int16_t>(cell.right() - 44), static_cast<int16_t>(cell.top() + 18), 44, 38), T_NEXT + o });
				}

				out.push_back({ rect(static_cast<int16_t>(c.left() + 14), static_cast<int16_t>(c.bottom() - 16 - 48), 140, 48), T_RANDOM });
				out.push_back({ rect(static_cast<int16_t>(c.left() + 14 + 150), static_cast<int16_t>(c.bottom() - 16 - 48),
					static_cast<int16_t>(c.width() - 28 - 150), 48), T_CREATE });
				break;
			}
		}

		return out;
	}

	int32_t UIMobileLobby::hit(Point<int16_t> pos) const
	{
		std::vector<Spot> list = spots();

		for (auto it = list.rbegin(); it != list.rend(); ++it)
			if (inset(it->rect, -2, -3).contains(pos))
				return it->target;

		return T_NONE;
	}

	// ---- input ------------------------------------------------------------

	Cursor::State UIMobileLobby::send_cursor(bool clicked, Point<int16_t> pos)
	{
		if (clicked)
		{
			uint32_t serial = UI::get().get_press_serial();

			if (!tracking || serial != press)
			{
				tracking = true;
				dragging = false;
				press = serial;
				press_start = pos;
				press_last = pos;
				target = hit(pos);

				// Fields take the keyboard as soon as they are touched.
				if (busy.empty())
				{
					if (target == T_ACCOUNT)
						account.set_state(Textfield::State::FOCUSED);
					else if (target == T_PASSWORD)
						password.set_state(Textfield::State::FOCUSED);
					else if (target == T_NAME)
						name.set_state(Textfield::State::FOCUSED);
				}

				return Cursor::State::CLICKING;
			}

			// A vertical move over the character list scrolls it.
			if (page == Page::CHARACTERS && !confirm_delete && !dragging && list_area().contains(press_start)
				&& std::abs(pos.y() - press_start.y()) > 8 && list_content() > list_area().height())
				dragging = true;

			if (dragging)
			{
				list_scroll = static_cast<int16_t>(list_scroll - (pos.y() - press_last.y()));
				clamp_list();
			}

			press_last = pos;

			return Cursor::State::CLICKING;
		}

		if (tracking)
		{
			int32_t t = target;
			bool tap = !dragging && t != T_NONE && hit(pos) == t;

			tracking = false;
			dragging = false;
			target = T_NONE;

			if (tap)
				activate(t);
		}

		return Cursor::State::IDLE;
	}

	void UIMobileLobby::send_scroll(double yoffset)
	{
		if (page == Page::CHARACTERS)
		{
			list_scroll = static_cast<int16_t>(list_scroll - yoffset * 48);
			clamp_list();
		}
	}

	void UIMobileLobby::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed)
			return;

		if (escape)
		{
			if (confirm_delete)
				confirm_delete = false;
			else if (page == Page::CREATE && busy.empty())
				go(Page::CHARACTERS);
			else if (page == Page::CHARACTERS && busy.empty())
				go(Page::WORLDS);

			return;
		}

		if (keycode == KeyAction::Id::RETURN)
		{
			if (page == Page::WORLDS)
				enter_channel();
			else if (page == Page::CHARACTERS && !confirm_delete)
				play();
		}
	}

	void UIMobileLobby::activate(int32_t t)
	{
		if (t == T_ACCOUNT || t == T_PASSWORD || t == T_NAME)
			return;

		// One request at a time.
		if (!busy.empty())
			return;

		Sound(Sound::Name::BUTTONCLICK).play();

		if (t >= T_SLOT)
		{
			size_t index = static_cast<size_t>(t - T_SLOT);

			if (index < slots.size())
			{
				if (index == selected)
					play();
				else
					selected = index;
			}
			else
			{
				start_create();
			}

			return;
		}

		if (t >= T_CHANNEL)
		{
			size_t w = static_cast<size_t>((t - T_CHANNEL) / 100);
			uint8_t ch = static_cast<uint8_t>((t - T_CHANNEL) % 100);

			if (w < worlds.size())
			{
				bool same = world_id == worlds[w].wid && channel_id == ch;
				world_id = worlds[w].wid;
				channel_id = ch;

				// A second tap on the chosen channel goes in.
				if (same)
					enter_channel();
			}

			return;
		}

		if (t >= T_NEXT && t < T_NEXT + NUM_OPTIONS)
		{
			step_option(t - T_NEXT, 1);
			return;
		}

		if (t >= T_PREV && t < T_PREV + NUM_OPTIONS)
		{
			step_option(t - T_PREV, -1);
			return;
		}

		if (t >= T_GENDER && t < T_GENDER + 2)
		{
			female = t == T_GENDER + 1;
			refresh_preview();
			return;
		}

		if (t >= T_CLASS && t < T_CLASS + 3)
		{
			creation_class = t - T_CLASS;
			refresh_preview();
			return;
		}

		switch (t)
		{
			case T_REMEMBER: remember = !remember; break;
			case T_LOGIN: log_in(); break;
			case T_LOGOUT: log_out(); break;
			case T_ENTER: enter_channel(); break;
			case T_BACK: go(Page::WORLDS); break;
			case T_PLAY: play(); break;
			case T_NEW: start_create(); break;
			case T_DELETE: if (selected < slots.size()) confirm_delete = true; break;
			case T_CONFIRM_NO: confirm_delete = false; break;
			case T_CONFIRM_YES:
				confirm_delete = false;

				if (selected < slots.size())
				{
					DeleteCharPacket(slots[selected].entry.id).dispatch();
					set_busy("Deleting...");
				}

				break;
			case T_CANCEL: go(Page::CHARACTERS); break;
			case T_RANDOM: randomize(); break;
			case T_CREATE: submit_create(); break;
		}
	}

	// ---- actions ----------------------------------------------------------

	void UIMobileLobby::log_in()
	{
		if (!busy.empty() || page != Page::LOGIN)
			return;

		std::string user = account.get_text();
		std::string pass = password.get_text();

		if (user.empty())
		{
			fail("Enter your username.");
			return;
		}

		if (pass.empty())
		{
			fail("Enter your password.");
			return;
		}

		Setting<SaveLogin>::get().save(remember);
		Setting<DefaultAccount>::get().save(remember ? user : "");
		Setting<DefaultPassword>::get().save(remember ? pass : "");
		Configuration::get().save();

		account.set_state(Textfield::State::NORMAL);
		password.set_state(Textfield::State::NORMAL);
		account_name = user;
		Resume::remember_login(user, pass);
		error_ticks = 0;

		set_busy("Signing in...");
		LoginPacket(user, pass).dispatch();
	}

	void UIMobileLobby::login_failed(int32_t reason)
	{
		// v83 asks to accept its terms once per account: accepted here.
		if (reason == 23)
		{
			TOSPacket().dispatch();
			return;
		}

		fail(login_message(reason));
	}

	void UIMobileLobby::logged_in(const Account& acc)
	{
		// Accounts made on the fly have no gender yet; characters pick their
		// own, so any answer does.
		if (acc.female == 10)
		{
			GenderPacket(false).dispatch();
			return;
		}

		if (!acc.name.empty())
			account_name = acc.name;

		worlds.clear();
		worlds_loaded = false;
		set_busy("Loading worlds...");
		go(Page::WORLDS);
		ServerRequestPacket().dispatch();
	}

	void UIMobileLobby::log_out()
	{
		// A new connection is a new login.
		Session::get().reconnect();
		LoginStartPacket().dispatch();

		worlds.clear();
		slots.clear();
		clear_busy();
		go(Page::LOGIN);
	}

	void UIMobileLobby::add_world(const World& world)
	{
		for (const World& w : worlds)
			if (w.wid == world.wid)
				return;

		worlds.push_back(world);
	}

	void UIMobileLobby::worlds_ready()
	{
		worlds_loaded = true;

		if (busy == "Loading worlds...")
			clear_busy();

		// Back to where the player was last time.
		uint8_t last_world = Configuration::get().get_worldid();
		uint8_t last_channel = Configuration::get().get_channelid();
		bool found = false;

		for (const World& w : worlds)
		{
			if (w.wid == last_world && last_channel < w.channelcount)
			{
				world_id = w.wid;
				channel_id = last_channel;
				found = true;
			}
		}

		if (!found && !worlds.empty())
		{
			world_id = worlds.front().wid;
			channel_id = 0;
		}

		if (resuming)
		{
			if (const Resume::Target* target = Resume::pending())
				for (const World& w : worlds)
					if (w.wid == target->world && target->channel < w.channelcount)
					{
						world_id = w.wid;
						channel_id = target->channel;
					}

			enter_channel();
		}
	}

	void UIMobileLobby::enter_channel()
	{
		if (!busy.empty() || worlds.empty())
			return;

		Configuration::get().set_worldid(static_cast<uint8_t>(world_id));
		Configuration::get().set_channelid(channel_id);

		set_busy("Loading characters...");
		CharlistRequestPacket(static_cast<uint8_t>(world_id), channel_id).dispatch();
	}

	void UIMobileLobby::set_characters(std::vector<CharEntry> characters, int32_t count, int8_t pic)
	{
		slots.clear();

		for (CharEntry& entry : characters)
		{
			CharLook look(entry.look);
			slots.push_back({ std::move(entry), std::move(look) });
		}

		slot_count = std::max<int32_t>(count, static_cast<int32_t>(slots.size()));
		require_pic = pic;
		selected = std::min<size_t>(Setting<DefaultCharacter>::get().load(), slots.empty() ? 0 : slots.size() - 1);
		list_scroll = 0;

		clear_busy();
		go(Page::CHARACTERS);

		if (resuming)
		{
			const Resume::Target* target = Resume::pending();
			bool found = false;

			for (size_t i = 0; target && i < slots.size(); i++)
			{
				if (slots[i].entry.id == target->cid)
				{
					selected = i;
					found = true;
				}
			}

			if (to_list)
			{
				// Changing character: the list, with the last one picked.
				resuming = false;
				to_list = false;
				Resume::clear();
			}
			else if (found)
			{
				play();
			}
			else
			{
				fail("Couldn't find " + resume_name + " to reconnect.");
			}
		}
	}

	void UIMobileLobby::play()
	{
		if (!busy.empty() || selected >= slots.size())
			return;

		if (require_pic != 2)
		{
			fail("This server asks for a PIC, which the phone can't enter yet.");
			return;
		}

		Setting<DefaultCharacter>::get().save(static_cast<uint8_t>(selected));
		Configuration::get().save();

		Sound(Sound::Name::GAMESTART).play();
		set_busy("Entering...");
		Resume::remember_character(world_id, channel_id, slots[selected].entry.id, slots[selected].entry.stats.name);
		Resume::clear();
		SelectCharPacket(slots[selected].entry.id).dispatch();
	}

	void UIMobileLobby::delete_result(int32_t id, uint8_t state)
	{
		// The server answers a refused new character the same way.
		if (page == Page::CREATE && !busy.empty())
		{
			fail("The server didn't accept that character. Try another name or look.");
			return;
		}

		if (state != 0)
		{
			fail(state == 20 ? "The PIC was wrong." : "Couldn't delete the character (code " + std::to_string(state) + ").");
			return;
		}

		std::string gone;

		for (size_t i = 0; i < slots.size(); i++)
		{
			if (slots[i].entry.id == id)
			{
				gone = slots[i].entry.stats.name;
				slots.erase(slots.begin() + static_cast<std::ptrdiff_t>(i));
				break;
			}
		}

		if (selected >= slots.size())
			selected = slots.empty() ? 0 : slots.size() - 1;

		clamp_list();
		clear_busy();

		notice = gone.empty() ? "Character deleted." : gone + " was deleted.";
		notice_ticks = TOAST_TICKS;
	}

	void UIMobileLobby::start_create()
	{
		if (static_cast<int32_t>(slots.size()) >= slot_count && slot_count > 0)
		{
			fail("Every character slot is used.");
			return;
		}

		name.change_text("");
		pending_name.clear();
		creation_class = 1;
		female = false;

		for (size_t& c : choice)
			c = 0;

		go(Page::CREATE);
		refresh_preview();
		name.set_state(Textfield::State::FOCUSED);
	}

	const UIMobileLobby::Pool& UIMobileLobby::pool() const
	{
		return pools[std::clamp(creation_class, 0, 2)][female ? 1 : 0];
	}

	void UIMobileLobby::step_option(int option, int direction)
	{
		const Pool& p = pool();
		size_t sizes[NUM_OPTIONS] = { p.faces.size(), p.hairs.size(), p.hair_colors.size(), p.skins.size(),
			p.tops.size(), p.bottoms.size(), p.shoes.size(), p.weapons.size() };
		size_t n = option >= 0 && option < NUM_OPTIONS ? sizes[option] : 0;

		if (n == 0)
			return;

		choice[option] = (choice[option] + n + direction) % n;
		refresh_preview();
	}

	void UIMobileLobby::randomize()
	{
		const Pool& p = pool();
		size_t sizes[NUM_OPTIONS] = { p.faces.size(), p.hairs.size(), p.hair_colors.size(), p.skins.size(),
			p.tops.size(), p.bottoms.size(), p.shoes.size(), p.weapons.size() };
		Randomizer random;

		for (int o = 0; o < NUM_OPTIONS; o++)
			if (sizes[o] > 0)
				choice[o] = static_cast<size_t>(random.next_int(static_cast<int32_t>(sizes[o])));

		refresh_preview();
	}

	void UIMobileLobby::refresh_preview()
	{
		const Pool& p = pool();
		size_t sizes[NUM_OPTIONS] = { p.faces.size(), p.hairs.size(), p.hair_colors.size(), p.skins.size(),
			p.tops.size(), p.bottoms.size(), p.shoes.size(), p.weapons.size() };

		// A class or gender with fewer choices keeps what still fits.
		for (int o = 0; o < NUM_OPTIONS; o++)
			if (choice[o] >= sizes[o])
				choice[o] = 0;

		preview = CharLook();

		if (!p.skins.empty())
			preview.set_body(p.skins[choice[3]]);

		if (!p.faces.empty())
			preview.set_face(p.faces[choice[0]]);

		if (!p.hairs.empty())
			preview.set_hair(p.hairs[choice[1]] + (p.hair_colors.empty() ? 0 : p.hair_colors[choice[2]]));

		if (!p.tops.empty())
			preview.add_equip(p.tops[choice[4]]);

		if (!p.bottoms.empty())
			preview.add_equip(p.bottoms[choice[5]]);

		if (!p.shoes.empty())
			preview.add_equip(p.shoes[choice[6]]);

		if (!p.weapons.empty())
			preview.add_equip(p.weapons[choice[7]]);
	}

	std::string UIMobileLobby::option_value(int option) const
	{
		const Pool& p = pool();

		switch (option)
		{
			case 0: return p.faces.empty() ? "-" : look_name("Face", p.faces[choice[0]]);
			case 1: return p.hairs.empty() ? "-" : look_name("Hair", p.hairs[choice[1]]);
			case 2: return p.hair_colors.empty() ? "-" : hair_color_name(p.hair_colors[choice[2]]);
			case 3: return p.skins.empty() ? "-" : skin_name(p.skins[choice[3]]);
			case 4: return p.tops.empty() ? "-" : ItemData::get(p.tops[choice[4]]).get_name();
			case 5: return p.bottoms.empty() ? "-" : ItemData::get(p.bottoms[choice[5]]).get_name();
			case 6: return p.shoes.empty() ? "-" : ItemData::get(p.shoes[choice[6]]).get_name();
			default: return p.weapons.empty() ? "-" : ItemData::get(p.weapons[choice[7]]).get_name();
		}
	}

	void UIMobileLobby::submit_create()
	{
		if (!busy.empty() || page != Page::CREATE)
			return;

		std::string chosen = name.get_text();

		if (chosen.size() < 4 || chosen.size() > 12)
		{
			fail("Names are 4 to 12 letters or numbers.");
			return;
		}

		for (char c : chosen)
		{
			if (!std::isalnum(static_cast<unsigned char>(c)))
			{
				fail("Names can only use letters and numbers.");
				return;
			}
		}

		if (forbidden(chosen))
		{
			fail("That name isn't allowed.");
			return;
		}

		const Pool& p = pool();

		if (p.faces.empty() || p.hairs.empty() || p.hair_colors.empty() || p.skins.empty()
			|| p.tops.empty() || p.bottoms.empty() || p.shoes.empty() || p.weapons.empty())
		{
			fail("This class has no looks in the game data.");
			return;
		}

		name.set_state(Textfield::State::NORMAL);
		pending_name = chosen;
		set_busy("Checking name...");
		NameCharPacket(chosen).dispatch();
	}

	void UIMobileLobby::name_checked(bool used)
	{
		if (page != Page::CREATE || busy.empty())
			return;

		if (used)
		{
			fail("\"" + pending_name + "\" is taken or not allowed.");
			return;
		}

		const Pool& p = pool();

		set_busy("Creating...");
		CreateCharPacket(pending_name, static_cast<uint16_t>(creation_class), p.faces[choice[0]], p.hairs[choice[1]],
			p.hair_colors[choice[2]], p.skins[choice[3]], p.tops[choice[4]], p.bottoms[choice[5]], p.shoes[choice[6]],
			p.weapons[choice[7]], female).dispatch();
	}

	void UIMobileLobby::character_added(CharEntry character)
	{
		std::string made = character.stats.name;
		CharLook look(character.look);
		slots.push_back({ std::move(character), std::move(look) });
		slot_count = std::max<int32_t>(slot_count, static_cast<int32_t>(slots.size()));
		selected = slots.size() - 1;

		clear_busy();
		go(Page::CHARACTERS);

		// Show the new character's row.
		list_scroll = std::max<int16_t>(0, static_cast<int16_t>(list_content() - list_area().height()));
		clamp_list();

		notice = "Welcome, " + made + "!";
		notice_ticks = TOAST_TICKS;
		Sound(Sound::Name::CHARSELECT).play();
	}

	// ---- update -----------------------------------------------------------

	void UIMobileLobby::update()
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		dimension = Point<int16_t>(vw, vh);

		UIElement::update();

		tick++;

		if (error_ticks > 0)
			error_ticks--;

		if (notice_ticks > 0)
			notice_ticks--;

		// A request the server never answered.
		if (!busy.empty() && busy != "Entering..."
			&& std::chrono::steady_clock::now() - busy_since > std::chrono::seconds(15))
			fail("The server didn't answer. Try again.");
		else if (busy == "Entering..." && std::chrono::steady_clock::now() - busy_since > std::chrono::seconds(30))
			fail("Couldn't enter the game. Try again.");

		account.update(account_field().get_left_top());
		password.update(password_field().get_left_top());
		name.update(name_field().get_left_top());

		if (page == Page::CREATE)
			preview.update(Constants::TIMESTEP);

		if (page == Page::CHARACTERS && selected < slots.size())
			slots[selected].look.update(Constants::TIMESTEP);
	}

	// ---- drawing ----------------------------------------------------------

	void UIMobileLobby::draw_backdrop() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		// The sky shows through the forest art's open edges.
		for (const Texture* layer : { &sky, &backdrop })
		{
			Point<int16_t> size = layer->get_dimensions();

			if (size.x() > 0 && size.y() > 0)
			{
				float s = std::max(static_cast<float>(vw) / size.x(), static_cast<float>(vh) / size.y());
				int16_t w = static_cast<int16_t>(std::ceil(size.x() * s));
				int16_t h = static_cast<int16_t>(std::ceil(size.y() * s));
				icon(*layer, rect(static_cast<int16_t>((vw - w) / 2), static_cast<int16_t>((vh - h) / 2), w, h));
			}
		}

		fill(rect(0, 0, vw, vh), RGBA{ 0.02f, 0.03f, 0.07f, 0.42f });
	}

	void UIMobileLobby::draw_banner(const Rect& r) const
	{
		(void)r;
		int16_t vw = Constants::Constants::get().get_viewwidth();

		bool showing_error = error_ticks > 0 && !error.empty();
		bool showing_notice = !showing_error && notice_ticks > 0 && !notice.empty();

		if (!showing_error && !showing_notice)
			return;

		const std::string& message = showing_error ? error : notice;
		int32_t ticks = showing_error ? error_ticks : notice_ticks;
		float fade = std::min(1.0f, ticks / 30.0f);
		int16_t w = static_cast<int16_t>(std::min<int>(vw - 40, text_width(message, Font::BODY_BOLD) + 56));
		Rect toast = rect(static_cast<int16_t>((vw - w) / 2), 14, w, 40);
		RGBA tint = showing_error ? Theme::RED : Theme::GREEN;

		fill(toast, RGBA{ 0.06f, 0.07f, 0.10f, 0.94f * fade }, 20);
		fill(rect(static_cast<int16_t>(toast.left() + 14), static_cast<int16_t>(toast.top() + 14), 12, 12), tint.alpha(fade), 6);
		text_in(ellipsize(message, Font::BODY_BOLD, static_cast<int16_t>(w - 56)), Font::BODY_BOLD, Theme::TEXT.alpha(fade),
			rect(static_cast<int16_t>(toast.left() + 34), toast.top(), static_cast<int16_t>(w - 44), toast.height()), Text::Alignment::LEFT, 0);
	}

	namespace
	{
		void field_box(const Rect& r, bool focused, bool empty, const std::string& placeholder)
		{
			fill(r, focused ? RGBA{ 1.0f, 1.0f, 1.0f, 0.15f } : RGBA{ 1.0f, 1.0f, 1.0f, 0.08f }, 12);

			if (focused)
				fill(rect(static_cast<int16_t>(r.left() + 12), static_cast<int16_t>(r.bottom() - 3), static_cast<int16_t>(r.width() - 24), 2),
					Theme::ACCENT, 1);

			if (empty && !focused)
				text_in(placeholder, Font::BODY, Theme::FAINT, r, Text::Alignment::LEFT, 14);
		}

		void segment(const Rect& r, const std::string& label, bool on, bool pressed)
		{
			fill(r, on ? Theme::ACCENT : (pressed ? Theme::CARD_HI : Theme::CARD), 12);
			text_in(label, Font::SMALL_BOLD, on ? DARK_TEXT : Theme::TEXT, r);
		}
	}

	void UIMobileLobby::draw_login() const
	{
		int32_t pressed = tracking && !dragging ? target : T_NONE;
		Rect c = login_card();
		Rect hero = rect(0, 0, c.left(), Constants::Constants::get().get_viewheight());
		Point<int16_t> m = centre(hero);

		// The game's logo on a light card, with where it connects to.
		Rect badge = rect(static_cast<int16_t>(m.x() - 190), static_cast<int16_t>(m.y() - 150), 380, 210);
		fill(badge, RGBA{ 1.0f, 1.0f, 1.0f, 0.90f }, 28);

		if (logo.is_valid())
			icon(logo, rect(static_cast<int16_t>(m.x() - 175), static_cast<int16_t>(badge.top() + 8), 350, 193));

		bool online = Session::get().is_connected();
		shadowed("Private server  \xC2\xB7  v83", Font::TITLE, Theme::TEXT, Point<int16_t>(m.x(), static_cast<int16_t>(badge.bottom() + 22)), Text::Alignment::CENTER);

		std::string status = online ? "Server online" : "Connecting to the server...";
		int16_t sw = text_width(status, Font::SMALL_BOLD);
		Point<int16_t> sp(static_cast<int16_t>(m.x() - (sw + 16) / 2), static_cast<int16_t>(badge.bottom() + 52));
		fill(rect(sp.x(), static_cast<int16_t>(sp.y() + 4), 10, 10), online ? Theme::GREEN : Theme::GOLD, 5);
		shadowed(status, Font::SMALL_BOLD, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(sp.x() + 16), sp.y()));

		// Sign in.
		fill(c, GLASS, 22);
		text("Welcome", Font::HEADLINE, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 22)));
		text("Sign in to play", Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 54)));

		text("USERNAME", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(c.left() + 26), static_cast<int16_t>(c.top() + 84)));
		Rect af = account_field();
		field_box(af, account.get_state() == Textfield::State::FOCUSED, account.empty(), "Username");
		account.draw(af.get_left_top());

		text("PASSWORD", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(c.left() + 26), static_cast<int16_t>(c.top() + 160)));
		Rect pf = password_field();
		field_box(pf, password.get_state() == Textfield::State::FOCUSED, password.empty(), "Password");
		password.draw(pf.get_left_top());

		// Remember me: a switch.
		Rect sw_r = rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 238), 44, 26);
		fill(sw_r, remember ? Theme::ACCENT : Theme::TRACK, 13);
		fill(rect(static_cast<int16_t>(remember ? sw_r.right() - 23 : sw_r.left() + 3), static_cast<int16_t>(sw_r.top() + 3), 20, 20), Theme::TEXT, 10);
		text_in("Remember me", Font::BODY, Theme::TEXT, rect(static_cast<int16_t>(sw_r.right() + 10), sw_r.top(), 140, sw_r.height()), Text::Alignment::LEFT, 0);

		// Inline reason a sign-in failed.
		if (error_ticks > 0 && !error.empty())
		{
			Rect band = rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 278), static_cast<int16_t>(c.width() - 48), 34);
			fill(band, Theme::RED.alpha(0.18f), 10);
			text_in(ellipsize(error, Font::SMALL_BOLD, static_cast<int16_t>(band.width() - 24)), Font::SMALL_BOLD, Theme::RED.mix(Theme::TEXT, 0.4f),
				band, Text::Alignment::LEFT, 12);
		}

		Rect go_r = rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 322), static_cast<int16_t>(c.width() - 48), 48);
		button(go_r, busy.empty() ? "Log in" : busy, Style::PRIMARY, busy.empty() && online, pressed == T_LOGIN);

		text("A new username makes a new account.", Font::SMALL, Theme::FAINT,
			Point<int16_t>(centre(c).x(), static_cast<int16_t>(c.top() + 384)), Text::Alignment::CENTER);
	}

	void UIMobileLobby::draw_worlds() const
	{
		int32_t pressed = tracking && !dragging ? target : T_NONE;
		Rect c = world_card();
		fill(c, GLASS, 22);

		text("Choose a channel", Font::HEADLINE, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 22)));
		text("Signed in as " + account_name, Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 54)));
		button(rect(static_cast<int16_t>(c.right() - 24 - 100), static_cast<int16_t>(c.top() + 22), 100, 36), "Log out", Style::SECONDARY,
			busy.empty(), pressed == T_LOGOUT, Font::SMALL_BOLD);

		if (worlds.empty())
		{
			text_in(worlds_loaded ? "No worlds are open right now." : "Asking the server for its worlds...", Font::BODY, Theme::MUTED,
				rect(c.left(), static_cast<int16_t>(c.top() + 90), c.width(), 50));
		}

		const World* chosen = nullptr;

		for (size_t w = 0; w < worlds.size(); w++)
		{
			const World& world = worlds[w];
			Rect first = channel_rect(w, 0);

			text(world.name, Font::TITLE, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(first.top() - 38)));

			if (!world.message.empty())
				text(ellipsize(world.message, Font::SMALL, static_cast<int16_t>(c.width() - 200)), Font::SMALL, Theme::GOLD,
					Point<int16_t>(static_cast<int16_t>(c.left() + 24 + text_width(world.name, Font::TITLE) + 12), static_cast<int16_t>(first.top() - 34)));

			if (world.wid == world_id)
				chosen = &world;

			for (size_t ch = 0; ch < world.channelcount; ch++)
			{
				Rect r = channel_rect(w, ch);
				bool on = world.wid == world_id && ch == channel_id;
				int32_t id = T_CHANNEL + static_cast<int32_t>(w * 100 + ch);
				fill(r, on ? Theme::ACCENT : (pressed == id ? Theme::CARD_HI : Theme::CARD), 14);

				int32_t load = ch < world.chloads.size() ? (world.chloads[ch] & 0xFFFF) : 0;
				float fraction = std::clamp(load / 1200.0f, 0.0f, 1.0f);
				const char* how = fraction < 0.35f ? "Quiet" : fraction < 0.8f ? "Busy" : "Full";

				text("Channel " + std::to_string(ch + 1), Font::BODY_BOLD, on ? DARK_TEXT : Theme::TEXT,
					Point<int16_t>(static_cast<int16_t>(r.left() + 14), static_cast<int16_t>(r.top() + 12)));
				text(how, Font::SMALL, on ? DARK_TEXT.alpha(0.75f) : Theme::MUTED,
					Point<int16_t>(static_cast<int16_t>(r.left() + 14), static_cast<int16_t>(r.top() + 32)));

				Rect bar_r = rect(static_cast<int16_t>(r.left() + 14), static_cast<int16_t>(r.bottom() - 12), static_cast<int16_t>(r.width() - 28), 4);
				fill(bar_r, on ? RGBA{ 0.0f, 0.0f, 0.0f, 0.2f } : Theme::TRACK, 2);

				if (fraction > 0.0f)
					fill(rect(bar_r.left(), bar_r.top(), static_cast<int16_t>(std::max(4.0f, bar_r.width() * fraction)), 4),
						on ? DARK_TEXT : (fraction < 0.8f ? Theme::GREEN : Theme::RED), 2);
			}
		}

		std::string label = !busy.empty() ? busy
			: chosen ? "Enter " + chosen->name + "  \xC2\xB7  Channel " + std::to_string(channel_id + 1) : "Enter";
		button(rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.bottom() - 24 - 50), static_cast<int16_t>(c.width() - 48), 50),
			label, Style::PRIMARY, busy.empty() && chosen, pressed == T_ENTER);
	}

	void UIMobileLobby::draw_characters() const
	{
		int32_t pressed = tracking && !dragging ? target : T_NONE;
		Rect c = side_card();
		Rect s = stage();
		Point<int16_t> m = centre(s);

		// The chosen character, large, standing on the forest floor.
		if (selected < slots.size())
		{
			const Slot& slot = slots[selected];
			int16_t floor = static_cast<int16_t>(s.height() * 0.66f);

			fill(rect(static_cast<int16_t>(m.x() - 80), static_cast<int16_t>(floor - 9), 160, 18), RGBA{ 0.0f, 0.0f, 0.0f, 0.35f }, 9);

			Point<int16_t> feet(m.x(), floor);
			slot.look.draw(DrawArgument(feet, feet, Point<int16_t>(0, 0), 2.2f, 2.2f, 1.0f, 0.0f), 1.0f);

			const StatsEntry& st = slot.entry.stats;
			std::string job = Job(st.stats[MapleStat::Id::JOB]).get_name();
			shadowed(st.name, Font::HEADLINE, Theme::TEXT, Point<int16_t>(m.x(), static_cast<int16_t>(floor + 22)), Text::Alignment::CENTER);
			shadowed("Lv " + std::to_string(st.stats[MapleStat::Id::LEVEL]) + "  \xC2\xB7  " + job, Font::BODY_BOLD, Theme::GOLD,
				Point<int16_t>(m.x(), static_cast<int16_t>(floor + 52)), Text::Alignment::CENTER);
			shadowed(NxHelper::Map::get_map_name(st.mapid), Font::SMALL, Theme::TEXT.alpha(0.85f),
				Point<int16_t>(m.x(), static_cast<int16_t>(floor + 74)), Text::Alignment::CENTER);
		}
		else
		{
			shadowed("No characters yet", Font::HEADLINE, Theme::TEXT, Point<int16_t>(m.x(), static_cast<int16_t>(m.y() - 30)), Text::Alignment::CENTER);
			shadowed("Make one to start your adventure.", Font::BODY, Theme::TEXT.alpha(0.85f), Point<int16_t>(m.x(), m.y()), Text::Alignment::CENTER);
			button(rect(static_cast<int16_t>(m.x() - 120), static_cast<int16_t>(m.y() + 30), 240, 50), "Create a character", Style::PRIMARY,
				busy.empty(), pressed == T_NEW);
		}

		// The list.
		fill(c, GLASS, 22);
		text("Characters", Font::TITLE, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(c.left() + 18), static_cast<int16_t>(c.top() + 18)));

		std::string where;

		for (const World& w : worlds)
			if (w.wid == world_id)
				where = w.name + "  \xC2\xB7  Ch " + std::to_string(channel_id + 1) + "  \xC2\xB7  ";

		text(where + std::to_string(slots.size()) + " of " + std::to_string(slot_count) + " slots", Font::SMALL, Theme::MUTED,
			Point<int16_t>(static_cast<int16_t>(c.left() + 18), static_cast<int16_t>(c.top() + 42)));
		button(rect(static_cast<int16_t>(c.right() - 16 - 104), static_cast<int16_t>(c.top() + 16), 104, 36), "Channels", Style::SECONDARY,
			busy.empty(), pressed == T_BACK, Font::SMALL_BOLD);

		Rect area = list_area();
		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		size_t rows = slots.size() + (static_cast<int32_t>(slots.size()) < slot_count ? 1 : 0);

		for (size_t i = 0; i < rows; i++)
		{
			Rect r = slot_rect(i);

			if (r.bottom() < area.top() || r.top() > area.bottom())
				continue;

			int32_t id = T_SLOT + static_cast<int32_t>(i);

			if (i >= slots.size())
			{
				fill(r, pressed == id ? Theme::CARD_HI : RGBA{ 1.0f, 1.0f, 1.0f, 0.05f }, 16);
				Rect plus = rect(static_cast<int16_t>(r.left() + 16), static_cast<int16_t>(r.top() + 20), 44, 44);
				fill(plus, Theme::ACCENT.alpha(0.2f), 22);
				glyph(plus, Glyph::PLUS, Theme::ACCENT, 18);
				text("Create a character", Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(r.left() + 76), static_cast<int16_t>(r.top() + 24)));
				text(std::to_string(slot_count - static_cast<int32_t>(slots.size())) + " empty slot" + (slot_count - static_cast<int32_t>(slots.size()) == 1 ? "" : "s"),
					Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(r.left() + 76), static_cast<int16_t>(r.top() + 46)));
				continue;
			}

			const Slot& slot = slots[i];
			bool on = i == selected;
			fill(r, on ? Theme::CARD_HI : (pressed == id ? Theme::CARD_HI : Theme::CARD), 16);

			if (on)
				fill(rect(r.left(), static_cast<int16_t>(r.top() + 14), 4, static_cast<int16_t>(r.height() - 28)), Theme::ACCENT, 2);

			// Head and shoulders: the look drawn small, cut by the row.
			GraphicsGL::get().begin_clip(std::max(area.top(), static_cast<int16_t>(r.top() + 4)), std::min(area.bottom(), static_cast<int16_t>(r.bottom() - 4)));
			Point<int16_t> feet(static_cast<int16_t>(r.left() + 44), static_cast<int16_t>(r.top() + 92));
			slot.look.draw(DrawArgument(feet, feet, Point<int16_t>(0, 0), 1.15f, 1.15f, 1.0f, 0.0f), 1.0f);
			GraphicsGL::get().end_clip();
			GraphicsGL::get().begin_clip(area.top(), area.bottom());

			const StatsEntry& st = slot.entry.stats;
			std::string job = Job(st.stats[MapleStat::Id::JOB]).get_name();
			text(st.name, Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(r.left() + 92), static_cast<int16_t>(r.top() + 18)));
			text("Lv " + std::to_string(st.stats[MapleStat::Id::LEVEL]) + "  \xC2\xB7  " + job, Font::SMALL_BOLD, on ? Theme::GOLD : Theme::MUTED,
				Point<int16_t>(static_cast<int16_t>(r.left() + 92), static_cast<int16_t>(r.top() + 40)));
			text(ellipsize(NxHelper::Map::get_map_name(st.mapid), Font::SMALL, static_cast<int16_t>(r.width() - 108)), Font::SMALL, Theme::FAINT,
				Point<int16_t>(static_cast<int16_t>(r.left() + 92), static_cast<int16_t>(r.top() + 58)));
		}

		GraphicsGL::get().end_clip();

		if (!slots.empty())
		{
			button(rect(static_cast<int16_t>(c.left() + 14), static_cast<int16_t>(c.bottom() - 16 - 50), 120, 50), "Delete", Style::DANGER,
				busy.empty(), pressed == T_DELETE, Font::SMALL_BOLD);
			button(rect(static_cast<int16_t>(c.left() + 14 + 130), static_cast<int16_t>(c.bottom() - 16 - 50), static_cast<int16_t>(c.width() - 28 - 130), 50),
				busy.empty() ? "Play" : busy, Style::PRIMARY, busy.empty(), pressed == T_PLAY);
		}
	}

	void UIMobileLobby::draw_create() const
	{
		int32_t pressed = tracking && !dragging ? target : T_NONE;
		Rect c = side_card();
		Rect s = stage();
		Point<int16_t> m = centre(s);
		int16_t floor = static_cast<int16_t>(s.height() * 0.64f);

		// The new look, large.
		fill(rect(static_cast<int16_t>(m.x() - 84), static_cast<int16_t>(floor - 9), 168, 18), RGBA{ 0.0f, 0.0f, 0.0f, 0.35f }, 9);
		Point<int16_t> feet(m.x(), floor);
		preview.draw(DrawArgument(feet, feet, Point<int16_t>(0, 0), 2.4f, 2.4f, 1.0f, 0.0f), 1.0f);

		int cls = std::clamp(creation_class, 0, 2);
		std::string shown = name.empty() ? std::string(CLASS_NAMES[cls]) : name.get_text();
		shadowed(shown, Font::HEADLINE, Theme::TEXT, Point<int16_t>(m.x(), static_cast<int16_t>(floor + 22)), Text::Alignment::CENTER);
		shadowed(std::string(CLASS_NAMES[cls]) + (female ? "  \xC2\xB7  Female" : "  \xC2\xB7  Male"), Font::BODY_BOLD, Theme::GOLD,
			Point<int16_t>(m.x(), static_cast<int16_t>(floor + 52)), Text::Alignment::CENTER);

		int16_t blurb_w = static_cast<int16_t>(std::min(360, s.width() - 60));
		int16_t blurb_h = text_height(CLASS_BLURBS[cls], Font::SMALL, blurb_w);
		fill(rect(static_cast<int16_t>(m.x() - blurb_w / 2 - 14), static_cast<int16_t>(floor + 78), static_cast<int16_t>(blurb_w + 28), static_cast<int16_t>(blurb_h + 18)),
			RGBA{ 0.02f, 0.03f, 0.06f, 0.55f }, 12);
		text(CLASS_BLURBS[cls], Font::SMALL, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(m.x() - blurb_w / 2), static_cast<int16_t>(floor + 87)),
			Text::Alignment::LEFT, blurb_w);

		// Choices.
		fill(c, GLASS, 22);
		text("New character", Font::TITLE, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(c.left() + 18), static_cast<int16_t>(c.top() + 22)));
		button(rect(static_cast<int16_t>(c.right() - 16 - 96), static_cast<int16_t>(c.top() + 16), 96, 36), "Cancel", Style::SECONDARY,
			busy.empty(), pressed == T_CANCEL, Font::SMALL_BOLD);

		text("NAME", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(c.left() + 16), static_cast<int16_t>(c.top() + 64)));
		Rect nf = name_field();
		field_box(nf, name.get_state() == Textfield::State::FOCUSED, name.empty(), "Pick a name");
		name.draw(nf.get_left_top());
		text("4 to 12 letters or numbers", Font::SMALL, Theme::FAINT, Point<int16_t>(static_cast<int16_t>(c.left() + 16), static_cast<int16_t>(c.top() + 132)));

		int16_t inner = static_cast<int16_t>(c.width() - 28);
		int16_t class_w = static_cast<int16_t>(inner * 0.62f);
		int16_t seg = static_cast<int16_t>((class_w - 12) / 3);
		int16_t gx = static_cast<int16_t>(c.left() + 14 + class_w + 12);
		int16_t gseg = static_cast<int16_t>((c.right() - 14 - gx - 6) / 2);

		text("CLASS", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(c.left() + 16), static_cast<int16_t>(c.top() + 154)));
		text("GENDER", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(gx + 2), static_cast<int16_t>(c.top() + 154)));

		for (int i = 0; i < 3; i++)
		{
			int job = CLASS_ORDER[i];
			segment(rect(static_cast<int16_t>(c.left() + 14 + i * (seg + 6)), static_cast<int16_t>(c.top() + 172), seg, 38), CLASS_NAMES[job],
				creation_class == job, pressed == T_CLASS + job);
		}

		for (int g = 0; g < 2; g++)
			segment(rect(static_cast<int16_t>(gx + g * (gseg + 6)), static_cast<int16_t>(c.top() + 172), gseg, 38), g ? "Female" : "Male",
				female == (g == 1), pressed == T_GENDER + g);

		for (int o = 0; o < NUM_OPTIONS; o++)
		{
			Rect cell = option_rect(o);
			text(OPTION_LABELS[o], Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(cell.left() + 2), cell.top()));

			Rect stepper = rect(cell.left(), static_cast<int16_t>(cell.top() + 18), cell.width(), 38);
			fill(stepper, Theme::CARD, 12);

			Rect prev = rect(stepper.left(), stepper.top(), 40, 38);
			Rect next = rect(static_cast<int16_t>(stepper.right() - 40), stepper.top(), 40, 38);

			if (pressed == T_PREV + o)
				fill(prev, Theme::CARD_HI, 12);

			if (pressed == T_NEXT + o)
				fill(next, Theme::CARD_HI, 12);

			glyph(prev, Glyph::LEFT, Theme::ACCENT, 14);
			glyph(next, Glyph::RIGHT, Theme::ACCENT, 14);
			text_in(ellipsize(option_value(o), Font::SMALL_BOLD, static_cast<int16_t>(stepper.width() - 88)), Font::SMALL_BOLD, Theme::TEXT,
				rect(static_cast<int16_t>(stepper.left() + 40), stepper.top(), static_cast<int16_t>(stepper.width() - 80), stepper.height()));
		}

		button(rect(static_cast<int16_t>(c.left() + 14), static_cast<int16_t>(c.bottom() - 16 - 48), 140, 48), "Randomize", Style::SECONDARY,
			busy.empty(), pressed == T_RANDOM, Font::SMALL_BOLD);
		button(rect(static_cast<int16_t>(c.left() + 14 + 150), static_cast<int16_t>(c.bottom() - 16 - 48), static_cast<int16_t>(c.width() - 28 - 150), 48),
			busy.empty() ? "Create" : busy, Style::PRIMARY, busy.empty() && !name.empty(), pressed == T_CREATE);
	}

	void UIMobileLobby::draw_confirm() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		int32_t pressed = tracking && !dragging ? target : T_NONE;

		fill(rect(0, 0, vw, vh), RGBA{ 0.0f, 0.0f, 0.0f, 0.55f });

		Rect c = confirm_card();
		fill(c, Theme::PANEL, 20);

		std::string who = selected < slots.size() ? slots[selected].entry.stats.name : "this character";
		text("Delete " + who + "?", Font::TITLE, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 24)));
		text("Their items, mesos and progress are gone for good.", Font::BODY, Theme::MUTED,
			Point<int16_t>(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.top() + 56)), Text::Alignment::LEFT, static_cast<int16_t>(c.width() - 48));

		int16_t bw = static_cast<int16_t>((c.width() - 48 - 10) / 2);
		button(rect(static_cast<int16_t>(c.left() + 24), static_cast<int16_t>(c.bottom() - 24 - 46), bw, 46), "Keep", Style::SECONDARY, true, pressed == T_CONFIRM_NO);
		button(rect(static_cast<int16_t>(c.left() + 24 + bw + 10), static_cast<int16_t>(c.bottom() - 24 - 46), bw, 46), "Delete", Style::DANGER, true, pressed == T_CONFIRM_YES);
	}

	void UIMobileLobby::draw_resume() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		Rect c = rect(static_cast<int16_t>((vw - 420) / 2), static_cast<int16_t>((vh - 170) / 2), 420, 170);

		fill(c, GLASS, 22);
		text_in(to_list ? "Changing character" : "Reconnecting", Font::HEADLINE, Theme::TEXT,
			rect(c.left(), static_cast<int16_t>(c.top() + 26), c.width(), 34));
		text_in(to_list ? std::string("Loading your characters")
			: "Taking " + (resume_name.empty() ? std::string("you") : resume_name) + " back where you left off",
			Font::BODY, Theme::MUTED, rect(c.left(), static_cast<int16_t>(c.top() + 70), c.width(), 24));

		// Three dots that fill in turn while it works.
		int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		int lit = static_cast<int>((ms / 350) % 3);

		for (int i = 0; i < 3; i++)
			fill(rect(static_cast<int16_t>(c.left() + c.width() / 2 - 30 + i * 24), static_cast<int16_t>(c.top() + 112), 12, 12),
				i == lit ? Theme::ACCENT : Theme::TRACK, 6);

		text_in(busy, Font::SMALL, Theme::FAINT, rect(c.left(), static_cast<int16_t>(c.top() + 134), c.width(), 22));
	}

	void UIMobileLobby::draw(float) const
	{
		draw_backdrop();

		if (resuming)
		{
			draw_resume();
			return;
		}

		switch (page)
		{
			case Page::LOGIN: draw_login(); break;
			case Page::WORLDS: draw_worlds(); break;
			case Page::CHARACTERS: draw_characters(); break;
			case Page::CREATE: draw_create(); break;
		}

		if (confirm_delete)
			draw_confirm();

		// The login page shows its errors on the card.
		if (page != Page::LOGIN || notice_ticks > 0)
			draw_banner({});
	}

	UIElement::Type UIMobileLobby::get_type() const
	{
		return TYPE;
	}
}
