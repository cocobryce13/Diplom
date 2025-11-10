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
        std::string pattern = "%" + query + "%";
        pqxx::result r = txn.exec_params("SELECT url FROM pages WHERE content ILIKE $1 LIMIT 10;", pattern);
        for(auto row: r) 
        {
            if(row.size() > 0) results.push_back(row[0].as<std::string>());
        }
        txn.commit();
    } catch (const std::exception &e) 
    {
        std::cerr << "search_pages DB error: " << e.what() << std::endl;
    }
    return results;
}