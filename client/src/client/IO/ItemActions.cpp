//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
#include "ItemActions.h"

#include "UI.h"

#include "UITypes/UIAvatarMegaphone.h"
#include "UITypes/UIItemInventory.h"
#include "UITypes/UIItemMegaphone.h"
#include "UITypes/UIMapleTV.h"
#include "UITypes/UIMegaphone.h"
#include "UITypes/UINotice.h"
#include "UITypes/UIOwl.h"

#include "../Audio/Audio.h"
#include "../Data/EquipData.h"
#include "../Data/ItemData.h"
#include "../Gameplay/MiniRooms.h"
#include "../Gameplay/Stage.h"
#include "../Net/Packets/InventoryPackets.h"
#include "../Net/Packets/TradePackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// An inventory item on its way to a slot, a worn item, a shop, a trade ...
		class CarriedItem : public Icon::Type
		{
		public:
			CarriedItem(InventoryType::Id tab, int16_t slot, int32_t item_id, int16_t count, bool untradable, bool cash) :
				tab(tab), slot(slot), item_id(item_id), count(count), untradable(untradable), cash(cash) {}

			void drop_on_stage() const override
			{
				ItemActions::drop(tab, slot, item_id, count, untradable, cash);
			}

			void drop_on_equips(EquipSlot::Id eqslot) const override
			{
				switch (tab)
				{
					case InventoryType::Id::EQUIP:
						if (ItemActions::fits_slot(item_id, eqslot) && ItemActions::can_wear(slot))
							EquipItemPacket(slot, ItemActions::wear_slot(item_id, eqslot)).dispatch();

						Sound(Sound::Name::DRAGEND).play();
						break;
					case InventoryType::Id::USE:
						ScrollEquipPacket(slot, eqslot).dispatch();
						break;
					default:
						break;
				}
			}

			bool drop_on_items(InventoryType::Id target_tab, EquipSlot::Id, int16_t target_slot, bool) const override
			{
				if (target_tab == tab && target_slot > 0 && target_slot != slot)
					MoveItemPacket(tab, slot, target_slot, 1).dispatch();

				return true;
			}

			void drop_on_bindings(Point<int16_t>, bool) const override {}
			void set_count(int16_t c) override { count = c; }
			Icon::IconType get_type() override { return Icon::IconType::ITEM; }
			int16_t get_source_slot() const override { return slot; }
			InventoryType::Id get_source_tab() const override { return tab; }
			int32_t get_action_id() const override { return item_id; }

		private:
			InventoryType::Id tab;
			int16_t slot;
			int32_t item_id;
			int16_t count;
			bool untradable;
			bool cash;
		};

		class CarriedWornItem : public Icon::Type
		{
		public:
			CarriedWornItem(EquipSlot::Id source, int32_t item_id) : source(source), item_id(item_id) {}

			void drop_on_stage() const override
			{
				Sound(Sound::Name::DRAGEND).play();
			}

			void drop_on_equips(EquipSlot::Id) const override
			{
				Sound(Sound::Name::DRAGEND).play();
			}

			bool drop_on_items(InventoryType::Id tab, EquipSlot::Id, int16_t slot, bool equip) const override
			{
				// Onto an empty Equip slot: take it off there. Onto another
				// equip that fits here: swap them.
				if (tab != InventoryType::Id::EQUIP || slot <= 0)
					return true;

				const Inventory& inventory = Stage::get().get_player().get_inventory();
				int32_t target = inventory.get_item_id(InventoryType::Id::EQUIP, slot);

				if (!target)
					UnequipItemPacket(source, slot).dispatch();
				else if (ItemActions::wear_slot(target, source) == source && ItemActions::can_wear(slot))
					EquipItemPacket(slot, static_cast<int16_t>(source)).dispatch();

				return true;
			}

			void drop_on_bindings(Point<int16_t>, bool) const override {}
			void set_count(int16_t) override {}
			Icon::IconType get_type() override { return Icon::IconType::EQUIP; }
			int32_t get_action_id() const override { return item_id; }

		private:
			EquipSlot::Id source;
			int32_t item_id;
		};
	}

	namespace ItemActions
	{
		const char* requirement_problem(int32_t item_id)
		{
			const CharStats& stats = Stage::get().get_player().get_stats();
			const EquipData& equipdata = EquipData::get(item_id);
			const ItemData& itemdata = equipdata.get_itemdata();

			int8_t gender = itemdata.get_gender();
			bool female = stats.get_female();

			if ((gender == 0 && female) || (gender == 1 && !female))
				return "This item is for the other gender.";

			const std::string& jobname = stats.get_jobname();

			if (jobname == "GM" || jobname == "SuperGM")
				return nullptr;

			if (!stats.get_job().meets_req_job(equipdata.get_reqstat(MapleStat::Id::JOB)))
				return "Your current job\\ncannot equip the selected item.";

			if (equipdata.get_reqstat(MapleStat::Id::LEVEL) > stats.get_stat(MapleStat::Id::LEVEL)
				|| equipdata.get_reqstat(MapleStat::Id::DEX) > stats.get_total(EquipStat::Id::DEX)
				|| equipdata.get_reqstat(MapleStat::Id::STR) > stats.get_total(EquipStat::Id::STR)
				|| equipdata.get_reqstat(MapleStat::Id::LUK) > stats.get_total(EquipStat::Id::LUK)
				|| equipdata.get_reqstat(MapleStat::Id::INT) > stats.get_total(EquipStat::Id::INT)
				|| equipdata.get_reqstat(MapleStat::Id::FAME) > stats.get_honor())
				return "Your stats are too low to equip this item\\nor you do not meet the job requirement.";

			return nullptr;
		}

		bool meets_requirements(int32_t item_id)
		{
			return requirement_problem(item_id) == nullptr;
		}

		bool is_ring_slot(int16_t slot)
		{
			switch (EquipSlot::base_of(slot))
			{
				case EquipSlot::Id::RING1:
				case EquipSlot::Id::RING2:
				case EquipSlot::Id::RING3:
				case EquipSlot::Id::RING4:
					return true;
				default:
					return false;
			}
		}

		bool fits_slot(int32_t item_id, int16_t slot)
		{
			if (EquipSlot::is_pet(slot) || EquipSlot::pet_slot(item_id))
				return EquipSlot::pet_slot(item_id) == slot;

			EquipSlot::Id own = EquipData::get(item_id).get_eqslot();
			int16_t base = EquipSlot::base_of(slot);

			return own == base || (is_ring_slot(own) && is_ring_slot(base));
		}

		int16_t wear_slot(int32_t item_id, int16_t near)
		{
			if (int16_t pet = EquipSlot::pet_slot(item_id))
				return pet;

			const Inventory& inventory = Stage::get().get_player().get_inventory();
			bool cash = ItemData::get(item_id).is_cashitem();
			int16_t offset = cash ? EquipSlot::CASH_OFFSET : 0;
			EquipSlot::Id own = EquipData::get(item_id).get_eqslot();

			// A ring goes where it was dropped, else into the first free ring.
			if (is_ring_slot(own))
			{
				if (near && is_ring_slot(near))
					return static_cast<int16_t>(EquipSlot::base_of(near) + offset);

				for (EquipSlot::Id ring : { EquipSlot::Id::RING1, EquipSlot::Id::RING2, EquipSlot::Id::RING3, EquipSlot::Id::RING4 })
					if (!inventory.get_item_id(InventoryType::Id::EQUIPPED, static_cast<int16_t>(ring + offset)))
						return static_cast<int16_t>(ring + offset);

				return static_cast<int16_t>(EquipSlot::Id::RING1 + offset);
			}

			return static_cast<int16_t>(own + offset);
		}

		bool can_wear(int16_t slot)
		{
			const Player& player = Stage::get().get_player();

			if (player.get_look().get_alerted())
			{
				UI::get().emplace<UIOk>("You cannot complete this action right now.\\nEvade the attack and try again.", [](bool) {});
				return false;
			}

			int32_t item_id = player.get_inventory().get_item_id(InventoryType::Id::EQUIP, slot);

			if (const char* problem = requirement_problem(item_id))
			{
				UI::get().emplace<UIOk>(problem, [](bool) {});
				return false;
			}

			return true;
		}

		void use(InventoryType::Id itemtab, int16_t slot)
		{
			const Inventory& inventory = Stage::get().get_player().get_inventory();

			{
				if (int32_t item_id = inventory.get_item_id(itemtab, slot))
				{
					// Consumables with an ItemEff sprite play their effect once on
					// use (the server only shows it to other players, so do it here
					// for ourselves). Persistent auras come from equipped rings, not
					// from consumables.
					if (nl::nx::effect["ItemEff.img"][std::to_string(item_id)])
						Stage::get().get_player().show_item_use_effect(item_id);

					switch (itemtab)
					{
						case InventoryType::Id::EQUIP:
						{
							if (can_wear(slot))
								EquipItemPacket(slot, wear_slot(item_id)).dispatch();

							break;
						}
						case InventoryType::Id::USE:
						{
							if (item_id / 10000 == 212)
								PetFoodPacket(slot, item_id).dispatch();
							else
								UseItemPacket(slot, item_id).dispatch();

							break;
						}
						case InventoryType::Id::SETUP:
						{
							Stage::get().get_player().use_item(item_id);
							break;
						}
						case InventoryType::Id::CASH:
						{
							if (item_id / 10000 == 503)
							{
								MiniRooms::get().set_pending_permit(item_id);
								HiredMerchantRequestPacket().dispatch();
								break;
							}

							if (item_id / 10000 == 514)
							{
								UI::get().emplace<UIEnterText>(
									"Shop name:",
									[item_id](const std::string& desc)
									{
										if (!desc.empty())
											CreateShopPacket(desc, item_id, 4).dispatch();
									},
									30
								);
								break;
							}

							if (item_id / 10000 == 523)
							{
								UI::get().remove(UIElement::Type::OWL);
								UI::get().emplace<UIOwl>(slot, item_id);
								break;
							}

							if (item_id / 10000 == 500)
							{
								SpawnPetPacket(slot).dispatch();
								break;
							}

							// Hair, face, lens and skin coupons: the server opens
							// the salon that takes the coupon.
							if (item_id / 10000 == 515)
							{
								UseCashItemPacket(slot, item_id).dispatch();
								break;
							}

							if (item_id / 10000 == 517)
							{
								UI::get().emplace<UIEnterText>(
									"New pet name:",
									[slot, item_id](const std::string& newname)
									{
										if (!newname.empty())
											UsePetNameTagPacket(slot, item_id, newname).dispatch();
									},
									12
								);
								break;
							}

							// Avatar megaphones (539xxxx): Diablo / Cloud 9 /
							// Lion King — open the AvatarMegaphone compose.
							if (item_id / 10000 == 539)
							{
								UI::get().remove(UIElement::Type::AVATARMEGAPHONE);
								UI::get().emplace<UIAvatarMegaphone>();
								if (auto am = UI::get().get_element<UIAvatarMegaphone>())
									am->configure_item(slot, item_id);
								break;
							}

							// Cash megaphones/speakers: prompt for message text,
							// then dispatch USE_CASH_ITEM with the appropriate
							// sub-type payload. Non-megaphone cash items fall
							// through to a generic USE_ITEM dispatch.
							int32_t category = item_id / 10000;
							if (category == 507)
							{
								int32_t subtype = (item_id / 1000) % 10;
								int16_t use_slot = slot;
								int32_t use_id = item_id;

								// Maple TV (5075xxx) — dedicated UIMapleTV window.
								if (subtype == 5)
								{
									UI::get().remove(UIElement::Type::MAPLETV);
									UI::get().emplace<UIMapleTV>();
									if (auto tv = UI::get().get_element<UIMapleTV>())
										tv->configure_item(use_slot, use_id);
									break;
								}

								// Regular / Super / Triple megaphone (5070/5071/
								// 5072/5077) — all share the Megaphone compose
								// window. Sub-type 7 reveals the 2 extra text rows;
								// sub-type 2 swaps the bg to backgrnd_super.
								if (subtype == 0 || subtype == 1 || subtype == 2 || subtype == 7)
								{
									UI::get().remove(UIElement::Type::MEGAPHONE);
									UI::get().emplace<UIMegaphone>();
									if (auto m = UI::get().get_element<UIMegaphone>())
										m->configure_item(use_slot, use_id);
									break;
								}

								// Item Megaphone (5076xxx) — opens the dedicated
								// UIItemMegaphone window.
								if (subtype == 6)
								{
									UI::get().remove(UIElement::Type::ITEMMEGAPHONE);
									UI::get().emplace<UIItemMegaphone>();
									if (auto im = UI::get().get_element<UIItemMegaphone>())
										im->configure_item(use_slot, use_id);
									break;
								}
							}
							else
							{
								UseItemPacket(slot, item_id).dispatch();
							}
							break;
						}
					}
				}
			}
		}

		void unequip(EquipSlot::Id slot)
		{
			const Inventory& inventory = Stage::get().get_player().get_inventory();

			if (!inventory.get_item_id(InventoryType::Id::EQUIPPED, slot))
				return;

			if (int16_t freeslot = inventory.find_free_slot(InventoryType::Id::EQUIP))
				UnequipItemPacket(slot, freeslot).dispatch();
			else
				UI::get().emplace<UIOk>("Your equipment inventory is full.", [](bool) {});
		}

		void drop(InventoryType::Id tab, int16_t slot, int32_t item_id, int16_t count, bool untradable, bool cash)
		{
			if (cash)
			{
				UI::get().emplace<UIOk>("You can't drop this item.", [](bool) {});
				return;
			}

			auto throw_away = [tab, slot, count]()
			{
				if (count <= 1)
				{
					MoveItemPacket(tab, slot, 0, 1).dispatch();
					return;
				}

				UI::get().emplace<UIEnterNumber>(
					"How many will you drop?",
					[tab, slot](int32_t qty) { MoveItemPacket(tab, slot, 0, static_cast<int16_t>(qty)).dispatch(); },
					count, count
				);
			};

			if (untradable)
			{
				UI::get().emplace<UIYesNo>(
					"This item can't be taken back once thrown away.\\nWill you still drop it?",
					[throw_away](bool yes) { if (yes) throw_away(); }
				);
			}
			else
			{
				throw_away();
			}
		}

		std::unique_ptr<Icon> make_item_icon(InventoryType::Id tab, int16_t slot)
		{
			const Inventory& inventory = Stage::get().get_player().get_inventory();
			int32_t item_id = inventory.get_item_id(tab, slot);

			if (!item_id)
				return nullptr;

			const ItemData& data = ItemData::get(item_id);
			int16_t count = tab == InventoryType::Id::EQUIP ? -1 : inventory.get_item_count(tab, slot);

			return std::make_unique<Icon>(
				std::make_unique<CarriedItem>(tab, slot, item_id, count, data.is_untradable(), data.is_cashitem()),
				data.get_icon(false), count
			);
		}

		std::unique_ptr<Icon> make_worn_icon(EquipSlot::Id slot)
		{
			const Inventory& inventory = Stage::get().get_player().get_inventory();
			int32_t item_id = inventory.get_item_id(InventoryType::Id::EQUIPPED, slot);

			if (!item_id)
				return nullptr;

			return std::make_unique<Icon>(std::make_unique<CarriedWornItem>(slot, item_id), ItemData::get(item_id).get_icon(false), -1);
		}
	}
}
