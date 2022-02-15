#include <cstdlib>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        auto function = [status](int document_id, DocumentStatus new_status, int rating) { return new_status == status; };
        return FindTopDocuments(raw_query, function);
    }

    template <typename Function>
    vector<Document> FindTopDocuments(const string& raw_query, Function function) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, function);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Function>
    vector<Document> FindAllDocuments(const Query& query, Function function) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto[document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (function(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto[document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto[document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    out << "[";
    bool is_first = true;
    for (const auto& s : container) {
    if (is_first == false) {
    out << ", "s;
    }
    is_first = false;
    out << s;
    }
    out << "]";
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        int x = 1;
        ASSERT_EQUAL(found_docs.size(), x);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestMinusWordsNotInclude() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(), "Back not empty result of searching"s);
    }
}

void TestMatchDocumentIsEmptyOrNot() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        tuple<vector<string>, DocumentStatus> test = server.MatchDocument("cat the"s, doc_id);
        vector<string> matched_words = get<0>(test);
        vector<string> mathced_words_new = {"cat"s, "the"s};
        ASSERT_EQUAL_HINT(matched_words, mathced_words_new, "Not all words"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        tuple<vector<string>, DocumentStatus> test = server.MatchDocument("cat -the"s, doc_id);
        vector<string> matched_words = get<0>(test);
        ASSERT(matched_words.empty());
    }
}

void TestSortDocumentByRelevance() {

    SearchServer server;
    server.SetStopWords("и в на"s);

    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

    vector<Document> found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
    int x_0 = 1;
    int x_1 = 0;
    int x_2 = 2;
    const double first = 0.866434;
    const double second = 0.173287;
    const double third = 0.173287;
    ASSERT_EQUAL_HINT(found_docs[0].id, x_0, "The first document is out of place"s);
        ASSERT((found_docs[0].relevance - first) < EPSILON);
    ASSERT_EQUAL_HINT(found_docs[1].id, x_1, "The second document is out of place"s);
        ASSERT((found_docs[1].relevance - second) < EPSILON);
    ASSERT_EQUAL_HINT(found_docs[2].id, x_2, "The third document is out of place"s);
        ASSERT((found_docs[2].relevance - third) < EPSILON);
}

void TestIsRightRating() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings_1 = {1, 2, 3};
    const vector<int> ratings_2 = {-1, -2, -3};
    const vector<int> ratings_3 = {-1, 2, -4};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("in"s);
        int x = 2;
        ASSERT_EQUAL(found_docs[0].rating, x);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings_2);
        const auto found_docs = server.FindTopDocuments("in"s);
        int x = -2;
        ASSERT_EQUAL(found_docs[0].rating, x);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("in"s);
        int x = -1;
        ASSERT_EQUAL(found_docs[0].rating, x);
    }
}

void TestFilter() {
    const int doc_id_1 = 0;
    const string content_1 = "один два три"s;
    const vector<int> ratings_1 = {7};

    const int doc_id_2 = 1;
    const string content_2 = "два три пять"s;
    const vector<int> ratings_2 = {4, 5};

    {
     SearchServer server;
     server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
     server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

     const auto found_docs = server.FindTopDocuments("три пять", [](int document_id, DocumentStatus status, int rating) {return rating >= 6;});
     int x = 1;
     ASSERT_EQUAL(found_docs.size(), x);
     const auto found_docs_2 = server.FindTopDocuments("три пять", [](int document_id, DocumentStatus status, int rating) {return rating >= 10;});
     ASSERT(found_docs_2.empty());
    }
}

void TestStatus() {
    const int doc_id_1 = 0;
    const string content_1 = "один два"s;
    const vector<int> ratings_1 = {7};

    const int doc_id_2 = 1;
    const string content_2 = "один три"s;
    const vector<int> ratings_2 = {4, 5};

    const int doc_id_3 = 2;
    const string content_3 = "один четыре"s;
    const vector<int> ratings_3 = {4, 5};

    const int doc_id_4 = 3;
    const string content_4 = "один пять"s;
    const vector<int> ratings_4 = {4, 5};

    {
     SearchServer server;
     server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
     server.AddDocument(doc_id_2, content_2, DocumentStatus::IRRELEVANT, ratings_2);
     server.AddDocument(doc_id_3, content_3, DocumentStatus::BANNED, ratings_3);
     server.AddDocument(doc_id_4, content_4, DocumentStatus::REMOVED, ratings_4);

     const int x = 1;
     const auto found_docs_1 = server.FindTopDocuments("два", DocumentStatus::ACTUAL);
     ASSERT_EQUAL(found_docs_1.size(), x);
     const Document& doc0_1 = found_docs_1[0];
     ASSERT_EQUAL_HINT(doc0_1.id, doc_id_1, "Invalid document id"s);

     const auto found_docs_2 = server.FindTopDocuments("три", DocumentStatus::IRRELEVANT);
     ASSERT_EQUAL(found_docs_2.size(), x);
     const Document& doc0_2 = found_docs_2[0];
     ASSERT_EQUAL_HINT(doc0_2.id, doc_id_2, "Invalid document id"s);

     const auto found_docs_3 = server.FindTopDocuments("четыре", DocumentStatus::BANNED);
     ASSERT_EQUAL(found_docs_3.size(), x);
     const Document& doc0_3 = found_docs_3[0];
     ASSERT_EQUAL_HINT(doc0_3.id, doc_id_3, "Invalid document id"s);

     const auto found_docs_4 = server.FindTopDocuments("пять", DocumentStatus::REMOVED);
     ASSERT_EQUAL(found_docs_4.size(), x);
     const Document& doc0_4 = found_docs_4[0];
     ASSERT_EQUAL_HINT(doc0_4.id, doc_id_4, "Invalid document id"s);
    }
}

void TestCorrectRelevance() {
    SearchServer server;
    server.SetStopWords("и в на"s);

    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

    vector<Document> found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
    const double first = 0.866434;
    const double second = 0.173287;
    const double third = 0.173287;
    ASSERT((found_docs[0].relevance - first) < EPSILON);
    ASSERT((found_docs[1].relevance - second) < EPSILON);
    ASSERT((found_docs[2].relevance - third) < EPSILON);
}

template <typename T>
void RunTestImpl(const T& t, const string& name) {
    t();
    cerr << name << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWordsNotInclude);
    RUN_TEST(TestMatchDocumentIsEmptyOrNot);
    RUN_TEST(TestSortDocumentByRelevance);
    RUN_TEST(TestIsRightRating);
    RUN_TEST(TestFilter);
    RUN_TEST(TestStatus);
    RUN_TEST(TestCorrectRelevance);
    cerr << endl;
}

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

int main() {
    TestSearchServer();
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}
