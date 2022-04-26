#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	set<int> duplicates;
	set<set<string>> set_of_words;
	for (const int document_id : search_server) {
		set<string> uniq_words;
		auto words_freq = search_server.GetWordFrequencies(document_id);
		for (auto [word, freq] : words_freq) {
			uniq_words.insert(word);
		}
		if (set_of_words.count(uniq_words)) {
			duplicates.insert(document_id);
		}
		else {
			set_of_words.insert(uniq_words);
		}
	}
	for (const int id : duplicates) {
		search_server.RemoveDocument(id);
	}
}