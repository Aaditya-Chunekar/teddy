#include "buffer.hpp"
#include <algorithm>

void Row::update(int tabstop) {
    render.clear();
    for (char c : chars) {
        if (c == '\t') {
            render += ' ';
            while ((int)render.size() % tabstop != 0) render += ' ';
        } else {
            render += c;
        }
    }
    hl.assign(render.size(), HlType::Normal);
}

void Buffer::insert_char(int cy, int cx, int c) {
    if (cy >= (int)rows.size()) {
        rows.push_back({});
        rows.back().update();
    }
    auto& r = rows[cy];
    int at = std::clamp(cx, 0, (int)r.chars.size());
    r.chars.insert(at, 1, (char)c);
    r.update();
    dirty = true;
}

void Buffer::delete_char(int cy, int cx) {
    if (cy >= (int)rows.size()) return;
    auto& r = rows[cy];
    if (cx <= 0 || cx > (int)r.chars.size()) return;
    r.chars.erase(cx - 1, 1);
    r.update();
    dirty = true;
}

void Buffer::insert_newline(int cy, int cx) {
    if (cy >= (int)rows.size()) {
        insert_row((int)rows.size(), "");
        return;
    }
    auto& r = rows[cy];
    std::string tail = r.chars.substr(cx);
    r.chars = r.chars.substr(0, cx);
    r.update();
    insert_row(cy + 1, tail);
    dirty = true;
}

void Buffer::delete_row(int at) {
    if (at < 0 || at >= (int)rows.size()) return;
    rows.erase(rows.begin() + at);
    dirty = true;
}

void Buffer::insert_row(int at, std::string_view s) {
    at = std::clamp(at, 0, (int)rows.size());
    Row r;
    r.chars = s;
    r.update();
    rows.insert(rows.begin() + at, std::move(r));
    dirty = true;
}

std::string Buffer::to_string() const {
    std::string out;
    for (auto& r : rows) {
        out += r.chars;
        out += '\n';
    }
    return out;
}

void Buffer::snapshot() {
    Edit e;
    e.rows = rows;
    e.cx = e.cy = 0;
    undo_stack.push_back(std::move(e));
    if ((int)undo_stack.size() > MAX_UNDO)
        undo_stack.pop_front();
    redo_stack.clear();
}

bool Buffer::undo(int& cx, int& cy) {
    if (undo_stack.empty()) return false;
    Edit cur;
    cur.rows = rows;
    cur.cx = cx;
    cur.cy = cy;
    redo_stack.push_back(std::move(cur));

    auto& e = undo_stack.back();
    rows = e.rows;
    cx = e.cx;
    cy = e.cy;
    undo_stack.pop_back();
    dirty = true;
    return true;
}

bool Buffer::redo(int& cx, int& cy) {
    if (redo_stack.empty()) return false;
    Edit cur;
    cur.rows = rows;
    cur.cx = cx;
    cur.cy = cy;
    undo_stack.push_back(std::move(cur));

    auto& e = redo_stack.back();
    rows = e.rows;
    cx = e.cx;
    cy = e.cy;
    redo_stack.pop_back();
    dirty = true;
    return true;
}
