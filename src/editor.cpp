#include "editor.hpp"
#include "terminal.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <unistd.h>

using namespace terminal;

static void write_all(const std::string& s) {
    const char* p = s.c_str();
    size_t rem = s.size();
    while (rem > 0) {
        ssize_t n = write(STDOUT_FILENO, p, rem);
        if (n <= 0) break;
        p += n; rem -= (size_t)n;
    }
}

// ── Constructor / destructor ──────────────────────────────────────────────────

Editor::Editor() {
    enable_raw_mode(orig_termios);
    if (!get_window_size(screenrows, screencols))
        throw std::runtime_error("can't get terminal size");
    screenrows -= 2; // status bar + message bar
    write_all("\x1b[?1049h");
}

Editor::~Editor() {
    write_all("\x1b[?1049l");
    disable_raw_mode(orig_termios);
    std::string out = clear_screen() + terminal::move_cursor(1, 1);
    write_all(out);
}

// ── File I/O ──────────────────────────────────────────────────────────────────

void Editor::open(const std::string& path) {
    buf.filename = path;
    syntax = detect_syntax(path);

    std::string ext;
    auto dot = path.rfind('.');
    if (dot != std::string::npos) ext = path.substr(dot);
    is_markdown = (ext == ".md" || ext == ".markdown");

    std::ifstream f(path);
    if (!f) { buf.insert_row(0, ""); return; }

    std::string line;
    while (std::getline(f, line))
        buf.insert_row((int)buf.rows.size(), line);
    if (buf.rows.empty()) buf.insert_row(0, "");
    buf.dirty = false;

    update_all_syntax(buf.rows, syntax);
    if (is_markdown && preview_on) rebuild_preview();
}

// ── Width helpers ─────────────────────────────────────────────────────────────

int Editor::editor_width() const {
    if (preview_on) return screencols / 2 - 1;
    return screencols;
}

int Editor::preview_width() const {
    return screencols - screencols / 2 - 1;
}

// ── Cursor helpers ────────────────────────────────────────────────────────────

int Editor::cx_to_rx(int row, int x) const {
    if (row >= (int)buf.rows.size()) return 0;
    int rx = 0;
    for (int j = 0; j < x && j < (int)buf.rows[row].chars.size(); ++j) {
        if (buf.rows[row].chars[j] == '\t')
            rx += 4 - (rx % 4);
        else
            ++rx;
    }
    return rx;
}

void Editor::scroll() {
    rx = cy < (int)buf.rows.size() ? cx_to_rx(cy, cx) : 0;
    if (cy < rowoff) rowoff = cy;
    if (cy >= rowoff + screenrows) rowoff = cy - screenrows + 1;
    if (rx < coloff) coloff = rx;
    if (rx >= coloff + editor_width()) coloff = rx - editor_width() + 1;

    if (preview_on) {
        int total = (int)preview_lines.size();
        float frac = total > 0 && !buf.rows.empty()
            ? (float)cy / (float)buf.rows.size() : 0;
        preview_scroll = (int)(frac * total);
        preview_scroll = std::clamp(preview_scroll, 0, std::max(0, total - screenrows));
    }
}

// ── Syntax ────────────────────────────────────────────────────────────────────

void Editor::update_syntax_from(int row) {
    if (!syntax) return;
    bool open = row > 0 ? buf.rows[row-1].hl_open_comment : false;
    for (int i = row; i < (int)buf.rows.size(); ++i)
        update_syntax(buf.rows[i], syntax, open);
}

// ── Preview ───────────────────────────────────────────────────────────────────

void Editor::rebuild_preview() {
    preview_doc = parse_markdown(buf.to_string());
    preview_lines = render_markdown(preview_doc, preview_width() - 1);
}

// ── Status ────────────────────────────────────────────────────────────────────

void Editor::set_status(const std::string& msg) { statusmsg = msg; }

// ── Drawing ───────────────────────────────────────────────────────────────────

static std::string hl_to_color(HlType t) {
    switch (t) {
        case HlType::Comment:     return "\x1b[38;5;244m";
        case HlType::Keyword:     return "\x1b[1;33m";
        case HlType::Type:        return "\x1b[36m";
        case HlType::String:      return "\x1b[32m";
        case HlType::Number:      return "\x1b[31m";
        case HlType::Preprocessor:return "\x1b[35m";
        case HlType::Operator:    return "\x1b[1;37m";
        case HlType::Match:       return "\x1b[30;43m";
        default:                  return "\x1b[0m";
    }
}

void Editor::draw_rows(std::string& out) {
    int ew = editor_width();
    for (int y = 0; y < screenrows; ++y) {
        out += terminal::move_cursor(y + 1, 1);
        int filerow = y + rowoff;
        if (filerow >= (int)buf.rows.size()) {
            if (buf.rows.empty() && y == screenrows / 3) {
                std::string msg = "teddy -- Ctrl-Q quit  Ctrl-S save  Ctrl-P preview";
                msg = msg.substr(0, std::min((int)msg.size(), ew));
                out += "\x1b[36m" + msg + reset();
            } else {
                out += sgr(2) + "~" + reset();
            }
        } else {
            auto& row = buf.rows[filerow];
            int rlen = (int)row.render.size() - coloff;
            rlen = std::clamp(rlen, 0, ew);
            HlType cur_hl = HlType::Normal;
            out += reset();
            for (int j = 0; j < rlen; ++j) {
                int ridx = j + coloff;
                HlType hl = ridx < (int)row.hl.size() ? row.hl[ridx] : HlType::Normal;
                if (hl != cur_hl) {
                    out += hl_to_color(hl);
                    cur_hl = hl;
                }
                out += row.render[ridx];
            }
            out += reset();
        }
        out += clear_line();
    }
}

void Editor::draw_divider(std::string& out) {
    if (!preview_on) return;
    int col = screencols / 2 + 1;
    for (int y = 1; y <= screenrows; ++y) {
        out += terminal::move_cursor(y, col);
        out += sgr(2) + "│" + reset();
    }
}

void Editor::draw_preview(std::string& out) {
    if (!preview_on) return;
    int pw = preview_width();
    int col = screencols / 2 + 2;
    for (int y = 0; y < screenrows; ++y) {
        out += terminal::move_cursor(y + 1, col);
        int idx = y + preview_scroll;
        if (idx < (int)preview_lines.size()) {
            std::string line = clip_visible(preview_lines[idx], pw);
            out += line;
            // pad to clear remainder
            int pad = pw - visible_len(preview_lines[idx]);
            if (pad > 0) out += std::string(pad, ' ');
        } else {
            out += std::string(pw, ' ');
        }
    }
}

void Editor::draw_status_bar(std::string& out) {
    out += terminal::move_cursor(screenrows + 1, 1);
    out += "\x1b[7m";
    std::string name = buf.filename.empty() ? "[No Name]" : buf.filename;
    std::string modified = buf.dirty ? " [+]" : "";
    std::string syn_name = syntax ? syntax->name : (is_markdown ? "Markdown" : "plain");
    std::string left  = " " + name + modified + "  " + syn_name;
    std::string right = std::to_string(cy + 1) + "/" + std::to_string(buf.rows.size()) +
                        (preview_on ? "  [preview]" : "") + " ";
    int padding = screencols - (int)left.size() - (int)right.size();
    out += left;
    if (padding > 0) out += std::string(padding, ' ');
    out += right;
    out += reset();
}

void Editor::draw_message_bar(std::string& out) {
    out += terminal::move_cursor(screenrows + 2, 1);
    out += clear_line();
    if (!statusmsg.empty())
        out += clip_visible(statusmsg, screencols);
}

void Editor::refresh_screen() {
    scroll();
    std::string out;
    out.reserve(screencols * screenrows * 2);
    out += hide_cursor();
    out += terminal::move_cursor(1, 1);
    draw_rows(out);
    draw_divider(out);
    draw_preview(out);
    draw_status_bar(out);
    draw_message_bar(out);
    // position cursor in editor pane
    int cur_col = (rx - coloff) + 1;
    int cur_row = (cy - rowoff) + 1;
    out += terminal::move_cursor(cur_row, cur_col);
    out += show_cursor();
    write_all(out);
}

// ── Prompt ────────────────────────────────────────────────────────────────────

std::string Editor::prompt(const std::string& prompt_str,
                            void (Editor::*cb)(const std::string&, int)) {
    std::string input;
    while (true) {
        set_status(prompt_str + input);
        refresh_screen();
        int k = read_key();
        if (k == DEL_KEY || k == CTRL_H || k == BACKSPACE) {
            if (!input.empty()) input.pop_back();
        } else if (k == ESC) {
            set_status("");
            if (cb) (this->*cb)(input, k);
            return "";
        } else if (k == ENTER) {
            set_status("");
            if (cb) (this->*cb)(input, k);
            return input;
        } else if (!iscntrl(k) && k < 128) {
            input += (char)k;
        }
        if (cb) (this->*cb)(input, k);
    }
}

// ── Search ────────────────────────────────────────────────────────────────────

void Editor::find_callback(const std::string& query, int key) {
    static std::vector<HlType> saved_hl;
    static int saved_hl_line = -1;

    if (saved_hl_line != -1) {
        buf.rows[saved_hl_line].hl = saved_hl;
        saved_hl_line = -1;
    }
    if (query.empty() || key == ESC) return;

    if (key == ARROW_RIGHT || key == ARROW_DOWN) search_direction = 1;
    else if (key == ARROW_LEFT || key == ARROW_UP) search_direction = -1;
    else { search_direction = 1; search_last_match = -1; }

    int current = search_last_match;
    for (int i = 0; i < (int)buf.rows.size(); ++i) {
        current += search_direction;
        if (current < 0) current = (int)buf.rows.size() - 1;
        if (current >= (int)buf.rows.size()) current = 0;
        auto& row = buf.rows[current];
        auto pos = row.render.find(query);
        if (pos != std::string::npos) {
            search_last_match = current;
            cy = current;
            cx = (int)pos;
            rowoff = (int)buf.rows.size();
            saved_hl = row.hl;
            saved_hl_line = current;
            for (size_t j = pos; j < pos + query.size() && j < row.hl.size(); ++j)
                row.hl[j] = HlType::Match;
            break;
        }
    }
}

void Editor::find() {
    int saved_cx = cx, saved_cy = cy;
    int saved_ro = rowoff, saved_co = coloff;
    search_last_match = -1;
    search_direction  = 1;
    std::string q = prompt("Search (ESC/Enter): ",
                           &Editor::find_callback);
    if (q.empty()) {
        cx = saved_cx; cy = saved_cy;
        rowoff = saved_ro; coloff = saved_co;
    }
}

// ── Save ──────────────────────────────────────────────────────────────────────

void Editor::save_file() {
    if (buf.filename.empty()) {
        buf.filename = prompt("Save as: ");
        if (buf.filename.empty()) { set_status("Save aborted"); return; }
        syntax = detect_syntax(buf.filename);
        std::string ext;
        auto dot = buf.filename.rfind('.');
        if (dot != std::string::npos) ext = buf.filename.substr(dot);
        is_markdown = (ext == ".md" || ext == ".markdown");
        update_all_syntax(buf.rows, syntax);
    }
    std::ofstream f(buf.filename);
    if (!f) { set_status("Save failed: " + std::string(strerror(errno))); return; }
    for (auto& row : buf.rows) f << row.chars << '\n';
    buf.dirty = false;
    set_status("Saved " + buf.filename);
    if (is_markdown && preview_on) rebuild_preview();
}

// ── Cursor movement ───────────────────────────────────────────────────────────

void Editor::move_caret(int key) {
    int nrows = (int)buf.rows.size();
    bool has_row = cy < nrows;
    int row_len = has_row ? (int)buf.rows[cy].chars.size() : 0;

    switch (key) {
        case ARROW_LEFT:
            if (cx > 0) --cx;
            else if (cy > 0) { --cy; cx = (int)buf.rows[cy].chars.size(); }
            break;
        case ARROW_RIGHT:
            if (cx < row_len) ++cx;
            else if (cx == row_len && cy < nrows - 1) { ++cy; cx = 0; }
            break;
        case ARROW_UP:
            if (cy > 0) --cy;
            break;
        case ARROW_DOWN:
            if (cy < nrows - 1) ++cy;
            break;
        case HOME_KEY: cx = 0; break;
        case END_KEY:  cx = has_row ? row_len : 0; break;
    }
    int new_len = cy < nrows ? (int)buf.rows[cy].chars.size() : 0;
    if (cx > new_len) cx = new_len;
}

// ── Key processing ────────────────────────────────────────────────────────────

void Editor::process_key(int k) {
    static int quit_confirm = 2;

    switch (k) {
        case CTRL_Q:
            if (buf.dirty && quit_confirm > 0) {
                set_status("Unsaved changes. Press Ctrl-Q " +
                           std::to_string(quit_confirm) + " more time(s) to quit.");
                --quit_confirm;
                return;
            }
            // clean exit handled by destructor via exception trick
            // — just write clear and exit
            {
                std::string out = clear_screen() + terminal::move_cursor(1,1);
                write_all(out);
                disable_raw_mode(orig_termios);
                exit(0);
            }
            break;

        case CTRL_S:
            buf.snapshot();
            save_file();
            break;

        case CTRL_F:
            find();
            break;

        case CTRL_P:
            if (!is_markdown) { set_status("Preview only available for .md files"); break; }
            preview_on = !preview_on;
            if (preview_on) {
                rebuild_preview();
                set_status("Preview on — Ctrl-P to toggle");
            } else {
                set_status("Preview off");
            }
            // force recalc of widths
            get_window_size(screenrows, screencols);
            screenrows -= 2;
            break;

        case CTRL_Z:
            buf.undo(cx, cy);
            update_all_syntax(buf.rows, syntax);
            if (is_markdown && preview_on) rebuild_preview();
            break;

        case CTRL_U: // redo
            buf.redo(cx, cy);
            update_all_syntax(buf.rows, syntax);
            if (is_markdown && preview_on) rebuild_preview();
            break;

        case ENTER:
            buf.snapshot();
            buf.insert_newline(cy, cx);
            ++cy; cx = 0;
            update_syntax_from(cy - 1);
            if (is_markdown && preview_on) rebuild_preview();
            break;

        case BACKSPACE:
        case CTRL_H:
        case DEL_KEY:
            if (k == DEL_KEY) move_caret(ARROW_RIGHT);
            if (cx > 0) {
                buf.snapshot();
                buf.delete_char(cy, cx);
                --cx;
            } else if (cy > 0) {
                buf.snapshot();
                int new_cx = (int)buf.rows[cy-1].chars.size();
                // merge lines
                buf.rows[cy-1].chars += buf.rows[cy].chars;
                buf.rows[cy-1].update();
                buf.delete_row(cy);
                --cy; cx = new_cx;
            }
            update_syntax_from(cy);
            if (is_markdown && preview_on) rebuild_preview();
            break;

        case PAGE_UP:
        case PAGE_DOWN: {
            int times = screenrows;
            int dir = (k == PAGE_UP) ? ARROW_UP : ARROW_DOWN;
            while (times--) move_caret(dir);
            break;
        }

        case ARROW_UP: case ARROW_DOWN:
        case ARROW_LEFT: case ARROW_RIGHT:
        case HOME_KEY: case END_KEY:
            move_caret(k);
            break;

        case CTRL_L: case ESC:
            break;

        default:
            if (!iscntrl(k)) {
                buf.insert_char(cy, cx, k);
                ++cx;
                update_syntax_from(cy);
                if (is_markdown && preview_on) rebuild_preview();
            }
            break;
    }
    quit_confirm = 2;
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void Editor::run() {
    set_status("Ctrl-S save | Ctrl-Q quit | Ctrl-F find | Ctrl-P md preview | Ctrl-Z undo");
    while (true) {
        refresh_screen();
        process_key(read_key());
    }
}
