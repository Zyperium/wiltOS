#include "strhelper.h"

int CompareLiteral(const char *s, const char *lit){
    while (*s == *lit){ if (!*s) return 1; ++s; ++lit; }
    return 0;
}

int BeginsWith(const char *s, const char *prefix){
    while (*prefix && *s == *prefix){ ++s; ++prefix; }
    return *prefix == 0;
}

int Contains(const char *s, const char *needle){
    if (!*needle) return 1;
    for (const char *p = s; *p; ++p){
        const char *a = p, *b = needle;
        while (*a && *b && *a == *b){ ++a; ++b; }
        if (*b == 0) return 1;
    }
    return 0;
}

int is_space(unsigned char c) { 
    return c==' '||c=='\t'||c=='\r'||c=='\n'; 
}

char* ltrim(char* s){ 
    while(is_space(*s)) ++s; return s; 
}

void rtrim(char* s){
    char* e = s; while(*e) ++e;
    while(e>s && is_space((unsigned char)*(e-1))) --e;
    *e = 0;
}

const char* after_first_space_inplace(char* s){
    while (*s==' '||*s=='\t') ++s;
    char* p = s;
    while (*p && *p!=' ') ++p;
    if (!*p) return (const char*)0;
    ++p;
    while (*p==' '||*p=='\t') ++p;
    return *p ? (const char*)p : (const char*)0;
}

const char* first_word_inplace(char* s){
    while (*s==' '||*s=='\t') ++s;
    char* p = s;
    while (*p && *p!=' ') ++p;
    if (*p) *p = 0;
    return (const char*)s;
}

char* trim(char* s){ 
    s = ltrim(s); 
    rtrim(s); 
    return s; 
}

int has_text(const char *s){
    if (!s) return 0;
    while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') ++s;
    return *s != 0;
}

char* trim_after_prefix(char* s, const char* prefix){
    const char *p = prefix, *q = s;
    while(*p && *q == *p){ ++p; ++q; }
    if(*p) return s;
    while(is_space((unsigned char)*q)) ++q;
    return (char*)q;
}
