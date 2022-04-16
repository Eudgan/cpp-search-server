#include <iostream>

#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {

std::map<std::set<std::string>, int> documents;
std::vector<int> remove_id;

for (auto document_id : search_server) {
    std::set<std::string> words;

    for (auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
        words.insert(word);
    }

    if (documents.find(words) == documents.end()) {
        documents[words] = document_id;
    } else {
        remove_id.push_back(document_id);
    }
}


for (int i : remove_id) {
    search_server.RemoveDocument(i);
    std::cout << "Found duplicate document id " << i << std::endl;
}

}

