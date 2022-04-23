#include "test_example_functions.h"


void AddDocument(SearchServer &search_server, int document_id, const std::string &document, DocumentStatus status,
                 const std::vector<int> &ratings)
{
    try
    {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "Ошибка добавления документа "s << document_id << ": " << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer &search_server, const std::string &raw_query)
{
    std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try
    {
        for (const Document &document : search_server.FindTopDocuments(raw_query))
        {
            PrintDocument(document);
        }
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "Ошибка поиска: "s << e.what() << std::endl;
    }
}

/*
void MatchDocuments(const SearchServer &search_server, const std::string &query)
{
    try
    {
        std::cout << "Матчинг документов по запросу: "s << query << std::endl;
        vector<int> document_ids = vector(search_server.begin(), search_server.end());
        for (auto document_id : document_ids)
        {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "Ошибка матчинга документов на запрос "s << query << ": " << e.what() << std::endl;
    }
}
*/