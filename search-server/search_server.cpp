#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text)) // Invoke delegating constructor
													// from string container
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings)
{
	if ((document_id < 0) || (documents_.count(document_id) > 0))
	{
		throw std::invalid_argument("Invalid document_id"s);
	}
	const auto words = SplitIntoWordsNoStop(document);

	const double inv_word_count = 1.0 / words.size();
	for (const std::string& word : words)
	{
		word_to_document_freqs_[word][document_id] += inv_word_count;
	}
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const
{
	return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating)
		{ return document_status == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const
{
	return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const
{
	return documents_.size();
}

std::vector<int>::iterator SearchServer::begin() {
	return document_ids_.begin();
}

std::vector<int>::iterator SearchServer::end() {
	return document_ids_.end();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const
{
	const auto query = ParseQuery(raw_query);

	std::vector<std::string> matched_words;
	for (const std::string& word : query.plus_words)
	{
		if (word_to_document_freqs_.count(word) == 0)
		{
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id))
		{
			matched_words.push_back(word);
		}
	}
	for (const std::string& word : query.minus_words)
	{
		if (word_to_document_freqs_.count(word) == 0)
		{
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id))
		{
			matched_words.clear();
			break;
		}
	}
	return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const std::string& word) const
{
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word)
{
	// A valid word must not contain special characters
	return none_of(word.begin(), word.end(), [](char c)
		{ return c >= '\0' && c < ' '; });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const
{
	std::vector<std::string> words;
	for (const std::string& word : SplitIntoWords(text))
	{
		if (!IsValidWord(word))
		{
			throw std::invalid_argument("Word "s + word + " is invalid"s);
		}
		if (!IsStopWord(word))
		{
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
	if (ratings.empty())
	{
		return 0;
	}
	int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const
{
	if (text.empty())
	{
		throw std::invalid_argument("Query word is empty"s);
	}
	std::string word = text;
	bool is_minus = false;
	if (word[0] == '-')
	{
		is_minus = true;
		word = word.substr(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(word))
	{
		throw std::invalid_argument("Query word "s + text + " is invalid"s);
	}

	return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const
{
	Query result;
	for (const std::string& word : SplitIntoWords(text))
	{
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop)
		{
			if (query_word.is_minus)
			{
				result.minus_words.insert(query_word.data);
			}
			else
			{
				result.plus_words.insert(query_word.data);
			}
		}
	}
	return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const
{
	return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
	static map<string, double> result;
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
	auto it = remove(begin(), end(), document_id);
	document_ids_.erase(it, end());
	documents_.erase(document_id);
	for (auto& elem : word_to_document_freqs_) {
		if (elem.second.count(document_id)) {
			elem.second.erase(document_id);
		}
	}
}