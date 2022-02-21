#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;

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
		auto f = [](int document_id, DocumentStatus status, int rating) {
			return status == DocumentStatus::ACTUAL;
		};
		return FindTopDocuments(raw_query, f);
	}

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus doc_status) const {
		auto f = [doc_status](int document_id, DocumentStatus status, int rating) {
			return status == doc_status;
		};
		return FindTopDocuments(raw_query, f);
	}

	template<typename Func>
	vector<Document> FindTopDocuments(const string& raw_query, Func f) const {
		const Query query = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query, f);

		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
				if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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
	template<typename Func>
	vector<Document> FindAllDocuments(const Query& query, Func f) const {
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
				if (f(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}

		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({
				document_id,
				relevance,
				documents_.at(document_id).rating
				});
		}
		return matched_documents;
	}
};
/* 
   Подставьте сюда вашу реализацию макросов 
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
template <typename key, typename value>
ostream& Print(ostream& out, const map<key, value>& container) {
    bool is_first = true;
    for (const auto& [k, v] : container) {
        if (is_first) {
            out << k << ": "s << v ;
            is_first = false;
        } else {
            out << ", " << k << ": "s << v ;
        }
    }
    return out;
}

template <typename cont>
ostream& Print(ostream& out, const cont& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if (is_first) {
            out << element;
            is_first = false;
        } else {
            out << ", " << element;
        }
    }
    return out;
}

template <typename type>
ostream& operator<<(ostream& out, const vector<type>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename type>
ostream& operator<<(ostream& out, const set<type>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename key, typename value>
ostream& operator<<(ostream& out, const map<key, value>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

// #define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

// #define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

// #define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

// #define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const string& func , T& t) {
    t();
    cerr << func << " OK" << endl;
}

// #define RUN_TEST(func)  RunTestImpl(#func, (func))

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/

void TestMinusWords() {
   // TestExcludeStopWordsFromAddedDocumentContent();
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {

        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in -the"s).empty());
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(0, "catomaran in the city"s, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in -the"s).empty());
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(0, "catomaran in the city"s, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("in -they"s).size(), 2);
    }
  //  cout << "EXCLUDE STOP WORDS in OK" << endl;

}


void TestMatch() {
    //TestExcludeStopWordsFromAddedDocumentContent();
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
  //  const int doc_id1 = 42;
   // const string content1 = "dog in the city"s;
  //  const vector<int> ratings1 = { 1, 2, 3 };
    {
        SearchServer server;
        
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in -the"s).empty());
    }
  //  cout << "TEST MATCH WORDS in OK" << endl;
}

void TestForDocumentStatus() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(0, content, DocumentStatus::BANNED, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("cat", DocumentStatus::ACTUAL).size(), 1);
        ASSERT_EQUAL(server.FindTopDocuments("cat", DocumentStatus::BANNED).size(), 1);
    }
}


void SortDocument() {
    {
        SearchServer server;
    server.SetStopWords("и в на"s);

   server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
        const auto result = server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(result[0].id, 1);
        ASSERT_EQUAL(result[1].id, 0);
        ASSERT_EQUAL(result[2].id, 2);
    }
   // cout << "SortDocument is OK" << endl;
}
void TestForPredicate() {
    SearchServer server;
    server.SetStopWords("и в на"s);

    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, { 5, -12, 2, 1 });
    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });


    {
        ASSERT_EQUAL(server.FindTopDocuments("кот"s).size(), 2);
    }
   // cout << "TestForPredicate is OK" << endl;
    {
        ASSERT_EQUAL(server.FindTopDocuments("ухоженный"s,DocumentStatus::BANNED).size(), 2);
        ASSERT_EQUAL(server.FindTopDocuments("пёс"s,DocumentStatus::BANNED).size(), 1);
        
    }
   // cout << "TestForPredicate is OK" << endl;
    
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatch);
    RUN_TEST(TestForDocumentStatus);
    RUN_TEST(SortDocument);
    RUN_TEST(TestForPredicate);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
	cin.get();
}