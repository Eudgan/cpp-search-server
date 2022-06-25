#include <iostream>

#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {

std::map<std::set<std::string_view>, int> documents;
std::vector<int> remove_id;

for (auto document_id : search_server) {
    std::set<std::string_view> words;

    for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
        words.insert(word);
    }

    if (documents.find(words) == documents.end()) {
        documents[words] = document_id;
    } else {
        remove_id.push_back(document_id);
    }
}


for (int id : remove_id) {
    search_server.RemoveDocument(id);
    std::cout << "Found duplicate document id "s << id << std::endl;
}

}

