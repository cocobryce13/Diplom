#include "crawler_utils.h"
#include "config.h"
#include "indexer.h"
#include <vector>
#include <string>
#include <iostream>
#include <queue>
#include <set>
#include <thread>
#include <mutex>
#include <regex>
#include <atomic>
#include <windows.h>

std::set<std::string> extract_links(const std::string& html, const std::string& base_url)
{
    std::set<std::string> links;
    std::regex link_regex("<a href=\"(.*?)\"", std::regex::icase);
    auto words_begin = std::sregex_iterator(html.begin(), html.end(), link_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
    {
        std::string link = (*i)[1].str();
        if (link.find("http") != 0)
        {
            if (link[0] == '/')
            {
                size_t pos = base_url.find("://");
                if (pos != std::string::npos)
                {
                    pos = base_url.find('/', pos+3);
                    if (pos != std::string::npos)
                    {
                        link = base_url.substr(0, pos) + link;
                    } else {
                        link = base_url + link;
                    }
                }
            } else
            {
                link = base_url + '/' + link;
            }
        }
        links.insert(link);
    }
    return links;
}

struct CrawlTask
{
    std::string url;
    int depth;
};

std::queue<CrawlTask> url_queue;
std::set<std::string> visited_urls;
std::mutex queue_mutex;
std::mutex visited_mutex;
std::atomic<int> active_threads{0};

void process_url(int max_depth, const std::string& conn_str)
{
    while (true)
    {
        CrawlTask task;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (url_queue.empty())
            {
                if (active_threads == 0) break;
                continue;
            }
            task = url_queue.front();
            url_queue.pop();
        }

        if (task.depth > max_depth) {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(visited_mutex);
            if (visited_urls.find(task.url) != visited_urls.end())
            {
                continue;
            }
            visited_urls.insert(task.url);
        }

        active_threads++;

        std::cout << "Обработка: " << task.url << " (глубина: " << task.depth << ")" << std::endl;
        
        std::string html = fetch_url(task.url);

        if (!html.empty())
        {
            save_page_to_db(conn_str, task.url, html);

            std::string text = clean_html(html);
            auto stats = count_words(text);
            
            save_word_stats(conn_str, task.url, stats);

            auto links = extract_links(html, task.url);
            
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                for (const auto& link : links)
                {
                    if (visited_urls.find(link) == visited_urls.end())
                    {
                        url_queue.push({link, task.depth + 1});
                    }
                }
            }
        }
        else
        {
            std::cout << "Ошибка загрузки: " << task.url << std::endl;
        }

        active_threads--;
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    Config cfg = load_config("config.ini");
    std::string start_url = cfg.get("crawler.start_url","https://example.com");
    int max_depth = cfg.get_int("crawler.max_depth", 1);

    init_db(make_conn_str(cfg));

    url_queue.push({start_url, 0});

    int num_threads = 1;
    std::vector<std::thread> threads;
    std::string conn_str = make_conn_str(cfg);

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(process_url, max_depth, conn_str);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    std::cout << "Краулер завершил работу" << std::endl;
    return 0;
}