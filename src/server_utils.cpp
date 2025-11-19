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
            std::cerr << "Cannot open DB connection in search_pages." << std::endl;
            return results;
        }
        
        pqxx::work txn(connp);
        
        std::vector<std::string> words;
        std::istringstream iss(query);
        std::string word;
        while (iss >> word) 
        {
            std::string clean_word;
            for (char c : word) 
            {
                if (!std::ispunct(static_cast<unsigned char>(c))) 
                {
                    clean_word += std::tolower(c);
                }
            }
            if (clean_word.size() >= 3 && clean_word.size() <= 32 && words.size() < 4) 
            {
                words.push_back(clean_word);
            }
        }
        
        if (words.empty()) return results;
        
        std::string sql = 
            "SELECT p.url, SUM(wp.count) as total_count "
            "FROM pages p "
            "JOIN word_page wp ON p.id = wp.page_id "
            "JOIN words w ON wp.word_id = w.id "
            "WHERE w.word IN (";
        
        for (size_t i = 0; i < words.size(); ++i) 
        {
            if (i > 0) sql += ", ";
            sql += "$" + std::to_string(i + 1);
        }
        
        sql += 
            ") "
            "GROUP BY p.id, p.url "
            "HAVING COUNT(DISTINCT w.word) = $" + std::to_string(words.size() + 1) + " "
            "ORDER BY total_count DESC "
            "LIMIT 10;";
        
        pqxx::params params;
        for (const auto& w : words) 
        {
            params.append(w);
        }
        params.append(static_cast<int>(words.size()));
        
        pqxx::result r = txn.exec_params(sql, params);
        for(auto row: r) 
        {
            results.push_back(row[0].as<std::string>());
        }
        
        txn.commit();
        
        std::cout << "Search for '" << query << "' found " << results.size() << " results" << std::endl;
        
    } 
    catch (const std::exception &e) 
    {
        std::cerr << "search_pages DB error: " << e.what() << std::endl;
    }
    return results;
}