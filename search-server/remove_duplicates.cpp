#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	set<int> duplicate;
	for (auto iter1 = search_server.begin(); iter1 != search_server.end(); advance(iter1, 1)) {
		auto tmp_iter = iter1 + 1;
		//advance(tmp_iter, 1);
		set<string> doc1 ;
		auto first_doc = search_server.GetWordFrequencies(*iter1);
		for (const auto& f : first_doc) {
			doc1.insert(f.first);
		}
		for (auto iter2 = tmp_iter; iter2 != search_server.end(); advance(iter2, 1)) {
			set<string> doc2;
			auto second_doc = search_server.GetWordFrequencies(*iter2);
			for (const auto& f : second_doc) {
				doc2.insert(f.first);
			}
			if (doc1 == doc2) {
				duplicate.insert(*iter2);
			}
		}
	}
	for (const auto id : duplicate) {
		cout << "Found duplicate document id " << id << endl;
		search_server.RemoveDocument(id);
	}
}