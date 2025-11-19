#pragma once
#include <string>
#include <set>

struct CrawlTask 
{
    std::string url;
    int depth;
};

std::string fetch_url(const std::string& url);
void save_page_to_db(const std::string &conn_str, const std::string& url, const std::string& html);
void init_db(const std::string &conn_str);
std::set<std::string> extract_links(const std::string& html, const std::string& base_url);