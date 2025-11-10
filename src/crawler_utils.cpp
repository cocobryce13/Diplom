#include "crawler_utils.h"
#include <curl/curl.h>
#include <pqxx/pqxx>
#include <iostream>


size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string fetch_url(const std::string& url)
{
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    curl = curl_easy_init();
    if(curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) 
        {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

void init_db(const std::string &conn_str) 
{
    try 
    {
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);
        txn.exec("CREATE TABLE IF NOT EXISTS pages (id SERIAL PRIMARY KEY, url TEXT UNIQUE, content TEXT);");
        txn.commit();
        std::cout << "База данных готова." << std::endl;
    } catch (const std::exception &e) 
    {
        std::cerr << "Ошибка инициализации БД: " << e.what() << std::endl;
    }
}

void save_page_to_db(const std::string &conn_str, const std::string& url, const std::string& html) 
{
    try 
    {
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);
        txn.exec_params("INSERT INTO pages (url, content) VALUES ($1, $2) ON CONFLICT (url) DO UPDATE SET content = EXCLUDED.content;", url, html);
        txn.commit();
    }
    catch (const std::exception &e) 
    {
        std::cerr << "DB error: " << e.what() << std::endl;
    }
}