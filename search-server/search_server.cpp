#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text))
{
}

SearchServer::SearchServer(const std::string_view stop_words_text) : SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id,const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }

    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();

    for (auto iter = words.begin(); iter != words.end(); ++iter) {
        dictionary_.push_back(std::string(*iter));
        word_to_document_freqs_[dictionary_.back()][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][dictionary_.back()] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq ,raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    const static std::map<std::string_view, double> word_freqs_;

    if (document_to_word_freqs_.count(document_id) == 1) {
        return document_to_word_freqs_.at(document_id);
    }
    return word_freqs_;
}

void SearchServer::RemoveDocument(int document_id) {
    documents_.erase(document_id);

    document_ids_.erase(document_id);

    for (const auto& [word, freq]: document_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }

    document_to_word_freqs_.erase(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id))
    {
        throw std::out_of_range("no document"s);
    }

    const auto query = ParseQuery(raw_query, std::execution::seq);

    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { matched_words, documents_.at(document_id).status };
        }
    }

    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    
    return {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id))
    {
        throw std::out_of_range("no document"s);
    }

    const auto query = ParseQuery(raw_query, std::execution::par);
    
    std::vector<std::string_view> matched_words;

    if (std::any_of(query.minus_words.begin(), query.minus_words.end(), [this, document_id](const std::string_view word) {
        return word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id) == 1;
        }))
    {
        return { matched_words, documents_.at(document_id).status };
    }

    matched_words.resize(query.plus_words.size());

    auto last_element = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [this, document_id](const std::string_view word) {
         return word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id) == 1;
        });
    matched_words.erase(last_element, matched_words.end());
    DeleteCopy(matched_words);

    return { matched_words, documents_.at(document_id).status };
}

std::set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

const std::set<int>::iterator SearchServer::begin_const() {
    return document_ids_.begin();
}

const std::set<int>::iterator SearchServer::end_const() {
    return document_ids_.end();
}

bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}


SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + std::string(text) + " is invalid"s);
    }

    return {word, is_minus, IsStopWord(word)};
}

void SearchServer::DeleteCopy(std::vector<std::string_view>& result) const {
    sort(result.begin(), result.end());
    auto last = unique(result.begin(), result.end());
    if (last == result.end()) {
        return;
    }
    result.erase(last, result.end());
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
