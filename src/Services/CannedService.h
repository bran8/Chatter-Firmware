#ifndef CHATTER_FIRMWARE_CANNEDSERVICE_H
#define CHATTER_FIRMWARE_CANNEDSERVICE_H

#include <string>
#include <cstddef>

/**
 * Persistent store for the 10 long-press "canned" messages shared by the Convo
 * and Broadcast screens (one shared set, not per-screen).
 *
 * Slots are kept in keypad display order: slot 0..8 map to keys 1..9, slot 9
 * maps to key 0 -- matching how the messages were historically assigned to
 * buttons. Persisted to /canned.txt on SPIFFS as one line per slot; an empty
 * line means that slot is disabled (no canned message on that button). Seeded
 * with the historical defaults on first boot. Mirrors CustomDictService's
 * flat-file SPIFFS pattern.
 *
 *   Settings/CannedScreen --set/reset--> Canned <--get-- TextEntry / screens
 */
class CannedService {
public:
	static constexpr size_t Count = 10;

	// Load the persisted messages, seeding + writing defaults if the file is
	// absent. Call once after SPIFFS is mounted.
	void begin();

	// Text for a slot; empty string for an out-of-range or disabled slot.
	const std::string& get(size_t slot) const;
	// Replace a slot (empty disables that button) and persist immediately.
	void set(size_t slot, const std::string& text);
	// Restore and persist the original 10 phrases.
	void resetDefaults();

	static const char* defaultText(size_t slot);  // historical default for a slot
	static const char* keyLabel(size_t slot);      // "1".."9","0" for the UI

private:
	std::string slots[Count];

	void load();
	void save();
};

extern CannedService Canned;

#endif //CHATTER_FIRMWARE_CANNEDSERVICE_H
