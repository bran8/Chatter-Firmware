#include "T9Dict.h"
#include "../t9_dict.h"
#include <cstring>
#include <algorithm>

const T9Dict::TrieNode* T9Dict::nodes = nullptr;
uint16_t T9Dict::nodeCount = 0;

void T9Dict::init(){
	// Header: uint16_t node_count, uint16_t reserved (little-endian), then nodes.
	nodeCount = (uint16_t) (t9DictData[0] | (t9DictData[1] << 8));
	nodes = reinterpret_cast<const TrieNode*>(t9DictData + T9_HEADER_SIZE);
}

const char* T9Dict::digitLetters(char digit){
	switch(digit){
		case '2': return "abc";
		case '3': return "def";
		case '4': return "ghi";
		case '5': return "jkl";
		case '6': return "mno";
		case '7': return "pqrs";
		case '8': return "tuv";
		case '9': return "wxyz";
		default:  return "";
	}
}

void T9Dict::collectSubtree(uint16_t idx, std::string& prefix,
							std::vector<std::pair<std::string, uint16_t>>& out){
	const TrieNode& node = nodes[idx];
	if(node.weight > 0){
		out.emplace_back(prefix, node.weight);
	}

	uint16_t first = node.first_child_index;
	for(uint16_t c = first; c < first + node.child_count; c++){
		prefix.push_back(nodes[c].character);
		collectSubtree(c, prefix, out);
		prefix.pop_back();
	}
}

void T9Dict::descend(uint16_t idx, const std::string& digits, size_t depth,
					 std::string& prefix,
					 std::vector<std::pair<std::string, uint16_t>>& out){
	if(depth == digits.size()){
		// All digits consumed: every word in this subtree shares the prefix.
		collectSubtree(idx, prefix, out);
		return;
	}

	const TrieNode& node = nodes[idx];
	const char* letters = digitLetters(digits[depth]);
	if(letters[0] == '\0') return;   // typed char wasn't a 2-9 digit

	uint16_t first = node.first_child_index;
	for(uint16_t c = first; c < first + node.child_count; c++){
		char cc = nodes[c].character;
		if(std::strchr(letters, cc) != nullptr){
			prefix.push_back(cc);
			descend(c, digits, depth + 1, prefix, out);
			prefix.pop_back();
		}
	}
}

std::vector<std::pair<std::string, uint16_t>> T9Dict::getMatches(
		const std::string& digits, size_t maxResults){
	std::vector<std::pair<std::string, uint16_t>> out;

	if(nodes == nullptr) init();
	if(digits.empty()) return out;

	std::string prefix;
	descend(0, digits, 0, prefix, out);

	std::sort(out.begin(), out.end(),
			  [](const std::pair<std::string, uint16_t>& a,
				 const std::pair<std::string, uint16_t>& b){
				  return a.second > b.second;
			  });

	if(out.size() > maxResults) out.resize(maxResults);
	return out;
}
