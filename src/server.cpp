#include "server_utils.h"
#include "config.h"
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

static std::string url_decode(const std::string &s) 
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) 
    {
        if (s[i] == '%') 
        {
            if (i + 2 < s.size()) 
            {
                int value = 0;
                std::istringstream is(s.substr(i + 1, 2));
                if (is >> std::hex >> value) 
                {
                    out += static_cast<char>(value);
                    i += 2;
                }
            }
        } 
        else if (s[i] == '+') 
        {
            out += ' ';
        } 
        else out += s[i];
    }
    return out;
}

static std::string build_search_form() 
{
    return R"(<!doctype html>
<html>
<head>
    <meta charset='utf-8'>
    <title>Search Engine</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .search-form { margin: 20px 0; }
        .search-input { padding: 10px; width: 400px; font-size: 16px; }
        .search-button { padding: 10px 20px; font-size: 16px; background: #0066cc; color: white; border: none; cursor: pointer; }
        .results { margin: 20px 0; }
        .result-item { margin: 10px 0; }
        .error { color: red; }
    </style>
</head>
<body>
    <h1>Search Engine</h1>
    <form method='POST' action='/' class='search-form'>
        <input type='text' name='q' class='search-input' placeholder='Enter your search query...'/>
        <button type='submit' class='search-button'>Search</button>
    </form>
    <div><small>Enter up to 4 words. Each word should be 3-32 characters long.</small></div>
</body>
</html>)";
}

static std::string build_page(const std::vector<std::string> &results, const std::string &query, const std::string& error = "") 
{
    std::ostringstream ss;
    ss << "<!doctype html><html><head><meta charset='utf-8'><title>Search Results</title>";
    ss << "<style>";
    ss << "body { font-family: Arial, sans-serif; margin: 40px; }";
    ss << ".search-form { margin: 20px 0; }";
    ss << ".search-input { padding: 10px; width: 400px; font-size: 16px; }";
    ss << ".search-button { padding: 10px 20px; font-size: 16px; background: #0066cc; color: white; border: none; cursor: pointer; }";
    ss << ".results { margin: 20px 0; }";
    ss << ".result-item { margin: 10px 0; }";
    ss << ".error { color: red; }";
    ss << "</style></head><body>";
    
    ss << "<h1>Search Engine</h1>";
    ss << "<form method='POST' action='/' class='search-form'>";
    ss << "<input type='text' name='q' class='search-input' value='" << query << "'/>";
    ss << "<button type='submit' class='search-button'>Search</button>";
    ss << "</form>";
    
    if (!error.empty()) 
    {
        ss << "<div class='error'>Error: " << error << "</div>";
    } 
    else 
    {
        ss << "<div class='results'>";
        ss << "<h3>Results (" << results.size() << ")</h3>";
        if (results.empty()) 
        {
            ss << "<p>No results found for '" << query << "'</p>";
        } 
        else 
        {
            ss << "<ul>";
            for (auto &r: results) 
            {
                ss << "<li class='result-item'><a href='" << r << "'>" << r << "</a></li>";
            }
            ss << "</ul>";
        }
        ss << "</div>";
    }
    
    ss << "</body></html>";
    return ss.str();
}

void handle_client(SOCKET client_socket) 
{
    const int BUFSIZE = 8192;
    char buffer[BUFSIZE];
    
    int read_bytes = recv(client_socket, buffer, BUFSIZE - 1, 0);
    if (read_bytes <= 0) 
    {
        closesocket(client_socket);
        return;
    }
    buffer[read_bytes] = 0;
    
    std::string req(buffer);
    std::istringstream reqs(req);
    std::string method, path, proto;
    reqs >> method >> path >> proto;
    
    std::string query;
    std::string qval;
    
    if (method == "GET") 
    {
        auto pos = path.find('?');
        if (pos != std::string::npos) 
        {
            query = path.substr(pos + 1);
            path = path.substr(0, pos);
            size_t p = query.find("q=");
            if (p != std::string::npos) 
            {
                qval = query.substr(p + 2);
                auto amp = qval.find('&');
                if (amp != std::string::npos) qval = qval.substr(0, amp);
                qval = url_decode(qval);
            }
        }
        
        if (qval.empty()) 
        {
            std::string body = build_search_form();
            std::ostringstream resp;
            resp << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n" << body;
            auto s = resp.str();
            send(client_socket, s.c_str(), (int)s.size(), 0);
            closesocket(client_socket);
            return;
        }
    } 
    else if (method == "POST") 
    {
        std::string body_str;
        size_t content_length = 0;
        
        std::string header_str(buffer, read_bytes);
        size_t cl_pos = header_str.find("Content-Length: ");
        if (cl_pos != std::string::npos) 
        {
            size_t end_line = header_str.find("\r\n", cl_pos);
            std::string cl_str = header_str.substr(cl_pos + 16, end_line - cl_pos - 16);
            content_length = std::stoul(cl_str);
        }
        
        size_t body_start = header_str.find("\r\n\r\n");
        if (body_start != std::string::npos) 
        {
            body_start += 4;
            body_str = header_str.substr(body_start);
            
            while (body_str.size() < content_length && read_bytes > 0) 
            {
                int additional_bytes = recv(client_socket, buffer, BUFSIZE - 1, 0);
                if (additional_bytes > 0) 
                {
                    buffer[additional_bytes] = 0;
                    body_str.append(buffer, additional_bytes);
                }
            }
        }
        
        size_t q_pos = body_str.find("q=");
        if (q_pos != std::string::npos) 
        {
            qval = body_str.substr(q_pos + 2);
            size_t amp_pos = qval.find('&');
            if (amp_pos != std::string::npos) 
            {
                qval = qval.substr(0, amp_pos);
            }
            qval = url_decode(qval);
        }
    }
    else 
    {
        std::string body = build_page({}, "", "Unsupported HTTP method");
        std::ostringstream resp;
        resp << "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n" << body;
        auto s = resp.str();
        send(client_socket, s.c_str(), (int)s.size(), 0);
        closesocket(client_socket);
        return;
    }
    
    std::vector<std::string> results;
    std::string error;
    
    if (!qval.empty()) 
    {
        try 
        {
            results = search_pages(qval);
        } 
        catch (const std::exception& e) 
        {
            error = "Internal server error: " + std::string(e.what());
        }
    }
    
    std::string body = build_page(results, qval, error);
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n" << body;
    auto s = resp.str();
    
    send(client_socket, s.c_str(), (int)s.size(), 0);
    closesocket(client_socket);
}

int main() 
{
    Config cfg = load_config("config.ini");
    int port = cfg.get_int("server.port", 8080);
    
    std::cout << "Config loaded. Server port from config: " << port << std::endl;
    std::cout << "Trying to start server on port: " << port << std::endl;
    
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) 
    {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }
    
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) 
    {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) 
    {
        std::cerr << "setsockopt failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_socket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) 
    {
        std::cerr << "bind failed on port " << port << ": " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    if (listen(server_socket, 10) == SOCKET_ERROR) 
    {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    std::cout << "Server successfully listening on port " << port << std::endl;
    std::cout << "Access the search engine at: http://localhost:" << port << std::endl;
    
    while (true) 
    {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) 
        {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }
        
        std::thread(handle_client, client_socket).detach();
    }
    
    closesocket(server_socket);
    WSACleanup();
    return 0;
}