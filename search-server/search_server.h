#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <execution>
#include <string_view>
#include <deque>
#include <type_traits>

#include "string_processing.h"
#include "document.h"
#include "log_duration.h"
#include "concurrent_map.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

const double EPSILON = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    explicit SearchServer(const std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& exec, const std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& exec, const std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& exec, const std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    int GetDocumentCount() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename Type>
    void RemoveDocument(Type exec, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const;

    std::set<int>::iterator begin();

    std::set<int>::iterator end();

    const std::set<int>::iterator begin_const();

    const std::set<int>::iterator end_const();
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::deque<std::string> dictionary_;
    const std::set<std::string, std::less<>> stop_words_;
    std::map<int, DocumentData> documents_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string_view word) const;

    static bool IsValidWord(const std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    void DeleteCopy(std::vector<std::string_view>& vec) const;

    template <typename ExecutionPolicy>
    Query ParseQuery(const std::string_view text, ExecutionPolicy exec) const; // ExecutionPolicy exec = std::execution::sequenced_policy (Как правильно по умолчанию?)
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy& exec, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& exec, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    std::vector<Document> matched_documents;
        auto query = ParseQuery(raw_query, exec);

        if constexpr (std::is_same_v<ExecutionPolicy, std::execution::parallel_policy>) {
            DeleteCopy(query.minus_words);
            DeleteCopy(query.plus_words);
        }

        matched_documents = FindAllDocuments(exec, query, document_predicate);

        sort(exec, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& exec, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(exec, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& exec, const std::string_view raw_query) const {
    return FindTopDocuments(exec, raw_query, DocumentStatus::ACTUAL);
}

template <typename Type>
void SearchServer::RemoveDocument(Type exec, int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }
    
    std::vector<const std::string*> words_to_delete;
    words_to_delete.reserve(document_to_word_freqs_.at(document_id).size());

    std::for_each(exec,
        document_to_word_freqs_.at(document_id).begin(),
        document_to_word_freqs_.at(document_id).end(),
        [&words_to_delete](const auto& elements) {
            words_to_delete.push_back(&elements.first); });

    std::for_each(exec,
        words_to_delete.begin(), words_to_delete.end(),
        [this, document_id](const std::string* word) {
            word_to_document_freqs_.at(*word).erase(document_id);
        });

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& exec, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(500);

    std::for_each(exec, query.plus_words.begin(), query.plus_words.end(),
        [this, &document_to_relevance, &document_predicate](const auto& word) {
            if (word_to_document_freqs_.count(word) != 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        });

    auto result = document_to_relevance.BuildOrdinaryMap();

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            result.erase(document_id);
        }
    }

    std::vector<Document> matched_documents(result.size());


    std::transform(exec, result.begin(), result.end(), matched_documents.begin(),
        [this](auto& element) {
            return Document{ element.first, element.second, documents_.at(element.first).rating};
        });

    return matched_documents;
}

template <typename ExecutionPolicy>
SearchServer::Query SearchServer::ParseQuery(const std::string_view text, ExecutionPolicy exec) const {
    Query result;
    for (const std::string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        DeleteCopy(result.minus_words);
        DeleteCopy(result.plus_words);
    }
    return result;

}
