#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(
          std::string_view(stop_words_text)) 
                                            
{}

SearchServer::SearchServer(const std::string_view stop_words_text) : SearchServer(
          SplitIntoWordsView(stop_words_text))
{}

void SearchServer::AddDocument(int document_id, const std::string_view document,
                               DocumentStatus status,
                               const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        auto insertion_result = documents_words_.insert(word);
        std::string_view inserted_word = *(insertion_result.first);
        word_to_document_freqs_[inserted_word][document_id] += inv_word_count;
        documents_words_freqs_[document_id][inserted_word] += inv_word_count;
    }
    documents_.emplace(document_id,
                       DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(
    const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status,
                            int rating) { return document_status == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(
    const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const { return documents_.size(); }

std::set<int>::iterator SearchServer::begin() { return document_ids_.begin(); }

std::set<int>::iterator SearchServer::end() { return document_ids_.end(); }

SearchServer::DocumentContent
SearchServer::MatchDocument(const std::string_view raw_query,
                            int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("Передан несуществующий document_id");
    }
    if (!IsValidWord(raw_query)) {
        throw std::invalid_argument("Некорректный роисковый запрос");
    }
    const auto query = ParseQuery(raw_query);

    if (std::any_of(query.minus_words.begin(), query.minus_words.end(),
                    [this, document_id](const std::string_view word) {
                        if (word_to_document_freqs_.count(word) == 0)
                            return false;
                        if (word_to_document_freqs_.at(word).count(document_id))
                            return true;
                        return false;
                    })) {
        return {{}, documents_.at(document_id).status};
    }
    
    std::vector<std::string_view> matched_words(query.plus_words.size());
    auto last_word_it = std::copy_if(
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(), [this, document_id](const std::string_view word) {
            if (word_to_document_freqs_.count(word) == 0) return false;
            if (word_to_document_freqs_.at(word).count(document_id))
                return true;
            return false;
        });
    matched_words.resize(last_word_it - matched_words.begin());
    return {matched_words, documents_.at(document_id).status};
}

SearchServer::DocumentContent
SearchServer::MatchDocument(const std::execution::sequenced_policy policy,
                            const std::string_view raw_query,
                            int document_id) const {
    return MatchDocument(raw_query, document_id);
}

SearchServer::DocumentContent
SearchServer::MatchDocument(const std::execution::parallel_policy policy,
                            const std::string_view raw_query,
                            int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("Передан несуществующий document_id");
    }
    if (!IsValidWord(raw_query)) {
        throw std::invalid_argument("Некорректный роисковый запрос");
    }
    Query query = ParseQuery(raw_query, true);
    if (std::any_of(policy, query.minus_words.begin(), query.minus_words.end(),
                    [this, document_id](const std::string_view word) {
                        if (word_to_document_freqs_.count(word) == 0)
                            return false;
                        if (word_to_document_freqs_.at(word).count(document_id))
                            return true;
                        return false;
                    })) {
        return {{}, documents_.at(document_id).status};
    }
    std::vector<std::string_view> matched_words(query.plus_words.size());
    auto last_word_it = std::copy_if(
        policy, query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(), [this, document_id](const std::string_view word) {
            if (word_to_document_freqs_.count(word) == 0) return false;
            if (word_to_document_freqs_.at(word).count(document_id))
                return true;
            return false;
        });
    matched_words.resize(last_word_it - matched_words.begin());
    std::sort(matched_words.begin(), matched_words.end());
    matched_words.erase(
        (std::unique(policy, matched_words.begin(), matched_words.end())),
        matched_words.end());
    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view word) {
    return none_of(word.begin(), word.end(),
                   [](char c) { return c >= '\0' && c < ' '; });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(
    const std::string_view text) const {
    std::vector<std::string> words;
    for (const std::string_view word : SplitIntoWordsView(text)) {
        const std::string string_word = std::string{word};
        if (!IsValidWord(string_word)) {
            throw std::invalid_argument("Word "s + string_word + " is invalid"s);
        }
        if (!IsStopWord(string_word)) {
            words.push_back(string_word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(
   std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw std::invalid_argument("Query word "s + std::string{text} + " is invalid"s);
    }

    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text,
                                             bool parallel) const {
    Query result;
 
    for (const std::string_view word : SplitIntoWordsView(text)) {
        const auto query_word = ParseQueryWord(word);
 
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    
  if(!parallel) {
      for(auto *words : {&result.plus_words, &result.minus_words}) {
        std::sort(words->begin(), words->end());
          words->erase(std::unique(words->begin(), words->end()), words->end());
      }
  }  
 
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(
    const std::string_view word) const {
    return std::log(GetDocumentCount() * 1.0 /
                    word_to_document_freqs_.at(word).size());
}

const map<string_view, double>& SearchServer::GetWordFrequencies(
    int document_id) const {
    static std::map<std::string_view, double> result;
    result.clear();
    double total_words = 0;
    for (const auto& elem : word_to_document_freqs_) {
        if (elem.second.count(document_id)) {
            result[elem.first] += elem.second.at(document_id);
            total_words += elem.second.at(document_id);
        }
    }
    for (auto elem : result) {
        elem.second /= total_words;
    }
    return result;
}

void SearchServer::RemoveDocument(int document_id) {
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    for (auto& elem : word_to_document_freqs_) {
        if (elem.second.count(document_id)) {
            elem.second.erase(document_id);
        }
    }
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy policy,
                                  int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy policy,
                                  int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    auto& document_words = documents_words_freqs_.at(document_id);
    std::vector<std::string_view> words(document_words.size());
    std::transform(
        policy, document_words.begin(), document_words.end(), words.begin(),
        [](const auto& word) { return word.first; });
    std::for_each(
        policy, words.begin(), words.end(),
        [document_id, &words_frequency = word_to_document_freqs_](auto word) {
            words_frequency.at(word).erase(document_id);
        });
}
