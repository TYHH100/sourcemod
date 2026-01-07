/**
 * vim: set ts=4 sw=4 tw=99 noet:
 * =============================================================================
 * SourceMod YAML Parser (Minimal Implementation)
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

#ifndef _SOURCEMOD_YAML_PARSER_H_
#define _SOURCEMOD_YAML_PARSER_H_

#include <exception>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cctype>

namespace YAML {

/**
 * @brief Simple YAML node representation
 */
class Node {
public:
    enum Type {
        Type_Null,
        Type_Scalar,
        Type_Sequence,
        Type_Map
    };

    Node(const char* tag = nullptr);
    Node(const Node& other);
    ~Node();

    Type GetType() const { return type_; }

    bool IsScalar() const { return type_ == Type_Scalar; }
    bool IsSequence() const { return type_ == Type_Sequence; }
    bool IsMap() const { return type_ == Type_Map; }
    bool IsNull() const { return type_ == Type_Null; }

    const char* as_string() const { return scalar_value_ ? scalar_value_ : ""; }
    int as_int() const { return scalar_value_ ? atoi(scalar_value_) : 0; }
    bool as_bool() const { 
        if (!scalar_value_) return false;
        return (strcmp(scalar_value_, "true") == 0 || 
                strcmp(scalar_value_, "yes") == 0 ||
                strcmp(scalar_value_, "1") == 0);
    }

    size_t size() const;

    Node operator[](size_t index) const;
    Node operator[](const char* key) const;

    bool HasKey(const char* key) const;

    const char* GetTag() const { return tag_; }

    // Mutable accessors for building
    Node& push_back(const Node& node);
    Node& push_back(const char* value);
    Node& operator=(const char* value);
    Node& operator=(int value);
    Node& operator=(bool value);

private:
    Type type_;
    char* tag_;
    char* scalar_value_;

    // For sequences and maps - using linked list
    Node* first_child_;
    Node* last_child_;
    Node* next_sibling_;

    // For map keys
    Node* map_key_;

    void AddChild(Node* child);
    void SetMapKey(Node* key);

    void Clear();

    friend class Parser;
};

/**
 * @brief Exception class for YAML errors
 */
class Exception : public std::exception {
public:
    Exception(const char* msg) {
        message_ = msg ? strdup(msg) : strdup("Unknown YAML error");
    }
    Exception(const Exception& other) {
        message_ = strdup(other.message_);
    }
    ~Exception() { 
        if (message_) free(message_); 
    }

    const char* what() const noexcept { return message_ ? message_ : "Unknown error"; }

private:
    char* message_;
};

/**
 * @brief Simple YAML parser
 */
class Parser {
public:
    Parser() {}
    ~Parser() {}

    Node Load(const char* content);
    Node LoadFile(const char* filename);
    
    // New method to get the root node
    Node GetRoot() const { return root_; }

private:
    struct ParseState {
        const char* content;
        size_t length;
        size_t position;
        int line;
        int column;
        int indent_stack[64];
        int indent_top;
        Node* root;
        Node* current;
        Node* current_key;
        bool in_sequence;
    };

    void SkipWhitespace(ParseState* state);
    void SkipComments(ParseState* state);
    bool AtEndOfLine(ParseState* state);
    void ReadToEndOfLine(ParseState* state, char* buffer, size_t size);
    int GetIndent(ParseState* state);
    void ProcessLine(ParseState* state, const char* line, int indent);
    void TrimTrailingWhitespace(char* str);

    Node* CreateNode(ParseState* state, Node::Type type);
    void SetNodeValue(Node* node, const char* value);

    void ParseError(ParseState* state, const char* format, ...);

    char* ExtractQuotedString(const char* str);
    char* ExtractUnquotedString(const char* str);
};

} // namespace YAML

#endif // _SOURCEMOD_YAML_PARSER_H_
