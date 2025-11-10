#pragma once
#include <string>

std::string fetch_url(const std::string& url);
void save_page_to_db(const std::string &conn_str, const std::string& url, const std::string& html);
void init_db(const std::string &conn_str);