#pragma once
#include "md_parser.hpp"
#include <string>
#include <vector>

// Renders a Document into a vector of terminal lines.
// Each line is a string that may contain ANSI escape codes.
// visible_width is the available column count (e.g. half the terminal).
std::vector<std::string> render_markdown(const Document& doc, int visible_width);
