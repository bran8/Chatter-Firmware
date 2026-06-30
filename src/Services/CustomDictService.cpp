#include "CustomDictService.h"
#include <Arduino.h>
#include <LITTLEFS.h>
#include <algorithm>

#define CUSTOM_DICT_PATH "/custom_dict.txt"

CustomDictService CustomDict;

// a  b  c  d  e  f  g  h  i  j  k  l  m  n  o  p  q  r  s  t  u  v  w  x  y  z
static const char CHAR_DIGIT[26] = {
	'2','2','2','3','3','3','4','4','4','5','5','5','6',
	'6','6','7','7','7','7','8','8','8','9','9','9','9'
};

void CustomDictService::begin(){
	load();
}

std::string CustomDictService::wordToDigits(const std::string& word){
	std::string digits;
	digits.reserve(word.size());
	for(char c : word){
		if(c < 'a' || c > 'z') return std::string();   // unmappable -> no match
		digits.push_back(CHAR_DIGIT[c - 'a']);
	}
	return digits;
}

void CustomDictService::learnText(const std::string& text){
	bool changed = false;
	std::string word;

	auto flush = [&](){
		if(word.size() >= 2){
			dict[word]++;
			changed = true;
		}
		word.clear();
	};

	for(char c : text){
		if(c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a');
		if(c >= 'a' && c <= 'z'){
			word.push_back(c);
		}else{
			flush();
		}
	}
	flush();

	if(changed) save();
}

std::vector<std::pair<std::string, uint32_t>> CustomDictService::getMatches(
		const std::string& digits, size_t maxResults){
	std::vector<std::pair<std::string, uint32_t>> out;
	if(digits.empty()) return out;

	for(const auto& kv : dict){
		std::string d = wordToDigits(kv.first);
		if(d.size() >= digits.size() &&
		   d.compare(0, digits.size(), digits) == 0){
			out.emplace_back(kv.first, kv.second);
		}
	}

	// Shortest words first (exact-length matches lead), ties broken by learned
	// frequency descending. Matches the ordering used by T9Dict::getMatches so
	// the merged candidate list stays length-sorted.
	std::sort(out.begin(), out.end(),
			  [](const std::pair<std::string, uint32_t>& a,
				 const std::pair<std::string, uint32_t>& b){
				  if(a.first.size() != b.first.size())
					  return a.first.size() < b.first.size();
				  return a.second > b.second;
			  });

	if(out.size() > maxResults) out.resize(maxResults);
	return out;
}

void CustomDictService::load(){
	dict.clear();
	if(!LITTLEFS.exists(CUSTOM_DICT_PATH)) return;

	File f = LITTLEFS.open(CUSTOM_DICT_PATH, "r");
	if(!f) return;

	while(f.available()){
		String line = f.readStringUntil('\n');
		line.trim();
		if(line.length() == 0) continue;

		int tab = line.indexOf('\t');
		if(tab < 0) continue;

		String w = line.substring(0, tab);
		uint32_t count = (uint32_t) line.substring(tab + 1).toInt();
		if(w.length() == 0 || count == 0) continue;

		dict[std::string(w.c_str())] = count;
	}
	f.close();
}

void CustomDictService::save(){
	File f = LITTLEFS.open(CUSTOM_DICT_PATH, "w");
	if(!f) return;

	for(const auto& kv : dict){
		f.print(kv.first.c_str());
		f.print('\t');
		f.println(kv.second);
	}
	f.close();
}
