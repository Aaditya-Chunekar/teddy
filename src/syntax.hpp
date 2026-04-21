#pragma once
#include "buffer.hpp"
#include <string>
#include <vector>

struct SyntaxDef {
    std::string name;
    std::vector<std::string> extensions;
    std::vector<std::string> keywords;
    std::vector<std::string> types;
    std::string line_comment;
    std::string block_comment_start;
    std::string block_comment_end;
    bool numbers = true;
    bool strings = true;
};

const SyntaxDef* detect_syntax(const std::string& filename);
void update_syntax(Row& row, const SyntaxDef* syn, bool& open_comment);
void update_all_syntax(std::vector<Row>& rows, const SyntaxDef* syn);
