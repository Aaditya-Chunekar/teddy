#include "md_parser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

// ── Out-of-line definitions for recursive wrapper types ───────────────────────
// These must be defined after InlineNode/BlockNode are complete.

InlineChildren::InlineChildren(std::vector<InlineNode> nodes) : v(std::move(nodes)) {}
InlineChildren::~InlineChildren() = default;
InlineChildren::InlineChildren(const InlineChildren&) = default;
InlineChildren::InlineChildren(InlineChildren&&) noexcept = default;
InlineChildren& InlineChildren::operator=(const InlineChildren&) = default;
InlineChildren& InlineChildren::operator=(InlineChildren&&) noexcept = default;

BlockChildren::BlockChildren(std::vector<BlockNode> nodes) : v(std::move(nodes)) {}
BlockChildren::~BlockChildren() = default;
BlockChildren::BlockChildren(const BlockChildren&) = default;
BlockChildren::BlockChildren(BlockChildren&&) noexcept = default;
BlockChildren& BlockChildren::operator=(const BlockChildren&) = default;
BlockChildren& BlockChildren::operator=(BlockChildren&&) noexcept = default;

// ── Utilities ────────────────────────────────────────────────────────────────

static std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char c){ return !std::isspace(c); }));
    return s;
}

static std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char c){ return !std::isspace(c); }).base(), s.end());
    return s;
}

static std::string trim(std::string s) { return ltrim(rtrim(s)); }

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

static int count_leading(const std::string& s, char c) {
    int n = 0;
    for (char x : s) { if (x == c) ++n; else break; }
    return n;
}

// ── Inline parser ────────────────────────────────────────────────────────────
// Handles: **bold**, *italic*, _italic_, ~~strike~~, `code`, [text](url), ![alt](url)

static std::vector<InlineNode> parse_inline(std::string_view src);

static std::vector<InlineNode> parse_inline_span(std::string_view src) {
    return parse_inline(src);
}

static std::vector<InlineNode> parse_inline(std::string_view src) {
    std::vector<InlineNode> out;
    std::string buf;
    size_t i = 0;
    auto flush = [&]() {
        if (!buf.empty()) { out.push_back(InlineText{buf}); buf.clear(); }
    };

    while (i < src.size()) {
        // Image ![alt](url)
        if (src[i] == '!' && i + 1 < src.size() && src[i+1] == '[') {
            flush();
            size_t j = i + 2;
            while (j < src.size() && src[j] != ']') ++j;
            std::string alt(src.substr(i + 2, j - i - 2));
            if (j + 1 < src.size() && src[j+1] == '(') {
                size_t k = j + 2;
                while (k < src.size() && src[k] != ')') ++k;
                std::string url(src.substr(j + 2, k - j - 2));
                out.push_back(InlineImage{alt, url});
                i = k + 1;
                continue;
            }
            buf += src[i++];
            continue;
        }

        // Link [text](url)
        if (src[i] == '[') {
            flush();
            size_t j = i + 1;
            while (j < src.size() && src[j] != ']') ++j;
            if (j < src.size() && j + 1 < src.size() && src[j+1] == '(') {
                std::string text(src.substr(i + 1, j - i - 1));
                size_t k = j + 2;
                while (k < src.size() && src[k] != ')') ++k;
                std::string url(src.substr(j + 2, k - j - 2));
                out.push_back(InlineLink{text, url});
                i = k + 1;
                continue;
            }
            buf += src[i++];
            continue;
        }

        // Inline code `...`
        if (src[i] == '`') {
            flush();
            size_t j = i + 1;
            while (j < src.size() && src[j] != '`') ++j;
            out.push_back(InlineCode{std::string(src.substr(i + 1, j - i - 1))});
            i = j + 1;
            continue;
        }

        // Strikethrough ~~...~~
        if (i + 1 < src.size() && src[i] == '~' && src[i+1] == '~') {
            flush();
            size_t j = i + 2;
            while (j + 1 < src.size() && !(src[j] == '~' && src[j+1] == '~')) ++j;
            if (j + 1 < src.size()) {
                auto inner = parse_inline_span(src.substr(i + 2, j - i - 2));
                out.push_back(InlineStrike{InlineChildren{std::move(inner)}});
                i = j + 2;
                continue;
            }
            buf += src[i++];
            continue;
        }

        // Bold **...** or __...__ (check before italic)
        if ((src[i] == '*' || src[i] == '_') &&
            i + 1 < src.size() && src[i+1] == src[i]) {
            char delim = src[i];
            flush();
            size_t j = i + 2;
            while (j + 1 < src.size() && !(src[j] == delim && src[j+1] == delim)) ++j;
            if (j + 1 < src.size()) {
                auto inner = parse_inline_span(src.substr(i + 2, j - i - 2));
                out.push_back(InlineBold{InlineChildren{std::move(inner)}});
                i = j + 2;
                continue;
            }
            buf += src[i++];
            continue;
        }

        // Italic *...* or _..._
        if (src[i] == '*' || src[i] == '_') {
            char delim = src[i];
            flush();
            size_t j = i + 1;
            while (j < src.size() && src[j] != delim) ++j;
            if (j < src.size()) {
                auto inner = parse_inline_span(src.substr(i + 1, j - i - 1));
                out.push_back(InlineItalic{InlineChildren{std::move(inner)}});
                i = j + 1;
                continue;
            }
            buf += src[i++];
            continue;
        }

        buf += src[i++];
    }
    flush();
    return out;
}

// ── Table parser ─────────────────────────────────────────────────────────────

static bool is_table_row(const std::string& line) {
    return !line.empty() && line.front() == '|';
}

static bool is_table_separator(const std::string& line) {
    for (char c : line)
        if (c != '|' && c != '-' && c != ':' && c != ' ') return false;
    return line.find('-') != std::string::npos;
}

static TableRow parse_table_row(const std::string& line, bool header) {
    TableRow row;
    std::string l = line;
    if (!l.empty() && l.front() == '|') l = l.substr(1);
    if (!l.empty() && l.back()  == '|') l.pop_back();
    std::istringstream ss(l);
    std::string cell;
    while (std::getline(ss, cell, '|')) {
        TableCell tc;
        tc.header = header;
        tc.inlines = parse_inline(trim(cell));
        row.cells.push_back(std::move(tc));
    }
    return row;
}

// ── Block parser ─────────────────────────────────────────────────────────────

static std::vector<BlockNode> parse_blocks(const std::vector<std::string>& lines,
                                           int start, int end);

static std::vector<BlockNode> parse_blocks_all(const std::vector<std::string>& lines) {
    return parse_blocks(lines, 0, (int)lines.size());
}

static std::vector<BlockNode> parse_blocks(const std::vector<std::string>& lines,
                                           int start, int end) {
    std::vector<BlockNode> blocks;
    int i = start;

    while (i < end) {
        const std::string& raw = lines[i];
        std::string line = rtrim(raw);

        // Blank line
        if (trim(line).empty()) { ++i; continue; }

        // Fenced code block ```
        if (starts_with(line, "```") || starts_with(line, "~~~")) {
            char fence = line[0];
            std::string lang = trim(line.substr(3));
            std::string code;
            ++i;
            while (i < end && !(lines[i].size() >= 3 &&
                   lines[i][0] == fence && lines[i][1] == fence && lines[i][2] == fence)) {
                code += lines[i] + "\n";
                ++i;
            }
            ++i; // closing fence
            if (!code.empty() && code.back() == '\n') code.pop_back();
            blocks.push_back(BlockCodeBlock{lang, code});
            continue;
        }

        // Horizontal rule --- or *** or ___
        {
            std::string t = trim(line);
            if (t.size() >= 3) {
                char c0 = t[0];
                if ((c0 == '-' || c0 == '*' || c0 == '_')) {
                    bool hr = true;
                    for (char c : t) if (c != c0 && c != ' ') { hr = false; break; }
                    if (hr) { blocks.push_back(BlockHRule{}); ++i; continue; }
                }
            }
        }

        // ATX Heading # ... ######
        {
            int hashes = count_leading(line, '#');
            if (hashes >= 1 && hashes <= 6 &&
                line.size() > (size_t)hashes && line[hashes] == ' ') {
                std::string content = trim(line.substr(hashes + 1));
                // strip trailing #
                while (!content.empty() && content.back() == '#') content.pop_back();
                content = trim(content);
                blocks.push_back(BlockHeading{hashes, parse_inline(content)});
                ++i;
                continue;
            }
        }

        // Setext heading (underline = or -)
        if (i + 1 < end) {
            std::string next = trim(lines[i + 1]);
            if (!next.empty() && (next.front() == '=' || next.front() == '-')) {
                bool setext = true;
                char delim = next.front();
                for (char c : next) if (c != delim) { setext = false; break; }
                if (setext) {
                    int level = (delim == '=') ? 1 : 2;
                    blocks.push_back(BlockHeading{level, parse_inline(trim(line))});
                    i += 2;
                    continue;
                }
            }
        }

        // Blockquote >
        if (starts_with(line, "> ") || line == ">") {
            std::vector<std::string> bq_lines;
            while (i < end) {
                std::string l = rtrim(lines[i]);
                if (starts_with(l, "> "))      bq_lines.push_back(l.substr(2));
                else if (l == ">")             bq_lines.push_back("");
                else if (!trim(l).empty())     break; // end of blockquote
                else                           break;
                ++i;
            }
            auto children = parse_blocks_all(bq_lines);
            blocks.push_back(BlockQuote{BlockChildren{std::move(children)}});
            continue;
        }

        // Table
        if (is_table_row(line) && i + 1 < end && is_table_separator(lines[i + 1])) {
            BlockTable table;
            table.rows.push_back(parse_table_row(line, true));
            ++i; // skip separator
            ++i;
            while (i < end && is_table_row(lines[i])) {
                table.rows.push_back(parse_table_row(lines[i], false));
                ++i;
            }
            blocks.push_back(std::move(table));
            continue;
        }

        // Unordered list
        {
            char marker = 0;
            if (!line.empty() && (line[0] == '-' || line[0] == '*' || line[0] == '+') &&
                line.size() >= 2 && line[1] == ' ')
                marker = line[0];
            if (marker) {
                BlockList lst;
                lst.ordered = false;
                lst.start = 1;
                while (i < end) {
                    std::string l = rtrim(lines[i]);
                    if (!l.empty() && l[0] == marker && l.size() >= 2 && l[1] == ' ') {
                        lst.items.push_back({parse_inline(trim(l.substr(2))), {}});
                        ++i;
                    } else if (!trim(l).empty() && (l[0] == ' ' || l[0] == '\t')) {
                        // continuation — attach to last item (simplified)
                        if (!lst.items.empty()) {
                            auto more = parse_inline(trim(l));
                            for (auto& n : more) lst.items.back().inlines.push_back(n);
                        }
                        ++i;
                    } else break;
                }
                blocks.push_back(std::move(lst));
                continue;
            }
        }

        // Ordered list 1. 2. etc.
        {
            size_t dot = line.find(". ");
            if (dot != std::string::npos && dot > 0 && dot <= 3) {
                bool is_num = true;
                for (size_t k = 0; k < dot; ++k)
                    if (!isdigit((unsigned char)line[k])) { is_num = false; break; }
                if (is_num) {
                    BlockList lst;
                    lst.ordered = true;
                    lst.start = std::stoi(line.substr(0, dot));
                    while (i < end) {
                        std::string l = rtrim(lines[i]);
                        size_t d = l.find(". ");
                        bool ok = (d != std::string::npos && d > 0 && d <= 3);
                        if (ok) {
                            bool nm = true;
                            for (size_t k = 0; k < d; ++k)
                                if (!isdigit((unsigned char)l[k])) { nm = false; break; }
                            ok = nm;
                        }
                        if (ok) {
                            lst.items.push_back({parse_inline(trim(l.substr(dot + 2))), {}});
                            ++i;
                        } else break;
                    }
                    blocks.push_back(std::move(lst));
                    continue;
                }
            }
        }

        // Paragraph — collect until blank line or block-level marker
        {
            std::string para_text;
            while (i < end) {
                std::string l = rtrim(lines[i]);
                if (trim(l).empty()) break;
                if (!para_text.empty()) para_text += ' ';
                para_text += l;
                ++i;
            }
            blocks.push_back(BlockParagraph{parse_inline(para_text)});
            continue;
        }
    }
    return blocks;
}

// ── Entry point ───────────────────────────────────────────────────────────────

Document parse_markdown(std::string_view src) {
    std::vector<std::string> lines;
    std::istringstream ss{std::string(src)};
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    Document doc;
    doc.blocks = parse_blocks_all(lines);
    return doc;
}
