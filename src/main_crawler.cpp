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
#include <sstream>
#include <condition_variable>
#include <algorithm>

std::queue<CrawlTask> url_queue;
std::set<std::string> visited_urls;
std::mutex queue_mutex;
std::mutex visited_mutex;
std::atomic<int> active_threads{0};
std::condition_variable queue_cv;
std::atomic<bool> stop_flag{false};

void process_url(int max_depth, const std::string& conn_str) 
{
    while (!stop_flag) 
    {
        CrawlTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            queue_cv.wait_for(lock, std::chrono::seconds(1), [&]() 
            {
                return !url_queue.empty() || stop_flag;
            });
            
            if (stop_flag && url_queue.empty()) 
            {
                break;
            }
            
            if (url_queue.empty()) 
            {
                continue;
            }
            
            task = url_queue.front();
            url_queue.pop();
        }

        if (task.depth > max_depth) 
        {
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

        std::cout << "Crawling: " << task.url << " (depth: " << task.depth << ")" << std::endl;
        
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
                        CrawlTask new_task;
                        new_task.url = link;
                        new_task.depth = task.depth + 1;
                        url_queue.push(new_task);
                        std::cout << "  -> Found link: " << link << std::endl;
                    }
                }
                queue_cv.notify_all();
            }
        }

        active_threads--;
    }
}

int main() 
{
    Config cfg = load_config("config.ini");
    std::string start_url = cfg.get("crawler.start_url","https://example.com");
    int max_depth = cfg.get_int("crawler.max_depth", 1);

    init_db(make_conn_str(cfg));

    CrawlTask start_task;
    start_task.url = start_url;
    start_task.depth = 0;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        url_queue.push(start_task);
    }

    unsigned int hw_concurrency = std::thread::hardware_concurrency();
    int num_threads = std::min(static_cast<int>(hw_concurrency), 8);
    std::vector<std::thread> threads;
    std::string conn_str = make_conn_str(cfg);

    std::cout << "Starting crawler with " << num_threads << " threads, max_depth: " << max_depth << std::endl;

    for (int i = 0; i < num_threads; ++i) 
    {
        threads.emplace_back(process_url, max_depth, conn_str);
    }

    for (auto& t : threads) 
    {
        t.join();
    }

    std::cout << "Краулер завершил работу. Обработано страниц: " << visited_urls.size() << std::endl;
    return 0;
}