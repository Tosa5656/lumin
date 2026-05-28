/* ecc.c - ECC C Compiler for Lumin OS
 * Generates x86-64 machine code directly, outputs ELF64 executables
 * Supports a practical subset of C89/C99:
 *   - int, char, long, void, unsigned, pointers, arrays
 *   - functions, local/global variables
 *   - if/else, while, do/while, for, return
 *   - all arithmetic/logical/bitwise operators
 *   - function calls, pointer deref, array indexing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

/* ─── ELF structures ─── */
#pragma pack(push,1)
typedef struct {
    u8  e_ident[16]; u16 e_type; u16 e_machine; u32 e_version; u64 e_entry;
    u64 e_phoff; u64 e_shoff; u32 e_flags; u16 e_ehsize; u16 e_phentsize;
    u16 e_phnum; u16 e_shentsize; u16 e_shnum; u16 e_shstrndx;
} Elf64_Ehdr;
typedef struct {
    u32 p_type; u32 p_flags; u64 p_offset; u64 p_vaddr; u64 p_paddr;
    u64 p_filesz; u64 p_memsz; u64 p_align;
} Elf64_Phdr;
#pragma pack(pop)

/* ─── Output section ─── */
#define MAX_SECTS 8
typedef struct {
    char name[32];
    u8 *data;
    u64 cap, len;
    u64 vaddr;
    u64 flags;
} Section;

static Section sects[MAX_SECTS];
static int sect_n;
static int cur_sec;

static int sec_add(const char *name, u64 flags) {
    Section *s = &sects[sect_n++];
    strncpy(s->name, name, 31); s->name[31]=0;
    s->data=0; s->cap=0; s->len=0; s->vaddr=0; s->flags=flags;
    return sect_n-1;
}

static void sec_emit(int si, void *d, u64 sz) {
    Section *s = &sects[si];
    if (!s->data) { s->cap=65536; s->data=malloc(65536); }
    while (s->len+sz > s->cap) { s->cap*=2; s->data=realloc(s->data,s->cap); }
    memcpy(s->data+s->len, d, sz); s->len+=sz;
}

static void sec_align(int si, u64 a) {
    Section *s = &sects[si];
    u64 r = s->len % a;
    if (r) { u64 p=a-r; memset(s->data+s->len,0,p); s->len+=p; }
}

static void sec_zero(int si, u64 sz) {
    static u8 z[64];
    for (u64 i=0; i<sz; i+=64) sec_emit(si, z, (sz-i)<64?(sz-i):64);
}

/* ─── Bytecode emission (to current section) ─── */
static u64 cur_off;

static void e8(u8 v) { sec_emit(cur_sec, &v, 1); cur_off++; }
static void e32(u32 v) { sec_emit(cur_sec, &v, 4); cur_off+=4; }
static void e64(u64 v) { sec_emit(cur_sec, &v, 8); cur_off+=8; }

/* ─── Lexer ─── */
enum {
    TK_EOF=0, TK_ID, TK_NUM, TK_STR,
    TK_LBRA, TK_RBRA, TK_LPAR, TK_RPAR, TK_LBRK, TK_RBRK,
    TK_SEMI, TK_COMMA, TK_DOT, TK_ARROW,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERC,
    TK_AMP, TK_PIPE, TK_HAT, TK_TILDE, TK_EXCL,
    TK_EQ, TK_NE, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_LAND, TK_LOR, TK_LSH, TK_RSH,
    TK_INC, TK_DEC,
    TK_ASN, TK_ADDASN, TK_SUBASN, TK_MULASN, TK_DIVASN, TK_MODASN,
    TK_ANDASN, TK_ORASN, TK_XORASN, TK_LSASN, TK_RSASN,
    TK_QUE, TK_COL,
    TK_SIZEOF,
    /* keywords */
    K_CHAR=200, K_INT, K_LONG, K_SHORT, K_UNSIGN, K_SIGNED, K_VOID,
    K_STRUCT, K_UNION, K_ENUM, K_CONST, K_VOL, K_EXTERN, K_STATIC, K_TYPEDEF,
    K_IF, K_ELSE, K_WHILE, K_DO, K_FOR, K_SWITCH, K_CASE, K_DEFAULT,
    K_BREAK, K_CONT, K_RETURN, K_GOTO,
};

typedef struct { int t; char *s; u64 v; int ln; } Token;
typedef struct { char *p; int pos; int ln; Token tk; } Lex;

static Lex lex;

static int isidc(int c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||(c>='0'&&c<='9'); }

static const struct { const char *w; int t; } kws[] = {
    {"char",K_CHAR},{"int",K_INT},{"long",K_LONG},{"short",K_SHORT},
    {"unsigned",K_UNSIGN},{"signed",K_SIGNED},{"void",K_VOID},
    {"struct",K_STRUCT},{"union",K_UNION},{"enum",K_ENUM},
    {"const",K_CONST},{"volatile",K_VOL},
    {"extern",K_EXTERN},{"static",K_STATIC},{"typedef",K_TYPEDEF},
    {"if",K_IF},{"else",K_ELSE},{"while",K_WHILE},{"do",K_DO},
    {"for",K_FOR},{"switch",K_SWITCH},{"case",K_CASE},{"default",K_DEFAULT},
    {"break",K_BREAK},{"continue",K_CONT},{"return",K_RETURN},{"goto",K_GOTO},
    {"sizeof",TK_SIZEOF},{0,0}
};

static int lex_next(void) {
    char *s=lex.p; int p=lex.pos;
    while (s[p]==' '||s[p]=='\t'||s[p]=='\r') p++;
    if (s[p]=='/'&&s[p+1]=='/') { while(s[p]&&s[p]!='\n') p++; }
    if (s[p]=='/'&&s[p+1]=='*') { p+=2; while(s[p]&&!(s[p]=='*'&&s[p+1]=='/')) { if(s[p]=='\n') lex.ln++; p++; } if(s[p]) p+=2; }
    if (s[p]=='\n') { p++; lex.ln++; lex.tk.t=TK_SEMI; lex.pos=p; return TK_SEMI; }
    if (!s[p]) { lex.tk.t=TK_EOF; return TK_EOF; }
    if (s[p]=='"') {
        p++; int st=p; while(s[p]&&s[p]!='"') { if(s[p]=='\\') p++; p++; }
        int l=p-st; char *t=malloc(l+1); int wi=0;
        for(int i=st;i<p;i++) {
            if(s[i]=='\\') { i++;
                if(s[i]=='n') t[wi++]=10; else if(s[i]=='t') t[wi++]=9;
                else if(s[i]=='0') t[wi++]=0; else if(s[i]=='\\') t[wi++]='\\';
                else if(s[i]=='"') t[wi++]='"'; else t[wi++]=s[i];
            } else t[wi++]=s[i];
        }
        t[wi]=0; if(s[p]=='"') p++;
        lex.tk.t=TK_STR; lex.tk.s=t; lex.pos=p; return TK_STR;
    }
    if (s[p]=='\'') {
        p++; int c;
        if(s[p]=='\\') { p++; if(s[p]=='n') c=10; else if(s[p]=='t') c=9; else if(s[p]=='0') c=0; else if(s[p]=='\\') c='\\'; else if(s[p]=='\'') c='\''; else c=s[p]; p++; }
        else { c=s[p]; p++; }
        if(s[p]=='\'') p++;
        lex.tk.t=TK_NUM; lex.tk.v=c; lex.pos=p; return TK_NUM;
    }
    if (s[p]>='0'&&s[p]<='9') {
        int b=10;
        if(s[p]=='0'&&(s[p+1]=='x'||s[p+1]=='X')) { b=16; p+=2; }
        else if(s[p]=='0'&&(s[p+1]=='b'||s[p+1]=='B')) { b=2; p+=2; }
        else if(s[p]=='0') { b=8; p++; }
        u64 v=0;
        while(1) {
            int d=-1;
            if(s[p]>='0'&&s[p]<='9') d=s[p]-'0';
            else if(s[p]>='a'&&s[p]<='f') d=s[p]-'a'+10;
            else if(s[p]>='A'&&s[p]<='F') d=s[p]-'A'+10;
            else break;
            v=v*b+d; p++;
        }
        lex.tk.t=TK_NUM; lex.tk.v=v; lex.pos=p; return TK_NUM;
    }
    if (isidc(s[p]) && !(s[p]>='0'&&s[p]<='9')) {
        int st=p; while(isidc(s[p])) p++;
        int l=p-st; char *t=malloc(l+1); memcpy(t,s+st,l); t[l]=0;
        for(int i=0;kws[i].w;i++) if(strcmp(t,kws[i].w)==0) { free(t); lex.tk.t=kws[i].t; lex.pos=p; return lex.tk.t; }
        lex.tk.t=TK_ID; lex.tk.s=t; lex.pos=p; return TK_ID;
    }
    /* multi-char operators */
    if(s[p]=='+'&&s[p+1]=='+') { p+=2; lex.tk.t=TK_INC; lex.pos=p; return TK_INC; }
    if(s[p]=='-'&&s[p+1]=='-') { p+=2; lex.tk.t=TK_DEC; lex.pos=p; return TK_DEC; }
    if(s[p]=='-'&&s[p+1]=='>') { p+=2; lex.tk.t=TK_ARROW; lex.pos=p; return TK_ARROW; }
    if(s[p]=='='&&s[p+1]=='=') { p+=2; lex.tk.t=TK_EQ; lex.pos=p; return TK_EQ; }
    if(s[p]=='!'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_NE; lex.pos=p; return TK_NE; }
    if(s[p]=='<'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_LE; lex.pos=p; return TK_LE; }
    if(s[p]=='>'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_GE; lex.pos=p; return TK_GE; }
    if(s[p]=='<'&&s[p+1]=='<') { p+=2; if(s[p]=='='){p++;lex.tk.t=TK_LSASN;lex.pos=p;return TK_LSASN;} lex.tk.t=TK_LSH; lex.pos=p; return TK_LSH; }
    if(s[p]=='>'&&s[p+1]=='>') { p+=2; if(s[p]=='='){p++;lex.tk.t=TK_RSASN;lex.pos=p;return TK_RSASN;} lex.tk.t=TK_RSH; lex.pos=p; return TK_RSH; }
    if(s[p]=='&'&&s[p+1]=='&') { p+=2; lex.tk.t=TK_LAND; lex.pos=p; return TK_LAND; }
    if(s[p]=='|'&&s[p+1]=='|') { p+=2; lex.tk.t=TK_LOR; lex.pos=p; return TK_LOR; }
    if(s[p]=='+'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_ADDASN; lex.pos=p; return TK_ADDASN; }
    if(s[p]=='-'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_SUBASN; lex.pos=p; return TK_SUBASN; }
    if(s[p]=='*'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_MULASN; lex.pos=p; return TK_MULASN; }
    if(s[p]=='/'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_DIVASN; lex.pos=p; return TK_DIVASN; }
    if(s[p]=='%'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_MODASN; lex.pos=p; return TK_MODASN; }
    if(s[p]=='&'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_ANDASN; lex.pos=p; return TK_ANDASN; }
    if(s[p]=='|'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_ORASN; lex.pos=p; return TK_ORASN; }
    if(s[p]=='^'&&s[p+1]=='=') { p+=2; lex.tk.t=TK_XORASN; lex.pos=p; return TK_XORASN; }
    /* single char */
    const char *sc = "{}()[];,.:?~";
    for(const char *cp=sc;*cp;cp++) { if(s[p]==*cp) { p++; lex.tk.t=TK_LBRA+(cp-sc); lex.pos=p; return lex.tk.t; } }
    if(s[p]=='=') { p++; lex.tk.t=TK_ASN; lex.pos=p; return TK_ASN; }
    if(s[p]=='+') { p++; lex.tk.t=TK_PLUS; lex.pos=p; return TK_PLUS; }
    if(s[p]=='-') { p++; lex.tk.t=TK_MINUS; lex.pos=p; return TK_MINUS; }
    if(s[p]=='*') { p++; lex.tk.t=TK_STAR; lex.pos=p; return TK_STAR; }
    if(s[p]=='/') { p++; lex.tk.t=TK_SLASH; lex.pos=p; return TK_SLASH; }
    if(s[p]=='%') { p++; lex.tk.t=TK_PERC; lex.pos=p; return TK_PERC; }
    if(s[p]=='&') { p++; lex.tk.t=TK_AMP; lex.pos=p; return TK_AMP; }
    if(s[p]=='|') { p++; lex.tk.t=TK_PIPE; lex.pos=p; return TK_PIPE; }
    if(s[p]=='^') { p++; lex.tk.t=TK_HAT; lex.pos=p; return TK_HAT; }
    if(s[p]=='!') { p++; lex.tk.t=TK_EXCL; lex.pos=p; return TK_EXCL; }
    if(s[p]=='<') { p++; lex.tk.t=TK_LT; lex.pos=p; return TK_LT; }
    if(s[p]=='>') { p++; lex.tk.t=TK_GT; lex.pos=p; return TK_GT; }
    if(s[p]=='?') { p++; lex.tk.t=TK_QUE; lex.pos=p; return TK_QUE; }
    lex.tk.t=TK_EOF; return TK_EOF;
}

/* ─── Type system ─── */
enum { TY_VOID, TY_CHAR, TY_INT, TY_LONG, TY_PTR, TY_ARR, TY_FUNC };

typedef struct Ty {
    int k; int sz; int uns;
    struct Ty *to;  /* pointer to / array element */
    int alen;       /* array length */  
    struct Ty *ret; /* function return */
    int pcn;        /* param count */
} Ty;

static Ty ty_void = {TY_VOID,0,0};
static Ty ty_char = {TY_CHAR,1,0};
static Ty ty_uchar = {TY_CHAR,1,1};
static Ty ty_int = {TY_INT,4,0};
static Ty ty_uint = {TY_INT,4,1};
static Ty ty_long = {TY_LONG,8,0};
static Ty ty_ulong = {TY_LONG,8,1};

static Ty *ty_ptr(Ty *b) { Ty *p=calloc(1,sizeof(Ty)); p->k=TY_PTR; p->sz=8; p->to=b; return p; }
static Ty *ty_arr(Ty *b, int n) { Ty *p=calloc(1,sizeof(Ty)); p->k=TY_ARR; p->sz=b->sz*n; p->to=b; p->alen=n; return p; }
static Ty *ty_func(Ty *r) { Ty *p=calloc(1,sizeof(Ty)); p->k=TY_FUNC; p->ret=r; return p; }

/* ─── Symbol table ─── */
#define SYM_MAX 4096
typedef struct Sym {
    char n[64];
    Ty *t;
    int glob;   /* 1=global, 0=local */
    int func;   /* 1=function */
    int def;    /* defined */
    u64 addr;   /* global vaddr */
    int soff;   /* stack offset (negative from rbp) */
} Sym;
static Sym syms[SYM_MAX];
static int sym_n;

static Sym *sym_find(const char *name) {
    for(int i=sym_n-1;i>=0;i--) if(strcmp(syms[i].n,name)==0) return &syms[i];
    return 0;
}

static Sym *sym_add(const char *name) {
    Sym *s = sym_find(name);
    if(s) return s;
    s = &syms[sym_n++];
    strncpy(s->n,name,63); s->n[63]=0;
    s->t=0; s->glob=0; s->func=0; s->def=0; s->addr=0; s->soff=0;
    return s;
}

/* ─── Labels and fixups ─── */
#define LB_MAX 4096
#define FX_MAX 8192

typedef struct { char name[64]; u64 offset; int sec; } Label;
static Label labels[LB_MAX];
static int label_n;

typedef struct { u64 off; int sz; int kind; char name[64]; int sec; } Fixup;
static Fixup fixups[FX_MAX];
static int fixup_n;

static int lbl_cnt;
static char lbl_buf[64];
static char *lbl_new(void) { snprintf(lbl_buf,64,".L%d",lbl_cnt++); return lbl_buf; }

static void label_def(const char *name) {
    Label *l = &labels[label_n++];
    strncpy(l->name, name, 63); l->name[63] = 0;
    l->offset = cur_off;
    l->sec = cur_sec;
}

static void fixup_add(int sz, int kind, const char *name) {
    Fixup *f = &fixups[fixup_n++];
    f->off = cur_off; f->sz = sz; f->kind = kind;
    strncpy(f->name, name, 63); f->name[63] = 0;
    f->sec = cur_sec;
}

/* ─── Codegen state ─── */
static int sec_text, sec_data, sec_rodata;
static int cur_func;
static u64 base_vaddr = 0x8000000000ULL;
static int stack_size;
static int errs;

static void expect(int t) {
    if(lex.tk.t!=t) { printf("line %d: expected %d got %d\n",lex.ln,t,lex.tk.t); errs++; }
    else lex_next();
}

/* ─── X86-64 codegen helpers ─── */
/* Using rax for expression results, r10/r11 for temps */

static void emit_mov_rr(int d, int s) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x89); e8(0xC0|((s&7)<<3)|(d&7));
}

static void emit_mov_ri(int d, u64 v) {
    u8 r=0x48|((d>=8)?1:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0xB8|(d&7)); e64(v);
}

static void emit_mov_ri32(int d, u32 v) {
    if(d>=8) e8(0x41);
    e8(0xB8|(d&7)); e32(v);
}

static void emit_load_mr(int d, int s) {
    /* mov d, [s] */
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x8B); e8(0x00|((d&7)<<3)|(s&7));
}

static void emit_store_rm(int d, int s) {
    /* mov [s], d */
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x89); e8(0x00|((d&7)<<3)|(s&7));
}

static void emit_load_mr_off(int d, int s, i64 off) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x8B);
    if(off==0) e8(0x00|((d&7)<<3)|(s&7));
    else if(off>=-128&&off<=127) { e8(0x40|((d&7)<<3)|(s&7)); e8((u8)(off&0xFF)); }
    else { e8(0x80|((d&7)<<3)|(s&7)); e32((u32)off); }
}

static void emit_store_rm_off(int d, int s, i64 off) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x89);
    if(off==0) e8(0x00|((d&7)<<3)|(s&7));
    else if(off>=-128&&off<=127) { e8(0x40|((d&7)<<3)|(s&7)); e8((u8)(off&0xFF)); }
    else { e8(0x80|((d&7)<<3)|(s&7)); e32((u32)off); }
}

static void emit_lea_rbp(int d, i64 off) {
    /* lea d, [rbp+off] */
    u8 r=0x48|((d>=8)?1:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x8D);
    if(off==0) e8(0x40|((d&7)<<3)|5);
    else if(off>=-128&&off<=127) { e8(0x40|((d&7)<<3)|5); e8((u8)(off&0xFF)); }
    else { e8(0x80|((d&7)<<3)|5); e32((u32)off); }
}

static void emit_push(int r) {
    if(r>=8) e8(0x41); e8(0x50|(r&7));
}

static void emit_pop(int r) {
    if(r>=8) e8(0x41); e8(0x58|(r&7));
}

static void emit_add_ri(int r, i64 v) {
    if(v==0) return;
    u8 rex=0x48|((r>=8)?1:0);
    if(rex!=0x48) e8(rex); else e8(0x48);
    if(v>=-128&&v<=127) { e8(0x83); e8(0xC0|(r&7)); e8((u8)(v&0xFF)); }
    else { e8(0x81); e8(0xC0|(r&7)); e32((u32)v); }
}

static void emit_sub_ri(int r, i64 v) {
    if(v==0) return;
    u8 rex=0x48|((r>=8)?1:0);
    if(rex!=0x48) e8(rex); else e8(0x48);
    if(v>=-128&&v<=127) { e8(0x83); e8(0xE8|(r&7)); e8((u8)(v&0xFF)); }
    else { e8(0x81); e8(0xE8|(r&7)); e32((u32)v); }
}

static void emit_add_rr(int d, int s) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x01); e8(0xC0|((s&7)<<3)|(d&7));
}

static void emit_sub_rr(int d, int s) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x29); e8(0xC0|((s&7)<<3)|(d&7));
}

static void emit_xor_rr(int d, int s) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x31); e8(0xC0|((s&7)<<3)|(d&7));
}

static void emit_imul_rr(int d, int s) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x0F); e8(0xAF); e8(0xC0|((s&7)<<3)|(d&7));
}

static void emit_cqo(void) { e8(0x48); e8(0x99); }

static void emit_idiv(int r) {
    u8 rex=0x48|((r>=8)?1:0);
    if(rex!=0x48) e8(rex); else e8(0x48);
    e8(0xF7); e8(0xF8|(r&7));
}

static void emit_and_rr(int d, int s) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x21); e8(0xC0|((s&7)<<3)|(d&7));
}

static void emit_or_rr(int d, int s) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x09); e8(0xC0|((s&7)<<3)|(d&7));
}

static void emit_cmp_rr(int a, int b) {
    u8 r=0x48|((a>=8)?1:0)|((b>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x39); e8(0xC0|((b&7)<<3)|(a&7));
}

static void emit_cmp_ri(int r, i64 v) {
    u8 rex=0x48|((r>=8)?1:0);
    if(rex!=0x48) e8(rex); else e8(0x48);
    if(v>=-128&&v<=127) { e8(0x83); e8(0xF8|(r&7)); e8((u8)(v&0xFF)); }
    else { e8(0x81); e8(0xF8|(r&7)); e32((u32)v); }
}

static void emit_setcc(int cc, int r) {
    e8(0x0F); e8(0x90|cc); e8(0xC0|(r&7));
}

static void emit_movzb(int d, int s) {
    u8 r=0x48|((d>=8)?1:0)|((s>=8)?4:0);
    if(r!=0x48) e8(r); else e8(0x48);
    e8(0x0F); e8(0xB6); e8(0xC0|((s&7)<<3)|(d&7));
}

static void emit_neg(int r) {
    u8 rex=0x48|((r>=8)?1:0);
    if(rex!=0x48) e8(rex); else e8(0x48);
    e8(0xF7); e8(0xD8|(r&7));
}

static void emit_not(int r) {
    u8 rex=0x48|((r>=8)?1:0);
    if(rex!=0x48) e8(rex); else e8(0x48);
    e8(0xF7); e8(0xD0|(r&7));
}

static void emit_jmp_rel32(const char *tgt) {
    e8(0xE9); fixup_add(4,0,tgt); e32(0);
}

static void emit_jcc_rel32(int cc, const char *tgt) {
    e8(0x0F); e8(0x80|cc); fixup_add(4,0,tgt); e32(0);
}

static void emit_call_rel32(const char *tgt) {
    e8(0xE8); fixup_add(4,0,tgt); e32(0);
}

static void emit_call_reg(int r) {
    if(r>=8) e8(0x41);
    e8(0xFF); e8(0xD0|(r&7));
}

static void emit_ret(void) { e8(0xC3); }
static void emit_leave(void) { e8(0xC9); }

/* ─── Value structure for lvalue/rvalue tracking ─── */
typedef struct {
    Ty *type;
    int is_lvalue;   /* 1 = has an address we know */
    int lv_reg;      /* register holding address, or -1 */
    int lv_off;      /* rbp-relative offset for locals */
    Sym *lv_sym;     /* symbol for globals */
} Value;

/* ─── Expression parsing + codegen ─── */

static Ty *parse_type(void);
static void parse_statement(Ty *ret_type);
static Value parse_expr(void);
static Value parse_assign_expr(void);

/* Forward declarations for expression parser */
static Value parse_logical_or(void);
static Value parse_logical_and(void);
static Value parse_bit_or(void);
static Value parse_bit_xor(void);
static Value parse_bit_and(void);
static Value parse_eq(void);
static Value parse_rel(void);
static Value parse_shift(void);
static Value parse_add_expr(void);
static Value parse_mul(void);
static Value parse_cast(void);
static Value parse_unary(void);
static Value parse_postfix(void);
static Value parse_primary(void);

/* Load an lvalue into an rvalue: if val is an lvalue, emit load into rax */
static Value val_rvalue(Value v) {
    if (v.is_lvalue && v.type && v.type->k != TY_FUNC) {
        if (v.lv_reg >= 0) {
            emit_load_mr(0, v.lv_reg);
        } else if (v.lv_sym) {
            /* global - emit absolute load */
            emit_mov_ri(0, 0);
            fixup_add(8, 1, v.lv_sym->n);
            emit_load_mr(0, 0);
        } else {
            emit_load_mr_off(0, 5, v.lv_off);
        }
        v.is_lvalue = 0;
    }
    return v;
}

static Value parse_add_expr(void) {
    Value v = parse_mul();
    while (lex.tk.t == TK_PLUS || lex.tk.t == TK_MINUS) {
        int op = lex.tk.t; lex_next();
        v = val_rvalue(v);
        emit_push(0);
        Value v2 = parse_mul(); v2 = val_rvalue(v2);
        emit_pop(10);
        if (op == TK_PLUS) emit_add_rr(0, 10);
        else { emit_sub_rr(10, 0); emit_mov_rr(0, 10); }
    }
    return v;
}

static Value parse_shift(void) {
    Value v = parse_add_expr();
    while (lex.tk.t == TK_LSH || lex.tk.t == TK_RSH) {
        int op = lex.tk.t; lex_next();
        v = val_rvalue(v);
        emit_push(0);
        Value v2 = parse_add_expr(); v2 = val_rvalue(v2);
        emit_pop(10); emit_mov_rr(1, 10);
        if (op == TK_LSH) { e8(0x48); e8(0xD3); e8(0xE0); }
        else { e8(0x48); e8(0xD3); e8(0xE8); }
    }
    return v;
}

static Value parse_rel(void) {
    Value v = parse_shift();
    while (lex.tk.t == TK_LT || lex.tk.t == TK_GT || lex.tk.t == TK_LE || lex.tk.t == TK_GE) {
        int op = lex.tk.t; lex_next();
        v = val_rvalue(v);
        emit_push(0);
        Value v2 = parse_shift(); v2 = val_rvalue(v2);
        emit_pop(10); emit_cmp_rr(10, 0);
        int cc = (op==TK_LT)?12:(op==TK_GT)?15:(op==TK_LE)?14:13;
        emit_setcc(cc, 0); emit_movzb(0, 0);
        v.type = &ty_int; v.is_lvalue = 0;
    }
    return v;
}

static Value parse_eq(void) {
    Value v = parse_rel();
    while (lex.tk.t == TK_EQ || lex.tk.t == TK_NE) {
        int op = lex.tk.t; lex_next();
        v = val_rvalue(v);
        emit_push(0);
        Value v2 = parse_rel(); v2 = val_rvalue(v2);
        emit_pop(10); emit_cmp_rr(10, 0);
        int cc = (op == TK_EQ) ? 4 : 5;
        emit_setcc(cc, 0); emit_movzb(0, 0);
        v.type = &ty_int; v.is_lvalue = 0;
    }
    return v;
}

static Value parse_bit_and(void) {
    Value v = parse_eq();
    while (lex.tk.t == TK_AMP) {
        lex_next(); v = val_rvalue(v); emit_push(0);
        Value v2 = parse_eq(); v2 = val_rvalue(v2);
        emit_pop(10); emit_and_rr(0, 10);
    }
    return v;
}

static Value parse_bit_xor(void) {
    Value v = parse_bit_and();
    while (lex.tk.t == TK_HAT) {
        lex_next(); v = val_rvalue(v); emit_push(0);
        Value v2 = parse_bit_and(); v2 = val_rvalue(v2);
        emit_pop(10); emit_xor_rr(0, 10);
    }
    return v;
}

static Value parse_bit_or(void) {
    Value v = parse_bit_xor();
    while (lex.tk.t == TK_PIPE) {
        lex_next(); v = val_rvalue(v); emit_push(0);
        Value v2 = parse_bit_xor(); v2 = val_rvalue(v2);
        emit_pop(10); emit_or_rr(0, 10);
    }
    return v;
}

static Value parse_logical_and(void) {
    Value v = parse_bit_or();
    while (lex.tk.t == TK_LAND) {
        lex_next();
        v = val_rvalue(v);
        char *l_false = lbl_new();
        emit_cmp_ri(0, 0);
        emit_jcc_rel32(4, l_false);
        Value v2 = parse_bit_or(); v2 = val_rvalue(v2);
        emit_cmp_ri(0, 0);
        emit_setcc(5, 0); emit_movzb(0, 0);
        v.type = &ty_int; v.is_lvalue = 0;
        label_def(l_false);
    }
    return v;
}

static Value parse_logical_or(void) {
    Value v = parse_logical_and();
    while (lex.tk.t == TK_LOR) {
        lex_next();
        v = val_rvalue(v);
        char *l_end = lbl_new();
        emit_cmp_ri(0, 0);
        emit_jcc_rel32(5, l_end);
        Value v2 = parse_logical_and(); v2 = val_rvalue(v2);
        emit_cmp_ri(0, 0);
        emit_setcc(5, 0); emit_movzb(0, 0);
        v.type = &ty_int; v.is_lvalue = 0;
        label_def(l_end);
    }
    return v;
}

static Value parse_cond_expr(void) {
    Value v = parse_logical_or();
    if (lex.tk.t == TK_QUE) {
        lex_next();
        v = val_rvalue(v);
        char *l_false = lbl_new();
        char *l_end = lbl_new();
        emit_cmp_ri(0, 0);
        emit_jcc_rel32(4, l_false);
        Value v_true = parse_cond_expr();
        emit_jmp_rel32(l_end);
        label_def(l_false);
        expect(TK_COL);
        Value v_false = parse_cond_expr();
        label_def(l_end);
        v = v_true;
    }
    return v;
}

static Value parse_mul(void) {
    Value v = parse_cast();
    while (lex.tk.t == TK_STAR || lex.tk.t == TK_SLASH || lex.tk.t == TK_PERC) {
        int op = lex.tk.t; lex_next();
        v = val_rvalue(v);
        emit_push(0);
        Value v2 = parse_cast(); v2 = val_rvalue(v2);
        emit_pop(10);
        if (op == TK_STAR) emit_imul_rr(0, 10);
        else if (op == TK_SLASH) { emit_mov_rr(11, 0); emit_mov_rr(0, 10); emit_cqo(); emit_idiv(11); }
        else { emit_mov_rr(11, 0); emit_mov_rr(0, 10); emit_cqo(); emit_idiv(11); emit_mov_rr(0, 11); }
    }
    return v;
}

static Value parse_cast(void) {
    if (lex.tk.t == TK_LPAR) {
        int saved_pos = lex.pos;
        int saved_line = lex.ln;
        lex_next();
        if (peek_type()) {
            Ty *t = parse_type();
            expect(TK_RPAR);
            Value val = parse_cast();
            val.type = t;
            return val;
        }
        lex.pos = saved_pos;
        lex.ln = saved_line;
    }
    return parse_unary();
}

static int peek_type(void) {
    int t = lex.tk.t;
    return t == K_CHAR || t == K_INT || t == K_LONG || t == K_SHORT ||
           t == K_UNSIGN || t == K_SIGNED || t == K_VOID ||
           t == K_STRUCT || t == K_CONST || t == K_VOL;
}

static Value parse_unary(void) {
    int op = lex.tk.t;
    if (op == TK_PLUS) { lex_next(); return parse_cast(); }
    if (op == TK_MINUS) {
        lex_next(); Value v = parse_cast(); v = val_rvalue(v); emit_neg(0); return v;
    }
    if (op == TK_EXCL) {
        lex_next(); Value v = parse_cast(); v = val_rvalue(v);
        emit_cmp_ri(0,0); emit_setcc(5,0); emit_movzb(0,0); return v;
    }
    if (op == TK_TILDE) {
        lex_next(); Value v = parse_cast(); v = val_rvalue(v); emit_not(0); return v;
    }
    if (op == TK_AMP) {
        lex_next();
        Value v = parse_cast();
        if (!v.is_lvalue) { printf("line %d: & requires lvalue\n", lex.ln); errs++; }
        v.type = ptr_to(v.type);
        v.is_lvalue = 0;
        return v;
    }
    if (op == TK_STAR) {
        lex_next();
        Value v = parse_cast();
        emit_load_mr(0, 0);
        v.type = v.type ? v.type->to : 0;
        v.is_lvalue = 1;
        v.lv_reg = 0;
        v.lv_off = 0;
        return v;
    }
    if (op == TK_INC || op == TK_DEC) {
        lex_next();
        Value v = parse_unary();
        if (!v.is_lvalue) { printf("line %d: ++/-- requires lvalue\n", lex.ln); errs++; }
        if (v.lv_reg >= 0) {
            emit_load_mr(0, v.lv_reg);
        } else if (v.lv_sym) {
            emit_mov_ri(0, 0); fixup_add(8, 1, v.lv_sym->n);
            emit_load_mr(0, 0);
        } else {
            emit_load_mr_off(0, 5, v.lv_off);
        }
        if (op == TK_DEC) emit_sub_ri(0, 1); else emit_add_ri(0, 1);
        if (v.lv_reg >= 0) emit_store_rm(0, v.lv_reg);
        else if (v.lv_sym) { emit_push(0); emit_mov_ri(0, 0); fixup_add(8, 1, v.lv_sym->n); emit_pop(10); emit_store_rm(10, 0); }
        else emit_store_rm_off(0, 5, v.lv_off);
        return v;
    }
    if (op == TK_SIZEOF) {
        lex_next();
        if (lex.tk.t == TK_LPAR) {
            lex_next();
            Ty *t = parse_type();
            expect(TK_RPAR);
            emit_mov_ri(0, t->sz);
            Value v = {0}; v.type = &ty_ulong; return v;
        }
        Value v = parse_unary();
        emit_mov_ri(0, v.type ? v.type->sz : 0);
        v.type = &ty_ulong; v.is_lvalue = 0;
        return v;
    }
    return parse_postfix();
}

static Value parse_postfix(void) {
    Value v = parse_primary();
    for (;;) {
        if (lex.tk.t == TK_LBRK) {
            lex_next();
            Value base = val_rvalue(v);
            emit_push(0);
            Value idx = parse_expr();
            idx = val_rvalue(idx);
            expect(TK_RBRK);
            emit_pop(10);
            if (v.type && v.type->to && v.type->to->sz > 1) {
                emit_mov_ri(11, v.type->to->sz);
                emit_imul_rr(0, 11);
            }
            emit_add_rr(10, 0);
            v.type = v.type ? v.type->to : 0;
            v.is_lvalue = 1;
            v.lv_reg = 10;
            v.lv_off = 0;
        } else if (lex.tk.t == TK_LPAR) {
            lex_next();
            v = val_rvalue(v);
            emit_mov_rr(10, 0);
            int arg_regs[] = {7, 6, 2, 1, 8, 9};
            int arg_count = 0;
            while (lex.tk.t != TK_RPAR) {
                Value a = parse_assign_expr();
                a = val_rvalue(a);
                if (arg_count < 6) {
                    int r = arg_regs[arg_count];
                    emit_mov_rr(r, 0);
                } else {
                    emit_push(0);
                }
                arg_count++;
                if (lex.tk.t == TK_COMMA) lex_next();
            }
            expect(TK_RPAR);
            emit_call_reg(10);
            v.type = v.type ? v.type->ret : 0;
            v.is_lvalue = 0;
        } else if (lex.tk.t == TK_DOT) {
            lex_next();
            if (lex.tk.t == TK_ID) lex_next();
        } else if (lex.tk.t == TK_ARROW) {
            lex_next();
            v = val_rvalue(v);
            emit_load_mr(0, 0);
            if (lex.tk.t == TK_ID) lex_next();
        } else if (lex.tk.t == TK_INC) {
            lex_next();
            val_rvalue(v);
            emit_push(0);
            emit_mov_ri(11, 1);
            emit_add_rr(0, 11);
            emit_pop(10);
            emit_store_rm(0, 10);
        } else if (lex.tk.t == TK_DEC) {
            lex_next();
            val_rvalue(v);
            emit_push(0);
            emit_mov_ri(11, 1);
            emit_sub_rr(0, 11);
            emit_pop(10);
            emit_store_rm(0, 10);
        } else break;
    }
    return v;
}

static Value parse_primary(void) {
    Value v = {0};
    if (lex.tk.t == TK_NUM) {
        emit_mov_ri(0, lex.tk.v);
        v.type = &ty_int;
        lex_next();
    } else if (lex.tk.t == TK_STR) {
        char *str = lex.tk.s;
        int len = strlen(str) + 1;
        static int str_cnt;
        char lbl[64]; snprintf(lbl, 64, ".str%d", str_cnt++);
        int saved_sec = cur_sec; cur_sec = sec_rodata;
        label_def(lbl);
        sect_emit(sec_rodata, str, len);
        cur_sec = saved_sec;
        emit_mov_ri(0, 0);
        fixup_add(8, 1, lbl);
        v.type = ptr_to(&ty_char);
        free(str);
        lex_next();
    } else if (lex.tk.t == TK_LPAR) {
        lex_next();
        v = parse_expr();
        expect(TK_RPAR);
    } else if (lex.tk.t == TK_ID) {
        char *name = lex.tk.s; lex_next();
        Sym *s = sym_find(name);
        if (!s) { printf("line %d: undefined '%s'\n", lex.ln, name); errs++; free(name); v.type = &ty_int; return v; }
        free(name);
        v.type = s->t;
        v.is_lvalue = 1;
        if (s->glob) {
            v.lv_sym = s;
            v.lv_reg = -1;
        } else {
            v.lv_off = -s->soff;
            v.lv_reg = -1;
        }
    }
    return v;
}

/* parse_assign_expr: handles =, +=, -= etc. */
static Value parse_assign_expr(void) {
    Value v = parse_cond_expr();
    if (lex.tk.t == TK_ASN || lex.tk.t == TK_ADDASN || lex.tk.t == TK_SUBASN ||
        lex.tk.t == TK_MULASN || lex.tk.t == TK_DIVASN || lex.tk.t == TK_MODASN ||
        lex.tk.t == TK_ANDASN || lex.tk.t == TK_ORASN || lex.tk.t == TK_XORASN ||
        lex.tk.t == TK_LSASN || lex.tk.t == TK_RSASN) {
        if (!v.is_lvalue) { printf("line %d: lvalue required\n", lex.ln); errs++; }
        int op = lex.tk.t; lex_next();
        if (op != TK_ASN) {
            /* Compound assignment: load current value, apply op, store back */
            if (v.lv_reg >= 0) emit_load_mr(0, v.lv_reg);
            else if (v.lv_sym) { emit_push(0); emit_mov_ri(0,0); fixup_add(8,1,v.lv_sym->n); emit_load_mr(0,0); emit_pop(1); }
            else emit_load_mr_off(0, 5, v.lv_off);
            emit_push(0);
        }
        /* Save lvalue address */
        if (v.lv_reg >= 0) emit_push(v.lv_reg);
        else if (v.lv_sym) { emit_mov_ri(0, 0); fixup_add(8, 1, v.lv_sym->n); emit_push(0); }
        else { emit_lea_rbp(0, v.lv_off); emit_push(0); }
        Value r = parse_assign_expr(); r = val_rvalue(r);
        emit_pop(10);
        emit_store_rm(0, 10);
        v.is_lvalue = 0;
    } else {
        v = val_rvalue(v);
    }
    return v;
}

static Value parse_expr(void) {
    return parse_assign_expr();
}

/* ─── type parser ─── */
static Ty *parse_type(void) {
    int uns = 0;
    while (lex.tk.t == K_CONST || lex.tk.t == K_VOL) lex_next();
    if (lex.tk.t == K_UNSIGN) { uns = 1; lex_next(); }
    if (lex.tk.t == K_SIGNED) { lex_next(); }
    if (lex.tk.t == K_CHAR) { lex_next(); Ty *t = calloc(1,sizeof(Ty)); t->k=TY_CHAR; t->sz=1; t->uns=uns; return t; }
    if (lex.tk.t == K_SHORT) { lex_next(); if(lex.tk.t==K_INT)lex_next(); Ty *t=calloc(1,sizeof(Ty)); t->k=TY_INT; t->sz=2; t->uns=uns; return t; }
    if (lex.tk.t == K_INT || lex.tk.t == K_LONG) {
        int is_long = (lex.tk.t == K_LONG);
        lex_next(); 
        if (is_long && lex.tk.t == K_LONG) lex_next(); /* long long */
        Ty *t = calloc(1,sizeof(Ty));
        t->k = is_long ? TY_LONG : TY_INT;
        t->sz = is_long ? 8 : 4;
        t->uns = uns;
        return t;
    }
    if (lex.tk.t == K_VOID) { lex_next(); return &ty_void; }
    if (lex.tk.t == K_STRUCT) {
        lex_next();
        char name[64] = {0};
        if (lex.tk.t == TK_ID) { strncpy(name, lex.tk.s, 63); free(lex.tk.s); lex_next(); }
        Ty *t = calloc(1,sizeof(Ty)); t->k = TY_INT; t->sz = 4; /* placeholder */
        return t;
    }
    printf("line %d: expected type, got %d\n", lex.ln, lex.tk.t);
    errs++;
    return &ty_int;
}

/* parse declarator: *ptr or name or name[5] or name() */
static Ty *parse_declarator(Ty *base, char *name_out) {
    int ptr_count = 0;
    while (lex.tk.t == TK_STAR) { ptr_count++; lex_next(); }
    if (lex.tk.t == TK_ID) {
        if (name_out) strncpy(name_out, lex.tk.s, 63);
        free(lex.tk.s);
        lex_next();
    }
    /* Postfix */
    if (lex.tk.t == TK_LBRK) {
        lex_next();
        int len = 0;
        if (lex.tk.t == TK_NUM) { len = (int)lex.tk.v; lex_next(); }
        expect(TK_RBRK);
        base = ty_arr(base, len ? len : 1);
    }
    if (lex.tk.t == TK_LPAR) {
        lex_next();
        base = ty_func(base);
        while (lex.tk.t != TK_RPAR) {
            if (lex.tk.t == K_VOID && lex_next() && lex.tk.t == TK_RPAR) break;
            parse_type();
            char dummy[64];
            parse_declarator(&ty_int, dummy);
            if (lex.tk.t == TK_COMMA) lex_next();
        }
        expect(TK_RPAR);
    }
    while (ptr_count--) base = ty_ptr(base);
    return base;
}

/* ─── statement parser ─── */
static void parse_statement(Ty *ret_type) {
    if (lex.tk.t == TK_LBRA) {
        lex_next();
        while (lex.tk.t != TK_RBRA && lex.tk.t != TK_EOF) {
            if (peek_type()) {
                Ty *t = parse_type();
                char name[64] = {0};
                t = parse_declarator(t, name);
                Sym *s = sym_add(name);
                s->t = t;
                s->glob = 0;
                stack_size += t->sz;
                s->soff = stack_size;
                if (lex.tk.t == TK_ASN) {
                    lex_next();
                    parse_expr();
                    emit_store_rm_off(0, 5, -s->soff);
                }
                expect(TK_SEMI);
            } else {
                parse_statement(ret_type);
            }
        }
        expect(TK_RBRA);
        return;
    }
    if (lex.tk.t == K_IF) {
        lex_next();
        expect(TK_LPAR);
        parse_expr();
        expect(TK_RPAR);
        char *l_else = lbl_new();
        char *l_end = lbl_new();
        emit_cmp_ri(0, 0);
        emit_jcc_rel32(4, l_else);
        parse_statement(ret_type);
        emit_jmp_rel32(l_end);
        label_def(l_else);
        if (lex.tk.t == K_ELSE) {
            lex_next();
            parse_statement(ret_type);
        }
        label_def(l_end);
        return;
    }
    if (lex.tk.t == K_WHILE) {
        lex_next();
        char *l_start = lbl_new();
        char *l_end = lbl_new();
        label_def(l_start);
        expect(TK_LPAR);
        parse_expr();
        expect(TK_RPAR);
        emit_cmp_ri(0, 0);
        emit_jcc_rel32(4, l_end);
        parse_statement(ret_type);
        emit_jmp_rel32(l_start);
        label_def(l_end);
        return;
    }
    if (lex.tk.t == K_FOR) {
        lex_next();
        expect(TK_LPAR);
        if (!peek_type()) parse_expr();
        expect(TK_SEMI);
        char *l_start = lbl_new();
        char *l_end = lbl_new();
        char *l_cont = lbl_new();
        label_def(l_start);
        parse_expr();
        expect(TK_SEMI);
        emit_cmp_ri(0, 0);
        emit_jcc_rel32(4, l_end);
        if (lex.tk.t != TK_RPAR) parse_expr();
        expect(TK_RPAR);
        emit_jmp_rel32(l_cont);
        parse_statement(ret_type);
        label_def(l_cont);
        emit_jmp_rel32(l_start);
        label_def(l_end);
        return;
    }
    if (lex.tk.t == K_DO) {
        lex_next();
        char *l_start = lbl_new();
        char *l_end = lbl_new();
        label_def(l_start);
        parse_statement(ret_type);
        expect(K_WHILE);
        expect(TK_LPAR);
        parse_expr();
        expect(TK_RPAR);
        expect(TK_SEMI);
        emit_cmp_ri(0, 0);
        emit_jcc_rel32(5, l_start);
        label_def(l_end);
        return;
    }
    if (lex.tk.t == K_RETURN) {
        lex_next();
        if (lex.tk.t != TK_SEMI) parse_expr();
        if (stack_size > 0) emit_add_ri(4, stack_size);
        emit_pop(5);
        emit_ret();
        expect(TK_SEMI);
        return;
    }
    if (lex.tk.t == K_BREAK) { lex_next(); expect(TK_SEMI); return; }
    if (lex.tk.t == K_CONT) { lex_next(); expect(TK_SEMI); return; }
    if (lex.tk.t == TK_SEMI) { lex_next(); return; }
    parse_expr();
    expect(TK_SEMI);
}

/* ─── global declarations ─── */
static void parse_global(void) {
    while (lex.tk.t != TK_EOF) {
        Ty *t = parse_type();
        char name[64] = {0};
        t = parse_declarator(t, name);
        if (!name[0]) { printf("line %d: missing name\n", lex.ln); errs++; return; }
        
        if (t->k == TY_FUNC) {
            Sym *s = sym_add(name);
            s->t = t;
            s->glob = 1;
            s->func = 1;
            
            if (lex.tk.t == TK_LBRA) {
                /* Function definition */
                s->def = 1;
                s->addr = sects[sec_text].len;
                cur_func = sym_n - 1;
                label_def(name);
                
                /* Prologue */
                emit_push(5); /* push rbp */
                emit_mov_rr(5, 4); /* mov rbp, rsp */
                stack_size = 0;
                
                /* Parse parameters */
                if (lex.tk.t == TK_LPAR) lex_next(); else { expect(TK_LPAR); }
                int param_regs[] = {7, 6, 2, 1, 8, 9};
                int pi = 0;
                while (lex.tk.t != TK_RPAR && lex.tk.t != TK_EOF) {
                    if (lex.tk.t == K_VOID) { lex_next(); break; }
                    Ty *pt = parse_type();
                    char pname[64] = {0};
                    pt = parse_declarator(pt, pname);
                    Sym *ps = sym_add(pname);
                    ps->t = pt;
                    ps->glob = 0;
                    stack_size += pt->sz;
                    ps->soff = stack_size;
                    /* Save parameter from register to stack */
                    if (pi < 6) {
                        int reg = param_regs[pi];
                        emit_store_rm_off(reg, 5, -stack_size);
                    }
                    pi++;
                    if (lex.tk.t == TK_COMMA) lex_next();
                }
                expect(TK_RPAR);
                
                /* Allocate stack frame */
                if (stack_size > 0) emit_sub_ri(4, stack_size);
                
                /* Parse body */
                parse_statement(t->ret);
                
                /* Epilogue */
                if (stack_size > 0) emit_add_ri(4, stack_size);
                emit_pop(5);
                emit_ret();
            } else {
                /* Function declaration (prototype) */
                expect(TK_SEMI);
            }
        } else {
            /* Global variable */
            Sym *s = sym_add(name);
            s->t = t;
            s->glob = 1;
            s->addr = sects[sec_data].len;
            
            if (lex.tk.t == TK_ASN) {
                lex_next();
                parse_expr();
                emit_store_rm_off(0, -1, 0); /* Will fix up */
            } else {
                sec_zero(sec_data, t->sz);
            }
            
            if (lex.tk.t == TK_COMMA) lex_next();
            else expect(TK_SEMI);
        }
    }
}

/* ─── ELF output ─── */
static u8 *write_elf(u64 *out_sz) {
    u64 text_vaddr = base_vaddr;
    u64 ro_vaddr = text_vaddr + sects[sec_text].len;
    u64 data_vaddr = ro_vaddr + sects[sec_rodata].len;
    u64 bss_vaddr = data_vaddr + sects[sec_data].len;
    
    sects[sec_text].vaddr = text_vaddr;
    sects[sec_rodata].vaddr = ro_vaddr;
    sects[sec_data].vaddr = data_vaddr;
    
    /* Resolve fixups */
    for (int i = 0; i < fixup_n; i++) {
        Fixup *f = &fixups[i];
        u64 tgt = 0;
        for (int j = 0; j < label_n; j++) {
            if (strcmp(labels[j].name, f->name) == 0) {
                tgt = sects[labels[j].sec].vaddr + labels[j].offset;
                break;
            }
        }
        if (tgt == 0) {
            Sym *s = sym_find(f->name);
            if (s && s->def && s->func) {
                tgt = base_vaddr + s->addr;
            } else if (s && s->def && s->glob) {
                u64 dv = base_vaddr + sects[sec_text].len + sects[sec_rodata].len;
                tgt = dv + s->addr;
            }
        }
        if (tgt == 0) continue;
        if (f->kind == 0) {
            u64 from = sects[f->sec].vaddr + f->off + 4;
            i64 disp = (i64)(tgt - from);
            u32 *loc = (u32*)(sects[f->sec].data + f->off);
            *loc = (u32)disp;
        } else if (f->kind == 1) {
            u64 *loc = (u64*)(sects[f->sec].data + f->off);
            *loc = tgt;
        }
    }
    
    /* Calculate file layout */
    int phcnt = 0;
    if (sects[sec_text].len > 0) phcnt++;
    if (sects[sec_rodata].len > 0) phcnt++;
    if (sects[sec_data].len > 0) phcnt++;
    
    u64 f_text = sects[sec_text].len;
    u64 f_rodata = sects[sec_rodata].len;
    u64 f_data = sects[sec_data].len;
    
    u64 text_pad = 0, ro_pad = 0, data_pad = 0;
    if (f_rodata > 0 && f_text % 16) text_pad = 16 - (f_text % 16);
    u64 ro_off = sizeof(Elf64_Ehdr) + phcnt * sizeof(Elf64_Phdr) + f_text + text_pad;
    u64 ro_pad2 = 0;
    if (f_data > 0 && (f_text+text_pad+f_rodata) % 16) ro_pad2 = 16 - ((f_text+text_pad+f_rodata) % 16);
    u64 data_off = ro_off + f_rodata + ro_pad2;
    
    u64 total_file = data_off + f_data;
    
    u8 *elf = calloc(1, total_file);
    Elf64_Ehdr *eh = (Elf64_Ehdr*)elf;
    eh->e_ident[0]=0x7F; eh->e_ident[1]='E'; eh->e_ident[2]='L'; eh->e_ident[3]='F';
    eh->e_ident[4]=2; eh->e_ident[5]=1; eh->e_ident[6]=1;
    eh->e_type=2; eh->e_machine=0x3E; eh->e_version=1;
    eh->e_phoff=sizeof(Elf64_Ehdr);
    eh->e_ehsize=sizeof(Elf64_Ehdr);
    eh->e_phentsize=sizeof(Elf64_Phdr);
    eh->e_phnum=phcnt;
    
    Sym *start = sym_find("_start");
    if (start && start->def) eh->e_entry = text_vaddr + start->addr;
    
    int pi = 0;
    u64 cur_off2 = sizeof(Elf64_Ehdr) + phcnt * sizeof(Elf64_Phdr);
    
    if (f_text > 0) {
        Elf64_Phdr *p = (Elf64_Phdr*)(elf + sizeof(Elf64_Ehdr) + pi * sizeof(Elf64_Phdr));
        p->p_type=1; p->p_flags=5; p->p_offset=cur_off2;
        p->p_vaddr=text_vaddr; p->p_paddr=text_vaddr;
        p->p_filesz=f_text; p->p_memsz=f_text; p->p_align=0x1000;
        memcpy(elf+cur_off2, sects[sec_text].data, f_text);
        cur_off2 += f_text + text_pad;
        pi++;
    }
    if (f_rodata > 0) {
        Elf64_Phdr *p = (Elf64_Phdr*)(elf + sizeof(Elf64_Ehdr) + pi * sizeof(Elf64_Phdr));
        p->p_type=1; p->p_flags=4; p->p_offset=cur_off2;
        p->p_vaddr=ro_vaddr; p->p_paddr=ro_vaddr;
        p->p_filesz=f_rodata; p->p_memsz=f_rodata; p->p_align=0x1000;
        memcpy(elf+cur_off2, sects[sec_rodata].data, f_rodata);
        pi++;
    }
    if (f_data > 0) {
        Elf64_Phdr *p = (Elf64_Phdr*)(elf + sizeof(Elf64_Ehdr) + pi * sizeof(Elf64_Phdr));
        p->p_type=1; p->p_flags=6; p->p_offset=cur_off2;
        p->p_vaddr=data_vaddr; p->p_paddr=data_vaddr;
        p->p_filesz=f_data; p->p_memsz=f_data; p->p_align=0x1000;
        memcpy(elf+cur_off2, sects[sec_data].data, f_data);
        pi++;
    }
    
    *out_sz = total_file;
    return elf;
}

/* ─── Main ─── */
int main(int argc, char *argv[]) {
    printf("ECC - Experimental C Compiler for Lumin OS\n");
    
    /* Initialize sections */
    sec_text = sec_add(".text", 4|2);
    sec_rodata = sec_add(".rodata", 4);
    sec_data = sec_add(".data", 4|2);
    
    /* Test compilation */
    const char *test_src = 
        "int x;\n"
        "int _start() {\n"
        "    int a;\n"
        "    a = 42;\n"
        "    return a;\n"
        "}\n";
    
    char *src = strdup(test_src);
    lex.p = src; lex.pos = 0; lex.ln = 1;
    lex_next();
    
    parse_global();
    
    if (errs) {
        printf("compilation failed with %d error(s)\n", errs);
        free(src);
        return 1;
    }
    
    u64 elf_sz;
    u8 *elf = write_elf(&elf_sz);
    printf("compiled %lld bytes ELF\n", (unsigned long long)elf_sz);
    printf("entry at 0x%llx\n", (unsigned long long)((Elf64_Ehdr*)elf)->e_entry);
    free(elf);
    free(src);
    
    /* Clean up sections */
    for (int i = 0; i < sect_n; i++) free(sects[i].data);
    
    printf("ecc: done\n");
    return 0;
}
