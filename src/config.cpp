#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

std::string find_config_file() 
{
    std::vector<std::string> paths = {
        "config.ini",
        "../config.ini",
        "../../config.ini", 
        "../../../config.ini",
        "C:/C++/Diplom/config.ini"
    };
    
    for (const auto& path : paths) 
    {
        std::ifstream file(path);
        if (file.good()) 
        {
            std::cout << "Found config at: " << path << std::endl;
            return path;
        }
    }
    std::cerr << "Config file not found, using defaults" << std::endl;
    return "config.ini";
}

Config load_config(const std::string &path) 
{
    std::string config_path = path.empty() ? find_config_file() : path;
    Config cfg;
    std::ifstream in(config_path);
    if(!in) 
    {
        std::cerr << "Cannot open config file: " << config_path << std::endl;
        return cfg;
    }
    
    std::cout << "Loading config from: " << config_path << std::endl;
    
    std::string line, section;
    while(std::getline(in,line)) 
    {
        auto start = line.find_first_not_of(" \t\r\n");
        if(start==std::string::npos) continue;
        if(line[start]=='#' || line[start]==';') continue;
        if(line[start]=='[') 
        {
            auto end = line.find(']', start+1);
            if(end!=std::string::npos) section = line.substr(start+1, end-start-1);
            continue;
        }
        auto eq = line.find('=', start);
        if(eq==std::string::npos) continue;
        std::string key = line.substr(start, eq-start);
        std::string val = line.substr(eq+1);
        auto trim = [](std::string &s)
        {
            auto a = s.find_first_not_of(" \t\r\n");
            auto b = s.find_last_not_of(" \t\r\n");
            if(a==std::string::npos) { s.clear(); return; }
            s = s.substr(a, b-a+1);
        };
        trim(key); trim(val);
        std::string full = section.empty() ? key : section + "." + key;
        cfg.values[full]=val;
        
        std::cout << "Config: " << full << " = " << val << std::endl;
    }
    return cfg;
}

std::string make_conn_str(const Config &cfg) 
{
    std::ostringstream ss;
    ss << "host=" << cfg.get("database.host","127.0.0.1")
       << " port=" << cfg.get("database.port","5432")
       << " dbname=" << cfg.get("database.dbname","crawler_db")
       << " user=" << cfg.get("database.user","postgres")
       << " password=" << cfg.get("database.password","qwerty");
    return ss.str();
}