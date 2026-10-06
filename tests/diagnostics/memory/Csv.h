#pragma once
#include <fstream>
#include <map>
#include <string>
#include <stdexcept>
#include <vector>
namespace memory_test {
using Row=std::map<std::string,std::string>;
inline std::vector<std::string> split(const std::string& text) {
    std::vector<std::string> fields;std::string value;bool quoted=false;
    for(std::size_t i=0;i<text.size();++i) {
        const char c=text[i];
        if(c=='"') {
            if(quoted && i+1<text.size() && text[i+1]=='"') {value+='"';++i;}
            else quoted=!quoted;
        } else if(c==',' && !quoted) {fields.push_back(value);value.clear();}
        else value+=c;
    }
    fields.push_back(value);return fields;
}
inline std::vector<Row> rows(const std::string& path) {
    std::ifstream file(path);std::string line;std::getline(file,line);const auto header=split(line);
    std::vector<Row> result;
    while(std::getline(file,line)) {
        const auto values=split(line);if(values.size()!=header.size()) throw std::runtime_error("Malformed memory CSV");
        Row row;for(std::size_t i=0;i<header.size();++i) row[header[i]]=values[i];result.push_back(std::move(row));
    }
    return result;
}
}
