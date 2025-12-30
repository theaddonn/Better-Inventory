#include "dllmain.hpp"

ClientInstance* client;

ShulkerRenderer shulkerRenderer;
ItemStack shulkerInventory[SHULKER_CACHE_SIZE][27];

Amethyst::InlineHook<decltype(&Item::appendFormattedHovertext)> _Item_appendFormattedHovertext;
Amethyst::InlineHook<decltype(&ShulkerBoxBlockItem::appendFormattedHovertext)> _ShulkerBoxBlockItem_appendFormattedHovertext;
Amethyst::InlineHook<decltype(&HoverRenderer::_renderHoverBox)> _HoverRenderer__renderHoverBox;

static void Item_appendFormattedHovertext(Item* self, const ItemStackBase& itemStack, Level& level, std::string& text, uint8_t a5) {
	_Item_appendFormattedHovertext(self, itemStack, level, text, a5);

	Item* item = itemStack.mItem;
	uint64_t maxDamage = item->getMaxDamage();

	if (maxDamage != 0) {
		uint64_t current = maxDamage - item->getDamageValue(itemStack.mUserData);
		text.append(std::format("\n{}7Durability: {} / {}{}r", "\xc2\xa7", current, maxDamage, "\xc2\xa7"));
	}

	std::string rawNameId = std::string(item->mRawNameId.c_str());

	if (rawNameId == "bee_nest" || rawNameId == "beehive") {
		AppendBeeNestInformation(text, itemStack);
	}

	if (rawNameId.find("shulker_box") != std::string::npos) {
		// Don't append the id for shulker boxes (makes it too long)
		text.append(std::format("\n{}8{}:{}{}r", "\xc2\xa7", item->mNamespace, rawNameId, "\xc2\xa7"));
		return;
	}

	text.append(std::format("\n{}8{}:{} ({}){}r", "\xc2\xa7", item->mNamespace, rawNameId, item->mId, "\xc2\xa7"));
}

static void AppendBeeNestInformation(std::string& text, const ItemStackBase& itemStack) {
	CompoundTag* userData = itemStack.mUserData;

	// There are no bees in the bee nest
	if (userData == nullptr || !userData->contains("Occupants")) {
		text.append(std::format("\n{}5Contains 0 bees", "\xc2\xa7"));
		return;
	};

	ListTag* occupants = userData->getList("Occupants");
	text.append(std::format("\n{}5Contains {:d} bee{}", "\xc2\xa7", occupants->size(), occupants->size() > 1 ? "s" : ""));
}

int index = 0;

static void ShulkerBoxBlockItem_appendFormattedHovertext(ShulkerBoxBlockItem* self, const ItemStackBase& itemStack, Level& level, std::string& text, uint8_t someBool) {
	// Use the appendFormattedHovertext for regular items, we don't want the list of items
	Item_appendFormattedHovertext(self, itemStack, level, text, someBool);

	index++;
	if (index >= SHULKER_CACHE_SIZE) index = 0;

	// Hide a secret index in the shulker name
	// We do this because appendFormattedHovertext gets called for the neightboring items so if there is a shulker
	// to the right of this one then its preview will get overriden, so we keep track of multiple at once using a rolling identifier
	text += std::format("{}{:x}", "\xc2\xa7", index);
	int thisIndex = index;

	// Reset all the currrent item stacks
	for (auto& itemStack : shulkerInventory[index]) {
		itemStack = ItemStack();
	}

	if (!itemStack.mUserData) return;
	if (!itemStack.mUserData->contains("Items")) return;

	const ListTag* items = itemStack.mUserData->getList("Items");

	for (int i = 0; i < items->size(); i++) {
		const CompoundTag* itemCompound = items->getCompound(i);
		byte slot = itemCompound->getByte("Slot");
		shulkerInventory[thisIndex][slot]._loadItem(itemCompound);
	}
}

static void HoverRenderer__renderHoverBox(HoverRenderer* self, MinecraftUIRenderContext* ctx, IClientInstance* client, RectangleArea* aabb, float someFloat) {
	// Freddie:
	// This is really bad code, it is relying on the fact that I have also hooked appendFormattedHovertext for items to append the item identifier
	// I have no idea where the currently hovered item is stored in the game! I can't find any references to it, so it might be set in some weird place?
	// 
	// Lucy:
	// No clue either but this should be more solid

	if (self->mFilteredContent.find("shulker_box") != std::string::npos) {
		// Find the position right after "shulker_box§r"
		size_t shulkerPos = self->mFilteredContent.find("shulker_box");
		if (shulkerPos != std::string::npos) {
			// Look for the hex index after "shulker_box§r"
			// The pattern is: "shulker_box§r§X" where X is our hex digit
			size_t searchStart = shulkerPos + 11; // length of "shulker_box"

			// Skip the "§r" that follows
			if (searchStart + 2 < self->mFilteredContent.size() &&
				self->mFilteredContent[searchStart] == '\xc2' &&
				self->mFilteredContent[searchStart + 1] == '\xa7' &&
				self->mFilteredContent[searchStart + 2] == 'r') {
				searchStart += 3;
			}

			// Now look for our index marker "§X" where X is the hex digit
			if (searchStart + 2 < self->mFilteredContent.size() &&
				self->mFilteredContent[searchStart] == '\xc2' &&
				self->mFilteredContent[searchStart + 1] == '\xa7') {

				char hexChar = self->mFilteredContent[searchStart + 2];

				try {
					std::string hexStr(1, hexChar);
					int index = std::stoi(hexStr, nullptr, 16);

					if (index >= 0 && index < SHULKER_CACHE_SIZE) {
						shulkerRenderer.Render(ctx, self, index);
						return;
					}
				}
				catch (...) {
					// Fall through to default rendering
				}
			}
		}
	}

	_HoverRenderer__renderHoverBox(self, ctx, client, aabb, someFloat);
}

ModFunction void Initialize(AmethystContext& ctx, const Amethyst::Mod& mod) {
	Amethyst::InitializeAmethystMod(ctx, mod);

	auto& hooks = Amethyst::GetHookManager();

	VHOOK(Item, appendFormattedHovertext, this);
	VHOOK(ShulkerBoxBlockItem, appendFormattedHovertext, this);
	HOOK(HoverRenderer, _renderHoverBox, this);
}