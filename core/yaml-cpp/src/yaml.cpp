/**
 * vim: set ts=4 sw=4 tw=99 noet:
 * =============================================================================
 * SourceMod YAML Parser Implementation
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

#include "yaml-cpp/yaml.h"

namespace YAML {

// ============================================================================
// Node Implementation
// ============================================================================

Node::Node(const char* tag)
    : type_(Node::Type_Null), tag_(tag ? strdup(tag) : nullptr), 
      scalar_value_(nullptr), first_child_(nullptr), last_child_(nullptr),
      next_sibling_(nullptr), map_key_(nullptr)
{
}

Node::Node(const Node& other)
    : type_(other.type_), 
      tag_(other.tag_ ? strdup(other.tag_) : nullptr),
      scalar_value_(other.scalar_value_ ? strdup(other.scalar_value_) : nullptr),
      first_child_(nullptr), last_child_(nullptr),
      next_sibling_(nullptr), map_key_(nullptr)
{
    // Deep copy children
    if (other.first_child_) {
        Node* child = other.first_child_;
        while (child) {
            Node* copy = new Node(*child);
            AddChild(copy);
            child = child->next_sibling_;
        }
    }
}

Node::~Node()
{
    if (tag_) free(tag_);
    if (scalar_value_) free(scalar_value_);
    
    // Delete all children
    Node* child = first_child_;
    while (child) {
        Node* next = child->next_sibling_;
        delete child;
        child = next;
    }
}

void Node::Clear()
{
    if (tag_) { free(tag_); tag_ = nullptr; }
    if (scalar_value_) { free(scalar_value_); scalar_value_ = nullptr; }
    
    // Delete children
    Node* child = first_child_;
    while (child) {
        Node* next = child->next_sibling_;
        delete child;
        child = next;
    }
    first_child_ = last_child_ = nullptr;
    next_sibling_ = nullptr;
    map_key_ = nullptr;
}

void Node::AddChild(Node* child)
{
    if (!child) return;
    
    child->next_sibling_ = nullptr;
    
    if (!first_child_) {
        first_child_ = last_child_ = child;
    } else {
        last_child_->next_sibling_ = child;
        last_child_ = child;
    }
}

void Node::SetMapKey(Node* key)
{
    map_key_ = key;
}

size_t Node::size() const
{
    size_t count = 0;
    Node* child = first_child_;
    while (child) {
        // Skip map keys
        if (!child->map_key_) {
            count++;
        }
        child = child->next_sibling_;
    }
    return count;
}

Node Node::operator[](size_t index) const
{
    size_t current = 0;
    Node* child = first_child_;
    while (child) {
        // Skip map keys
        if (!child->map_key_) {
            if (current == index) {
                return *child;
            }
            current++;
        }
        child = child->next_sibling_;
    }
    return Node(); // Return null node
}

Node Node::operator[](const char* key) const
{
    Node* child = first_child_;
    while (child) {
        if (child->map_key_ && child->map_key_->scalar_value_) {
            if (strcmp(child->map_key_->scalar_value_, key) == 0) {
                return *child;
            }
        }
        child = child->next_sibling_;
    }
    return Node(); // Return null node
}

bool Node::HasKey(const char* key) const
{
    Node* child = first_child_;
    while (child) {
        if (child->map_key_ && child->map_key_->scalar_value_) {
            if (strcmp(child->map_key_->scalar_value_, key) == 0) {
                return true;
            }
        }
        child = child->next_sibling_;
    }
    return false;
}

Node& Node::push_back(const Node& node)
{
    Node* copy = new Node(node);
    AddChild(copy);
    return *this;
}

Node& Node::push_back(const char* value)
{
    Node* child = new Node();
    child->type_ = Node::Type_Scalar;
    child->scalar_value_ = strdup(value);
    AddChild(child);
    return *this;
}

Node& Node::operator=(const char* value)
{
    Clear();
    type_ = Node::Type_Scalar;
    scalar_value_ = strdup(value);
    return *this;
}

Node& Node::operator=(int value)
{
    Clear();
    type_ = Node::Type_Scalar;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    scalar_value_ = strdup(buffer);
    return *this;
}

Node& Node::operator=(bool value)
{
    Clear();
    type_ = Node::Type_Scalar;
    scalar_value_ = strdup(value ? "true" : "false");
    return *this;
}

// ============================================================================
// Parser Implementation
// ============================================================================

Node Parser::Load(const char* content)
{
    ParseState state;
    state.content = content;
    state.length = strlen(content);
    state.position = 0;
    state.line = 0;
    state.column = 0;
    state.indent_top = 0;
    state.indent_stack[0] = -1;
    state.root = new Node();
    state.current = state.root;
    state.current_key = nullptr;
    state.in_sequence = false;

    char line_buffer[4096];

    while (state.position < state.length) {
        ReadToEndOfLine(&state, line_buffer, sizeof(line_buffer));

        // Skip empty lines and comments
        SkipWhitespace(&state);
        if (state.position >= state.length || line_buffer[0] == '\0' || line_buffer[0] == '#') {
            continue;
        }

        int indent = GetIndent(&state);
        ProcessLine(&state, line_buffer, indent);
    }

    return *state.root;
}

Node Parser::LoadFile(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        throw Exception("Failed to open file");
    }

    // Read entire file
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(fp);
        throw Exception("Failed to allocate memory");
    }

    size_t read = fread(content, 1, size, fp);
    content[read] = '\0';
    fclose(fp);

    Node result = Load(content);
    free(content);

    return result;
}

void Parser::SkipWhitespace(ParseState* state)
{
    while (state->position < state->length && 
           isspace((unsigned char)state->content[state->position])) {
        if (state->content[state->position] == '\n') {
            state->line++;
            state->column = 0;
        }
        state->position++;
        state->column++;
    }
}

void Parser::SkipComments(ParseState* state)
{
    if (state->position < state->length && 
        state->content[state->position] == '#') {
        while (state->position < state->length && 
               state->content[state->position] != '\n') {
            state->position++;
        }
    }
}

bool Parser::AtEndOfLine(ParseState* state)
{
    return state->position >= state->length || 
           state->content[state->position] == '\n';
}

void Parser::ReadToEndOfLine(ParseState* state, char* buffer, size_t size)
{
    size_t i = 0;
    while (i < size - 1 && state->position < state->length && 
           state->content[state->position] != '\n') {
        buffer[i++] = state->content[state->position++];
    }
    buffer[i] = '\0';

    // Skip the newline
    if (state->position < state->length && state->content[state->position] == '\n') {
        state->position++;
        state->line++;
        state->column = 0;
    }
}

int Parser::GetIndent(ParseState* state)
{
    int indent = 0;
    const char* p = state->content + state->position;
    
    while (*p == ' ' || *p == '\t') {
        indent += (*p == ' ') ? 1 : 4; // Tab = 4 spaces
        p++;
    }
    
    return indent;
}

void Parser::TrimTrailingWhitespace(char* str)
{
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

void Parser::ProcessLine(ParseState* state, const char* line, int indent)
{
    // Determine line type
    const char* p = line;
    while (*p && *p != ':' && !isspace((unsigned char)*p)) {
        p++;
    }
    
    bool has_colon = (*p == ':');
    
    // Extract key
    char key_buffer[1024];
    size_t key_len = p - line;
    if (key_len >= sizeof(key_buffer)) {
        key_len = sizeof(key_buffer) - 1;
    }
    strncpy(key_buffer, line, key_len);
    key_buffer[key_len] = '\0';
    
    // Skip past colon and whitespace
    const char* value_start = p;
    if (has_colon) {
        value_start++;
        while (*value_start && isspace((unsigned char)*value_start)) {
            value_start++;
        }
    }
    
    // Determine parent node based on indent
    while (state->indent_top > 0 && indent <= state->indent_stack[state->indent_top]) {
        state->indent_top--;
        state->current = state->current->map_key_ ? state->current->map_key_ : state->current;
        // Move up to find the parent map
        Node* parent = state->current;
        while (parent && parent->first_child_) {
            Node* last = parent->last_child_;
            if (last && last->map_key_) {
                state->current = last;
                break;
            }
            break;
        }
    }
    
    if (has_colon && (*value_start == '\0' || *value_start == '#' || 
                      (state->indent_top >= 0 && indent > state->indent_stack[state->indent_top]))) {
        // This is a new mapping (key: value with nested content)
        Node* new_node = new Node();
        new_node->type_ = Node::Type_Map;
        new_node->scalar_value_ = strdup(key_buffer);
        
        // Add to current node
        state->current->AddChild(new_node);
        
        // Set as map key
        new_node->map_key_ = new Node();
        new_node->map_key_->type_ = Node::Type_Scalar;
        new_node->map_key_->scalar_value_ = strdup(key_buffer);
        
        // Push indent
        state->indent_top++;
        state->indent_stack[state->indent_top] = indent;
        state->current = new_node;
    } else if (*value_start == '-' && *(value_start + 1) == ' ') {
        // This is a sequence item
        Node* seq_node = new Node();
        seq_node->type_ = Node::Type_Scalar;
        
        // Skip "- "
        value_start += 2;
        
        // Extract value
        char value_buffer[1024];
        strncpy(value_buffer, value_start, sizeof(value_buffer) - 1);
        value_buffer[sizeof(value_buffer) - 1] = '\0';
        TrimTrailingWhitespace(value_buffer);
        
        seq_node->scalar_value_ = strdup(value_buffer);
        
        state->current->AddChild(seq_node);
    } else {
        // This is a simple key: value
        Node* value_node = new Node();
        value_node->type_ = Node::Type_Scalar;
        
        char value_buffer[1024];
        strncpy(value_buffer, value_start, sizeof(value_buffer) - 1);
        value_buffer[sizeof(value_buffer) - 1] = '\0';
        TrimTrailingWhitespace(value_buffer);
        
        value_node->scalar_value_ = strdup(value_buffer);
        
        Node* key_node = new Node();
        key_node->type_ = Node::Type_Scalar;
        key_node->scalar_value_ = strdup(key_buffer);
        
        value_node->map_key_ = key_node;
        state->current->AddChild(value_node);
    }
}

Node* Parser::CreateNode(ParseState* state, Node::Type type)
{
    Node* node = new Node();
    node->type_ = type;
    return node;
}

void Parser::SetNodeValue(Node* node, const char* value)
{
    if (node->scalar_value_) {
        free(node->scalar_value_);
    }
    node->scalar_value_ = strdup(value);
}

void Parser::ParseError(ParseState* state, const char* format, ...)
{
    char buffer[256];
    va_list ap;
    
    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);
    
    throw Exception(buffer);
}

char* Parser::ExtractQuotedString(const char* str)
{
    if (!str || (*str != '"' && *str != '\'')) {
        return nullptr;
    }
    
    char quote = *str;
    const char* start = str + 1;
    const char* end = start;
    
    while (*end && *end != quote) {
        if (*end == '\\' && *(end + 1)) {
            end += 2;
        } else {
            end++;
        }
    }
    
    size_t len = end - start;
    char* result = (char*)malloc(len + 1);
    if (!result) return nullptr;
    
    size_t j = 0;
    for (size_t i = 0; i < len && start[i]; i++) {
        if (start[i] == '\\' && start[i + 1]) {
            result[j++] = start[i + 1];
            i++;
        } else {
            result[j++] = start[i];
        }
    }
    result[j] = '\0';
    
    return result;
}

char* Parser::ExtractUnquotedString(const char* str)
{
    if (!str) return nullptr;
    
    // Skip leading whitespace
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    
    if (!*str) return nullptr;
    
    const char* end = str;
    while (*end && !isspace((unsigned char)*end) && *end != '#') {
        end++;
    }
    
    size_t len = end - str;
    char* result = (char*)malloc(len + 1);
    if (!result) return nullptr;
    
    strncpy(result, str, len);
    result[len] = '\0';
    
    return result;
}

} // namespace YAML
