#pragma once
#include <string>
#include <termios.h>

namespace terminal {

enum Key {
    KEY_NULL    = 0,
    CTRL_A      = 1,
    CTRL_C      = 3,
    CTRL_D      = 4,
    CTRL_F      = 6,
    CTRL_H      = 8,
    TAB         = 9,
    CTRL_L      = 12,
    ENTER       = 13,
    CTRL_P      = 16,
    CTRL_Q      = 17,
    CTRL_S      = 19,
    CTRL_U      = 21,
    CTRL_Z      = 26,
    ESC         = 27,
    BACKSPACE   = 127,
    ARROW_LEFT  = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

void enable_raw_mode(struct termios& orig);
void disable_raw_mode(struct termios& orig);
int  read_key();
bool get_window_size(int& rows, int& cols);

// ANSI helpers — all return escape sequences as strings
std::string move_cursor(int row, int col); // 1-indexed
std::string clear_screen();
std::string clear_line();
std::string hide_cursor();
std::string show_cursor();
std::string sgr(int code);               // e.g. sgr(1) = bold
std::string reset();

// Strip ANSI escape sequences, return visible character count
int visible_len(const std::string& s);
// Clip string to n visible characters, preserving ANSI codes
std::string clip_visible(const std::string& s, int n);

} // namespace terminal
