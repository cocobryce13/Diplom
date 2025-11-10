#pragma once
#include <string>
#include <map>

std::string clean_html(const std::string& html);
std::map<std::string, int> count_words(const std::string& text);
void save_word_stats(const std::string& conn_str, const std::string& url, const std::map<std::string, int>& stats);