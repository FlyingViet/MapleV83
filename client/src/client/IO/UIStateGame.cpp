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
#include "UIStateGame.h"

#include "MobileUI.h"
#include "ServerInfo.h"
#include "UITypes/UIMobileStore.h"
#include "UITypes/UIMobileQuests.h"
#include "UITypes/UIMobileSocial.h"
#include "UITypes/UIMobileWorldMap.h"

#include <algorithm>
#include <cmath>


#include "UI.h"
#include "../Constants.h"

#include "UITypes/UIBuddyList.h"
#include "UITypes/UIBuffList.h"
#include "UITypes/UIClock.h"
#include "UITypes/UIChannel.h"
#include "UITypes/UICharInfo.h"
#include "UITypes/UIChat.h"
#include "UITypes/UIChatBar.h"
#include "UITypes/UIFarmChat.h"
#include "UITypes/UISocialChat.h"
#include "UITypes/UIEquipInventory.h"
#include "UITypes/UIEvent.h"
#include "UITypes/UIItemInventory.h"
#include "UITypes/UIKeyConfig.h"
#include "UITypes/UIMobileKeyConfig.h"
#include "UITypes/UIMiniMap.h"
#include "UITypes/UIPartyHUD.h"
#include "UITypes/UIQuestHelper.h"
#include "UITypes/UIQuestLog.h"
#include "UITypes/UINotice.h"
#include "UITypes/UIOptionMenu.h"
#include "UITypes/UIQuit.h"
#include "UITypes/UIShop.h"
#include "UITypes/UISkillBook.h"
#include "UITypes/UIStatsInfo.h"
#include "UITypes/UIStatusBar.h"
#include "UITypes/UIStatusMessenger.h"
#include "UITypes/UIUserList.h"
#include "UITypes/UIWorldMap.h"
#include "UITypes/UIGuild.h"
#include "UITypes/UIGuildBBS.h"
#include "UITypes/UIMessenger.h"
#include "UITypes/UIPersonalShop.h"
#include "UITypes/UIHiredMerchant.h"
#include "UITypes/UIMinigame.h"
#include "UITypes/UIMonsterBook.h"
#include "UITypes/UIWedding.h"
#include "UITypes/UIPartySearch.h"
#include "UITypes/UIRanking.h"
#include "UITypes/UIFamily.h"
#include "UITypes/UIMapleTV.h"
#include "UITypes/UIMonsterCarnival.h"
#include "UITypes/UIRPSGame.h"
#include "UITypes/UIStorage.h"
#include "UITypes/UITrade.h"
#include "UITypes/UIAlliance.h"
#include "UITypes/UIComboCounter.h"
#include "UITypes/UIMTS.h"
#include "UITypes/UIWhisper.h"
#include "UITypes/UIReport.h"

#include "UITypes/UIMobileItemMenu.h"

#include "../Data/ItemData.h"
#include "../Net/Packets/GameplayPackets.h"
#include "../Net/Session.h"
#include "Window.h"

namespace ms
{
	namespace
	{
		// Touch "Move": the item rides on the next tap, so instead of drawing
		// it under a finger that has already lifted, say what that tap does.
		void draw_carry_hint(const Icon& icon, Range<int16_t> span)
		{
			using namespace MobileUI;

			int32_t id = icon.get_action_id();
			InventoryType::Id from = icon.get_source_tab();
			std::string title = id ? "Moving " + ItemData::get(id).get_name() : "Moving item";
			std::string hint;

			if (from == InventoryType::Id::NONE)
				hint = "Tap an empty bag slot to take it off  \xC2\xB7  tap it again to cancel";
			else if (from == InventoryType::Id::USE && id / 10000 == 204)
				hint = "Tap a worn item to apply it  \xC2\xB7  tap the scroll again to cancel";
			else
				hint = "Tap a slot to place it, or outside the bag to drop it  \xC2\xB7  tap it again to cancel";

			int16_t room = static_cast<int16_t>(std::max(160, span.second() - span.first() - 16));
			int16_t text_room = static_cast<int16_t>(room - 76);
			title = ellipsize(title, Font::BODY_BOLD, text_room);
			hint = ellipsize(hint, Font::SMALL, text_room);

			int16_t w = static_cast<int16_t>(std::max(text_width(title, Font::BODY_BOLD), text_width(hint, Font::SMALL)) + 76);
			int16_t x = static_cast<int16_t>(span.first() + (span.second() - span.first() - w) / 2);
			Rect card = rect(x, 58, w, 54);

			fill(card, Theme::PANEL, 14);
			fill(rect(card.left(), static_cast<int16_t>(card.top() + 10), 4, 34), Theme::ACCENT, 2);

			Rect well = rect(static_cast<int16_t>(card.left() + 14), static_cast<int16_t>(card.top() + 8), 38, 38);
			fill(well, Theme::CARD, 9);
			MobileUI::icon(icon.get_texture(), inset(well, 3, 3));

			text(title, Font::BODY_BOLD, Theme::TEXT, Point<int16_t>(static_cast<int16_t>(well.right() + 12), static_cast<int16_t>(card.top() + 9)));
			text(hint, Font::SMALL, Theme::MUTED, Point<int16_t>(static_cast<int16_t>(well.right() + 12), static_cast<int16_t>(card.top() + 29)));
		}
	}
	UIStateGame::UIStateGame() : stats(Stage::get().get_player().get_stats()), dragged(nullptr)
	{
		focused = UIElement::Type::NONE;
		tooltipparent = Tooltip::Parent::NONE;

#ifdef PLATFORM_IOS
		// Item and equipment face each other so items can be dragged across;
		// stats, skills and quests share the left side with the equipment.
		docks = {
			{ UIElement::Type::ITEMINVENTORY, Side::RIGHT },
			{ UIElement::Type::EQUIPINVENTORY, Side::LEFT },
			{ UIElement::Type::MOBILEINVENTORY, Side::RIGHT },
			{ UIElement::Type::MOBILEEQUIP, Side::LEFT },
			{ UIElement::Type::STATSINFO, Side::LEFT },
			{ UIElement::Type::SKILLBOOK, Side::LEFT },
			{ UIElement::Type::QUESTLOG, Side::LEFT },
			{ UIElement::Type::MOBILEKEYCONFIG, Side::LEFT },
			{ UIElement::Type::MOBILESTATS, Side::LEFT },
			{ UIElement::Type::MOBILESKILLS, Side::LEFT },
			{ UIElement::Type::MOBILEMENU, Side::RIGHT },
			{ UIElement::Type::MOBILESTORE, Side::LEFT },
			{ UIElement::Type::MOBILEADMIN, Side::LEFT },
			{ UIElement::Type::MOBILEQUESTS, Side::LEFT },
			{ UIElement::Type::MOBILESOCIAL, Side::LEFT },
			{ UIElement::Type::MOBILEWORLDMAP, Side::LEFT },
			{ UIElement::Type::MOBILECHANNEL, Side::LEFT },
			{ UIElement::Type::MOBILEOPTIONS, Side::LEFT },
			{ UIElement::Type::MOBILEINBOX, Side::RIGHT },
			{ UIElement::Type::NPCTALK, Side::CENTER },
		};
#endif

		const CharLook& look = Stage::get().get_player().get_look();
		const Inventory& inventory = Stage::get().get_player().get_inventory();

		emplace<UIStatusMessenger>();
		emplace<UIStatusBar>(stats);
		int16_t screen_h = Constants::Constants::get().get_viewheight();

		// The phone's chat window opens over the minimap and the other HUD
		// pieces, so it goes in after them.
		bool chat_on_top = MobileUI::enabled();

		if (!chat_on_top)
			emplace<UIChatBar>(Point<int16_t>(512, screen_h));

		emplace<UIMiniMap>(stats);
		emplace<UIBuffList>();
		emplace<UIShop>(look, inventory);
		emplace<UIQuestHelper>(Stage::get().get_player().get_quests());
		emplace<UIPartyHUD>();

		if (chat_on_top)
			emplace<UIChatBar>(Point<int16_t>(512, screen_h));

		VWIDTH = Constants::Constants::get().get_viewwidth();
		VHEIGHT = Constants::Constants::get().get_viewheight();

		// GM level and world rates, for the menu's Admin panel.
		ServerInfo::request();
	}

	void UIStateGame::draw(float inter, Point<int16_t> cursor) const
	{
		auto scrim_at = first_open_dock();

		for (auto iter = elementorder.begin(); iter != elementorder.end(); ++iter)
		{
			UIElement::Type type = *iter;

			// AVATARBANNER is drawn explicitly by UI::draw after the
			// scrolling world-notice, so it sits above every other layer.
			// Skipping here prevents a double-composite.
			if (type == UIElement::Type::AVATARBANNER)
				continue;

			auto& element = elements[type];

			if (!element || !element->is_active())
				continue;

			if (iter == scrim_at)
			{
				// Dim the game below the side panels, fading in with them.
				float shown = 0.0f;

				for (const Dock& dock : docks)
					if (dock.side != Side::CENTER)
						shown = std::max(shown, dock.progress);

				GraphicsGL::get().drawrectangle(0, 0, VWIDTH, VHEIGHT, 0.0f, 0.0f, 0.0f, 0.40f * shown);
			}

			const Dock* dock = dock_for(type);
			DockGeometry geometry;

			if (dock && dock_geometry(*element, *dock, geometry))
			{
				GraphicsGL::get().begin_transform(geometry.from, geometry.to, geometry.scale);
				element->draw(inter);
				GraphicsGL::get().end_transform();
			}
			else
			{
				element->draw(inter);
			}
		}

		if (tooltip)
			tooltip->draw(cursor + Point<int16_t>(0, 22));

		if (draggedicon)
		{
			if (MobileUI::enabled())
				draw_carry_hint(*draggedicon, free_span());
			else
				draggedicon->dragdraw(cursor);
		}
	}

	void UIStateGame::update()
	{
		// Safe point: no element code is on the stack here, so removed
		// elements parked by remove() can finally be freed.
		graveyard.clear();

		update_docks();

		if (MobileUI::enabled())
			UIMobileStore::watch();

		// The panel an item was picked up from closed: put the item back.
		if (draggedicon)
		{
			auto& owner = elements[icon_owner(draggedicon->get_type())];

			if (MobileUI::enabled() && (!owner || !owner->is_active()))
				remove_icon();
		}

		bool update_screen = false;
		int16_t new_width = Constants::Constants::get().get_viewwidth();
		int16_t new_height = Constants::Constants::get().get_viewheight();

		if (VWIDTH != new_width || VHEIGHT != new_height)
		{
			update_screen = true;
			VWIDTH = new_width;
			VHEIGHT = new_height;

			UI::get().remove(UIElement::Type::STATUSBAR);

			const CharStats& stats = Stage::get().get_player().get_stats();
			emplace<UIStatusBar>(stats);
		}

		for (auto& type : elementorder)
		{
			auto& element = elements[type];

			if (element && element->is_active())
			{
				element->update();

				if (update_screen)
					element->update_screen(new_width, new_height);
			}
		}
	}

	bool UIStateGame::drop_icon(const Icon& icon, Point<int16_t> pos)
	{
		if (UIElement* front = get_front(pos))
		{
			if (scrim_covers(front))
			{
				// Touch "Move" carries the item to this tap: outside the bag
				// means drop it. A mouse drag let go over the dimmed game
				// puts it back instead.
				if (MobileUI::enabled())
					icon.drop_on_stage();

				return true;
			}

			return front->send_icon(icon, to_local(front, pos));
		}
		else if (first_open_dock() != elementorder.end())
		{
			if (MobileUI::enabled())
				icon.drop_on_stage();

			return true;
		}
		else
		{
			icon.drop_on_stage();
		}

		return true;
	}

	void UIStateGame::remove_icon()
	{
		// The drop itself may have rebuilt the owner's icons (purge_icon then
		// already cleared the reference)
		if (draggedicon)
			draggedicon->reset();

		draggedicon = {};
	}

	void UIStateGame::remove_cursors()
	{
		for (auto type : elementorder)
		{
			auto element = elements[type].get();

			if (element && element->is_active())
				element->remove_cursor();
		}
	}

	void UIStateGame::remove_cursor(UIElement::Type t)
	{
		for (auto type : elementorder)
		{
			auto element = elements[type].get();

			if (element && element->is_active() && element->get_type() != t)
				element->remove_cursor();
		}
	}

	void UIStateGame::doubleclick(Point<int16_t> pos)
	{
		if (UIElement* front = get_front(pos))
			if (!scrim_covers(front))
				front->doubleclick(to_local(front, pos));
	}

	void UIStateGame::rightclick(Point<int16_t> pos)
	{
		if (UIElement* front = get_front(pos))
			if (!scrim_covers(front))
				front->rightclick(to_local(front, pos));
	}

	void UIStateGame::send_key(KeyType::Id type, int32_t action, bool pressed, bool escape)
	{
		if (UIElement* focusedelement = get(focused))
		{
			if (focusedelement->is_active())
			{
				return focusedelement->send_key(action, pressed, escape);
			}

			// A stale focus (closed dialog) is dropped and the key handled normally.
			focused = UIElement::NONE;
		}

		{
			// ESC key opens the quit dialog when no other dialog is open
			if (escape && pressed)
			{
				UI::get().emplace<UIYesNo>(
					"Do you want to quit?",
					[](bool yes)
					{
						if (yes)
						{
							float fadestep = 0.025f;

							Window::get().fadeout(
								fadestep,
								[]()
								{
									GraphicsGL::get().clear();
									UI::get().change_state(UI::State::LOGIN);
									UI::get().set_scrollnotice("");
									Session::get().reconnect();
									UI::get().enable();
									Timer::get().start();
									GraphicsGL::get().unlock();
								}
							);

							GraphicsGL::get().lock();
							Stage::get().clear();
							Timer::get().start();
						}
					}
				);
				return;
			}

			switch (type)
			{
				case KeyType::Id::MENU:
				{
					if (pressed)
					{
						switch (action)
						{
							case KeyAction::Id::EQUIPMENT:
							{
								if (MobileUI::enabled())
								{
									MobileUI::open_equip();
									break;
								}

								emplace<UIEquipInventory>(
									Stage::get().get_player().get_inventory()
									);

								break;
							}
							case KeyAction::Id::ITEMS:
							{
								if (MobileUI::enabled())
								{
									MobileUI::open_items();
									break;
								}

								emplace<UIItemInventory>(
									Stage::get().get_player().get_inventory()
									);

								break;
							}
							case KeyAction::Id::STATS:
							{
								if (MobileUI::enabled())
								{
									MobileUI::open_stats();
									break;
								}

								emplace<UIStatsInfo>(
									Stage::get().get_player().get_stats()
									);

								break;
							}
							case KeyAction::Id::SCREENSHOT:
							{
								Window::get().take_screenshot();
								break;
							}
							case KeyAction::Id::SKILLS:
							{
								if (MobileUI::enabled())
								{
									MobileUI::open_skills();
									break;
								}

								emplace<UISkillBook>(
									Stage::get().get_player().get_stats(),
									Stage::get().get_player().get_skills()
									);

								break;
							}
							case KeyAction::Id::FRIENDS:
							case KeyAction::Id::PARTY:
							case KeyAction::Id::BOSSPARTY:
							{
								if (MobileUI::enabled())
								{
									bool party = action != KeyAction::Id::FRIENDS;
									auto social = UI::get().get_element<UIMobileSocial>();

									if (social && social->is_active() && (social->get_tab() == UIMobileSocial::Tab::PARTY) == party)
										social->deactivate();
									else
										MobileUI::open_social(party);

									break;
								}

								UIUserList::Tab tab;

								switch (action)
								{
									case KeyAction::Id::FRIENDS:
										tab = UIUserList::Tab::FRIEND;
										break;
									case KeyAction::Id::PARTY:
										tab = UIUserList::Tab::PARTY;
										break;
									case KeyAction::Id::BOSSPARTY:
										tab = UIUserList::Tab::BOSS;
										break;
								}

								auto userlist = UI::get().get_element<UIUserList>();

								if (userlist && userlist->get_tab() != tab && userlist->is_active())
								{
									userlist->change_tab(tab);
								}
								else
								{
									emplace<UIUserList>(tab);

									if (userlist && userlist->get_tab() != tab && userlist->is_active())
										userlist->change_tab(tab);
								}

								break;
							}
							case KeyAction::Id::WORLDMAP:
							{
								if (MobileUI::enabled())
								{
									auto map = UI::get().get_element<UIMobileWorldMap>();

									if (map && map->is_active())
										map->deactivate();
									else
										MobileUI::open_world_map();

									break;
								}

								emplace<UIWorldMap>();
								break;
							}
							case KeyAction::Id::MAPLECHAT:
							{
								auto chat = UI::get().get_element<UIChat>();

								if (!chat || !chat->is_active())
									emplace<UIChat>();

								break;
							}
							case KeyAction::Id::MINIMAP:
							{
								if (auto minimap = UI::get().get_element<UIMiniMap>())
									minimap->send_key(action, pressed, escape);

								break;
							}
							case KeyAction::Id::QUESTLOG:
							{
								if (MobileUI::enabled())
								{
									auto quests = UI::get().get_element<UIMobileQuests>();

									if (quests && quests->is_active())
										quests->deactivate();
									else
										MobileUI::open_quests();

									break;
								}

								emplace<UIQuestLog>(
									Stage::get().get_player().get_quests()
									);

								break;
							}
							case KeyAction::Id::KEYBINDINGS:
							{
								// The iPhone binds its touch buttons instead.
								if (MobileUI::enabled())
								{
									MobileUI::open_keys();
									break;
								}

								auto keyconfig = UI::get().get_element<UIKeyConfig>();

								if (!keyconfig || !keyconfig->is_active())
								{
									emplace<UIKeyConfig>(
										Stage::get().get_player().get_inventory(),
										Stage::get().get_player().get_skills()
										);
								}
								else if (keyconfig && keyconfig->is_active())
								{
									keyconfig->close();
								}

								break;
							}
							case KeyAction::Id::TOGGLECHAT:
							{
								if (auto chatbar = UI::get().get_element<UIChatBar>())
									if (!chatbar->is_chatfieldopen())
										chatbar->toggle_chat();

								break;
							}
							case KeyAction::Id::MENU:
							{
								if (MobileUI::enabled())
								{
									MobileUI::open_menu();
									break;
								}

								if (auto statusbar = UI::get().get_element<UIStatusBar>())
									statusbar->toggle_menu();

								break;
							}
							case KeyAction::Id::QUICKSLOTS:
							{
								if (auto statusbar = UI::get().get_element<UIStatusBar>())
									statusbar->toggle_qs();

								break;
							}
							case KeyAction::Id::CASHSHOP:
							{
								EnterCashShopPacket().dispatch();
								break;
							}
							case KeyAction::Id::EVENT:
							{
								emplace<UIEvent>();
								break;
							}
							case KeyAction::Id::CHARINFO:
							{
								emplace<UICharInfo>(
									Stage::get().get_player().get_oid()
									);

								break;
							}
							case KeyAction::Id::GUILD:
							{
								emplace<UIGuild>();
								break;
							}
							case KeyAction::Id::FAMILY:
							// v83 Maple never shipped icon 42 so the KeyConfig
							// "Family" slot in this NX ends up being dragged from
							// the MEDALS sprite (action 26). Treat MEDALS as a
							// Family hotkey since MEDALS has no real handler here.
							case KeyAction::Id::MEDALS:
							{
								emplace<UIFamily>();
								break;
							}
							case KeyAction::Id::MONSTERBOOK:
							{
								emplace<UIMonsterBook>();
								break;
							}
							case KeyAction::Id::WHISPER:
							{
								emplace<UIWhisper>();
								break;
							}
							case KeyAction::Id::SAY:
							{
								if (auto chatbar = UI::get().get_element<UIChatBar>())
								{
									chatbar->set_chat_target(UIChatBar::CHT_ALL);
									chatbar->toggle_chatfield(true);
								}
								break;
							}
							case KeyAction::Id::PARTYCHAT:
							{
								if (auto chatbar = UI::get().get_element<UIChatBar>())
								{
									chatbar->set_chat_target(UIChatBar::CHT_PARTY);
									chatbar->toggle_chatfield(true);
								}
								break;
							}
							case KeyAction::Id::FRIENDSCHAT:
							{
								if (auto chatbar = UI::get().get_element<UIChatBar>())
								{
									chatbar->set_chat_target(UIChatBar::CHT_BUDDY);
									chatbar->toggle_chatfield(true);
								}
								break;
							}
							case KeyAction::Id::GUILDCHAT:
							{
								if (auto chatbar = UI::get().get_element<UIChatBar>())
								{
									chatbar->set_chat_target(UIChatBar::CHT_GUILD);
									chatbar->toggle_chatfield(true);
								}
								break;
							}
							case KeyAction::Id::ALLIANCECHAT:
							{
								if (auto chatbar = UI::get().get_element<UIChatBar>())
								{
									chatbar->set_chat_target(UIChatBar::CHT_ALLIANCE);
									chatbar->toggle_chatfield(true);
								}
								break;
							}
							case KeyAction::Id::CHANGECHANNEL:
							{
								emplace<UIChannel>();
								break;
							}
							case KeyAction::Id::MAINMENU:
							{
								UI::get().emplace<UIOptionMenu>();
								break;
							}
							default:
							{
								break;
							}
						}
					}

					break;
				}
				case KeyType::Id::ACTION:
				{
					// Forward Enter key to chatbar to open/close chat input
					if (action == KeyAction::Id::RETURN && pressed)
					{
						if (auto chatbar = UI::get().get_element<UIChatBar>())
						{
							chatbar->send_key(action, pressed, false);
							break;
						}
					}

					Stage::get().send_key(type, action, pressed);
					break;
				}
				case KeyType::Id::FACE:
				case KeyType::Id::ITEM:
				case KeyType::Id::SKILL:
				case KeyType::Id::MACRO:
				{
					Stage::get().send_key(type, action, pressed);
					break;
				}
			}
		}
	}

	Cursor::State UIStateGame::send_cursor(Cursor::State cursorstate, Point<int16_t> cursorpos)
	{
		if (draggedicon)
		{
			if (cursorstate == Cursor::State::CLICKING)
			{
				if (drop_icon(*draggedicon, cursorpos))
					remove_icon();

				return cursorstate;
			}

			return Cursor::State::GRABBING;
		}
		else
		{
			bool clicked = cursorstate == Cursor::State::CLICKING || cursorstate == Cursor::State::VSCROLLIDLE;

			if (auto focusedelement = get(focused))
			{
				if (focusedelement->is_active())
				{
					remove_cursor(focusedelement->get_type());

					// Cursor moves dispatch as IDLE even while the button is
					// held; the dragged-element path below compensates by
					// forcing clicked, but the focused path must do the same
					// or dragging cancels on the first move after mousedown
					return focusedelement->send_cursor(
						clicked || UI::get().is_mouse_held(), to_local(focusedelement, cursorpos));
				}
				else
				{
					focused = UIElement::Type::NONE;

					return cursorstate;
				}
			}
			else
			{
				if (!clicked)
				{
					// Only reset dragged when mouse is actually released,
					// not on idle cursor movement while held
					if (!UI::get().is_mouse_held())
						dragged = nullptr;

					// If still dragging (mouse held), continue the drag
					if (dragged)
						return dragged->send_cursor(true, to_local(dragged, cursorpos));

					auto front = get_front(cursorpos);

					if (front && !scrim_covers(front))
					{
						UIElement::Type front_type = front->get_type();

						if (tooltipparent != UIElement::Type::NONE)
							if (front_type != tooltipparent)
								clear_tooltip(tooltipparent);

						remove_cursor(front_type);

						return front->send_cursor(clicked, to_local(front, cursorpos));
					}
					else
					{
						remove_cursors();

						if (first_open_dock() != elementorder.end())
							return Cursor::State::IDLE;

						return Stage::get().send_cursor(clicked, cursorpos);
					}
				}
				else
				{
					// The item menu is a popover: a press outside it only closes it.
					if (!dragged)
					{
						auto& menu = elements[UIElement::Type::MOBILEITEMMENU];

						if (menu && menu->is_active() && !menu->is_in_range(cursorpos))
						{
							menu->deactivate();

							return Cursor::State::IDLE;
						}
					}

					if (!dragged)
					{
						// A tap on the dimmed game around the side panels
						// closes them.
						if (first_open_dock() != elementorder.end())
						{
							UIElement* front = get_front(cursorpos);

							if (!front || scrim_covers(front))
							{
								close_docks();

								return Cursor::State::IDLE;
							}
						}

						UIElement::Type drag_element_type = UIElement::Type::NONE;

						for (auto iter = elementorder.rbegin(); iter != elementorder.rend(); ++iter)
						{
							auto& element = elements[*iter];

							if (element && element->is_active() && element->is_in_range(to_local(element.get(), cursorpos)))
							{
								dragged = element.get();
								drag_element_type = *iter;
								break;
							}
						}

						if (drag_element_type != UIElement::Type::NONE)
						{
							// Don't reorder fixed HUD elements to avoid them covering each other
							if (drag_element_type != UIElement::Type::STATUSBAR &&
								drag_element_type != UIElement::Type::CHATBAR &&
								drag_element_type != UIElement::Type::BUFFLIST &&
								drag_element_type != UIElement::Type::STATUSMESSENGER)
							{
								elementorder.remove(drag_element_type);
								elementorder.push_back(drag_element_type);
							}
						}
					}

					if (dragged)
						return dragged->send_cursor(clicked, to_local(dragged, cursorpos));
					else
						return Stage::get().send_cursor(clicked, cursorpos);
				}
			}
		}
	}

	void UIStateGame::send_scroll(double yoffset)
	{
		// Route scroll only to the topmost UI under the cursor so the
		// chat bar doesn't consume wheel events when the player is
		// hovering the trade window / inventory / any other panel.
		Point<int16_t> cursorpos = UI::get().get_cursor_position();
		if (UIElement* front = get_front(cursorpos))
		{
			front->send_scroll(yoffset);
			return;
		}

		// No UI under the cursor — fall back to broadcast so chat
		// still scrolls when the player's mouse is over the map.
		for (auto& type : elementorder)
		{
			auto& element = elements[type];
			if (element && element->is_active())
				element->send_scroll(yoffset);
		}
	}

	void UIStateGame::send_close()
	{
		UI::get().emplace<UIYesNo>(
			"Do you want to quit?",
			[](bool yes)
			{
				if (yes)
				{
					float fadestep = 0.025f;

					Window::get().fadeout(
						fadestep,
						[]()
						{
							GraphicsGL::get().clear();
							UI::get().change_state(UI::State::LOGIN);
							UI::get().set_scrollnotice("");
							Session::get().reconnect();
							UI::get().enable();
							Timer::get().start();
							GraphicsGL::get().unlock();
						}
					);

					GraphicsGL::get().lock();
					Stage::get().clear();
					Timer::get().start();
				}
			}
		);
	}

	UIElement::Type UIStateGame::icon_owner(Icon::IconType type)
	{
		// On the iPhone items and worn gear are carried from the mobile panels.
		if (MobileUI::enabled())
		{
			if (type == Icon::IconType::ITEM)
				return UIElement::Type::MOBILEINVENTORY;

			if (type == Icon::IconType::EQUIP)
				return UIElement::Type::MOBILEEQUIP;
		}

		return icon_map[type];
	}

	void UIStateGame::drag_icon(Icon* drgic)
	{
		draggedicon = drgic;
	}

	void UIStateGame::purge_icon(const Icon* icon)
	{
		// The icon object is about to die — just forget the pointer, do NOT
		// touch the object
		if (draggedicon.get() == icon)
			draggedicon = {};
	}

	void UIStateGame::clear_tooltip(Tooltip::Parent parent)
	{
		if (parent == tooltipparent)
		{
			eqtooltip.set_equip(Tooltip::Parent::NONE, 0);
			ittooltip.set_item(0);
			tetooltip.set_text("");
			matooltip.reset();
			tooltip = {};
			tooltipparent = Tooltip::Parent::NONE;
		}
	}

	void UIStateGame::show_equip(Tooltip::Parent parent, int16_t slot)
	{
		eqtooltip.set_equip(parent, slot);

		if (slot)
		{
			tooltip = eqtooltip;
			tooltipparent = parent;
		}
	}

	void UIStateGame::show_item(Tooltip::Parent parent, int32_t itemid)
	{
		ittooltip.set_item(itemid);

		if (itemid)
		{
			tooltip = ittooltip;
			tooltipparent = parent;
		}
	}

	void UIStateGame::show_skill(Tooltip::Parent parent, int32_t skill_id, int32_t level, int32_t masterlevel, int64_t expiration)
	{
		sktooltip.set_skill(skill_id, level, masterlevel, expiration);

		if (skill_id)
		{
			tooltip = sktooltip;
			tooltipparent = parent;
		}
	}

	void UIStateGame::show_text(Tooltip::Parent parent, std::string text)
	{
		tetooltip.set_text(text);

		if (!text.empty())
		{
			tooltip = tetooltip;
			tooltipparent = parent;
		}
	}

	void UIStateGame::show_map(Tooltip::Parent parent, std::string name, std::string description, int32_t mapid, bool bolded)
	{
		matooltip.set_name(parent, name, bolded);
		matooltip.set_desc(description);
		matooltip.set_mapid(mapid);

		if (!name.empty())
		{
			tooltip = matooltip;
			tooltipparent = parent;
		}
	}

	template <class T, typename...Args>
	void UIStateGame::emplace(Args&& ...args)
	{
		if (auto iter = pre_add(T::TYPE, T::TOGGLED, T::FOCUSED))
		{
			(*iter).second = std::make_unique<T>(
				std::forward<Args>(args)...
				);

			auto silent_types = {
				UIElement::Type::STATUSMESSENGER,
				UIElement::Type::STATUSBAR,
				UIElement::Type::CHATBAR,
				UIElement::Type::MINIMAP,
				UIElement::Type::BUFFLIST,
				UIElement::Type::NPCTALK,
				UIElement::Type::SHOP
			};

			if (std::find(silent_types.begin(), silent_types.end(), T::TYPE) == silent_types.end())
			{
				if (T::TYPE == UIElement::Type::WORLDMAP)
					Sound(Sound::Name::WORLDMAPOPEN).play();
				else
					Sound(Sound::Name::MENUUP).play();

				UI::get().send_cursor(false);
			}
		}
	}

	UIState::Iterator UIStateGame::pre_add(UIElement::Type type, bool is_toggled, bool is_focused)
	{
		auto& element = elements[type];

		if (element && is_toggled)
		{
			elementorder.remove(type);
			elementorder.push_back(type);

			bool active = element->is_active();

			element->toggle_active();

			if (active != element->is_active())
			{
				if (element->is_active())
				{
					if (type == UIElement::Type::WORLDMAP)
						Sound(Sound::Name::WORLDMAPOPEN).play();
					else
						Sound(Sound::Name::MENUUP).play();

					UI::get().send_cursor(false);
				}
				else
				{
					if (type == UIElement::Type::WORLDMAP)
						Sound(Sound::Name::WORLDMAPCLOSE).play();
					else
						Sound(Sound::Name::MENUDOWN).play();

					element->remove_cursor();

					if (draggedicon)
						if (element->get_type() == icon_owner(draggedicon.get()->get_type()))
							remove_icon();

					UI::get().send_cursor(false);
				}
			}

			return elements.end();
		}
		else
		{
			remove(type);
			elementorder.push_back(type);

			if (is_focused)
				focused = type;

			return elements.find(type);
		}
	}

	void UIStateGame::remove(UIElement::Type type)
	{
		if (type == focused)
			focused = UIElement::Type::NONE;

		if (type == tooltipparent)
			clear_tooltip(tooltipparent);

		// If the element being removed owns the icon currently being dragged,
		// cancel the drag now. Otherwise draggedicon keeps pointing into the
		// element after it is parked in the graveyard and freed next update(),
		// and the following dragdraw() reads freed memory (access violation).
		if (draggedicon && icon_owner(draggedicon.get()->get_type()) == type)
			remove_icon();

		elementorder.remove(type);

		if (auto& element = elements[type])
		{
			element->deactivate();

			// Park instead of deleting in place — remove() can be reached
			// from inside the element's own handler (e.g. a dialog's ok
			// callback), and freeing it here would be a use-after-free.
			// update() clears the graveyard next tick.
			graveyard.push_back(std::move(element));
		}
	}

	UIElement* UIStateGame::get(UIElement::Type type)
	{
		return elements[type].get();
	}

	UIElement* UIStateGame::get_front(std::list<UIElement::Type> types)
	{
		auto begin = elementorder.rbegin();
		auto end = elementorder.rend();

		for (auto iter = begin; iter != end; ++iter)
		{
			if (std::find(types.begin(), types.end(), *iter) != types.end())
			{
				auto& element = elements[*iter];

				if (element && element->is_active())
					return element.get();
			}
		}

		return nullptr;
	}

	UIElement* UIStateGame::get_front(Point<int16_t> pos)
	{
		auto begin = elementorder.rbegin();
		auto end = elementorder.rend();

		for (auto iter = begin; iter != end; ++iter)
		{
			auto& element = elements[*iter];

			if (element && element->is_active() && element->is_in_range(to_local(element.get(), pos)))
				return element.get();
		}

		return nullptr;
	}

	// ---- iPhone side panels ----

	bool UIStateGame::docking() const
	{
		return !docks.empty();
	}

	const UIStateGame::Dock* UIStateGame::dock_for(UIElement::Type type) const
	{
		for (const Dock& dock : docks)
			if (dock.type == type)
				return &dock;

		return nullptr;
	}

	bool UIStateGame::dock_geometry(const UIElement& element, const Dock& dock, DockGeometry& geometry) const
	{
		if (!element.is_active())
			return false;

		Point<int16_t> dim = element.get_dimension();

		if (dim.x() <= 0 || dim.y() <= 0)
			return false;

		// As large as fits the height with a small margin, capped so the two
		// sides still leave the middle of the game visible.
		constexpr int16_t MARGIN = 6;
		float fit = static_cast<float>(VHEIGHT - MARGIN * 2) / dim.y();

		if (dock.side == Side::CENTER)
		{
			// Grown about its own centre, above the status bar.
			float wide = static_cast<float>(VWIDTH - MARGIN * 2) / dim.x();
			float scale = std::clamp(std::min({ fit, wide, 1.4f }), 1.0f, 1.4f);
			Point<int16_t> pos = element.get_position();
			Point<int16_t> centre(
				static_cast<int16_t>(pos.x() + dim.x() / 2),
				static_cast<int16_t>(pos.y() + dim.y() / 2));
			int16_t half_h = static_cast<int16_t>(std::lround(dim.y() * scale / 2));
			int16_t cy = static_cast<int16_t>(std::clamp<int>(centre.y(), MARGIN + half_h, VHEIGHT - 72 - half_h));

			geometry.from = centre;
			geometry.to = Point<int16_t>(centre.x(), cy);
			geometry.scale = scale;

			return true;
		}

		float scale = std::clamp(fit, 1.0f, 1.6f);
		bool right = dock.side == Side::RIGHT;

		int16_t width = static_cast<int16_t>(std::lround(dim.x() * scale));
		int16_t height = static_cast<int16_t>(std::lround(dim.y() * scale));

		// Slide in from the edge: ease-out over the opening.
		float t = std::clamp(dock.progress, 0.0f, 1.0f);
		float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
		int16_t slide = static_cast<int16_t>(std::lround((1.0f - eased) * (width + MARGIN)));

		int16_t x = right ? static_cast<int16_t>(VWIDTH - MARGIN - width + slide)
			: static_cast<int16_t>(MARGIN - slide);
		int16_t y = static_cast<int16_t>(std::max<int>(MARGIN, (VHEIGHT - height) / 2));

		geometry.from = element.get_position();
		geometry.to = Point<int16_t>(x, y);
		geometry.scale = scale;

		return true;
	}

	Point<int16_t> UIStateGame::to_local(const UIElement* element, Point<int16_t> screen) const
	{
		if (!element)
			return screen;

		const Dock* dock = dock_for(element->get_type());
		DockGeometry geometry;

		if (!dock || !dock_geometry(*element, *dock, geometry))
			return screen;

		return Point<int16_t>(
			static_cast<int16_t>(geometry.from.x() + std::lround((screen.x() - geometry.to.x()) / geometry.scale)),
			static_cast<int16_t>(geometry.from.y() + std::lround((screen.y() - geometry.to.y()) / geometry.scale)));
	}

	std::list<UIElement::Type>::const_iterator UIStateGame::first_open_dock() const
	{
		if (!docking())
			return elementorder.end();

		for (auto iter = elementorder.begin(); iter != elementorder.end(); ++iter)
		{
			auto& element = elements[*iter];

			const Dock* dock = dock_for(*iter);

			if (element && element->is_active() && dock && dock->side != Side::CENTER)
				return iter;
		}

		return elementorder.end();
	}

	bool UIStateGame::scrim_covers(const UIElement* front) const
	{
		auto scrim_at = first_open_dock();

		if (!front || scrim_at == elementorder.end())
			return false;

		// Covered when it is drawn before the first side panel, i.e. under the
		// dimming.
		for (auto iter = elementorder.begin(); iter != scrim_at; ++iter)
			if (elements[*iter].get() == front)
				return true;

		return false;
	}

	void UIStateGame::close_docks()
	{
		bool closed = false;

		for (const Dock& dock : docks)
		{
			auto& element = elements[dock.type];

			if (dock.side != Side::CENTER && element && element->is_active())
			{
				element->remove_cursor();
				element->deactivate();
				closed = true;
			}
		}

		if (closed)
		{
			if (tooltipparent != Tooltip::Parent::NONE)
				clear_tooltip(tooltipparent);

			Sound(Sound::Name::MENUDOWN).play();
		}
	}

	void UIStateGame::update_docks()
	{
		for (Dock& dock : docks)
		{
			auto& element = elements[dock.type];
			bool open = element && element->is_active();

			if (element)
				element->set_docked(true);

			if (!open)
			{
				dock.progress = 0.0f;
				continue;
			}

			if (dock.progress == 0.0f && dock.side != Side::CENTER)
			{
				// Just opened: one panel per side.
				for (const Dock& other : docks)
				{
					if (&other == &dock || other.side != dock.side)
						continue;

					auto& sibling = elements[other.type];

					if (sibling && sibling->is_active())
					{
						sibling->remove_cursor();
						sibling->deactivate();
					}
				}

				elementorder.remove(dock.type);
				elementorder.push_back(dock.type);
			}

			// ~130 ms to slide all the way in.
			dock.progress = std::min(1.0f, dock.progress + Constants::TIMESTEP / 130.0f);
		}
	}

	bool UIStateGame::npc_talk_open() const
	{
		auto& element = elements[UIElement::Type::NPCTALK];

		return element && element->is_active();
	}

	bool UIStateGame::holds_press() const
	{
		if (focused != UIElement::Type::NONE)
		{
			const auto& focusedelement = elements[focused];

			if (focusedelement && focusedelement->is_active())
				return focusedelement->captures_hold();
		}

		return dragged && dragged->captures_hold();
	}

	Range<int16_t> UIStateGame::free_span() const
	{
		int16_t left = 0;
		int16_t right = VWIDTH;

		for (const Dock& dock : docks)
		{
			if (dock.side == Side::CENTER)
				continue;

			const auto& element = elements[dock.type];
			DockGeometry geometry;

			if (!element || !dock_geometry(*element, dock, geometry))
				continue;

			int16_t width = static_cast<int16_t>(std::lround(element->get_dimension().x() * geometry.scale));

			if (dock.side == Side::LEFT)
				left = std::max<int16_t>(left, static_cast<int16_t>(geometry.to.x() + width));
			else
				right = std::min<int16_t>(right, geometry.to.x());
		}

		return Range<int16_t>(left, right);
	}

	bool UIStateGame::side_panel_open() const
	{
		return first_open_dock() != elementorder.end();
	}
}