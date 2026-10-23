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
#include "UICashShop.h"

#include "UIMobileConfirm.h"

#include "../UI.h"
#include "../Window.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Timer.h"
#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Util/Misc.h"

#include "../../Net/Handlers/CashShopHandlers.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/LoginPackets.h"

#include <algorithm>
#include <ctime>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	using namespace MobileUI;

	namespace
	{
		constexpr int16_t M = 12;
		constexpr int16_t TOP_H = 56;
		constexpr int16_t LEFT_W = 264;
		constexpr int16_t PREVIEW_H = 232;
		constexpr int16_t TAB_H = 36;
		constexpr int16_t SUB_H = 28;
		constexpr int16_t GAP = 10;
		constexpr int16_t CARD_MIN_W = 150;
		constexpr int16_t CARD_H = 128;
		constexpr int16_t ROW_H = 64;
		constexpr int16_t LOCKER_HEADER = 40;

		constexpr int32_t EXIT = 1;
		constexpr int32_t BUY = 2;
		constexpr int32_t RESET = 3;
		constexpr int32_t MOVE_ALL = 4;
		constexpr int32_t TAB = 100;
		constexpr int32_t SUB = 200;
		constexpr int32_t LOCKER_MOVE = 1000;
		constexpr int32_t CARD = 100000;

		constexpr RGBA BACKDROP = { 0.035f, 0.043f, 0.063f, 1.0f };

		// FILETIME of Cosmic's "never expires"
		constexpr int64_t PERMANENT = 150842304000000000LL;
		constexpr int64_t FILETIME_UNIX = 116444736000000000LL;

		int16_t view_width()
		{
			return Constants::Constants::get().get_viewwidth();
		}

		int16_t view_height()
		{
			return Constants::Constants::get().get_viewheight();
		}

		int16_t right_x()
		{
			return M + LEFT_W + M;
		}

		int16_t right_w()
		{
			return static_cast<int16_t>(view_width() - right_x() - M);
		}

		std::string with_commas(int32_t value)
		{
			std::string s = std::to_string(value);
			string_format::split_number(s);

			return s;
		}

		std::string expiry_text(int64_t filetime)
		{
			if (filetime <= 0 || filetime >= PERMANENT)
				return "Permanent";

			int64_t unix_time = (filetime - FILETIME_UNIX) / 10000000LL;
			int64_t days = (unix_time - static_cast<int64_t>(std::time(nullptr))) / 86400;

			if (days <= 0)
				return "Expires today";

			return "Expires in " + std::to_string(days) + (days == 1 ? " day" : " days");
		}

		const char* currency_name(int32_t currency)
		{
			switch (currency)
			{
				case 1: return "NX Credit";
				case 2: return "Maple Points";
				case 4: return "NX Prepaid";
				default: return "NX";
			}
		}

		bool is_try_on(int32_t item_id)
		{
			int32_t category = item_id / 10000;

			// Pet gear (180-183) isn't worn by the character.
			return item_id / 1000000 == 1 && (category < 180 || category > 183);
		}
	}

	UICashShop::UICashShop() : UIMobilePanel(Edge::LEFT, view_width())
	{
		position = Point<int16_t>(0, 0);
		dimension = Point<int16_t>(view_width(), view_height());

		cols = static_cast<int16_t>(std::max(2, (right_w() + GAP) / (CARD_MIN_W + GAP)));
		card_w = static_cast<int16_t>((right_w() - (cols - 1) * GAP) / cols);

		const Player& player = Stage::get().get_player();
		bool female = player.get_stats().get_female();

		// Every item Cosmic sells, from the same Etc/Commodity.img; packages
		// (no item data of their own) and quest deliveries are left out.
		for (nl::node entry : nl::nx::etc["Commodity.img"])
		{
			if (static_cast<int32_t>(entry["OnSale"]) != 1)
				continue;

			int32_t sn = entry["SN"];
			Tab category;

			switch (sn / 10000000)
			{
				case 1: category = FEATURED; break;
				case 2: category = EQUIP; break;
				case 3: category = USE; break;
				case 5: category = ETC; break;
				case 6: category = PET; break;
				default: continue;
			}

			int32_t item_id = entry["ItemId"];
			const ItemData& data = ItemData::get(item_id);

			if (!data.is_valid())
				continue;

			nl::node gender_node = entry["Gender"];
			int32_t gender = gender_node ? static_cast<int32_t>(gender_node) : 2;

			if ((gender == 0 && female) || (gender == 1 && !female))
				continue;

			Commodity c;
			c.sn = sn;
			c.item_id = item_id;
			c.price = entry["Price"];
			c.count = static_cast<int16_t>(std::max(1, static_cast<int32_t>(entry["Count"])));
			c.period = static_cast<int16_t>(static_cast<int32_t>(entry["Period"]));
			c.tab = category;
			c.sub = static_cast<int8_t>((sn / 100000) % 100);
			c.name = data.get_name();
			c.price_text = with_commas(c.price) + " NX";
			c.icon = data.get_icon(false);

			if (c.count > 1)
				c.price_text += "  \xC2\xB7  x" + std::to_string(c.count);

			catalog.push_back(std::move(c));
		}

		preview_look = player.get_look();
		preview_look.set_stance(Stance::Id::STAND1);

		notice_serial = CashShopData::notice().serial;

		rebuild();
	}

	// ---- Layout -------------------------------------------------------------

	const std::vector<UICashShop::SubFilter>& UICashShop::sub_filters() const
	{
		static const std::vector<SubFilter> NONE;
		static const std::vector<SubFilter> EQUIPS = {
			{ "All", -1 }, { "Hats", 0 }, { "Tops", 4 }, { "Bottoms", 5 }, { "Overalls", 3 }, { "Shoes", 6 },
			{ "Gloves", 7 }, { "Weapons", 8 }, { "Capes", 11 }, { "Accessories", 1 }, { "Eyewear", 2 }, { "Rings", 9 }
		};
		static const std::vector<SubFilter> PETS = {
			{ "All", -1 }, { "Pets", 0 }, { "Pet gear", 1 }, { "Food & extras", 2 }
		};

		switch (tab)
		{
			case EQUIP: return EQUIPS;
			case PET: return PETS;
			default: return NONE;
		}
	}

	Rect UICashShop::exit_rect() const
	{
		return rect(static_cast<int16_t>(view_width() - M - 92), 10, 92, 36);
	}

	Rect UICashShop::preview_rect() const
	{
		return rect(M, TOP_H + M, LEFT_W, PREVIEW_H);
	}

	Rect UICashShop::reset_rect() const
	{
		Rect p = preview_rect();

		return rect(static_cast<int16_t>(p.right() - 12 - 74), static_cast<int16_t>(p.top() + 10), 74, 28);
	}

	Rect UICashShop::detail_rect() const
	{
		int16_t top = static_cast<int16_t>(preview_rect().bottom() + M);

		return rect(M, top, LEFT_W, static_cast<int16_t>(view_height() - M - top));
	}

	Rect UICashShop::buy_rect() const
	{
		Rect d = detail_rect();

		return rect(static_cast<int16_t>(d.left() + 14), static_cast<int16_t>(d.bottom() - 14 - 46), static_cast<int16_t>(LEFT_W - 28), 46);
	}

	Rect UICashShop::tab_rect(uint8_t index) const
	{
		int16_t w = static_cast<int16_t>((right_w() - 6 * (NUM_TABS - 1)) / NUM_TABS);

		return rect(static_cast<int16_t>(right_x() + index * (w + 6)), TOP_H + M, w, TAB_H);
	}

	int16_t UICashShop::grid_top() const
	{
		if (!sub_rects.empty())
			return static_cast<int16_t>(sub_rects.back().bottom() + 12);

		return TOP_H + M + TAB_H + 12;
	}

	Rectangle<int16_t> UICashShop::scroll_area() const
	{
		int16_t top = static_cast<int16_t>(grid_top() + (tab == LOCKER ? LOCKER_HEADER : 0));

		return rect(right_x(), top, right_w(), static_cast<int16_t>(view_height() - M - top));
	}

	int16_t UICashShop::scroll_content() const
	{
		if (tab == LOCKER)
		{
			size_t rows = CashShopData::locker().size();

			return static_cast<int16_t>(rows ? rows * (ROW_H + 8) - 8 : 0);
		}

		size_t rows = (shown.size() + cols - 1) / cols;

		return static_cast<int16_t>(rows ? std::min<size_t>(rows * (CARD_H + GAP) - GAP, 32000) : 0);
	}

	Rect UICashShop::card_rect(size_t index) const
	{
		Rectangle<int16_t> area = scroll_area();
		int16_t col = static_cast<int16_t>(index % cols);
		int32_t row = static_cast<int32_t>(index / cols);

		return rect(static_cast<int16_t>(right_x() + col * (card_w + GAP)),
			static_cast<int16_t>(area.top() + row * (CARD_H + GAP) - scroll), card_w, CARD_H);
	}

	Rect UICashShop::locker_row(size_t index) const
	{
		Rectangle<int16_t> area = scroll_area();

		return rect(right_x(), static_cast<int16_t>(area.top() + index * (ROW_H + 8) - scroll), right_w(), ROW_H);
	}

	Rect UICashShop::locker_button(size_t index) const
	{
		Rect row = locker_row(index);

		return rect(static_cast<int16_t>(row.right() - 12 - 140), static_cast<int16_t>(row.top() + 14), 140, 36);
	}

	Rect UICashShop::move_all_rect() const
	{
		return rect(static_cast<int16_t>(right_x() + right_w() - 120), grid_top(), 120, 30);
	}

	// ---- State --------------------------------------------------------------

	void UICashShop::rebuild()
	{
		shown.clear();

		const std::vector<SubFilter>& filters = sub_filters();
		int8_t want = sub_index < filters.size() ? filters[sub_index].sub : -1;

		for (size_t i = 0; i < catalog.size(); i++)
			if (catalog[i].tab == tab && (want < 0 || catalog[i].sub == want))
				shown.push_back(static_cast<int32_t>(i));

		// Sub-category chips flow onto as many rows as they need.
		sub_rects.clear();
		int16_t x = right_x();
		int16_t y = TOP_H + M + TAB_H + 8;

		for (const SubFilter& f : filters)
		{
			int16_t w = static_cast<int16_t>(text_width(f.label, Font::SMALL_BOLD) + 24);

			if (x + w > right_x() + right_w() && x > right_x())
			{
				x = right_x();
				y = static_cast<int16_t>(y + SUB_H + 6);
			}

			sub_rects.push_back(rect(x, y, w, SUB_H));
			x = static_cast<int16_t>(x + w + 6);
		}

		scroll = 0;
	}

	void UICashShop::select_tab(uint8_t t)
	{
		if (t == tab)
			return;

		Sound(Sound::Name::TAB).play();
		tab = t;
		sub_index = 0;
		rebuild();
	}

	void UICashShop::select_sub(size_t index)
	{
		if (index == sub_index)
			return;

		Sound(Sound::Name::BUTTONCLICK).play();
		sub_index = index;
		rebuild();
	}

	void UICashShop::select_item(int32_t index)
	{
		Sound(Sound::Name::BUTTONCLICK).play();
		selected = index;

		const Commodity& c = catalog[index];

		if (is_try_on(c.item_id))
		{
			preview_look = Stage::get().get_player().get_look();
			preview_look.try_on(c.item_id);
			preview_look.set_stance(Stance::Id::STAND1);
			trying_on = true;
		}
	}

	int32_t UICashShop::pay_with(int32_t price) const
	{
		if (get_cash_balance(0) >= price)
			return 1;

		if (get_cash_balance(2) >= price)
			return 4;

		if (get_cash_balance(1) >= price)
			return 2;

		return 0;
	}

	void UICashShop::buy_selected()
	{
		if (selected < 0 || CashShopData::buying())
			return;

		const Commodity& c = catalog[selected];
		int32_t currency = pay_with(c.price);

		if (!currency)
		{
			CashShopData::post("You don't have enough NX for that.", true);
			return;
		}

		int32_t sn = c.sn;
		int32_t item_id = c.item_id;
		std::string message = with_commas(c.price) + " NX from your " + currency_name(currency)
			+ ". It goes to your Locker first, then you can move it to your inventory.";

		UI::get().emplace<UIMobileConfirm>("Buy " + c.name + "?", message, "Buy", false,
			[sn, item_id, currency]()
			{
				CashShopData::buy(sn, item_id, currency);
			}
		);
	}

	void UICashShop::leave()
	{
		if (leaving)
			return;

		leaving = true;
		Sound(Sound::Name::MENUDOWN).play();

		// The server answers with a channel change; exit_cashshop() follows.
		Window::get().fadeout(
			0.025f,
			[]()
			{
				GraphicsGL::get().clear();
				ChangeMapPacket().dispatch();
			}
		);

		GraphicsGL::get().lock();
		Stage::get().clear();
		Timer::get().start();
	}

	void UICashShop::exit_cashshop()
	{
		UI& ui = UI::get();
		ui.change_state(UI::State::GAME);

		Stage& stage = Stage::get();
		Player& player = stage.get_player();

		PlayerLoginPacket(player.get_oid()).dispatch();

		int32_t mapid = player.get_stats().get_mapid();
		uint8_t portalid = player.get_stats().get_portal();

		stage.load(mapid, portalid);
		stage.transfer_player();

		ui.enable();
		Timer::get().start();
		GraphicsGL::get().unlock();
	}

	// ---- Input --------------------------------------------------------------

	int32_t UICashShop::hit(Point<int16_t> pos) const
	{
		if (leaving)
			return NO_TARGET;

		if (exit_rect().contains(pos))
			return EXIT;

		if (trying_on && reset_rect().contains(pos))
			return RESET;

		if (selected >= 0 && buy_rect().contains(pos))
			return BUY;

		for (uint8_t i = 0; i < NUM_TABS; i++)
			if (tab_rect(i).contains(pos))
				return TAB + i;

		for (size_t i = 0; i < sub_rects.size(); i++)
			if (sub_rects[i].contains(pos))
				return SUB + static_cast<int32_t>(i);

		Rectangle<int16_t> area = scroll_area();

		if (tab == LOCKER)
		{
			const std::vector<CashLockerItem>& locker = CashShopData::locker();

			if (!locker.empty() && move_all_rect().contains(pos))
				return MOVE_ALL;

			if (area.contains(pos))
				for (size_t i = 0; i < locker.size(); i++)
					if (locker_button(i).contains(pos))
						return LOCKER_MOVE + static_cast<int32_t>(i);

			return NO_TARGET;
		}

		if (area.contains(pos))
		{
			int32_t row = (pos.y() - area.top() + scroll) / (CARD_H + GAP);
			int32_t col = (pos.x() - right_x()) / (card_w + GAP);

			if (row >= 0 && col >= 0 && col < cols)
			{
				size_t index = static_cast<size_t>(row) * cols + col;

				if (index < shown.size() && card_rect(index).contains(pos))
					return CARD + static_cast<int32_t>(index);
			}
		}

		return NO_TARGET;
	}

	void UICashShop::activate(int32_t target)
	{
		if (target == EXIT)
		{
			leave();
		}
		else if (target == RESET)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			preview_look = Stage::get().get_player().get_look();
			preview_look.set_stance(Stance::Id::STAND1);
			trying_on = false;
		}
		else if (target == BUY)
		{
			buy_selected();
		}
		else if (target == MOVE_ALL)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			moving_all = true;
		}
		else if (target >= CARD)
		{
			size_t index = static_cast<size_t>(target - CARD);

			if (index < shown.size())
				select_item(shown[index]);
		}
		else if (target >= LOCKER_MOVE)
		{
			size_t index = static_cast<size_t>(target - LOCKER_MOVE);
			const std::vector<CashLockerItem>& locker = CashShopData::locker();

			if (index < locker.size())
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				CashShopData::take_out(locker[index]);
			}
		}
		else if (target >= SUB)
		{
			select_sub(static_cast<size_t>(target - SUB));
		}
		else if (target >= TAB)
		{
			select_tab(static_cast<uint8_t>(target - TAB));
		}
	}

	void UICashShop::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed || !escape)
			return;

		if (auto confirm = UI::get().get_element<UIMobileConfirm>())
		{
			if (confirm->is_active())
			{
				confirm->send_key(keycode, pressed, escape);
				return;
			}
		}

		leave();
	}

	void UICashShop::update()
	{
		UIMobilePanel::update();

		preview_look.update(Constants::TIMESTEP);

		CashShopData::expire_requests();

		const CashShopData::Notice& n = CashShopData::notice();

		if (n.serial != notice_serial)
		{
			notice_serial = n.serial;
			notice_at = std::chrono::steady_clock::now();

			if (n.error)
				moving_all = false;

			Sound(n.error ? Sound::Name::DLGNOTICE : Sound::Name::BUTTONCLICK).play();
		}

		// "Move all" works through the locker one item at a time.
		if (moving_all)
		{
			const std::vector<CashLockerItem>& locker = CashShopData::locker();

			if (locker.empty())
				moving_all = false;
			else if (!CashShopData::taking_out(locker.front().cash_id))
				CashShopData::take_out(locker.front());
		}

		clamp_scroll();
	}

	UIElement::Type UICashShop::get_type() const
	{
		return TYPE;
	}

	// ---- Drawing ------------------------------------------------------------

	void UICashShop::draw(float inter) const
	{
		int32_t pressed = pressed_target();

		fill(rect(0, 0, view_width(), view_height()), BACKDROP);

		draw_top_bar(pressed);
		draw_preview(inter, pressed);
		draw_detail(pressed);

		for (uint8_t i = 0; i < NUM_TABS; i++)
		{
			static const char* NAMES[NUM_TABS] = { "Featured", "Equip", "Use", "Etc", "Pets", "Locker" };
			std::string label = NAMES[i];

			if (i == LOCKER && !CashShopData::locker().empty())
				label += " (" + std::to_string(CashShopData::locker().size()) + ")";

			button(tab_rect(i), label, i == tab ? Style::SELECTED : Style::SECONDARY, true, pressed == TAB + i, Font::SMALL_BOLD);
		}

		const std::vector<SubFilter>& filters = sub_filters();

		for (size_t i = 0; i < sub_rects.size() && i < filters.size(); i++)
			button(sub_rects[i], filters[i].label, i == sub_index ? Style::SELECTED : Style::GHOST, true,
				pressed == SUB + static_cast<int32_t>(i), Font::SMALL_BOLD);

		if (tab == LOCKER)
			draw_locker(pressed);
		else
			draw_catalog(pressed);

		// Scroll position, in the right margin.
		int16_t max = scroll_max();

		if (max > 0)
		{
			Rectangle<int16_t> area = scroll_area();
			float visible = static_cast<float>(area.height()) / scroll_content();
			int16_t bar = static_cast<int16_t>(std::max(28.0f, area.height() * visible));
			int16_t y = static_cast<int16_t>(area.top() + (area.height() - bar) * (static_cast<float>(scroll) / max));

			fill(rect(static_cast<int16_t>(view_width() - 8), y, 4, bar), Theme::FAINT.alpha(0.7f), 2);
		}

		draw_notice();
	}

	void UICashShop::draw_top_bar(int32_t pressed) const
	{
		int16_t vw = view_width();

		fill(rect(0, 0, vw, TOP_H), Theme::PANEL);
		fill(rect(0, TOP_H - 1, vw, 1), Theme::LINE);

		text("Cash Shop", Font::HEADLINE, Theme::TEXT, Point<int16_t>(M + 6, 15));

		const std::string& name = Stage::get().get_player().get_stats().get_name();
		text(name, Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(M + 18 + text_width("Cash Shop", Font::HEADLINE)), 22));

		// Balances, right to left before Exit.
		struct Balance { const char* label; int32_t value; };
		Balance balances[3] = {
			{ "NX Prepaid", get_cash_balance(2) },
			{ "Maple Points", get_cash_balance(1) },
			{ "NX Credit", get_cash_balance(0) }
		};

		int16_t x = static_cast<int16_t>(exit_rect().left() - 12);

		for (const Balance& b : balances)
		{
			std::string value = with_commas(b.value);
			int16_t w = static_cast<int16_t>(std::max(text_width(b.label, Font::SMALL), text_width(value, Font::BODY_BOLD)) + 24);
			Rect pill = rect(static_cast<int16_t>(x - w), 8, w, 40);

			fill(pill, Theme::CARD, 10);
			text(b.label, Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(pill.left() + 12), static_cast<int16_t>(pill.top() + 5)));
			text(value, Font::BODY_BOLD, Theme::GOLD, Point<int16_t>(static_cast<int16_t>(pill.left() + 12), static_cast<int16_t>(pill.top() + 20)));

			x = static_cast<int16_t>(pill.left() - 8);
		}

		button(exit_rect(), leaving ? "Leaving..." : "Exit", Style::SECONDARY, !leaving, pressed == EXIT);
	}

	void UICashShop::draw_preview(float inter, int32_t pressed) const
	{
		Rect p = preview_rect();
		fill(p, Theme::CARD, 16);

		text("Preview", Font::SMALL_BOLD, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(p.left() + 14), static_cast<int16_t>(p.top() + 14)));

		if (trying_on)
			button(reset_rect(), "Reset", Style::GHOST, true, pressed == RESET, Font::SMALL_BOLD);

		int16_t cx = static_cast<int16_t>(p.left() + p.width() / 2);
		int16_t floor = static_cast<int16_t>(p.bottom() - 34);

		fill(rect(static_cast<int16_t>(cx - 56), static_cast<int16_t>(floor - 6), 112, 12), RGBA{ 0.0f, 0.0f, 0.0f, 0.28f }, 6);

		// Twice the size, standing on the floor shadow.
		preview_look.draw(DrawArgument(Point<int16_t>(cx, floor), 2.0f, 2.0f), inter);
	}

	void UICashShop::draw_detail(int32_t pressed) const
	{
		Rect d = detail_rect();
		fill(d, Theme::CARD, 16);

		if (selected < 0)
		{
			text_in(tab == LOCKER ? "Things you buy wait in the Locker until you move them to your inventory."
				: "Tap an item to try it on and see its price.",
				Font::BODY, Theme::FAINT, inset(d, 24, 0), Text::Alignment::CENTER);

			return;
		}

		const Commodity& c = catalog[selected];

		Rect well = rect(static_cast<int16_t>(d.left() + 14), static_cast<int16_t>(d.top() + 14), 64, 64);
		fill(well, Theme::TRACK, 12);
		icon(c.icon, inset(well, 10, 10));

		int16_t tx = static_cast<int16_t>(well.right() + 12);
		int16_t tw = static_cast<int16_t>(d.right() - 14 - tx);

		GraphicsGL::get().begin_clip(d.top(), static_cast<int16_t>(well.bottom() + 2));
		text(c.name, Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(tx, static_cast<int16_t>(d.top() + 16)), Text::Alignment::LEFT, tw);
		GraphicsGL::get().end_clip();

		text(with_commas(c.price) + " NX", Font::TITLE, Theme::GOLD, Point<int16_t>(static_cast<int16_t>(d.left() + 14), static_cast<int16_t>(well.bottom() + 12)));

		// The server sells every item without an expiry date.
		std::string meta = "Permanent";

		if (c.count > 1)
			meta = "Qty " + std::to_string(c.count) + "  \xC2\xB7  " + meta;

		text(meta, Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(d.left() + 14), static_cast<int16_t>(well.bottom() + 36)));

		Rect buy = buy_rect();
		int16_t desc_top = static_cast<int16_t>(well.bottom() + 58);
		int16_t desc_bottom = static_cast<int16_t>(buy.top() - 28);

		if (desc_bottom > desc_top)
		{
			GraphicsGL::get().begin_clip(desc_top, desc_bottom);
			text(ItemData::get(c.item_id).get_desc(), Font::SMALL, Theme::MUTED,
				Point<int16_t>(static_cast<int16_t>(d.left() + 14), desc_top), Text::Alignment::LEFT, static_cast<int16_t>(LEFT_W - 28));
			GraphicsGL::get().end_clip();
		}

		int32_t currency = pay_with(c.price);
		bool buying = CashShopData::buying();
		std::string note = currency ? std::string("Pays with ") + currency_name(currency) : "Not enough NX";

		text(note, Font::SMALL, currency ? Theme::FAINT : Theme::RED,
			Point<int16_t>(static_cast<int16_t>(d.left() + LEFT_W / 2), static_cast<int16_t>(buy.top() - 20)), Text::Alignment::CENTER);

		button(buy, buying ? "Buying..." : "Buy", Style::PRIMARY, currency != 0 && !buying, pressed == BUY);
	}

	void UICashShop::draw_catalog(int32_t pressed) const
	{
		Rectangle<int16_t> area = scroll_area();

		if (shown.empty())
		{
			text_in("Nothing here right now.", Font::BODY, Theme::FAINT, rect(area.left(), area.top(), area.width(), 80));
			return;
		}

		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		int32_t pitch = CARD_H + GAP;
		size_t first = static_cast<size_t>(std::max(0, scroll / pitch)) * cols;
		size_t last = std::min(shown.size(), static_cast<size_t>((scroll + area.height()) / pitch + 1) * cols);

		for (size_t i = first; i < last; i++)
		{
			const Commodity& c = catalog[shown[i]];
			Rect r = card_rect(i);
			bool is_selected = shown[i] == selected;
			bool down = pressed == CARD + static_cast<int32_t>(i);

			if (is_selected)
			{
				fill(r, Theme::ACCENT, 14);
				fill(inset(r, 2, 2), Theme::CARD_HI, 12);
			}
			else
			{
				fill(r, down ? Theme::CARD_HI : Theme::CARD, 14);
			}

			int16_t cx = static_cast<int16_t>(r.left() + r.width() / 2);
			Rect well = rect(static_cast<int16_t>(cx - 30), static_cast<int16_t>(r.top() + 12), 60, 60);
			fill(well, Theme::TRACK, 12);
			icon(c.icon, inset(well, 10, 10));

			text(ellipsize(c.name, Font::SMALL_BOLD, static_cast<int16_t>(r.width() - 16)), Font::SMALL_BOLD, Theme::TEXT,
				Point<int16_t>(cx, static_cast<int16_t>(r.top() + 80)), Text::Alignment::CENTER);
			text(ellipsize(c.price_text, Font::SMALL_BOLD, static_cast<int16_t>(r.width() - 16)), Font::SMALL_BOLD, Theme::GOLD,
				Point<int16_t>(cx, static_cast<int16_t>(r.top() + 100)), Text::Alignment::CENTER);
		}

		GraphicsGL::get().end_clip();
	}

	void UICashShop::draw_locker(int32_t pressed) const
	{
		const std::vector<CashLockerItem>& locker = CashShopData::locker();
		int16_t top = grid_top();

		text("Purchases wait here until you move them to your inventory.", Font::SMALL, Theme::MUTED,
			Point<int16_t>(right_x(), static_cast<int16_t>(top + 8)));

		if (locker.empty())
		{
			Rectangle<int16_t> area = scroll_area();
			text_in("Your Locker is empty.", Font::BODY, Theme::FAINT, rect(area.left(), area.top(), area.width(), 80));
			return;
		}

		button(move_all_rect(), moving_all ? "Moving..." : "Move all", Style::SECONDARY, !moving_all, pressed == MOVE_ALL, Font::SMALL_BOLD);

		Rectangle<int16_t> area = scroll_area();
		GraphicsGL::get().begin_clip(area.top(), area.bottom());

		for (size_t i = 0; i < locker.size(); i++)
		{
			Rect row = locker_row(i);

			if (row.bottom() < area.top() || row.top() > area.bottom())
				continue;

			const CashLockerItem& item = locker[i];
			const ItemData& data = ItemData::get(item.item_id);

			fill(row, Theme::CARD, 12);

			Rect well = rect(static_cast<int16_t>(row.left() + 10), static_cast<int16_t>(row.top() + 10), 44, 44);
			fill(well, Theme::TRACK, 10);
			icon(data.get_icon(false), inset(well, 6, 6));

			int16_t tx = static_cast<int16_t>(well.right() + 12);
			Rect move = locker_button(i);
			int16_t tw = static_cast<int16_t>(move.left() - 12 - tx);

			text(ellipsize(data.get_name(), Font::BODY_BOLD, tw), Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(tx, static_cast<int16_t>(row.top() + 12)));

			std::string meta = "x" + std::to_string(item.count) + "  \xC2\xB7  " + expiry_text(item.expiration);
			text(ellipsize(meta, Font::SMALL, tw), Font::SMALL, Theme::MUTED, Point<int16_t>(tx, static_cast<int16_t>(row.top() + 34)));

			bool moving = CashShopData::taking_out(item.cash_id);
			button(move, moving ? "Moving..." : "To inventory", Style::PRIMARY, !moving,
				pressed == LOCKER_MOVE + static_cast<int32_t>(i), Font::SMALL_BOLD);
		}

		GraphicsGL::get().end_clip();
	}

	void UICashShop::draw_notice() const
	{
		const CashShopData::Notice& n = CashShopData::notice();

		if (n.text.empty() || n.serial != notice_serial)
			return;

		float age = std::chrono::duration<float>(std::chrono::steady_clock::now() - notice_at).count();
		constexpr float SHOWN = 3.5f;

		if (age >= SHOWN)
			return;

		float k = std::min(1.0f, (SHOWN - age) / 0.4f);
		int16_t max_w = static_cast<int16_t>(right_w() - 40);
		std::string msg = ellipsize(n.text, Font::BODY, static_cast<int16_t>(max_w - 40));
		int16_t w = static_cast<int16_t>(text_width(msg, Font::BODY) + 40);
		Rect toast = rect(static_cast<int16_t>(right_x() + (right_w() - w) / 2), static_cast<int16_t>(view_height() - M - 54), w, 44);

		fill(toast, Theme::PANEL.alpha(k), 12);
		fill(rect(static_cast<int16_t>(toast.left() + 8), static_cast<int16_t>(toast.top() + 10), 4, 24), (n.error ? Theme::RED : Theme::GREEN).alpha(k), 2);
		text_in(msg, Font::BODY, Theme::TEXT.alpha(k), toast, Text::Alignment::CENTER, 8);
	}
}
