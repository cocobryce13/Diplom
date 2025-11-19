#include "crawler_utils.h"
#include <pqxx/pqxx>
#include <iostream>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <regex>
#include <set>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::string fetch_url(const std::string& url)
{
    try 
    {
        std::string host, target;
        size_t pos = url.find("://");
        if (pos != std::string::npos) 
        {
            std::string protocol = url.substr(0, pos);
            std::string rest = url.substr(pos + 3);
            
            size_t slash_pos = rest.find('/');
            if (slash_pos != std::string::npos) 
            {
                host = rest.substr(0, slash_pos);
                target = rest.substr(slash_pos);
            } 
            else 
            {
                host = rest;
                target = "/";
            }
        } 
        else 
        {
            host = url;
            target = "/";
        }

        size_t colon_pos = host.find(':');
        if (colon_pos != std::string::npos) 
        {
            host = host.substr(0, colon_pos);
        }

        std::string port = "80";

        net::io_context ioc;
        tcp::resolver resolver(ioc);
        tcp::socket socket(ioc);

        auto const results = resolver.resolve(host, port);
        net::connect(socket, results.begin(), results.end());

        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "SearchBot/1.0");
        req.set(http::field::accept, "*/*");

        http::write(socket, req);

        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(socket, buffer, res);

        beast::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);

        if (ec && ec != beast::errc::not_connected) 
        {
            throw beast::system_error{ec};
        }

        return boost::beast::buffers_to_string(res.body().data());
        
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error fetching URL " << url << ": " << e.what() << std::endl;
        return "";
    }
}

void init_db(const std::string &conn_str) 
{
    try 
    {
        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);
        txn.exec("CREATE TABLE IF NOT EXISTS pages (id SERIAL PRIMARY KEY, url TEXT UNIQUE, content TEXT);");
        txn.exec("CREATE TABLE IF NOT EXISTS words (id SERIAL PRIMARY KEY, word TEXT UNIQUE);");
        txn.exec("CREATE TABLE IF NOT EXISTS word_page (word_id INT, page_id INT, count INT, PRIMARY KEY (word_id, page_id));");
        txn.commit();
        std::cout << "База данных готова." << std::endl;
    } 
    catch (const std::exception &e) 
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
        txn.exec_params(
            "INSERT INTO pages (url, content) VALUES ($1, $2) ON CONFLICT (url) DO UPDATE SET content = EXCLUDED.content;", 
            url, html
        );
        txn.commit();
    }
    catch (const std::exception &e) 
    {
        std::cerr << "DB error: " << e.what() << std::endl;
    }
}

std::string normalize_url(const std::string& link, const std::string& base_url) 
{
    if (link.empty()) return "";
    
    if (link.find("http") == 0) 
    {
        return link;
    }
    
    std::string base;
    size_t proto_pos = base_url.find("://");
    if (proto_pos != std::string::npos) 
    {
        size_t path_pos = base_url.find('/', proto_pos + 3);
        if (path_pos != std::string::npos) 
        {
            base = base_url.substr(0, path_pos);
        } 
        else 
        {
            base = base_url;
        }
    } 
    else 
    {
        base = base_url;
    }
    
    if (link[0] == '/') 
    {
        return base + link;
    } 
    else if (link.find("./") == 0) 
    {
        std::string base_path = base_url;
        size_t last_slash = base_path.find_last_of('/');
        if (last_slash != std::string::npos && last_slash > proto_pos + 2) 
        {
            base_path = base_path.substr(0, last_slash);
        }
        return base_path + link.substr(1);
    } 
    else if (link.find("../") == 0) 
    {
        std::string base_path = base_url;
        size_t last_slash = base_path.find_last_of('/');
        if (last_slash != std::string::npos && last_slash > proto_pos + 2) 
        {
            base_path = base_path.substr(0, last_slash);
            last_slash = base_path.find_last_of('/');
            if (last_slash != std::string::npos && last_slash > proto_pos + 2) 
            {
                base_path = base_path.substr(0, last_slash);
            }
        }
        return base_path + '/' + link.substr(3);
    } 
    else 
    {
        if (base_url.back() == '/') 
        {
            return base_url + link;
        } 
        else 
        {
            return base_url + '/' + link;
        }
    }
}

std::set<std::string> extract_links(const std::string& html, const std::string& base_url) 
{
    std::set<std::string> links;
    
    std::regex link_regex("<a\\s+[^>]*href\\s*=\\s*[\"']([^\"']*)[\"'][^>]*>", std::regex::icase);
    auto words_begin = std::sregex_iterator(html.begin(), html.end(), link_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) 
    {
        std::string link = (*i)[1].str();
        
        if (link.empty() || link[0] == '#') 
        {
            continue;
        }
        
        if (link.find("javascript:") == 0 || link.find("mailto:") == 0) 
        {
            continue;
        }
        
        std::string normalized_link = normalize_url(link, base_url);
        if (!normalized_link.empty()) 
        {
            links.insert(normalized_link);
        }
    }
    return links;
}