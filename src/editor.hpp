#pragma once
#include "buffer.hpp"
#include "syntax.hpp"
#include "md_parser.hpp"
#include "md_render.hpp"
#include <string>
#include <vector>
#include <termios.h>

class Editor {
public:
    Editor();
    ~Editor();

    void open(const std::string& path);
    void run();

private:
    // ── Terminal state ────────────────────────────────────────────────────────
    struct termios orig_termios;
    int screenrows = 0;
    int screencols  = 0;

    // ── Cursor / scroll ───────────────────────────────────────────────────────
    int cx = 0, cy = 0; // char position in buffer
    int rx = 0;         // render-x (tab-expanded)
    int rowoff = 0, coloff = 0;

    // ── Buffer + syntax ───────────────────────────────────────────────────────
    Buffer buf;
    const SyntaxDef* syntax = nullptr;
    bool is_markdown = false;

    // ── Preview state ─────────────────────────────────────────────────────────
    bool preview_on = false;
    std::vector<std::string> preview_lines;
    int preview_scroll = 0;
    Document preview_doc;

    // ── Search ────────────────────────────────────────────────────────────────
    std::string last_search;
    int search_last_match = -1;
    int search_direction  =  1;

    // ── Status ────────────────────────────────────────────────────────────────
    std::string statusmsg;

    // ── Core loop ─────────────────────────────────────────────────────────────
    void process_key(int k);
    void refresh_screen();

    // ── Drawing ───────────────────────────────────────────────────────────────
    void draw_rows(std::string& out);
    void draw_preview(std::string& out);
    void draw_status_bar(std::string& out);
    void draw_message_bar(std::string& out);
    void draw_divider(std::string& out);

    // ── Helpers ───────────────────────────────────────────────────────────────
    int  editor_width() const;
    int  preview_width() const;
    int  cx_to_rx(int row, int cx) const;
    void scroll();
    void update_syntax_from(int row);
    void rebuild_preview();
    void set_status(const std::string& msg);
    void save_file();
    void find();
    void find_callback(const std::string& query, int key);
    std::string prompt(const std::string& prompt_str,
                       void (Editor::*cb)(const std::string&, int) = nullptr);
    void move_caret(int key);
};
