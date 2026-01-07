/**
 * vim: set ts=4 sw=4 tw=99 noet:
 * =============================================================================
 * SourceMod YAML Configuration Parser
 * Copyright (C) 2024 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Version: 1.0.0
 */

#ifndef _INCLUDE_SOURCEMOD_YAML_CONFIG_PARSER_H_
#define _INCLUDE_SOURCEMOD_YAML_CONFIG_PARSER_H_

#include <string>
#include <vector>
#include <map>
#include <cstring>

/**
 * @brief YAML Configuration Parser for SourceMod
 * 
 * This class provides a high-level interface for parsing YAML configuration
 * files, specifically designed for modegroup.yaml support.
 */
class YAMLConfigParser
{
public:
    struct ModeGroupConfig {
        std::string name;
        std::string description;
        std::vector<std::string> required_plugins;
        std::map<std::string, std::string> settings;
        std::vector<std::string> dependencies;
    };

    struct PluginConfig {
        std::string file;
        std::string name;
        bool enabled;
        std::string mode;
        std::map<std::string, std::string> settings;
    };

public:
    YAMLConfigParser();
    ~YAMLConfigParser();

    /**
     * @brief Load and parse a modegroup.yaml file
     * 
     * @param path      Path to the YAML file
     * @param error     Error message buffer
     * @param maxlength Maximum length of error message
     * @return          True on success, false on failure
     */
    bool LoadModeGroupConfig(const char* path, char* error, size_t maxlength);

    /**
     * @brief Load YAML from a string buffer
     * 
     * @param content   YAML content string
     * @param error     Error message buffer
     * @param maxlength Maximum length of error message
     * @return          True on success, false on failure
     */
    bool LoadFromString(const char* content, char* error, size_t maxlength);

    /**
     * @brief Parse YAML content (internal method)
     */
    bool ParseContent(const char* content, char* error, size_t maxlength);

    /**
     * @brief Get the list of parsed mode groups
     */
    const std::vector<ModeGroupConfig>& GetModeGroups() const { return mode_groups_; }

    /**
     * @brief Get the list of parsed plugin configurations
     */
    const std::vector<PluginConfig>& GetPlugins() const { return plugins_; }

    /**
     * @brief Find a mode group by name
     * 
     * @param name Name of the mode group to find
     * @return     Pointer to the mode group, or nullptr if not found
     */
    const ModeGroupConfig* FindModeGroup(const char* name) const;

    /**
     * @brief Check if a plugin should be loaded for a given mode
     * 
     * @param filename Plugin filename to check
     * @param mode     Mode name to check against (can be nullptr for all modes)
     * @return         True if the plugin should be loaded
     */
    bool ShouldLoadPlugin(const char* filename, const char* mode = nullptr) const;

    /**
     * @brief Clear all parsed data
     */
    void Clear();

private:
    bool ParseModeGroup(const YAML::Node& node, char* error, size_t maxlength);
    bool ParsePlugin(const YAML::Node& node, char* error, size_t maxlength);
    bool ParseSettings(const YAML::Node& settingsNode, std::map<std::string, std::string>& settings);

    std::vector<ModeGroupConfig> mode_groups_;
    std::vector<PluginConfig> plugins_;
    bool parse_error_;
    char last_error_[256];
};

#endif // _INCLUDE_SOURCEMOD_YAML_CONFIG_PARSER_H_
