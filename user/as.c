#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

/* ELF structures */
#define ELF_MAGIC 0x464C457F
#define ELF_EXEC 2
#define PT_LOAD 1
#define PF_R 4
#define PF_W 2
#define PF_X 1
#define SHT_PROGBITS 1
#define SHT_NOBITS 8
#define SHT_STRTAB 3
#define SHF_WRITE 1
#define SHF_ALLOC 2
#define SHF_EXECINSTR 4

#pragma pack(push,1)
typedef struct {
    u8  e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} Elf64_Phdr;

typedef struct {
    u32 sh_name;
    u32 sh_type;
    u64 sh_flags;
    u64 sh_addr;
    u64 sh_offset;
    u64 sh_size;
    u32 sh_link;
    u32 sh_info;
    u64 sh_addralign;
    u64 sh_entsize;
} Elf64_Shdr;

typedef struct {
    u32 st_name;
    u8  st_info;
    u8  st_other;
    u16 st_shndx;
    u64 st_value;
    u64 st_size;
} Elf64_Sym;
#pragma pack(pop)

/* ---------- tokenizer ---------- */
#define T_EOF     0
#define T_ID      1
#define T_NUM     2
#define T_STRING  3
#define T_COMMA   4
#define T_COLON   5
#define T_LBRACK  6
#define T_RBRACK  7
#define T_LPAREN  8
#define T_RPAREN  9
#define T_PLUS    10
#define T_MINUS   11
#define T_STAR    12
#define T_SLASH   13
#define T_PERCENT 14
#define T_NEWLINE 15
#define T_DOT     16
#define T_DOLLAR  17
#define T_LSHIFT  18
#define T_RSHIFT  19
#define T_TILDE   20
#define T_AMP     21
#define T_PIPE    22
#define T_HAT     23
#define T_EXCL    24
#define T_LABEL   25
#define T_DREG    26

typedef struct {
    int type;
    char *str;
    u64 val;
    int line;
} Token;

typedef struct {
    char *src;
    int pos;
    int line;
    Token tok;
} Lexer;

static void lex_init(Lexer *l, char *src) {
    l->src = src; l->pos = 0; l->line = 1;
}

static int is_idchar(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9') || c == '.';
}

static int is_digit(int c) { return c >= '0' && c <= '9'; }

static int is_hex(int c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int lex_next(Lexer *l) {
    char *s = l->src;
    int p = l->pos;
    while (s[p] == ' ' || s[p] == '\t' || s[p] == '\r') p++;
    if (s[p] == ';') {
        while (s[p] && s[p] != '\n') p++;
    }
    if (s[p] == '\n') { p++; l->line++; l->tok.type = T_NEWLINE; l->pos = p; return T_NEWLINE; }
    if (s[p] == '\0') { l->tok.type = T_EOF; return T_EOF; }
    if (s[p] == ',') { p++; l->tok.type = T_COMMA; l->pos = p; return T_COMMA; }
    if (s[p] == ':') { p++; l->tok.type = T_COLON; l->pos = p; return T_COLON; }
    if (s[p] == '[') { p++; l->tok.type = T_LBRACK; l->pos = p; return T_LBRACK; }
    if (s[p] == ']') { p++; l->tok.type = T_RBRACK; l->pos = p; return T_RBRACK; }
    if (s[p] == '(') { p++; l->tok.type = T_LPAREN; l->pos = p; return T_LPAREN; }
    if (s[p] == ')') { p++; l->tok.type = T_RPAREN; l->pos = p; return T_RPAREN; }
    if (s[p] == '+') { p++; l->tok.type = T_PLUS; l->pos = p; return T_PLUS; }
    if (s[p] == '-') { p++; l->tok.type = T_MINUS; l->pos = p; return T_MINUS; }
    if (s[p] == '~') { p++; l->tok.type = T_TILDE; l->pos = p; return T_TILDE; }
    if (s[p] == '!') { p++; l->tok.type = T_EXCL; l->pos = p; return T_EXCL; }
    if (s[p] == '&') { p++; l->tok.type = T_AMP; l->pos = p; return T_AMP; }
    if (s[p] == '|') { p++; l->tok.type = T_PIPE; l->pos = p; return T_PIPE; }
    if (s[p] == '^') { p++; l->tok.type = T_HAT; l->pos = p; return T_HAT; }
    if (s[p] == '$') { p++; l->tok.type = T_DOLLAR; l->pos = p; return T_DOLLAR; }
    if (s[p] == '.') {
        if (p > 0 && s[p-1] == '\n') { p++; l->tok.type = T_DOT; l->pos = p; return T_DOT; }
        p++;
        l->tok.type = T_DOT; l->pos = p; return T_DOT;
    }
    if (s[p] == '*') { p++; l->tok.type = T_STAR; l->pos = p; return T_STAR; }
    if (s[p] == '/') { p++; l->tok.type = T_SLASH; l->pos = p; return T_SLASH; }
    if (s[p] == '%') { p++; l->tok.type = T_PERCENT; l->pos = p; return T_PERCENT; }
    if (s[p] == '<' && s[p+1] == '<') { p += 2; l->tok.type = T_LSHIFT; l->pos = p; return T_LSHIFT; }
    if (s[p] == '>' && s[p+1] == '>') { p += 2; l->tok.type = T_RSHIFT; l->pos = p; return T_RSHIFT; }
    if (s[p] == '"') {
        p++;
        int start = p;
        while (s[p] && s[p] != '"') { if (s[p] == '\\') p++; p++; }
        int len = p - start;
        char *str = malloc(len + 1);
        int wi = 0;
        for (int i = start; i < p; i++) {
            if (s[i] == '\\') { i++; if (s[i] == 'n') str[wi++] = '\n'; else if (s[i] == 't') str[wi++] = '\t'; else if (s[i] == '0') str[wi++] = '\0'; else if (s[i] == '\\') str[wi++] = '\\'; else if (s[i] == '"') str[wi++] = '"'; else str[wi++] = s[i]; }
            else str[wi++] = s[i];
        }
        str[wi] = 0;
        if (s[p] == '"') p++;
        l->tok.type = T_STRING; l->tok.str = str; l->pos = p; return T_STRING;
    }
    if (s[p] == '\'') {
        p++;
        int c;
        if (s[p] == '\\') { p++; if (s[p] == 'n') c = '\n'; else if (s[p] == 't') c = '\t'; else if (s[p] == '0') c = '\0'; else if (s[p] == '\\') c = '\\'; else if (s[p] == '\'') c = '\''; else c = s[p]; p++; }
        else { c = s[p]; p++; }
        if (s[p] == '\'') p++;
        l->tok.type = T_NUM; l->tok.val = (u64)(unsigned char)c; l->pos = p; return T_NUM;
    }
    if (is_digit(s[p])) {
        int base = 10;
        if (s[p] == '0' && (s[p+1] == 'x' || s[p+1] == 'X')) { base = 16; p += 2; }
        else if (s[p] == '0' && (s[p+1] == 'b' || s[p+1] == 'B')) { base = 2; p += 2; }
        else if (s[p] == '0') { base = 8; p++; }
        u64 val = 0;
        while (is_hex(s[p]) && (base != 10 || is_digit(s[p]))) {
            int d;
            if (s[p] >= '0' && s[p] <= '9') d = s[p] - '0';
            else if (s[p] >= 'a' && s[p] <= 'f') d = s[p] - 'a' + 10;
            else if (s[p] >= 'A' && s[p] <= 'F') d = s[p] - 'A' + 10;
            else break;
            val = val * base + d;
            p++;
        }
        l->tok.type = T_NUM; l->tok.val = val; l->pos = p; return T_NUM;
    }
    if (is_idchar(s[p])) {
        int start = p;
        while (is_idchar(s[p])) p++;
        int len = p - start;
        char *str = malloc(len + 1);
        memcpy(str, s + start, len); str[len] = 0;
        l->tok.type = T_ID; l->tok.str = str; l->pos = p; return T_ID;
    }
    l->tok.type = T_EOF; return T_EOF;
}

static int lex_peek(Lexer *l) { return l->tok.type; }

/* ---------- expression parser ---------- */
static u64 parse_expr(Lexer *l, int rbp);

static u64 parse_prim(Lexer *l) {
    if (l->tok.type == T_NUM) { u64 v = l->tok.val; lex_next(l); return v; }
    if (l->tok.type == T_DOLLAR) { lex_next(l); return l->tok.val; }
    if (l->tok.type == T_ID) {
        char *name = l->tok.str;
        lex_next(l);
        if (l->tok.type == T_COLON) {
            lex_next(l);
            return 0;
        }
        return 0;
    }
    if (l->tok.type == T_PLUS) { lex_next(l); return parse_prim(l); }
    if (l->tok.type == T_MINUS) { lex_next(l); return -parse_prim(l); }
    if (l->tok.type == T_TILDE) { lex_next(l); return ~parse_prim(l); }
    if (l->tok.type == T_LPAREN) { lex_next(l); u64 v = parse_expr(l, 0); if (l->tok.type == T_RPAREN) lex_next(l); return v; }
    return 0;
}

static u64 parse_expr(Lexer *l, int rbp) {
    u64 left = parse_prim(l);
    for (;;) {
        int op = l->tok.type;
        int prec = 0;
        if (op == T_STAR || op == T_SLASH || op == T_PERCENT) prec = 50;
        else if (op == T_PLUS || op == T_MINUS) prec = 40;
        else if (op == T_LSHIFT || op == T_RSHIFT) prec = 30;
        else if (op == T_AMP) prec = 20;
        else if (op == T_PIPE) prec = 10;
        else if (op == T_HAT) prec = 15;
        else break;
        if (prec < rbp) break;
        lex_next(l);
        u64 right = parse_expr(l, prec);
        switch (op) {
            case T_PLUS: left += right; break;
            case T_MINUS: left -= right; break;
            case T_STAR: left *= right; break;
            case T_SLASH: left /= right; break;
            case T_PERCENT: left %= right; break;
            case T_LSHIFT: left <<= right; break;
            case T_RSHIFT: left >>= right; break;
            case T_AMP: left &= right; break;
            case T_PIPE: left |= right; break;
            case T_HAT: left ^= right; break;
        }
    }
    return left;
}

/* ---------- sections & symbol table ---------- */
#define MAX_SECTIONS 16
#define MAX_SYMS 4096
#define MAX_RELOCS 4096

typedef struct {
    char name[64];
    u8 *data;
    u64 cap;
    u64 len;
    u64 addr;
    u64 flags;
    int idx;
} Section;

typedef struct {
    char name[128];
    u64 addr;
    int section;
    int defined;
    int is_global;
} Symbol;

typedef struct {
    int offset;
    int size;
    char target[128];
    int section;
    int rel_section;
    int addend;
} Reloc;

static Section sects[MAX_SECTIONS];
static int sect_count;
static Symbol syms[MAX_SYMS];
static int sym_count;
static Reloc relocs[MAX_RELOCS];
static int reloc_count;
static u64 org_vaddr;
static int current_sect;
static int errors;

static Section *find_sect(const char *name) {
    for (int i = 0; i < sect_count; i++)
        if (strcmp(sects[i].name, name) == 0) return &sects[i];
    return 0;
}

static int add_sect(const char *name, u64 flags) {
    Section *s = find_sect(name);
    if (s) return s->idx;
    s = &sects[sect_count];
    strncpy(s->name, name, 63); s->name[63] = 0;
    s->data = 0; s->cap = 0; s->len = 0; s->addr = 0; s->flags = flags;
    s->idx = sect_count++;
    return s->idx;
}

static void sect_emit(Section *s, const void *data, u64 sz) {
    if (s->len + sz > s->cap) {
        s->cap = s->cap ? s->cap * 2 : 4096;
        while (s->len + sz > s->cap) s->cap *= 2;
        s->data = realloc(s->data, s->cap);
    }
    memcpy(s->data + s->len, data, sz);
    s->len += sz;
}

static void sect_align(Section *s, u64 align) {
    u64 off = s->len % align;
    if (off) { u64 pad = align - off; memset(s->data + s->len, 0, pad); s->len += pad; }
}

static Symbol *find_sym(const char *name) {
    for (int i = 0; i < sym_count; i++)
        if (strcmp(syms[i].name, name) == 0) return &syms[i];
    return 0;
}

static Symbol *add_sym(const char *name) {
    Symbol *s = find_sym(name);
    if (s) return s;
    s = &syms[sym_count++];
    strncpy(s->name, name, 127); s->name[127] = 0;
    s->addr = 0; s->section = -1; s->defined = 0; s->is_global = 0;
    return s;
}

static void add_reloc(int offset, int size, const char *target, int section, int addend) {
    if (reloc_count >= MAX_RELOCS) { printf("reloc overflow\n"); exit(1); }
    Reloc *r = &relocs[reloc_count++];
    r->offset = offset; r->size = size;
    strncpy(r->target, target, 127); r->target[127] = 0;
    r->section = section; r->addend = addend;
}

/* ---------- register encoding ---------- */
typedef struct {
    char name[8];
    int num;     
    int w;       
    int is_xmm;  
} Reg;

static Reg parse_reg(const char *s) {
    Reg r = {0}; r.num = -1;
    static const char *regs[] = {
        "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
        "r8","r9","r10","r11","r12","r13","r14","r15",
        "eax","ebx","ecx","edx","esi","edi","ebp","esp",
        "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d",
        "ax","bx","cx","dx","si","di","bp","sp",
        "r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w",
        "al","bl","cl","dl","ah","bh","ch","dh",
        "spl","bpl","sil","dil",
        "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b",
        "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
        "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15",
    };
    static int nums[] = {
        0,3,1,2,6,7,5,4,
        0,1,2,3,4,5,6,7,
        0,3,1,2,6,7,5,4,
        8,9,10,11,12,13,14,15,
        0,3,1,2,6,7,5,4,
        8,9,10,11,12,13,14,15,
        0,3,1,2,4,5,6,7,
        0,3,1,2,4,5,6,7,
        4,5,6,7,
        8,9,10,11,12,13,14,15,
        0,1,2,3,4,5,6,7,
        8,9,10,11,12,13,14,15,
    };
    for (int i = 0; i < (int)(sizeof(regs)/sizeof(regs[0])); i++) {
        if (strcmp(s, regs[i]) == 0) {
            Reg r2;
            if (strstr(s, "xmm")) { strncpy(r2.name, s, 7); r2.num = nums[i]; r2.w = 0; r2.is_xmm = 1; return r2; }
            strncpy(r2.name, s, 7); r2.num = nums[i];
            r2.is_xmm = 0;
            if (s[0] == 'r' && (s[1] >= '0' && s[1] <= '9')) r2.w = 0; 
            else if (s[0] == 'e' && s[1] >= 'a' && s[1] <= 's') r2.w = 1;
            else if (s[0] == 'r' && s[2] != 'w' && s[2] != 'b') r2.w = 0;
            else if (s[2] == 'w' || (s[0] == 'a' && s[1] == 'h') || (s[0] == 'b' && s[1] == 'h') || (s[0] == 'c' && s[1] == 'h') || (s[0] == 'd' && s[1] == 'h')) r2.w = 2;
            else if (s[strlen(s)-1] == 'd' && s[0] == 'r') r2.w = 1;
            else r2.w = 3;
            return r2;
        }
    }
    return r;
}

/* ---------- memory operand ---------- */
typedef struct {
    int has_base; int base;
    int has_index; int index; int scale;
    i64 disp;
    int is_rip;
    char sym[128];
} MemOp;

/* ---------- instruction encoding ---------- */
static u8 *code;
static u64 code_cap, code_len;

static void emit(void *d, u64 sz) {
    if (code_len + sz > code_cap) {
        code_cap = code_cap ? code_cap * 2 : 4096;
        while (code_len + sz > code_cap) code_cap *= 2;
        code = realloc(code, code_cap);
    }
    memcpy(code + code_len, d, sz);
    code_len += sz;
}

static void emit8(u8 v) { emit(&v, 1); }
static void emit16(u16 v) { emit(&v, 2); }
static void emit32(u32 v) { emit(&v, 4); }
static void emit64(u64 v) { emit(&v, 8); }

/* ModRM: mod=0/1/2/3, reg=opcode/reg, rm */
static u8 modrm(u8 mod, u8 reg, u8 rm) {
    return (mod << 6) | ((reg & 7) << 3) | (rm & 7);
}

/* SIB: scale=0/1/2/3, index, base */
static u8 sib(u8 scale, u8 index, u8 base) {
    return ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7);
}

/* REX: W,R,X,B */
static u8 rex(int w, int r, int x, int b) {
    return 0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (b ? 1 : 0);
}

static int needs_rex(int reg) { return reg >= 8; }

/* Encode an instruction with opcode(s), ModRM, optional displacement, optional immediate */
/* Returns bytes emitted */
static void encode_op2(int opcode, int has_rex, u8 rex_val, int mod, int reg, int rm, i64 disp, int disp_size, u64 imm, int imm_size) {
    if (has_rex) emit8(rex_val);
    emit8(opcode);
    emit8(modrm(mod, reg, rm));
    if (disp_size == 1) emit8((u8)(disp & 0xFF));
    else if (disp_size == 4) emit32((u32)disp);
    else if (disp_size == 8) emit64(disp);
    if (imm_size == 1) emit8((u8)(imm & 0xFF));
    else if (imm_size == 2) emit16((u16)(imm & 0xFFFF));
    else if (imm_size == 4) emit32((u32)imm);
    else if (imm_size == 8) emit64(imm);
}

static void encode_mr(int op, const Reg *dst, const Reg *src, int w) {
    (void)dst;
    int r = w ? 1 : 0;
    u8 rv = 0;
    int nr = 0;
    if (needs_rex(src->num) || needs_rex(dst->num) || w) {
        rv = rex(w, (src->num & 8) ? 1 : 0, 0, (dst->num & 8) ? 1 : 0);
        nr = 1;
    }
    encode_op2(op, nr, rv, 3, src->num & 7, dst->num & 7, 0, 0, 0, 0);
}

/* ---------- operand types ---------- */
#define OPR_REG  1
#define OPR_MEM  2
#define OPR_IMM  3
#define OPR_NONE 0

typedef struct {
    int type;
    Reg reg;
    MemOp mem;
    u64 imm;
    int is_sym;
    char sym[128];
    int w;
} Operand;

/* ---------- assembler main logic ---------- */
static int assemble_line(Lexer *l, Section *sect) {
    if (l->tok.type == T_NEWLINE || l->tok.type == T_EOF) return 0;
    if (l->tok.type == T_DOT) {
        lex_next(l);
        if (l->tok.type != T_ID) { printf("expected directive name\n"); errors++; return 1; }
        char *dir = l->tok.str;
        lex_next(l);
        if (strcmp(dir, "text") == 0) {
            current_sect = add_sect(".text", SHF_ALLOC | SHF_EXECINSTR);
            sect = &sects[current_sect];
        } else if (strcmp(dir, "data") == 0) {
            current_sect = add_sect(".data", SHF_ALLOC | SHF_WRITE);
            sect = &sects[current_sect];
        } else if (strcmp(dir, "rodata") == 0) {
            current_sect = add_sect(".rodata", SHF_ALLOC);
            sect = &sects[current_sect];
        } else if (strcmp(dir, "bss") == 0) {
            current_sect = add_sect(".bss", SHF_ALLOC | SHF_WRITE);
            sect = &sects[current_sect];
        } else if (strcmp(dir, "section") == 0) {
            if (l->tok.type == T_STRING || l->tok.type == T_ID) {
                char *sname;
                if (l->tok.type == T_STRING) sname = l->tok.str;
                else sname = l->tok.str;
                u64 flags = SHF_ALLOC;
                if (strcmp(sname, ".text") == 0) flags = SHF_ALLOC | SHF_EXECINSTR;
                else if (strcmp(sname, ".data") == 0) flags = SHF_ALLOC | SHF_WRITE;
                else if (strcmp(sname, ".bss") == 0) flags = SHF_ALLOC | SHF_WRITE;
                current_sect = add_sect(sname, flags);
                sect = &sects[current_sect];
                lex_next(l);
            }
        } else if (strcmp(dir, "global") == 0) {
            while (l->tok.type == T_ID) {
                Symbol *s = add_sym(l->tok.str);
                s->is_global = 1;
                lex_next(l);
                if (l->tok.type == T_COMMA) lex_next(l);
            }
        } else if (strcmp(dir, "extern") == 0) {
            while (l->tok.type == T_ID) {
                Symbol *s = add_sym(l->tok.str);
                s->defined = 1;
                lex_next(l);
                if (l->tok.type == T_COMMA) lex_next(l);
            }
        } else if (strcmp(dir, "align") == 0) {
            u64 align = parse_expr(l, 0);
            sect_align(sect, align);
        } else if (strcmp(dir, "db") == 0) {
            for (;;) {
                if (l->tok.type == T_STRING) {
                    for (char *p = l->tok.str; *p; p++) emit8((u8)*p);
                    free(l->tok.str);
                    lex_next(l);
                } else if (l->tok.type == T_NUM) {
                    u64 v = parse_expr(l, 0);
                    emit8((u8)(v & 0xFF));
                } else break;
                if (l->tok.type == T_COMMA) lex_next(l); else break;
            }
            sect_emit(sect, (u8*)"", 0);
        } else if (strcmp(dir, "dw") == 0) {
            for (;;) {
                u64 v = parse_expr(l, 0);
                emit16((u16)(v & 0xFFFF));
                sect_emit(sect, (u8*)"", 0);
                if (l->tok.type == T_COMMA) lex_next(l); else break;
            }
        } else if (strcmp(dir, "dd") == 0) {
            for (;;) {
                u64 v = parse_expr(l, 0);
                emit32((u32)v);
                sect_emit(sect, (u8*)"", 0);
                if (l->tok.type == T_COMMA) lex_next(l); else break;
            }
        } else if (strcmp(dir, "dq") == 0) {
            for (;;) {
                u64 v = parse_expr(l, 0);
                emit64(v);
                sect_emit(sect, (u8*)"", 0);
                if (l->tok.type == T_COMMA) lex_next(l); else break;
            }
        } else if (strcmp(dir, "resb") == 0) {
            u64 cnt = parse_expr(l, 0);
            for (u64 i = 0; i < cnt; i++) emit8(0);
            sect_emit(sect, (u8*)"", 0);
        } else if (strcmp(dir, "resw") == 0) {
            u64 cnt = parse_expr(l, 0);
            for (u64 i = 0; i < cnt; i++) emit16(0);
            sect_emit(sect, (u8*)"", 0);
        } else if (strcmp(dir, "resd") == 0) {
            u64 cnt = parse_expr(l, 0);
            for (u64 i = 0; i < cnt; i++) emit32(0);
            sect_emit(sect, (u8*)"", 0);
        } else if (strcmp(dir, "resq") == 0) {
            u64 cnt = parse_expr(l, 0);
            for (u64 i = 0; i < cnt; i++) emit64(0);
            sect_emit(sect, (u8*)"", 0);
        }
        return 1;
    }
    if (l->tok.type == T_ID) {
        char *first = l->tok.str;
        lex_next(l);
        if (l->tok.type == T_COLON) {
            lex_next(l);
            Symbol *s = add_sym(first);
            s->addr = sect->len;
            s->section = current_sect;
            s->defined = 1;
            free(first);
            return 1;
        }
        /* Parse instruction */
        int line_errors = 0;
        char *mnemonic = first;
        
        /* Parse operands */
        Operand ops[3];
        int op_count = 0;
        memset(ops, 0, sizeof(ops));
        
        if (l->tok.type != T_NEWLINE && l->tok.type != T_EOF) {
            /* Parse first operand */
            if (l->tok.type == T_LBRACK) {
                /* Memory: [ ... ] */
                lex_next(l);
                ops[op_count].type = OPR_MEM;
                if (l->tok.type == T_ID) {
                    Reg r = parse_reg(l->tok.str);
                    if (r.num >= 0) {
                        ops[op_count].mem.has_base = 1;
                        ops[op_count].mem.base = r.num;
                        lex_next(l);
                    } else {
                        strncpy(ops[op_count].mem.sym, l->tok.str, 127);
                        ops[op_count].is_sym = 1;
                        ops[op_count].mem.is_rip = 1;
                        lex_next(l);
                    }
                    if (l->tok.type == T_PLUS) {
                        lex_next(l);
                        if (l->tok.type == T_ID) {
                            Reg r2 = parse_reg(l->tok.str);
                            if (r2.num >= 0) {
                                ops[op_count].mem.has_index = 1;
                                ops[op_count].mem.index = r2.num;
                                lex_next(l);
                            } else {
                                /* symbol + offset */
                                ops[op_count].mem.disp = parse_expr(l, 0);
                            }
                        } else {
                            ops[op_count].mem.disp = parse_expr(l, 0);
                        }
                    } else if (l->tok.type == T_STAR) {
                        lex_next(l);
                        if (l->tok.type == T_NUM) {
                            ops[op_count].mem.scale = (int)l->tok.val;
                            lex_next(l);
                        }
                    } else if (l->tok.type == T_MINUS) {
                        ops[op_count].mem.disp = -parse_expr(l, 0);
                    }
                } else if (l->tok.type == T_NUM || l->tok.type == T_PLUS || l->tok.type == T_MINUS) {
                    ops[op_count].mem.disp = parse_expr(l, 0);
                }
                if (l->tok.type == T_RBRACK) lex_next(l);
                op_count++;
            } else if (l->tok.type == T_NUM || l->tok.type == T_PLUS || l->tok.type == T_MINUS || l->tok.type == T_TILDE || l->tok.type == T_DOLLAR) {
                ops[op_count].type = OPR_IMM;
                ops[op_count].imm = parse_expr(l, 0);
                op_count++;
            } else if (l->tok.type == T_STRING) {
                ops[op_count].type = OPR_IMM;
                ops[op_count].imm = (u64)l->tok.str;
                ops[op_count].is_sym = 1;
                lex_next(l);
                op_count++;
            } else if (l->tok.type == T_ID) {
                Reg r = parse_reg(l->tok.str);
                if (r.num >= 0) {
                    ops[op_count].type = OPR_REG;
                    ops[op_count].reg = r;
                    lex_next(l);
                    op_count++;
                } else {
                    ops[op_count].is_sym = 1;
                    ops[op_count].type = OPR_IMM;
                    strncpy(ops[op_count].sym, l->tok.str, 127);
                    lex_next(l);
                    op_count++;
                }
            }
            
            if (l->tok.type == T_COMMA) {
                lex_next(l);
                /* Parse second operand */
                if (l->tok.type == T_LBRACK) {
                    lex_next(l);
                    int idx = op_count;
                    ops[idx].type = OPR_MEM;
                    if (l->tok.type == T_ID) {
                        Reg r = parse_reg(l->tok.str);
                        if (r.num >= 0) {
                            ops[idx].mem.has_base = 1;
                            ops[idx].mem.base = r.num;
                            lex_next(l);
                        } else {
                            strncpy(ops[idx].mem.sym, l->tok.str, 127);
                            ops[idx].is_sym = 1;
                            ops[idx].mem.is_rip = 1;
                            lex_next(l);
                        }
                        if (l->tok.type == T_PLUS) {
                            lex_next(l);
                            if (l->tok.type == T_ID) {
                                Reg r2 = parse_reg(l->tok.str);
                                if (r2.num >= 0) {
                                    ops[idx].mem.has_index = 1;
                                    ops[idx].mem.index = r2.num;
                                    lex_next(l);
                                } else {
                                    ops[idx].mem.disp = parse_expr(l, 0);
                                }
                            } else {
                                ops[idx].mem.disp = parse_expr(l, 0);
                            }
                        } else if (l->tok.type == T_STAR) {
                            lex_next(l);
                            if (l->tok.type == T_NUM) {
                                ops[idx].mem.scale = (int)l->tok.val;
                                lex_next(l);
                            }
                        } else if (l->tok.type == T_MINUS) {
                            ops[idx].mem.disp = -parse_expr(l, 0);
                        }
                    } else if (l->tok.type == T_NUM || l->tok.type == T_PLUS || l->tok.type == T_MINUS) {
                        ops[idx].mem.disp = parse_expr(l, 0);
                    }
                    if (l->tok.type == T_RBRACK) lex_next(l);
                    op_count++;
                } else if (l->tok.type == T_NUM || l->tok.type == T_PLUS || l->tok.type == T_MINUS || l->tok.type == T_TILDE) {
                    ops[op_count].type = OPR_IMM;
                    ops[op_count].imm = parse_expr(l, 0);
                    op_count++;
                } else if (l->tok.type == T_ID) {
                    Reg r = parse_reg(l->tok.str);
                    if (r.num >= 0) {
                        ops[op_count].type = OPR_REG;
                        ops[op_count].reg = r;
                        lex_next(l);
                        op_count++;
                    } else {
                        ops[op_count].is_sym = 1;
                        ops[op_count].type = OPR_IMM;
                        strncpy(ops[op_count].sym, l->tok.str, 127);
                        lex_next(l);
                        ops[op_count].imm = 0;
                        op_count++;
                    }
                }
            }
            
            if (l->tok.type == T_COMMA) {
                lex_next(l);
                if (l->tok.type == T_NUM || l->tok.type == T_PLUS || l->tok.type == T_MINUS || l->tok.type == T_TILDE) {
                    ops[op_count].type = OPR_IMM;
                    ops[op_count].imm = parse_expr(l, 0);
                    op_count++;
                } else if (l->tok.type == T_ID) {
                    Reg r = parse_reg(l->tok.str);
                    if (r.num >= 0) {
                        ops[op_count].type = OPR_REG;
                        ops[op_count].reg = r;
                        lex_next(l);
                        op_count++;
                    } else {
                        ops[op_count].is_sym = 1;
                        ops[op_count].type = OPR_IMM;
                        strncpy(ops[op_count].sym, l->tok.str, 127);
                        lex_next(l);
                        op_count++;
                    }
                }
            }
        }
        
        /* Now encode the instruction */
        int op1_type = op_count > 0 ? ops[0].type : OPR_NONE;
        int op2_type = op_count > 1 ? ops[1].type : OPR_NONE;
        int op3_type = op_count > 2 ? ops[2].type : OPR_NONE;
        
        Reg r1 = ops[0].reg, r2 = ops[1].reg, r3 = ops[2].reg;
        u64 imm1 = ops[0].imm, imm2 = ops[1].imm, imm3 = ops[2].imm;
        MemOp m1 = ops[0].mem, m2 = ops[1].mem;
        int w1 = ops[0].w, w2 = ops[1].w;
        
        /* Determine operand size from registers */
        int sz = 8;
        int sz16 = 0, sz8 = 0;
        if (op1_type == OPR_REG) {
            if (r1.w == 1) { sz = 4; }
            else if (r1.w == 2) { sz = 2; sz16 = 1; }
            else if (r1.w == 3) { sz = 1; sz8 = 1; }
        }
        if (op2_type == OPR_REG && op1_type != OPR_REG) {
            if (r2.w == 1) { sz = 4; }
            else if (r2.w == 2) { sz = 2; sz16 = 1; }
            else if (r2.w == 3) { sz = 1; sz8 = 1; }
        }
        
        int w = (sz == 8) ? 1 : 0;
        
        if (strcmp(mnemonic, "mov") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) {
                if (r1.w == 3) {
                    u8 rexb = (r1.num >= 8) ? 4 : 0;
                    u8 rexr = (r2.num >= 8) ? 1 : 0;
                    if (rexb || rexr) emit8(0x40 | rexb | rexr);
                    emit8(0x88 | ((r2.num & 7) << 3) | (r1.num & 7));
                } else if (r1.w == 2) {
                    u8 rexb = (r1.num >= 8) ? 4 : 0;
                    u8 rexr = (r2.num >= 8) ? 1 : 0;
                    if (rexb || rexr) emit8(0x40 | rexb | rexr);
                    emit8(0x66);
                    emit8(0x89);
                    emit8(modrm(3, r2.num & 7, r1.num & 7));
                } else {
                    int is64 = (r1.w == 0);
                    u8 rex = 0x40 | (is64 ? 8 : 0) | (r2.num >= 8 ? 1 : 0) | (r1.num >= 8 ? 4 : 0);
                    if (rex != 0x40) emit8(rex);
                    emit8(0x89);
                    emit8(modrm(3, r2.num & 7, r1.num & 7));
                }
            } else if (op1_type == OPR_REG && op2_type == OPR_MEM) {
                int has_rex = 0; u8 rv = 0;
                if (needs_rex(r1.num) || (sz == 8)) { rv = rex(sz == 8 ? 1 : 0, 0, 0, (r1.num & 8) ? 1 : 0); has_rex = 1; }
                if (r1.w == 3) { emit8(0x40 | (r1.num >= 8 ? 4 : 0) | ((r1.num >= 8 && m2.has_base) ? 1 : 0)); emit8(0x8A); }
                else {
                    if (has_rex) emit8(rv);
                    if (r1.w == 2) emit8(0x66);
                    emit8(0x8B);
                }
                if (m2.has_base) {
                    int mod = 0;
                    if (m2.disp == 0 && m2.base != 5) mod = 0;
                    else if (m2.disp >= -128 && m2.disp <= 127) mod = 1;
                    else mod = 2;
                    if (m2.has_index) {
                        emit8(modrm(mod, r1.num & 7, 4));
                        emit8(sib(m2.scale, m2.index & 7, m2.base & 7));
                    } else {
                        emit8(modrm(mod, r1.num & 7, m2.base & 7));
                    }
                    if (mod == 1) emit8((u8)(m2.disp & 0xFF));
                    else if (mod == 2) emit32((u32)m2.disp);
                    else if (m2.base == 5 && mod == 0) emit32(0);
                } else if (m2.sym[0]) {
                    emit8(modrm(0, r1.num & 7, 5));
                    add_reloc(sect->len, 4, m2.sym, current_sect, -4);
                    emit32(0);
                } else {
                    emit8(modrm(0, r1.num & 7, 4));
                    emit8(sib(0, 4, 5));
                    emit32((u32)m2.disp);
                }
            } else if (op1_type == OPR_MEM && op2_type == OPR_REG) {
                int has_rex = 0; u8 rv = 0;
                if (needs_rex(r2.num) || (sz == 8)) { rv = rex(sz == 8 ? 1 : 0, (r2.num & 8) ? 1 : 0, 0, 0); has_rex = 1; }
                if (r2.w == 3) { emit8(0x40 | (r2.num >= 8 ? 4 : 0)); emit8(0x88); }
                else {
                    if (has_rex) emit8(rv);
                    if (r2.w == 2) emit8(0x66);
                    emit8(0x89);
                }
                if (m1.has_base) {
                    int mod = 0;
                    if (m1.disp == 0 && m1.base != 5) mod = 0;
                    else if (m1.disp >= -128 && m1.disp <= 127) mod = 1;
                    else mod = 2;
                    if (m1.has_index) {
                        emit8(modrm(mod, r2.num & 7, 4));
                        emit8(sib(m1.scale, m1.index & 7, m1.base & 7));
                    } else {
                        emit8(modrm(mod, r2.num & 7, m1.base & 7));
                    }
                    if (mod == 1) emit8((u8)(m1.disp & 0xFF));
                    else if (mod == 2) emit32((u32)m1.disp);
                    else if (m1.base == 5 && mod == 0) emit32(0);
                } else if (m1.sym[0]) {
                    emit8(modrm(0, r2.num & 7, 5));
                    add_reloc(sect->len, 4, m1.sym, current_sect, -4);
                    emit32(0);
                } else {
                    emit8(modrm(0, r2.num & 7, 4));
                    emit8(sib(0, 4, 5));
                    emit32((u32)m1.disp);
                }
            } else if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if (r1.w == 3) {
                    emit8(0x40 | (r1.num >= 8 ? 4 : 0));
                    emit8(0xB0 | (r1.num & 7));
                    emit8((u8)(imm2 & 0xFF));
                } else if (r1.w == 2) {
                    if (r1.num >= 8) emit8(0x41);
                    emit8(0x66);
                    emit8(0xB8 | (r1.num & 7));
                    emit16((u16)(imm2 & 0xFFFF));
                } else if (r1.w == 1) {
                    if (r1.num >= 8) emit8(0x41);
                    emit8(0xB8 | (r1.num & 7));
                    emit32((u32)imm2);
                } else {
                    if (r1.num >= 8) emit8(rex(1,0,0,(r1.num&8)?1:0));
                    else emit8(0x48);
                    emit8(0xB8 | (r1.num & 7));
                    if (ops[1].is_sym && ops[1].sym[0]) {
                        emit64(0);
                        add_reloc(sect->len - 8, 8, ops[1].sym, current_sect, 0);
                    } else {
                        emit64(imm2);
                    }
                }
            } else if (op1_type == OPR_MEM && op2_type == OPR_IMM) {
                if (m1.has_base) {
                    int mod = 0;
                    if (m1.disp == 0 && m1.base != 5) mod = 0;
                    else if (m1.disp >= -128 && m1.disp <= 127) mod = 1;
                    else mod = 2;
                    emit8(0x48);
                    emit8(0xC7);
                    if (m1.has_index) { emit8(modrm(mod, 0, 4)); emit8(sib(m1.scale, m1.index & 7, m1.base & 7)); }
                    else emit8(modrm(mod, 0, m1.base & 7));
                    if (mod == 1) emit8((u8)(m1.disp & 0xFF));
                    else if (mod == 2) emit32((u32)m1.disp);
                    else if (m1.base == 5 && mod == 0) emit32(0);
                    emit32((u32)imm2);
                } else if (m1.sym[0]) {
                    emit8(modrm(0, 0, 5));
                    add_reloc(sect->len, 4, m1.sym, current_sect, -4);
                    emit32(0);
                    emit32((u32)imm2);
                } else {
                    emit8(0x48); emit8(modrm(0, 0, 4)); emit8(sib(0, 4, 5));
                    emit32((u32)m1.disp);
                    emit32((u32)imm2);
                }
            } else if (op1_type == OPR_MEM && op2_type == OPR_MEM) {
                printf("error: mov cannot have two memory operands\n"); errors++;
            }
        } else if (strcmp(mnemonic, "push") == 0) {
            if (op1_type == OPR_REG) {
                if (r1.num >= 8) emit8(0x41);
                emit8(0x50 | (r1.num & 7));
            } else if (op1_type == OPR_MEM) {
                if (m1.has_base) {
                    int mod = m1.disp == 0 ? 0 : (m1.disp >= -128 && m1.disp <= 127 ? 1 : 2);
                    emit8(0xFF);
                    if (m1.has_index) { emit8(modrm(mod, 6, 4)); emit8(sib(m1.scale, m1.index & 7, m1.base & 7)); }
                    else emit8(modrm(mod, 6, m1.base & 7));
                    if (mod == 1) emit8((u8)(m1.disp & 0xFF));
                    else if (mod == 2) emit32((u32)m1.disp);
                } else {
                    emit8(0xFF); emit8(modrm(0, 6, 5));
                    emit32((u32)m1.disp);
                }
            } else {
                emit8(0x68); emit32((u32)imm1);
            }
        } else if (strcmp(mnemonic, "pop") == 0) {
            if (r1.num >= 8) emit8(0x41);
            emit8(0x58 | (r1.num & 7));
        } else if (strcmp(mnemonic, "add") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) {
                encode_mr(0x01, &r1, &r2, 1);
            } else if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if (imm2 == 1) { u8 rv = 0; if (needs_rex(r1.num)) rv = rex(1,0,0,(r1.num&8)?1:0); if (rv) emit8(rv); else emit8(0x48); emit8(0xFF); emit8(modrm(3, 0, r1.num & 7)); }
                else if ((i64)imm2 >= -128 && (i64)imm2 <= 127) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x83); emit8(modrm(3, 0, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x81); emit8(modrm(3, 0, r1.num & 7)); emit32((u32)imm2); }
            } else {
                printf("unsupported add form\n"); errors++;
            }
        } else if (strcmp(mnemonic, "sub") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) {
                encode_mr(0x29, &r1, &r2, 1);
            } else if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if (imm2 == 1) { u8 rv = 0; if (needs_rex(r1.num)) rv = rex(1,0,0,(r1.num&8)?1:0); if (rv) emit8(rv); else emit8(0x48); emit8(0xFF); emit8(modrm(3, 1, r1.num & 7)); }
                else if ((i64)imm2 >= -128 && (i64)imm2 <= 127) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x83); emit8(modrm(3, 5, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x81); emit8(modrm(3, 5, r1.num & 7)); emit32((u32)imm2); }
            } else {
                printf("unsupported sub form\n"); errors++;
            }
        } else if (strcmp(mnemonic, "and") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) encode_mr(0x21, &r1, &r2, 1);
            else if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if ((i64)imm2 >= -128 && (i64)imm2 <= 127) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x83); emit8(modrm(3, 4, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x81); emit8(modrm(3, 4, r1.num & 7)); emit32((u32)imm2); }
            }
        } else if (strcmp(mnemonic, "or") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) encode_mr(0x09, &r1, &r2, 1);
            else if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if ((i64)imm2 >= -128 && (i64)imm2 <= 127) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x83); emit8(modrm(3, 1, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x81); emit8(modrm(3, 1, r1.num & 7)); emit32((u32)imm2); }
            }
        } else if (strcmp(mnemonic, "xor") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) encode_mr(0x31, &r1, &r2, 1);
            else if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if ((i64)imm2 >= -128 && (i64)imm2 <= 127) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x83); emit8(modrm(3, 6, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x81); emit8(modrm(3, 6, r1.num & 7)); emit32((u32)imm2); }
            }
        } else if (strcmp(mnemonic, "cmp") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) encode_mr(0x39, &r1, &r2, 1);
            else if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if ((i64)imm2 >= -128 && (i64)imm2 <= 127) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x83); emit8(modrm(3, 7, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0x81); emit8(modrm(3, 7, r1.num & 7)); emit32((u32)imm2); }
            }
        } else if (strcmp(mnemonic, "test") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) {
                if (r1.num >= 8 || r2.num >= 8) emit8(rex(1, (r2.num&8)?1:0, 0, (r1.num&8)?1:0));
                else emit8(0x48);
                emit8(0x85); emit8(modrm(3, r2.num & 7, r1.num & 7));
            }
        } else if (strcmp(mnemonic, "lea") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_MEM) {
                int has_rex = 0; u8 rv = 0;
                if (needs_rex(r1.num) || (sz == 8)) { rv = rex(1, 0, 0, (r1.num & 8) ? 1 : 0); has_rex = 1; }
                if (has_rex) emit8(rv); else emit8(0x48);
                emit8(0x8D);
                if (m2.has_base) {
                    int mod = 0;
                    if (m2.disp == 0 && m2.base != 5) mod = 0;
                    else if (m2.disp >= -128 && m2.disp <= 127) mod = 1;
                    else mod = 2;
                    if (m2.has_index) { emit8(modrm(mod, r1.num & 7, 4)); emit8(sib(m2.scale, m2.index & 7, m2.base & 7)); }
                    else emit8(modrm(mod, r1.num & 7, m2.base & 7));
                    if (mod == 1) emit8((u8)(m2.disp & 0xFF));
                    else if (mod == 2) emit32((u32)m2.disp);
                } else if (m2.sym[0]) {
                    emit8(modrm(0, r1.num & 7, 5));
                    add_reloc(sect->len, 4, m2.sym, current_sect, -4);
                    emit32(0);
                } else {
                    emit8(modrm(0, r1.num & 7, 4));
                    emit8(sib(0, 4, 5));
                    emit32((u32)m2.disp);
                }
            }
        } else if (strcmp(mnemonic, "imul") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_REG) {
                if (r1.num >= 8 || r2.num >= 8) emit8(rex(1, (r2.num&8)?1:0, 0, (r1.num&8)?1:0));
                else emit8(0x48);
                emit8(0x0F); emit8(0xAF); emit8(modrm(3, r2.num & 7, r1.num & 7));
            } else if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if ((i64)imm2 >= -128 && (i64)imm2 <= 127) { if (r1.num >= 8) emit8(0x4D); else emit8(0x4C); emit8(0x6B); emit8(modrm(3, r1.num & 7, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
                else { if (r1.num >= 8) emit8(0x4D); else emit8(0x4C); emit8(0x69); emit8(modrm(3, r1.num & 7, r1.num & 7)); emit32((u32)imm2); }
            }
        } else if (strcmp(mnemonic, "idiv") == 0) {
            if (op1_type == OPR_REG) {
                if (r1.num >= 8) emit8(0x49); else emit8(0x48);
                emit8(0xF7); emit8(modrm(3, 7, r1.num & 7));
            }
        } else if (strcmp(mnemonic, "div") == 0) {
            if (op1_type == OPR_REG) {
                if (r1.num >= 8) emit8(0x49); else emit8(0x48);
                emit8(0xF7); emit8(modrm(3, 6, r1.num & 7));
            }
        } else if (strcmp(mnemonic, "neg") == 0) {
            if (r1.num >= 8) emit8(0x49); else emit8(0x48);
            emit8(0xF7); emit8(modrm(3, 3, r1.num & 7));
        } else if (strcmp(mnemonic, "not") == 0) {
            if (r1.num >= 8) emit8(0x49); else emit8(0x48);
            emit8(0xF7); emit8(modrm(3, 2, r1.num & 7));
        } else if (strcmp(mnemonic, "inc") == 0) {
            if (r1.num >= 8) emit8(0x49); else emit8(0x48);
            emit8(0xFF); emit8(modrm(3, 0, r1.num & 7));
        } else if (strcmp(mnemonic, "dec") == 0) {
            if (r1.num >= 8) emit8(0x49); else emit8(0x48);
            emit8(0xFF); emit8(modrm(3, 1, r1.num & 7));
        } else if (strcmp(mnemonic, "shl") == 0 || strcmp(mnemonic, "sal") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if (imm2 == 1) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0xD1); emit8(modrm(3, 4, r1.num & 7)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0xC1); emit8(modrm(3, 4, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
            } else if (op1_type == OPR_REG && op2_type == OPR_REG && r2.num == 1) {
                if (r1.num >= 8) emit8(0x49); else emit8(0x48);
                emit8(0xD3); emit8(modrm(3, 4, r1.num & 7));
            }
        } else if (strcmp(mnemonic, "shr") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if (imm2 == 1) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0xD1); emit8(modrm(3, 5, r1.num & 7)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0xC1); emit8(modrm(3, 5, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
            } else if (op1_type == OPR_REG && op2_type == OPR_REG && r2.num == 1) {
                if (r1.num >= 8) emit8(0x49); else emit8(0x48);
                emit8(0xD3); emit8(modrm(3, 5, r1.num & 7));
            }
        } else if (strcmp(mnemonic, "sar") == 0) {
            if (op1_type == OPR_REG && op2_type == OPR_IMM) {
                if (imm2 == 1) { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0xD1); emit8(modrm(3, 7, r1.num & 7)); }
                else { if (r1.num >= 8) emit8(0x49); else emit8(0x48); emit8(0xC1); emit8(modrm(3, 7, r1.num & 7)); emit8((u8)(imm2 & 0xFF)); }
            } else if (op1_type == OPR_REG && op2_type == OPR_REG && r2.num == 1) {
                if (r1.num >= 8) emit8(0x49); else emit8(0x48);
                emit8(0xD3); emit8(modrm(3, 7, r1.num & 7));
            }
        } else if (strcmp(mnemonic, "cdq") == 0) {
            emit8(0x99);
        } else if (strcmp(mnemonic, "cqo") == 0) {
            emit8(0x48); emit8(0x99);
        } else if (strcmp(mnemonic, "cdqe") == 0) {
            emit8(0x48); emit8(0x98);
        } else if (strcmp(mnemonic, "nop") == 0) {
            emit8(0x90);
        } else if (strcmp(mnemonic, "ret") == 0) {
            emit8(0xC3);
        } else if (strcmp(mnemonic, "call") == 0) {
            if (op1_type == OPR_REG) {
                emit8(0xFF); emit8(modrm(3, 2, r1.num & 7));
            } else if (op1_type == OPR_IMM) {
                if (ops[0].is_sym && ops[0].sym[0]) {
                    emit8(0xE8);
                    add_reloc(sect->len, 4, ops[0].sym, current_sect, -4);
                    emit32(0);
                } else {
                    emit8(0xE8); emit32((u32)imm1);
                }
            }
        } else if (strcmp(mnemonic, "jmp") == 0) {
            if (op1_type == OPR_REG) {
                emit8(0xFF); emit8(modrm(3, 4, r1.num & 7));
            } else if (op1_type == OPR_IMM) {
                if (ops[0].is_sym && ops[0].sym[0]) {
                    emit8(0xE9);
                    add_reloc(sect->len, 4, ops[0].sym, current_sect, -4);
                    emit32(0);
                } else {
                    emit8(0xE9); emit32((u32)imm1);
                }
            }
        } else if (strcmp(mnemonic, "je") == 0 || strcmp(mnemonic, "jz") == 0) { emit8(0x0F); emit8(0x84); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jne") == 0 || strcmp(mnemonic, "jnz") == 0) { emit8(0x0F); emit8(0x85); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jg") == 0 || strcmp(mnemonic, "jnle") == 0) { emit8(0x0F); emit8(0x8F); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jge") == 0 || strcmp(mnemonic, "jnl") == 0) { emit8(0x0F); emit8(0x8D); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jl") == 0 || strcmp(mnemonic, "jnge") == 0) { emit8(0x0F); emit8(0x8C); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jle") == 0 || strcmp(mnemonic, "jng") == 0) { emit8(0x0F); emit8(0x8E); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jb") == 0 || strcmp(mnemonic, "jc") == 0 || strcmp(mnemonic, "jnae") == 0) { emit8(0x0F); emit8(0x82); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jbe") == 0 || strcmp(mnemonic, "jna") == 0) { emit8(0x0F); emit8(0x86); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "ja") == 0 || strcmp(mnemonic, "jnbe") == 0) { emit8(0x0F); emit8(0x87); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jae") == 0 || strcmp(mnemonic, "jnc") == 0 || strcmp(mnemonic, "jnb") == 0) { emit8(0x0F); emit8(0x83); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jo") == 0) { emit8(0x0F); emit8(0x80); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jno") == 0) { emit8(0x0F); emit8(0x81); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "js") == 0) { emit8(0x0F); emit8(0x88); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "jns") == 0) { emit8(0x0F); emit8(0x89); add_reloc(sect->len, 4, ops[0].sym, current_sect, -4); emit32(0);
        } else if (strcmp(mnemonic, "syscall") == 0) {
            emit8(0x0F); emit8(0x05);
        } else if (strcmp(mnemonic, "int") == 0) {
            emit8(0xCD); emit8((u8)(imm1 & 0xFF));
        } else if (strcmp(mnemonic, "leave") == 0) {
            emit8(0xC9);
        } else {
            printf("unknown instruction: %s\n", mnemonic);
            errors++;
        }
        
done:
        free(mnemonic);
        return 1;
    }
    return 0;
}

/* ---------- main assembly function ---------- */
static u8 *assemble(char *src, u64 *out_size, u64 base_vaddr) {
    org_vaddr = base_vaddr;
    sect_count = 0;
    sym_count = 0;
    reloc_count = 0;
    errors = 0;
    current_sect = add_sect(".text", SHF_ALLOC | SHF_EXECINSTR);
    code = 0; code_cap = 0; code_len = 0;
    
    Lexer l;
    lex_init(&l, src);
    lex_next(&l);
    
    while (l.tok.type != T_EOF) {
        if (l.tok.type == T_NEWLINE) { lex_next(&l); continue; }
        Section *sect = &sects[current_sect];
        code_len = 0; code_cap = 0; code = 0;
        assemble_line(&l, sect);
        
        if (code_len > 0) {
            sect_emit(sect, code, code_len);
        }
        free(code); code = 0; code_len = 0; code_cap = 0;
        
        while (l.tok.type == T_NEWLINE) lex_next(&l);
    }
    
    /* Resolve relocations */
    for (int ri = 0; ri < reloc_count; ri++) {
        Reloc *r = &relocs[ri];
        Symbol *s = find_sym(r->target);
        u64 target_addr = 0;
        if (s && s->defined) {
            target_addr = sects[s->section].addr + s->addr;
        }
        if (r->section < sect_count) {
            u8 *d = sects[r->section].data;
            u64 reloc_offset = r->offset;
            u64 final_val = target_addr - (r->addend ? r->addend : 0);
            if (r->size == 4) {
                i64 disp = (i64)(final_val - (sects[r->section].addr + r->offset + 4));
                *(u32*)(d + reloc_offset) = (u32)disp;
            } else if (r->size == 8) {
                *(u64*)(d + reloc_offset) = final_val;
            }
        }
    }
    
    /* Build ELF */
    int has_text = 0, has_data = 0, has_rodata = 0, has_bss = 0;
    for (int i = 0; i < sect_count; i++) {
        if (strcmp(sects[i].name, ".text") == 0) has_text = 1;
        else if (strcmp(sects[i].name, ".data") == 0) has_data = 1;
        else if (strcmp(sects[i].name, ".rodata") == 0) has_rodata = 1;
        else if (strcmp(sects[i].name, ".bss") == 0) has_bss = 1;
    }
    
    /* Calculate section layout */
    u64 cur_addr = base_vaddr;
    int text_idx = -1, rodata_idx = -1, data_idx = -1, bss_idx = -1;
    for (int i = 0; i < sect_count; i++) {
        Section *s = &sects[i];
        if (strcmp(s->name, ".text") == 0) {
            sects[i].addr = cur_addr; cur_addr += s->len; text_idx = i;
        }
    }
    for (int i = 0; i < sect_count; i++) {
        Section *s = &sects[i];
        if (strcmp(s->name, ".rodata") == 0) {
            u64 align = 16;
            if (cur_addr % align) cur_addr += align - (cur_addr % align);
            sects[i].addr = cur_addr; cur_addr += s->len; rodata_idx = i;
        }
    }
    for (int i = 0; i < sect_count; i++) {
        Section *s = &sects[i];
        if (strcmp(s->name, ".data") == 0) {
            u64 align = 16;
            if (cur_addr % align) cur_addr += align - (cur_addr % align);
            sects[i].addr = cur_addr; cur_addr += s->len; data_idx = i;
        }
    }
    for (int i = 0; i < sect_count; i++) {
        Section *s = &sects[i];
        if (strcmp(s->name, ".bss") == 0) {
            u64 align = 16;
            if (cur_addr % align) cur_addr += align - (cur_addr % align);
            sects[i].addr = cur_addr; cur_addr += s->len; bss_idx = i;
        }
    }
    
    /* Find entry point */
    u64 entry = 0;
    Symbol *start_sym = find_sym("_start");
    if (start_sym && start_sym->defined) entry = sects[start_sym->section].addr + start_sym->addr;
    else entry = base_vaddr;
    
    /* Build ELF header + program headers */
    int phdr_count = 0;
    if (has_text) phdr_count++;
    if (has_rodata) phdr_count++;
    if (has_data) phdr_count++;
    if (has_bss) phdr_count++;
    
    u64 filesz = cur_addr - base_vaddr;
    u64 memsz = cur_addr - base_vaddr;
    if (bss_idx >= 0) memsz += sects[bss_idx].len;
    
    int ehdr_size = sizeof(Elf64_Ehdr);
    int phdr_size = sizeof(Elf64_Phdr) * phdr_count;
    u64 phdr_off = ehdr_size;
    
    u8 *elf = malloc(ehdr_size + phdr_size + filesz);
    memset(elf, 0, ehdr_size + phdr_size + filesz);
    
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf;
    ehdr->e_ident[0] = 0x7F; ehdr->e_ident[1] = 'E'; ehdr->e_ident[2] = 'L'; ehdr->e_ident[3] = 'F';
    ehdr->e_ident[4] = 2; ehdr->e_ident[5] = 1; ehdr->e_ident[6] = 1;
    ehdr->e_type = 2;
    ehdr->e_machine = 0x3E;
    ehdr->e_version = 1;
    ehdr->e_entry = entry;
    ehdr->e_phoff = phdr_off;
    ehdr->e_ehsize = ehdr_size;
    ehdr->e_phentsize = sizeof(Elf64_Phdr);
    ehdr->e_phnum = phdr_count;
    
    int pi = 0;
    u64 data_off = ehdr_size + phdr_size;
    
    if (text_idx >= 0) {
        Elf64_Phdr *p = (Elf64_Phdr *)(elf + phdr_off + pi * sizeof(Elf64_Phdr));
        p->p_type = PT_LOAD; p->p_flags = PF_R | PF_X;
        p->p_offset = data_off;
        p->p_vaddr = sects[text_idx].addr;
        p->p_paddr = sects[text_idx].addr;
        p->p_filesz = sects[text_idx].len;
        p->p_memsz = sects[text_idx].len;
        p->p_align = 0x1000;
        memcpy(elf + data_off, sects[text_idx].data, sects[text_idx].len);
        data_off += sects[text_idx].len;
        pi++;
    }
    if (rodata_idx >= 0) {
        u64 align_pad = 0;
        if (data_off % 16) align_pad = 16 - (data_off % 16);
        data_off += align_pad;
        Elf64_Phdr *p = (Elf64_Phdr *)(elf + phdr_off + pi * sizeof(Elf64_Phdr));
        p->p_type = PT_LOAD; p->p_flags = PF_R;
        p->p_offset = data_off;
        p->p_vaddr = sects[rodata_idx].addr;
        p->p_paddr = sects[rodata_idx].addr;
        p->p_filesz = sects[rodata_idx].len;
        p->p_memsz = sects[rodata_idx].len;
        p->p_align = 0x1000;
        memcpy(elf + data_off, sects[rodata_idx].data, sects[rodata_idx].len);
        data_off += sects[rodata_idx].len;
        pi++;
    }
    if (data_idx >= 0) {
        u64 align_pad = 0;
        if (data_off % 16) align_pad = 16 - (data_off % 16);
        data_off += align_pad;
        Elf64_Phdr *p = (Elf64_Phdr *)(elf + phdr_off + pi * sizeof(Elf64_Phdr));
        p->p_type = PT_LOAD; p->p_flags = PF_R | PF_W;
        p->p_offset = data_off;
        p->p_vaddr = sects[data_idx].addr;
        p->p_paddr = sects[data_idx].addr;
        p->p_filesz = sects[data_idx].len;
        p->p_memsz = sects[data_idx].len;
        p->p_align = 0x1000;
        memcpy(elf + data_off, sects[data_idx].data, sects[data_idx].len);
        data_off += sects[data_idx].len;
        pi++;
    }
    if (bss_idx >= 0) {
        Elf64_Phdr *p = (Elf64_Phdr *)(elf + phdr_off + pi * sizeof(Elf64_Phdr));
        p->p_type = PT_LOAD; p->p_flags = PF_R | PF_W;
        p->p_offset = data_off;
        p->p_vaddr = sects[bss_idx].addr;
        p->p_paddr = sects[bss_idx].addr;
        p->p_filesz = 0;
        p->p_memsz = sects[bss_idx].len;
        p->p_align = 0x1000;
        pi++;
    }
    
    *out_size = ehdr_size + phdr_size + (cur_addr - base_vaddr);
    
    /* Clean up sections */
    for (int i = 0; i < sect_count; i++)
        free(sects[i].data);
    
    return elf;
}

/* ---------- main ---------- */
int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        printf("as - x86_64 assembler for Lumin OS\n");
        printf("usage: as [-o output] <source>\n");
        printf("assembles source and writes ELF64 executable to output\n");
        return 0;
    }
    
    printf("as: running test assembly...\n");
    
    const char *test_src = 
        "section .text\n"
        "global _start\n"
        "_start:\n"
        "  mov rax, 1\n"
        "  mov rdi, 1\n"
        "  mov rsi, msg\n"
        "  lea rsi, [rel msg]\n"
        "  mov rdx, 14\n"
        "  syscall\n"
        "  mov rdi, 0\n"
        "  mov rax, 60\n"
        "  syscall\n"
        "section .rodata\n"
        "msg: db \"Hello from as!\", 10\n";
    
    char *src_copy = strdup(test_src);
    u64 out_size;
    u8 *elf = assemble(src_copy, &out_size, 0x8000000000ULL);
    
    if (elf) {
        printf("as: assembled %lld bytes\n", (unsigned long long)out_size);
        printf("as: ELF at %p, size=%lld\n", (void*)elf, (unsigned long long)out_size);
        free(elf);
    } else {
        printf("as: assembly failed\n");
    }
    
    free(src_copy);
    return 0;
}
