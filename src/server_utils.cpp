#include "server_utils.h"
#include "config.h"
#include <pqxx/pqxx>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

std::vector<std::string> search_pages(const std::string& query)
{
    std::vector<std::string> results;
    if(query.empty()) return results;
    
    try
    {
        Config cfg = load_config("config.ini");
        std::string conn = make_conn_str(cfg);
        pqxx::connection connp(conn);
        if(!connp.is_open())
        {
            std::cerr << "Ошибка подключения к БД" << std::endl;
            return results;
        }
        pqxx::work txn(connp);

        // Разбиваем запрос на слова
        std::istringstream iss(query);
        std::vector<std::string> words;
        std::string word;
        while (iss >> word) {
            if (!word.empty()) {
                words.push_back(word);
            }
        }
        
        if (words.empty()) return results;
        
        // Поиск страниц по словам
        std::string sql = 
            "SELECT p.url, SUM(wd.count) as relevance "
            "FROM pages p "
            "JOIN word_document wd ON p.id = wd.doc_id "
            "JOIN words w ON w.id = wd.word_id "
            "WHERE w.word = ANY($1) "
            "GROUP BY p.url "
            "HAVING COUNT(DISTINCT w.word) >= 1 "
            "ORDER BY relevance DESC "
            "LIMIT 10";
        
        // Формируем массив для PostgreSQL
        std::string words_array = "{";
        for (size_t i = 0; i < words.size(); ++i) {
            if (i > 0) words_array += ",";
            words_array += "\"" + words[i] + "\"";
        }
        words_array += "}";
        
        pqxx::result r = txn.exec_params(sql, words_array);
        
        for(auto row: r)
        {
            results.push_back(row[0].as<std::string>());
        }
        
        txn.commit();
        
        std::cout << "Поиск '" << query << "' нашел " << results.size() << " результатов" << std::endl;
        
    } catch (const std::exception &e)
    {
        std::cerr << "Ошибка поиска: " << e.what() << std::endl;
    }
    return results;
}