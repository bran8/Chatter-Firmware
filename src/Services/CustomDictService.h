#ifndef CHATTER_FIRMWARE_CUSTOMDICTSERVICE_H
#define CHATTER_FIRMWARE_CUSTOMDICTSERVICE_H

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <cstdint>

/**
 * Learns words from sent/received messages and offers them as high-priority T9
 * candidates. Lives as a standalone global (like Buzz / Messages) so it both
 * outlives the transient TextEntry instances AND avoids the Elements<->Services
 * include cycle that would arise if the trie lookup reached into MessageService.
 *
 *   MessageService  --learnText-->  CustomDict  <--getMatches--  TextEntry
 *
 * Persisted to /custom_dict.txt on SPIFFS as one "word<TAB>count" line each.
 */
class CustomDictService {
public:
	// Load the persisted dictionary. Call once after SPIFFS is mounted.
	void begin();

	// Split `text` into lowercase alphabetic words, increment their counts, and
	// persist. Words shorter than 2 letters are ignored.
	void learnText(const std::string& text);

	// Custom words whose T9 encoding starts with `digits`, most-used first.
	std::vector<std::pair<std::string, uint32_t>> getMatches(
			const std::string& digits, size_t maxResults = 8);

private:
	std::unordered_map<std::string, uint32_t> dict;

	void load();
	void save();

	// Encode an a-z word to its T9 digit string; empty if any char is unmappable.
	static std::string wordToDigits(const std::string& word);
};

extern CustomDictService CustomDict;

#endif //CHATTER_FIRMWARE_CUSTOMDICTSERVICE_H
