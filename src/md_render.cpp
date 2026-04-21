#include "md_render.hpp"
#include "terminal.hpp"
#include <sstream>
#include <algorithm>
#include <numeric>

using namespace terminal;

// ── Inline rendering ─────────────────────────────────────────────────────────

static std::string render_inline_node(const InlineNode& node);

static std::string render_inlines(const std::vector<InlineNode>& nodes) {
    std::string out;
    for (auto& n : nodes) out += render_inline_node(n);
    return out;
}

static std::string render_inline_node(const InlineNode& node) {
    return std::visit([](auto&& n) -> std::string {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, InlineText>)
            return n.text;
        if constexpr (std::is_same_v<T, InlineCode>)
            return sgr(7) + " " + n.code + " " + reset();
        if constexpr (std::is_same_v<T, InlineBold>)
            return sgr(1) + render_inlines(n.children.v) + reset();
        if constexpr (std::is_same_v<T, InlineItalic>)
            return sgr(3) + render_inlines(n.children.v) + reset();
        if constexpr (std::is_same_v<T, InlineStrike>)
            return sgr(9) + render_inlines(n.children.v) + reset();
        if constexpr (std::is_same_v<T, InlineLink>)
            return "\x1b[4;34m" + n.text + reset() +
                   sgr(2) + " (" + n.url + ")" + reset();
        if constexpr (std::is_same_v<T, InlineImage>)
            return sgr(2) + "[img: " + n.alt + "]" + reset() +
                   "\x1b[4;34m " + n.url + reset();
        return "";
    }, node);
}

// ── Word wrap ─────────────────────────────────────────────────────────────────
// Wraps a rendered (may contain ANSI) string to fit visible_width columns.
// Splits on spaces only; ANSI codes don't count toward width.

static std::vector<std::string> wrap(const std::string& text, int width, int indent = 0) {
    if (width <= 0) return {text};
    std::string prefix(indent, ' ');
    std::vector<std::string> out;
    // tokenize by spaces (plain text words — ANSI codes stay attached)
    std::vector<std::string> words;
    std::string cur;
    bool in_esc = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (in_esc) {
            cur += c;
            if (c == 'm') in_esc = false;
        } else if (c == '\x1b' && i + 1 < text.size() && text[i+1] == '[') {
            in_esc = true;
            cur += c;
        } else if (c == ' ') {
            words.push_back(cur); cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) words.push_back(cur);

    std::string line = prefix;
    int line_vlen = indent;
    for (auto& w : words) {
        int wvlen = visible_len(w);
        if (line_vlen + (line_vlen > indent ? 1 : 0) + wvlen > width && line_vlen > indent) {
            out.push_back(line);
            line = prefix;
            line_vlen = indent;
        }
        if (line_vlen > indent) { line += ' '; ++line_vlen; }
        line += w;
        line_vlen += wvlen;
    }
    if (line_vlen > indent || out.empty()) out.push_back(line);
    return out;
}

// ── Block rendering ───────────────────────────────────────────────────────────

static void push(std::vector<std::string>& lines, const std::string& s) {
    lines.push_back(s);
}

static std::string hline(int w, char c = '-', const std::string& box = "─") {
    std::string out;
    for (int i = 0; i < w; ++i) out += (c == '-') ? box : std::string(1, c);
    return out;
}

// heading colors by level: 1=cyan bold, 2=yellow bold, 3=green bold, 4-6=white
static const char* heading_color(int level) {
    switch (level) {
        case 1: return "\x1b[1;36m";
        case 2: return "\x1b[1;33m";
        case 3: return "\x1b[1;32m";
        default: return "\x1b[1;37m";
    }
}

static void render_block(const BlockNode& block, std::vector<std::string>& out,
                          int width, int depth = 0);

static void render_block_list(const std::vector<BlockNode>& blocks,
                               std::vector<std::string>& out, int width, int depth = 0) {
    for (auto& b : blocks) render_block(b, out, width, depth);
}

static void render_block(const BlockNode& block, std::vector<std::string>& out,
                          int width, int depth) {
    std::visit([&](auto&& b) {
        using T = std::decay_t<decltype(b)>;

        // ── Heading ──────────────────────────────────────────────────────────
        if constexpr (std::is_same_v<T, BlockHeading>) {
            std::string text = render_inlines(b.inlines);
            std::string colored = heading_color(b.level) + text + reset();
            push(out, "");
            push(out, colored);
            if (b.level <= 2)
                push(out, sgr(2) + hline(std::min(visible_len(text) + 2, width)) + reset());
            push(out, "");
        }

        // ── Paragraph ────────────────────────────────────────────────────────
        else if constexpr (std::is_same_v<T, BlockParagraph>) {
            std::string text = render_inlines(b.inlines);
            auto wrapped = wrap(text, width - depth * 2, depth * 2);
            for (auto& l : wrapped) push(out, l);
            push(out, "");
        }

        // ── Code block ───────────────────────────────────────────────────────
        else if constexpr (std::is_same_v<T, BlockCodeBlock>) {
            int w = std::min(width, 72);
            std::string lang_label = b.lang.empty() ? "" : " " + b.lang + " ";
            // top border
            push(out, sgr(2) + "┌─" + lang_label +
                 hline(w - 4 - (int)b.lang.size()) + "─┐" + reset());
            // code lines
            std::istringstream ss(b.code);
            std::string line;
            while (std::getline(ss, line)) {
                std::string content = "\x1b[48;5;236m" + sgr(2) + "│" + reset() +
                                      "\x1b[48;5;236m " +
                                      clip_visible(line, w - 4) +
                                      std::string(std::max(0, w - 4 - visible_len(line)), ' ') +
                                      reset() + sgr(2) + " │" + reset();
                push(out, content);
            }
            // bottom border
            push(out, sgr(2) + "└" + hline(w - 2) + "┘" + reset());
            push(out, "");
        }

        // ── Blockquote ───────────────────────────────────────────────────────
        else if constexpr (std::is_same_v<T, BlockQuote>) {
            std::vector<std::string> inner;
            render_block_list(b.children.v, inner, width - 2, depth + 1);
            for (auto& l : inner) {
                push(out, "\x1b[2;36m│\x1b[0m " + l);
            }
            push(out, "");
        }

        // ── List ─────────────────────────────────────────────────────────────
        else if constexpr (std::is_same_v<T, BlockList>) {
            // bullet chars by depth
            static const char* bullets[] = {"•", "◦", "▸", "–"};
            const char* bullet = bullets[std::min(depth, 3)];
            int num = b.start;
            for (auto& item : b.items) {
                std::string marker;
                if (b.ordered)
                    marker = "\x1b[1m" + std::to_string(num++) + ".\x1b[0m ";
                else
                    marker = "\x1b[33m" + std::string(bullet) + "\x1b[0m ";

                std::string text = render_inlines(item.inlines);
                int prefix_len = b.ordered ? 3 : 2;
                int indent = depth * 2 + prefix_len;
                auto wrapped = wrap(marker + text, width, depth * 2);
                // continuation lines indented by marker width
                bool first = true;
                for (auto& l : wrapped) {
                    if (first) { push(out, l); first = false; }
                    else       { push(out, std::string(indent, ' ') + l); }
                }
                if (!item.sublist.v.empty())
                    render_block_list(item.sublist.v, out, width - 2, depth + 1);
            }
            push(out, "");
        }

        // ── Table ────────────────────────────────────────────────────────────
        else if constexpr (std::is_same_v<T, BlockTable>) {
            if (b.rows.empty()) return;
            int ncols = (int)b.rows[0].cells.size();
            if (ncols == 0) return;

            // compute column widths — max visible content width per col
            std::vector<int> col_w(ncols, 1);
            for (auto& row : b.rows) {
                for (int c = 0; c < (int)row.cells.size() && c < ncols; ++c) {
                    std::string text = render_inlines(row.cells[c].inlines);
                    col_w[c] = std::max(col_w[c], visible_len(text));
                }
            }
            // cap total width
            int total = std::accumulate(col_w.begin(), col_w.end(), (ncols + 1) * 3);
            if (total > width) {
                int excess = total - width;
                int max_c = (int)(std::max_element(col_w.begin(), col_w.end()) - col_w.begin());
                col_w[max_c] = std::max(3, col_w[max_c] - excess);
            }

            auto border_row = [&](const char* l, const char* m, const char* r, const char* h) {
                std::string s = l;
                for (int c = 0; c < ncols; ++c) {
                    for (int k = 0; k < col_w[c] + 2; ++k) s += h;
                    s += (c + 1 < ncols ? m : r);
                }
                return sgr(2) + s + reset();
            };

            push(out, border_row("┌", "┬", "┐", "─"));
            bool first_row = true;
            for (auto& row : b.rows) {
                std::string line = sgr(2) + "│" + reset();
                for (int c = 0; c < ncols; ++c) {
                    std::string text = c < (int)row.cells.size()
                        ? render_inlines(row.cells[c].inlines) : "";
                    if (row.cells[c].header)
                        text = sgr(1) + text + reset();
                    int pad = col_w[c] - visible_len(
                        c < (int)row.cells.size() ? render_inlines(row.cells[c].inlines) : "");
                    line += " " + text + std::string(std::max(0, pad + 1), ' ');
                    line += sgr(2) + "│" + reset();
                }
                push(out, line);
                if (first_row) {
                    push(out, border_row("├", "┼", "┤", "─"));
                    first_row = false;
                }
            }
            push(out, border_row("└", "┴", "┘", "─"));
            push(out, "");
        }

        // ── HRule ────────────────────────────────────────────────────────────
        else if constexpr (std::is_same_v<T, BlockHRule>) {
            push(out, sgr(2) + hline(width) + reset());
            push(out, "");
        }

    }, block);
}

// ── Entry point ───────────────────────────────────────────────────────────────

std::vector<std::string> render_markdown(const Document& doc, int visible_width) {
    std::vector<std::string> out;
    render_block_list(doc.blocks, out, visible_width);
    return out;
}
