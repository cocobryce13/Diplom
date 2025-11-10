#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

Config load_config(const std::string &path) 
{
    Config cfg;
    std::ifstream in(path);
    if(!in) return cfg;
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
    }
    return cfg;
}

    std::string make_conn_str(const Config& cfg) 
    {
    std::ostringstream ss;
    ss << "host=" << cfg.get("database.host", "127.0.0.1")
        << " port=" << cfg.get("database.port", "5432")
        << " dbname=" << cfg.get("database.dbname", "crawler_db")
        << " user=" << cfg.get("database.user", "postgres")
        << " password=" << cfg.get("database.password", "qwerty");
    return ss.str();
    }