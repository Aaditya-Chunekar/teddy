#include "terminal.hpp"
#include <stdexcept>
#include <cstdint>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>

namespace terminal {

void enable_raw_mode(struct termios& orig) {
    if (tcgetattr(STDIN_FILENO, &orig) == -1)
        throw std::runtime_error(strerror(errno));
    struct termios raw = orig;
    raw.c_iflag &= ~(uint32_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(uint32_t)(OPOST);
    raw.c_cflag |=  (uint32_t)(CS8);
    raw.c_lflag &= ~(uint32_t)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        throw std::runtime_error(strerror(errno));
}

void disable_raw_mode(struct termios& orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

int read_key() {
    char c = 0;
    while (read(STDIN_FILENO, &c, 1) != 1) {}

    if (c != '\x1b') return (unsigned char)c;

    char seq[4] = {};
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return ESC;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return ESC;

    if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
            if (read(STDIN_FILENO, &seq[2], 1) != 1) return ESC;
            if (seq[2] == '~') {
                switch (seq[1]) {
                    case '1': return HOME_KEY;
                    case '3': return DEL_KEY;
                    case '4': return END_KEY;
                    case '5': return PAGE_UP;
                    case '6': return PAGE_DOWN;
                    case '7': return HOME_KEY;
                    case '8': return END_KEY;
                }
            }
        } else {
            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
    } else if (seq[0] == 'O') {
        switch (seq[1]) {
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
        }
    }
    return ESC;
}

bool get_window_size(int& rows, int& cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return false;
    rows = ws.ws_row;
    cols = ws.ws_col;
    return true;
}

std::string move_cursor(int row, int col) {
    return "\x1b[" + std::to_string(row) + ";" + std::to_string(col) + "H";
}

std::string clear_screen() { return "\x1b[2J"; }
std::string clear_line()   { return "\x1b[K"; }
std::string hide_cursor()  { return "\x1b[?25l"; }
std::string show_cursor()  { return "\x1b[?25h"; }
std::string sgr(int code)  { return "\x1b[" + std::to_string(code) + "m"; }
std::string reset()        { return "\x1b[0m"; }

int visible_len(const std::string& s) {
    int n = 0;
    bool in_esc = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (in_esc) {
            if (s[i] == 'm') in_esc = false;
        } else if (s[i] == '\x1b' && i + 1 < s.size() && s[i+1] == '[') {
            in_esc = true;
            ++i;
        } else {
            ++n;
        }
    }
    return n;
}

std::string clip_visible(const std::string& s, int n) {
    std::string out;
    int count = 0;
    bool in_esc = false;
    for (size_t i = 0; i < s.size() && count < n; ++i) {
        if (in_esc) {
            out += s[i];
            if (s[i] == 'm') in_esc = false;
        } else if (s[i] == '\x1b' && i + 1 < s.size() && s[i+1] == '[') {
            in_esc = true;
            out += s[i];
        } else {
            out += s[i];
            ++count;
        }
    }
    // close any open SGR
    out += "\x1b[0m";
    return out;
}

} // namespace terminal
