#pragma once
#include <string>
#include <vector>
#include <deque>
#include <cstdint>

// Highlight types — stored per render character
enum class HlType : uint8_t {
    Normal = 0,
    Comment,
    Keyword,
    Type,
    String,
    Number,
    Preprocessor,
    Operator,
    Match, // search match
};

struct Row {
    std::string chars;          // raw content
    std::string render;         // tabs expanded
    std::vector<HlType> hl;     // parallel to render
    bool hl_open_comment = false;

    void update(int tabstop = 4);
};

// Undo/redo — store full row snapshots for simplicity
struct Edit {
    std::vector<Row> rows;
    int cx, cy;
};

class Buffer {
public:
    std::vector<Row> rows;
    bool dirty = false;
    std::string filename;

    void insert_char(int cy, int cx, int c);
    void delete_char(int cy, int cx);
    void insert_newline(int cy, int cx);
    void delete_row(int at);
    void insert_row(int at, std::string_view s);
    std::string to_string() const;

    void snapshot();
    bool undo(int& cx, int& cy);
    bool redo(int& cx, int& cy);

private:
    std::deque<Edit> undo_stack;
    std::deque<Edit> redo_stack;
    static constexpr int MAX_UNDO = 200;
};
