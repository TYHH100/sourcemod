/**
 * vim: set ts=4 sw=4 tw=99 noet:
 * =============================================================================
 * SourceMod YAML Configuration Parser Implementation
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

#include "YAMLConfigParser.h"
#include <bridge/include/ILogger.h>
#include <cstdio>
#include <cstdlib>

YAMLConfigParser::YAMLConfigParser()
    : parse_error_(false)
{
    last_error_[0] = '\0';
}

YAMLConfigParser::~YAMLConfigParser()
{
}

static const char* SkipWhitespace(const char* str)
{
    while (str && *str && (*str == ' ' || *str == '\t')) {
        str++;
    }
    return str;
}

static const char* FindLineEnd(const char* str)
{
    while (str && *str && *str != '\n') {
        str++;
    }
    return str;
}

static void TrimTrailingWhitespace(char* str)
{
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

static bool IsCommentLine(const char* line)
{
    const char* p = SkipWhitespace(line);
    return (p && *p == '#');
}

static bool IsEmptyLine(const char* line)
{
    const char* p = SkipWhitespace(line);
    return (p == nullptr || *p == '\0' || *p == '\n');
}

static int GetIndentLevel(const char* line)
{
    const char* p = line;
    int indent = 0;
    while (*p == ' ') {
        indent++;
        p++;
    }
    return indent;
}

static const char* ExtractKey(const char* line, char* keyBuffer, size_t bufferSize)
{
    const char* p = line;
    
    // Skip leading whitespace
    p = SkipWhitespace(p);
    if (!p || *p == '\0') {
        return nullptr;
    }
    
    // Handle quoted keys
    if (*p == '"' || *p == '\'') {
        char quote = *p;
        p++;
        size_t i = 0;
        while (p && *p && *p != quote && i < bufferSize - 1) {
            if (*p == '\\' && *(p + 1)) {
                p++;
            }
            keyBuffer[i++] = *p++;
        }
        keyBuffer[i] = '\0';
        return p; // Return position after closing quote
    }
    
    // Handle unquoted keys
    size_t i = 0;
    while (p && *p && *p != ':' && !isspace((unsigned char)*p) && i < bufferSize - 1) {
        keyBuffer[i++] = *p++;
    }
    keyBuffer[i] = '\0';
    return p;
}

static const char* ExtractValue(const char* keyEnd, char* valueBuffer, size_t bufferSize)
{
    const char* p = keyEnd;
    
    // Skip past the colon if present
    if (*p == ':') {
        p++;
    }
    
    // Skip whitespace after colon
    p = SkipWhitespace(p);
    if (!p || *p == '\0' || *p == '#') {
        valueBuffer[0] = '\0';
        return p;
    }
    
    // Handle quoted values
    if (*p == '"' || *p == '\'') {
        char quote = *p;
        p++;
        size_t i = 0;
        while (p && *p && *p != quote && i < bufferSize - 1) {
            if (*p == '\\' && *(p + 1)) {
                p++;
            }
            valueBuffer[i++] = *p++;
        }
        valueBuffer[i] = '\0';
        return p;
    }
    
    // Handle unquoted values (until comment or end of line)
    size_t i = 0;
    while (p && *p && *p != '#' && *p != '\n' && i < bufferSize - 1) {
        valueBuffer[i++] = *p++;
    }
    valueBuffer[i] = '\0';
    
    // Trim trailing whitespace
    TrimTrailingWhitespace(valueBuffer);
    
    return p;
}

static bool IsSequenceItem(const char* line, char* valueBuffer, size_t bufferSize)
{
    const char* p = SkipWhitespace(line);
    if (!p || *p != '-') {
        return false;
    }
    
    p++; // Skip '-'
    p = SkipWhitespace(p);
    
    if (valueBuffer && bufferSize > 0) {
        size_t i = 0;
        while (p && *p && *p != '\n' && i < bufferSize - 1) {
            valueBuffer[i++] = *p++;
        }
        valueBuffer[i] = '\0';
        TrimTrailingWhitespace(valueBuffer);
    }
    
    return true;
}

bool YAMLConfigParser::LoadModeGroupConfig(const char* path, char* error, size_t maxlength)
{
    Clear();
    
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        if (error && maxlength > 0) {
            snprintf(error, maxlength, "Could not open file: %s", path);
        }
        parse_error_ = true;
        return false;
    }
    
    // Read entire file
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(fp);
        if (error && maxlength > 0) {
            snprintf(error, maxlength, "Memory allocation failed");
        }
        parse_error_ = true;
        return false;
    }
    
    size_t read = fread(content, 1, size, fp);
    content[read] = '\0';
    fclose(fp);
    
    bool result = ParseContent(content, error, maxlength);
    free(content);
    
    return result;
}

bool YAMLConfigParser::LoadFromString(const char* content, char* error, size_t maxlength)
{
    Clear();
    return ParseContent(content, error, maxlength);
}

bool YAMLConfigParser::ParseContent(const char* content, char* error, size_t maxlength)
{
    if (!content) {
        if (error && maxlength > 0) {
            snprintf(error, maxlength, "Null content provided");
        }
        parse_error_ = true;
        return false;
    }
    
    const char* lineStart = content;
    ModeGroupConfig* currentGroup = nullptr;
    PluginConfig* currentPlugin = nullptr;
    int currentIndent = -1;
    int sectionIndent = -1; // modegroups or plugins section
    
    char lineBuffer[4096];
    
    while (lineStart && *lineStart) {
        // Find end of current line
        const char* lineEnd = FindLineEnd(lineStart);
        size_t lineLen = lineEnd - lineStart;
        
        if (lineLen >= sizeof(lineBuffer)) {
            lineLen = sizeof(lineBuffer) - 1;
        }
        
        strncpy(lineBuffer, lineStart, lineLen);
        lineBuffer[lineLen] = '\0';
        
        // Skip to next line
        lineStart = (*lineEnd == '\n') ? lineEnd + 1 : lineEnd;
        
        // Skip empty lines and comments
        if (IsEmptyLine(lineBuffer) || IsCommentLine(lineBuffer)) {
            continue;
        }
        
        int indent = GetIndentLevel(lineBuffer);
        const char* lineContent = SkipWhitespace(lineBuffer);
        
        // Check for modegroups section
        if (strncmp(lineContent, "modegroups:", 11) == 0) {
            sectionIndent = indent;
            currentGroup = nullptr;
            currentPlugin = nullptr;
            continue;
        }
        
        // Check for plugins section
        if (strncmp(lineContent, "plugins:", 8) == 0) {
            sectionIndent = indent;
            currentGroup = nullptr;
            currentPlugin = nullptr;
            continue;
        }
        
        // Check if we're inside a modegroup
        if (sectionIndent >= 0 && indent > sectionIndent) {
            char keyBuffer[256];
            char valueBuffer[1024];
            
            // Check for sequence item (required_plugins, dependencies)
            char seqValue[1024];
            if (IsSequenceItem(lineContent, seqValue, sizeof(seqValue))) {
                if (currentGroup && indent == sectionIndent + 2) {
                    // This is a required_plugins or dependencies item
                    // Check context by looking at parent key
                    if (strstr(lineContent, "required_plugins") || 
                        (currentGroup && currentGroup->required_plugins.size() > 0 && 
                         strstr(lineContent, "- ") == lineContent)) {
                        currentGroup->required_plugins.push_back(seqValue);
                    } else if (strstr(lineContent, "dependencies") ||
                               (currentGroup && currentGroup->dependencies.size() > 0)) {
                        currentGroup->dependencies.push_back(seqValue);
                    }
                }
                continue;
            }
            
            // Extract key-value pair
            const char* keyEnd = ExtractKey(lineContent, keyBuffer, sizeof(keyBuffer));
            if (!keyBuffer[0]) {
                continue;
            }
            
            const char* valueStart = ExtractValue(keyEnd, valueBuffer, sizeof(valueBuffer));
            
            // Determine what we're setting based on context
            if (strcmp(keyBuffer, "name") == 0) {
                // Start a new mode group
                ModeGroupConfig newGroup;
                newGroup.name = valueBuffer;
                newGroup.description = "";
                mode_groups_.push_back(newGroup);
                currentGroup = &mode_groups_.back();
                currentPlugin = nullptr;
            }
            else if (strcmp(keyBuffer, "description") == 0 && currentGroup) {
                currentGroup->description = valueBuffer;
            }
            else if (strcmp(keyBuffer, "settings") == 0 && currentGroup) {
                // Settings will be handled by nested content
            }
            else if (strcmp(keyBuffer, "key") == 0 && currentGroup) {
                // This is part of a settings sequence
                // We'll handle this with the value on the next iteration
            }
            else if (strcmp(keyBuffer, "value") == 0 && currentGroup) {
                // This is part of a settings sequence - look for key in previous content
                // For simplicity, skip complex nested parsing
            }
            else if (strcmp(keyBuffer, "file") == 0) {
                // Start a new plugin
                PluginConfig newPlugin;
                newPlugin.file = valueBuffer;
                newPlugin.name = valueBuffer;
                newPlugin.enabled = true;
                newPlugin.mode = "";
                plugins_.push_back(newPlugin);
                currentPlugin = &plugins_.back();
                currentGroup = nullptr;
            }
            else if (strcmp(keyBuffer, "enabled") == 0 && currentPlugin) {
                currentPlugin->enabled = (strcmp(valueBuffer, "true") == 0 || 
                                         strcmp(valueBuffer, "yes") == 0 ||
                                         strcmp(valueBuffer, "1") == 0);
            }
            else if (strcmp(keyBuffer, "mode") == 0 && currentPlugin) {
                currentPlugin->mode = valueBuffer;
            }
            else if (strcmp(keyBuffer, "key") == 0 && currentPlugin) {
                // Plugin setting key
                currentPlugin->settings[valueBuffer] = "";
            }
            else if (strcmp(keyBuffer, "value") == 0 && currentPlugin && 
                     !currentPlugin->settings.empty()) {
                // Plugin setting value - find the last key
                if (!currentPlugin->settings.empty()) {
                    auto it = currentPlugin->settings.rbegin();
                    it->second = valueBuffer;
                }
            }
            else if (strcmp(keyBuffer, "key") == 0 && currentGroup && 
                     !currentGroup->settings.empty()) {
                // Group setting key
                currentGroup->settings[valueBuffer] = "";
            }
            else if (strcmp(keyBuffer, "value") == 0 && currentGroup && 
                     !currentGroup->settings.empty()) {
                // Group setting value
                if (!currentGroup->settings.empty()) {
                    auto it = currentGroup->settings.rbegin();
                    it->second = valueBuffer;
                }
            }
        }
    }
    
    return true;
}

const YAMLConfigParser::ModeGroupConfig* YAMLConfigParser::FindModeGroup(const char* name) const
{
    if (!name || !name[0]) {
        return nullptr;
    }
    
    for (const auto& group : mode_groups_) {
        if (group.name == name) {
            return &group;
        }
    }
    
    return nullptr;
}

bool YAMLConfigParser::ShouldLoadPlugin(const char* filename, const char* mode) const
{
    if (!filename || !filename[0]) {
        return true; // Allow unknown plugins by default
    }
    
    // If no mode specified, load all non-disabled plugins
    if (!mode || !mode[0]) {
        for (const auto& plugin : plugins_) {
            if (plugin.file == filename) {
                return plugin.enabled;
            }
        }
        return true; // Plugin not in config, allow loading
    }
    
    // Check if plugin should be loaded for this mode
    for (const auto& plugin : plugins_) {
        if (plugin.file == filename) {
            // Check if plugin is enabled
            if (!plugin.enabled) {
                return false;
            }
            
            // Check if plugin has mode restrictions
            if (plugin.mode.empty()) {
                return true; // No mode restriction, allow
            }
            
            // Check if plugin's mode matches requested mode
            return (plugin.mode == mode);
        }
    }
    
    // Plugin not in config, allow loading
    return true;
}

void YAMLConfigParser::Clear()
{
    mode_groups_.clear();
    plugins_.clear();
    parse_error_ = false;
    last_error_[0] = '\0';
}
