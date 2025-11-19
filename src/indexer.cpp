#include "indexer.h"
#include <pqxx/pqxx>
#include <regex>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

std::string clean_html(const std::string& html) 
{
    std::string text = std::regex_replace(html, std::regex("<[^>]*>"), " ");
    
    std::string result;
    for (char c : text) 
    {
        if (std::ispunct(static_cast<unsigned char>(c)) && c != '-' && c != '\'') 
        {
            result += ' ';
        } 
        else 
        {
            result += c;
        }
    }
    
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) 
    {
        return std::tolower(c);
    });
    return result;
}

std::map<std::string, int> count_words(const std::string& text) 
{
    std::map<std::string, int> freq;
    std::istringstream ss(text);
    std::string word;
    
    while(ss >> word) 
    {
        if(word.size() >= 3 && word.size() <= 32) 
        {
            freq[word]++;
        }
    }
    return freq;
}

void save_word_stats(const std::string& conn_str, const std::string& url, const std::map<std::string, int>& stats) 
{
    try 
    {
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);
        
        pqxx::result r = txn.exec_params(
            "INSERT INTO pages (url, content) VALUES ($1, '') ON CONFLICT (url) DO UPDATE SET url=EXCLUDED.url RETURNING id;", 
            url
        );
        int page_id = r[0][0].as<int>();

        for (const auto& [word, count] : stats) 
        {
            if (word.empty()) continue;
            
            pqxx::result r_word = txn.exec_params(
                "INSERT INTO words (word) VALUES ($1) ON CONFLICT (word) DO UPDATE SET word=EXCLUDED.word RETURNING id;", 
                word
            );
            int word_id = r_word[0][0].as<int>();

            txn.exec_params(
                "INSERT INTO word_page (word_id, page_id, count) VALUES ($1, $2, $3) "
                "ON CONFLICT (word_id, page_id) DO UPDATE SET count = EXCLUDED.count;", 
                word_id, page_id, count
            );
        }

        txn.commit();
    } 
    catch (const std::exception &e) 
    {
        std::cerr << "Error in save_word_stats: " << e.what() << std::endl;
    }
}