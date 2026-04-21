#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>

// ── Inline nodes ─────────────────────────────────────────────────────────────
// Recursive variants require heap indirection. InlineChildren wraps the
// vector so InlineBold/InlineItalic/InlineStrike can be defined before
// InlineNode is fully resolved.

struct InlineNode; // forward declaration for InlineChildren

struct InlineChildren {
    std::vector<InlineNode> v;
    InlineChildren() = default;
    explicit InlineChildren(std::vector<InlineNode> nodes);
    ~InlineChildren();
    InlineChildren(const InlineChildren&);
    InlineChildren(InlineChildren&&) noexcept;
    InlineChildren& operator=(const InlineChildren&);
    InlineChildren& operator=(InlineChildren&&) noexcept;
};

struct InlineText   { std::string text; };
struct InlineCode   { std::string code; };
struct InlineBold   { InlineChildren children; };
struct InlineItalic { InlineChildren children; };
struct InlineStrike { InlineChildren children; };
struct InlineLink   { std::string text; std::string url; };
struct InlineImage  { std::string alt;  std::string url; };

struct InlineNode : std::variant<
    InlineText, InlineCode, InlineBold, InlineItalic,
    InlineStrike, InlineLink, InlineImage
> {
    using variant::variant;
};

// ── Block nodes ──────────────────────────────────────────────────────────────

struct BlockNode; // forward declaration for BlockChildren

struct BlockChildren {
    std::vector<BlockNode> v;
    BlockChildren() = default;
    explicit BlockChildren(std::vector<BlockNode> nodes);
    ~BlockChildren();
    BlockChildren(const BlockChildren&);
    BlockChildren(BlockChildren&&) noexcept;
    BlockChildren& operator=(const BlockChildren&);
    BlockChildren& operator=(BlockChildren&&) noexcept;
};

struct BlockHeading {
    int level;
    std::vector<InlineNode> inlines;
};

struct BlockParagraph {
    std::vector<InlineNode> inlines;
};

struct BlockCodeBlock {
    std::string lang;
    std::string code;
};

struct BlockQuote {
    BlockChildren children;
};

struct ListItem {
    std::vector<InlineNode> inlines;
    BlockChildren sublist;
};

struct BlockList {
    bool ordered;
    int  start;
    std::vector<ListItem> items;
};

struct TableCell {
    std::vector<InlineNode> inlines;
    bool header;
};

struct TableRow {
    std::vector<TableCell> cells;
};

struct BlockTable {
    std::vector<TableRow> rows;
};

struct BlockHRule {};

struct BlockNode : std::variant<
    BlockHeading, BlockParagraph, BlockCodeBlock,
    BlockQuote, BlockList, BlockTable, BlockHRule
> {
    using variant::variant;
};

struct Document {
    std::vector<BlockNode> blocks;
};

// ── Parser entry point ───────────────────────────────────────────────────────

Document parse_markdown(std::string_view src);
