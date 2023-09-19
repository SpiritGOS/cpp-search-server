#pragma once

#include <algorithm>
#include <cmath>
#include <execution>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <vector>

#include "concurrent_map.h"
#include "document.h"
#include "read_input_functions.h"
#include "string_processing.h"

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPS = 1e-6;

class SearchServer {
   public:
    explicit SearchServer(const std::string &stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);

    template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words);

    void AddDocument(int document_id, const std::string_view document,
                     DocumentStatus status, const std::vector<int> &ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(
        const std::string_view raw_query,
        DocumentPredicate document_predicate) const;

    template <typename Policy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(
        const Policy policy,
        const std::string_view raw_query,
        DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query,
                                           DocumentStatus status) const;

    template <typename Policy>
    std::vector<Document> FindTopDocuments(
        const Policy policy,
        const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(
        const std::string_view raw_query) const;

    template <typename Policy>
    std::vector<Document> FindTopDocuments(
        const Policy policy,
        const std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::iterator begin();

    std::set<int>::iterator end();

    using DocumentContent = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    
    DocumentContent MatchDocument(
        const std::string_view raw_query, int document_id) const;

    DocumentContent MatchDocument(
        const std::execution::sequenced_policy policy,
        const std::string_view raw_query, int document_id) const;

    DocumentContent MatchDocument(
        const std::execution::parallel_policy policy,
        const std::string_view raw_query, int document_id) const;

    const std::map<std::string_view, double> &GetWordFrequencies(
        int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy policy,
                        int document_id);
    void RemoveDocument(const std::execution::parallel_policy policy,
                        int document_id);

   private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    std::set<std::string> documents_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> documents_words_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string_view word) const;

    static bool IsValidWord(const std::string_view word);

    std::vector<std::string> SplitIntoWordsNoStop(
        const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int> &ratings);

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

    Query ParseQuery(const std::string_view text, bool parallel = false) const;
    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(
        const Query &query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(
        const std::execution::sequenced_policy policy, const Query &query,
        DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(
        const std::execution::parallel_policy policy, const Query &query,
        DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer &stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(
          stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(
    const std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename Policy, typename DocumentPredicate>
    std::vector<Document> SearchServer::FindTopDocuments(
        const Policy policy,
        const std::string_view raw_query,
        DocumentPredicate document_predicate) const {
    
    const auto query = ParseQuery(raw_query, typeid(policy) == typeid(std::execution::par));

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
             if (abs(lhs.relevance - rhs.relevance) < EPS) {
                 return lhs.rating > rhs.rating;
             } else {
                 return lhs.relevance > rhs.relevance;
             }
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(
        const Policy policy,
        const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy,
        raw_query, [status](int document_id, DocumentStatus document_status,
                            int rating) { return document_status == status; });
}

template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(
        const Policy policy,
        const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
    const SearchServer::Query &query,
    DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq =
            ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] :
             word_to_document_freqs_.at(word)) {
            const auto &document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status,
                                   document_data.rating)) {
                document_to_relevance[document_id] +=
                    term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
    const std::execution::sequenced_policy policy, const Query &query,
    DocumentPredicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
    const std::execution::parallel_policy policy, const Query &query,
    DocumentPredicate document_predicate) const {
    
    std::vector<std::string_view> plus_words = query.plus_words;
    std::sort(plus_words.begin(), plus_words.end());
    plus_words.erase(
        (std::unique(policy, plus_words.begin(), plus_words.end())),
        plus_words.end());
    
    ConcurrentMap<int, double> document_to_relevance_cm(8);

    std::for_each(policy, 
                  plus_words.begin(), 
                  plus_words.end(), 
                  [this, &policy, &document_to_relevance_cm, document_predicate](const std::string_view& word) {
        if (word_to_document_freqs_.count(word) == 0) return;

        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

        const auto& word_to_doc_freq = word_to_document_freqs_.at(word);
        std::for_each(policy, word_to_doc_freq.begin(), word_to_doc_freq.end(), [inverse_document_freq, this, &document_to_relevance_cm, document_predicate](const auto& doc_freq){
            const auto& document_data = documents_.at(doc_freq.first);
            if (document_predicate(doc_freq.first, document_data.status, document_data.rating)) {
                document_to_relevance_cm[doc_freq.first].ref_to_value += doc_freq.second * inverse_document_freq;
            }
    });
});
	std::for_each(policy, 
                  query.minus_words.begin(), 
                  query.minus_words.end(), 
                  [this, &policy, &document_to_relevance_cm](const std::string_view& word) {
        if (word_to_document_freqs_.count(word) == 0) return;
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance_cm.Erase(document_id);
        }
    });

    auto document_to_relevance = document_to_relevance_cm.BuildOrdinaryMap();
    std::vector<Document> matched_documents(document_to_relevance.size());
    std::transform(policy, document_to_relevance.begin(), document_to_relevance.end(), matched_documents.begin(), [this](const auto& node) {
        return Document{ node.first, node.second, documents_.at(node.first).rating };
    });
    return matched_documents;
}