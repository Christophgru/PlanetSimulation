#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>

namespace config {

class Config {
public:
    Config() = default;
    explicit Config(nlohmann::json&& json) : m_data(std::move(json)) {}
    
    static Config load(const std::string& path);
    
    const std::string& get(const std::string& key) const;
    double getDouble(const std::string& key, double defaultVal = 0.0) const;
    int getInt(const std::string& key, int defaultVal = 0) const;
    bool getBool(const std::string& key, bool defaultVal = false) const;
    std::vector<double> getArray(const std::string& key, std::vector<double> defaultVal = {}) const;
    
    const nlohmann::json& data() const { return m_data; }

private:
    nlohmann::json m_data;
};

inline Config Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }
    
    nlohmann::json json;
    file >> json;
    return Config{std::move(json)};
}

inline const std::string& Config::get(const std::string& key) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it.value();
    }
    static const std::string empty;
    return empty;
}

inline double Config::getDouble(const std::string& key, double defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it.value<double>(defaultVal);
    }
    return defaultVal;
}

inline int Config::getInt(const std::string& key, int defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it.value<int>(defaultVal);
    }
    return defaultVal;
}

inline bool Config::getBool(const std::string& key, bool defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it.value<bool>(defaultVal);
    }
    return defaultVal;
}

inline std::vector<double> Config::getArray(const std::string& key, std::vector<double> defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        // Check if the value is actually an array
        if (it.value().is_array()) {
            std::vector<double> result;
            for (const auto& val : it.value()) {
                result.push_back(val.get<double>());
            }
            return result;
        }
    }
    return defaultVal;
}

} // namespace config
