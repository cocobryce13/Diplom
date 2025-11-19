#pragma once
#include <string>
#include <map>

struct Config 
{
    std::map<std::string,std::string> values;
    
    std::string get(const std::string &key, const std::string &def="") const 
    {
        auto it = values.find(key);
        if(it==values.end()) return def;
        return it->second;
    }
    
    int get_int(const std::string &key, int def=0) const 
    {
        auto v = get(key,"");
        if(v.empty()) return def;
        try { return std::stoi(v); } catch(...) { return def; }
    }
};

Config load_config(const std::string &path="config.ini");
std::string make_conn_str(const Config &cfg);