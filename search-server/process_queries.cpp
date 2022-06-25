#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par,
        queries.begin(), queries.end(),
        result.begin(),
        [&search_server](const std::string& querie) { return search_server.FindTopDocuments(querie); }
    );
    return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::list<Document> result;
    for (const auto& local_documents : ProcessQueries(search_server, queries)) {
        for (const auto& local_document : local_documents) {
            result.push_back(local_document);
        }
    }
    return result;
}