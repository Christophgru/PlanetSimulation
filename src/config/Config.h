#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace config {

class Config {
public:
    static Config load(const std::string& path);
    
    const std::string& get(const std::string& key) const;
    double getDouble(const std::string& key, double defaultVal = 0.0) const;
    int getInt(const std::string& key, int defaultVal = 0) const;
    bool getBool(const std::string& key, bool defaultVal = false) const;
    std::vector<double> getArray(const std::string& key, std::vector<double> defaultVal = {}) const;

private:
    nlohmann::json m_data;
};

} // namespace config
