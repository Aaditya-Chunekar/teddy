# teddy

A C++20 terminal text editor derived from kilo, with syntax highlighting,
incremental search, undo/redo, and a live split-pane markdown preview.

```
┌─ teddy ──────────────────────────────────────────────────────────────────────┐
│ # Heading            │  # Heading                                            │
│                      │  ─────────                                            │
│ **bold** text        │  **bold** text                                        │
│                      │                                                       │
│ | A  | B  |          │  ┌─────┬─────┐                                        │
│ |----|----|          │  │ A   │ B   │                                        │
│ | 1  | 2  |          │  ├─────┼─────┤                                        │
│                      │  │ 1   │ 2   │                                        │
│                      │  └─────┴─────┘                                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

## Building

Requires: `cmake >= 3.16`, `g++ >= 11` or `clang++ >= 13`, POSIX terminal.

```sh
git clone https://github.com/Aaditya-Chunekar/teddy
cd teddy
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/teddy [file]
```

No external dependencies. No curses.

## Keybinds

| Key        | Action                                      |
|------------|---------------------------------------------|
| `Ctrl-P`   | Toggle markdown preview (`.md` files only)  |
| `Ctrl-S`   | Save (prompts for filename if new)          |
| `Ctrl-F`   | Incremental search (arrows = next/prev)     |
| `Ctrl-Z`   | Undo                                        |
| `Ctrl-U`   | Redo                                        |
| `Ctrl-Q`   | Quit (press twice if unsaved changes)       |
| `Arrows`   | Move cursor                                 |
| `PgUp/Dn`  | Scroll by screen                            |
| `Home/End` | Start/end of line                           |

## Markdown preview

Activate with `Ctrl-P` on any `.md` or `.markdown` file. The preview pane
occupies the right half of the terminal and updates on every keystroke.

Rendered constructs:

| Construct       | Rendering                                        |
|-----------------|--------------------------------------------------|
| H1              | Bold cyan + full-width underline                 |
| H2              | Bold yellow + underline                          |
| H3–H6           | Bold green / white, no underline                 |
| Bold            | ANSI bold (`\e[1m`)                              |
| Italic          | ANSI italic (`\e[3m`)                            |
| Strikethrough   | ANSI strikethrough (`\e[9m`)                     |
| Inline code     | Reverse video                                    |
| Code block      | Dark background region with box border + lang    |
| Blockquote      | `│` gutter, dimmed content                       |
| Unordered list  | `• ◦ ▸` by nesting depth                        |
| Ordered list    | Numeric, aligned                                 |
| Table           | Full box-drawing borders, two-pass column sizing |
| Link            | Blue underline text + dimmed URL                 |
| Image           | `[img: alt]` + dimmed URL                        |
| Horizontal rule | Full-width `─`                                   |

Preview scroll tracks editor cursor position proportionally.

## Syntax highlighting

Detected by file extension:

| Language | Extensions               |
|----------|--------------------------|
| C/C++    | `.c .h .cpp .hpp .cc`    |
| Python   | `.py`                    |
| Rust     | `.rs`                    |

Highlighted: keywords, types, strings, numbers, comments, preprocessor directives, search matches.

## Architecture

```
src/
├── main.cpp          — entry point
├── terminal.{hpp,cpp} — raw mode, VT100 escape sequences, visible_len, clip_visible
├── buffer.{hpp,cpp}   — row storage (std::vector<Row>), undo/redo stack
├── syntax.{hpp,cpp}   — highlight database + per-row tokenizer
├── md_parser.{hpp,cpp} — block parser + inline parser → AST
│                          BlockNode / InlineNode (std::variant, recursive via wrapper)
├── md_render.{hpp,cpp} — AST → vector<string> of ANSI-annotated terminal lines
└── editor.{hpp,cpp}   — Editor class: event loop, split-pane draw, search, save
```

### Data structures

- **Text buffer**: `std::vector<Row>`, each `Row` holds `chars` (raw), `render`
  (tab-expanded), and `hl` (parallel highlight byte array). Simple and fast
  for typical file sizes; replace with a piece table if you need large-file
  or column-selection support.
- **Undo stack**: `std::deque<Edit>` of full row snapshots, capped at 200
  entries. Snapshot taken on each destructive action (save, enter, delete).
- **AST**: `std::variant`-based node types with out-of-line `InlineChildren` /
  `BlockChildren` wrapper structs to break the recursive type dependency.
- **Screen render**: single `std::string` buffer assembled per frame, written
  atomically with one `write()` call. No double buffering needed.

## Limitations / known gaps

- No mouse support.
- No Unicode width accounting (CJK wide chars will misalign).
- Inline emphasis uses greedy delimiter matching, not the full CommonMark
  left/right-flanking algorithm — edge cases will differ from spec.
- No soft-wrap in editor pane (horizontal scroll only).
- Kitty/sixel image rendering not implemented; images show as `[img: alt]`.
