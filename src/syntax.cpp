#include "syntax.hpp"
#include <algorithm>
#include <cctype>

static const std::vector<SyntaxDef> SYNTAX_DB = {
    {
        "C/C++",
        {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx"},
        {"if","else","for","while","do","switch","case","break","continue",
         "return","goto","sizeof","typedef","struct","union","enum","class",
         "namespace","template","typename","public","private","protected",
         "virtual","override","final","static","const","constexpr","inline",
         "extern","volatile","register","auto","new","delete","try","catch",
         "throw","nullptr","true","false","this","operator","friend","explicit",
         "noexcept","decltype","using","default"},
        {"int","long","short","char","unsigned","signed","float","double",
         "void","bool","size_t","uint8_t","uint16_t","uint32_t","uint64_t",
         "int8_t","int16_t","int32_t","int64_t","string","vector","map",
         "unordered_map","set","pair","tuple","optional","variant","unique_ptr",
         "shared_ptr","weak_ptr"},
        "//", "/*", "*/",
        true, true
    },
    {
        "Python",
        {".py"},
        {"and","as","assert","async","await","break","class","continue","def",
         "del","elif","else","except","finally","for","from","global","if",
         "import","in","is","lambda","nonlocal","not","or","pass","raise",
         "return","try","while","with","yield","True","False","None"},
        {"int","float","str","bool","list","dict","set","tuple","bytes",
         "bytearray","type","object","Exception"},
        "#", "", "",
        true, true
    },
    {
        "Rust",
        {".rs"},
        {"as","async","await","break","const","continue","crate","dyn","else",
         "enum","extern","false","fn","for","if","impl","in","let","loop",
         "match","mod","move","mut","pub","ref","return","self","Self","static",
         "struct","super","trait","true","type","unsafe","use","where","while"},
        {"i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128",
         "usize","f32","f64","bool","char","str","String","Vec","Option",
         "Result","Box","Rc","Arc"},
        "//", "/*", "*/",
        true, true
    },
};

const SyntaxDef* detect_syntax(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return nullptr;
    std::string ext = filename.substr(dot);
    for (auto& s : SYNTAX_DB)
        for (auto& e : s.extensions)
            if (e == ext) return &s;
    return nullptr;
}

static bool is_sep(char c) {
    return isspace((unsigned char)c) || c == '\0' ||
           std::string(",.()+-/*=~%<>[];{}|&^!?:").find(c) != std::string::npos;
}

void update_syntax(Row& row, const SyntaxDef* syn, bool& open_comment) {
    row.hl.assign(row.render.size(), HlType::Normal);
    if (!syn) return;

    const std::string& s = row.render;
    int n = (int)s.size();
    int i = 0;
    bool prev_sep = true;
    bool in_string = false;
    char str_char = 0;
    bool in_comment = open_comment;

    while (i < n) {
        char c = s[i];

        if (in_comment) {
            row.hl[i] = HlType::Comment;
            if (!syn->block_comment_end.empty() &&
                s.substr(i, syn->block_comment_end.size()) == syn->block_comment_end) {
                for (int k = 0; k < (int)syn->block_comment_end.size(); ++k)
                    row.hl[i + k] = HlType::Comment;
                i += (int)syn->block_comment_end.size();
                in_comment = false;
            } else {
                ++i;
            }
            continue;
        }

        if (syn->strings && in_string) {
            row.hl[i] = HlType::String;
            if (c == '\\' && i + 1 < n) { row.hl[++i] = HlType::String; ++i; continue; }
            if (c == str_char) in_string = false;
            ++i; prev_sep = false;
            continue;
        }

        if (syn->strings && (c == '"' || c == '\'')) {
            in_string = true;
            str_char = c;
            row.hl[i++] = HlType::String;
            prev_sep = false;
            continue;
        }

        if (!syn->line_comment.empty() &&
            s.substr(i, syn->line_comment.size()) == syn->line_comment) {
            for (int k = i; k < n; ++k) row.hl[k] = HlType::Comment;
            break;
        }

        if (!syn->block_comment_start.empty() &&
            s.substr(i, syn->block_comment_start.size()) == syn->block_comment_start) {
            in_comment = true;
            row.hl[i++] = HlType::Comment;
            continue;
        }

        if (c == '#' && syn->name == "C/C++") {
            for (int k = i; k < n && !isspace((unsigned char)s[k]); ++k)
                row.hl[k] = HlType::Preprocessor;
            break;
        }

        if (syn->numbers && (isdigit((unsigned char)c) ||
            (c == '.' && i+1 < n && isdigit((unsigned char)s[i+1]))) && prev_sep) {
            while (i < n && (isalnum((unsigned char)s[i]) || s[i] == '.' || s[i] == '_'))
                row.hl[i++] = HlType::Number;
            prev_sep = false;
            continue;
        }

        if (prev_sep && isalpha((unsigned char)c)) {
            int j = i;
            while (j < n && (isalnum((unsigned char)s[j]) || s[j] == '_')) ++j;
            std::string word = s.substr(i, j - i);
            bool found = false;
            for (auto& kw : syn->keywords)
                if (kw == word) { for (int k=i;k<j;++k) row.hl[k]=HlType::Keyword; found=true; break; }
            if (!found)
                for (auto& tp : syn->types)
                    if (tp == word) { for (int k=i;k<j;++k) row.hl[k]=HlType::Type; found=true; break; }
            if (found) { i = j; prev_sep = false; continue; }
        }

        prev_sep = is_sep(c);
        ++i;
    }

    open_comment = in_comment;
    row.hl_open_comment = open_comment;
}

void update_all_syntax(std::vector<Row>& rows, const SyntaxDef* syn) {
    bool open = false;
    for (auto& r : rows) update_syntax(r, syn, open);
}
