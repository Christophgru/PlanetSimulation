#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace config {

class Config {
public:
    Config() = default;
    explicit Config(nlohmann::json&& json) : m_data(std::move(json)) {}
    
    static Config load(const std::string& path);

    std::string get(std::string key) const;
    std::string get(std::string key, std::string defaultVal) const;
    double getDouble(std::string key, double defaultVal = 0.0) const;
    int getInt(std::string key, int defaultVal = 0) const;
    bool getBool(std::string key, bool defaultVal = false) const;
    std::vector<double> getArray(std::string key, std::vector<double> defaultVal = {}) const;
    
    const nlohmann::json& data() const { return m_data; }

private:
    nlohmann::json m_data;
};

} // namespace config
