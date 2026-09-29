#include "config/Config.h"
#include <fstream>
#include <stdexcept>

namespace config {
Config Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }
    
    nlohmann::json json;
    file >> json;
    return Config{std::move(json)};
}

std::string Config::get(std::string key) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->get<std::string>();
    }
    static const std::string empty;
    return empty;
}

std::string Config::get(std::string key, std::string defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->get<std::string>();
    }
    return defaultVal;
}

double Config::getDouble(std::string key, double defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->get<double>();
    }
    return defaultVal;
}

int Config::getInt(std::string key, int defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->get<int>();
    }
    return defaultVal;
}

bool Config::getBool(std::string key, bool defaultVal) const {
    auto it = m_data.find(key);
    if (it != m_data.end() && it->is_boolean()) {
        return it->get<bool>();
    }
    return defaultVal;
}

std::vector<double> Config::getArray(std::string key, std::vector<double> defaultVal) const {
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
