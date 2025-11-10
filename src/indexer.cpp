#include "indexer.h"
#include <pqxx/pqxx>
#include <regex>
#include <sstream>
#include <iostream>
#include <algorithm>

std::string clean_html(const std::string& html) 
{
    std::string text = std::regex_replace(html, std::regex("<[^>]*>"), " ");
    std::replace_if(text.begin(), text.end(), [](char c){ return ispunct(c); }, ' ');
    std::transform(text.begin(), text.end(), text.begin(), ::tolower);
    return text;
}

std::map<std::string, int> count_words(const std::string& text) 
{
    std::map<std::string, int> freq;
    std::istringstream ss(text);
    std::string word;
    while(ss >> word) 
    {
        if(word.size() > 1) freq[word]++;
    }
    return freq;
}

void save_word_stats(const std::string& conn_str, const std::string& url, const std::map<std::string, int>& stats) 
{
    try 
    {
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);
        
        // Создаем таблицы если их нет
        txn.exec("CREATE TABLE IF NOT EXISTS documents (id SERIAL PRIMARY KEY, url TEXT UNIQUE);");
        txn.exec("CREATE TABLE IF NOT EXISTS words (id SERIAL PRIMARY KEY, word TEXT UNIQUE);");
        txn.exec("CREATE TABLE IF NOT EXISTS word_document (word_id INT, doc_id INT, count INT, PRIMARY KEY (word_id, doc_id));");

        // Вставляем или получаем ID документа
        pqxx::result r = txn.exec_params("INSERT INTO documents (url) VALUES ($1) ON CONFLICT (url) DO UPDATE SET url=EXCLUDED.url RETURNING id;", url);
        int doc_id = r[0][0].as<int>();

        // Обрабатываем каждое слово
        for (const auto& [word, count] : stats) 
        {
            if (word.empty()) continue;
            
            // Вставляем или получаем ID слова
            pqxx::result r_word = txn.exec_params("INSERT INTO words (word) VALUES ($1) ON CONFLICT (word) DO UPDATE SET word=EXCLUDED.word RETURNING id;", word);
            int word_id = r_word[0][0].as<int>();

            // Вставляем или обновляем связь слово-документ
            txn.exec_params(
                "INSERT INTO word_document (word_id, doc_id, count) VALUES ($1, $2, $3) "
                "ON CONFLICT (word_id, doc_id) DO UPDATE SET count = EXCLUDED.count;", 
                word_id, doc_id, count
            );
        }

        txn.commit();
    } catch (const std::exception &e) 
    {
        std::cerr << "Error in save_word_stats: " << e.what() << std::endl;
    }
}