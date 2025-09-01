#include <stddef.h>

static int is_sep(char c){ return c=='/'; }

void path_resolve(const char* cwd, const char* in, char* out, size_t cap){
    size_t k=0;
    if (in && in[0]=='/') { out[k++]='/'; }
    else {
        for (size_t i=0; cwd && cwd[i] && k+1<cap; i++) out[k++]=cwd[i];
        if (k==0) out[k++]='/';
        if (!is_sep(out[k-1]) && k+1<cap) out[k++]='/';
    }
    for (size_t i=0; in && in[i] && k+1<cap; i++) out[k++]=in[i];
    if (k==0){ out[k++]='/'; }
    out[k]=0;
    size_t w=0, r=0;
    while (out[r]){
        while (out[r]=='/' && out[r+1]=='/') r++;
        if (out[r]=='.' && (out[r+1]==0 || out[r+1]=='/')){ r+=1; if (out[r]=='/') r++; continue; }
        if (out[r]=='.' && out[r+1]=='.' && (out[r+2]==0 || out[r+2]=='/')){
            if (w>1){ w--; while (w>0 && out[w-1]!='/') w--; }
            r+=2; if (out[r]=='/') r++;
            continue;
        }
        out[w++]=out[r++];
    }
    if (w>1 && out[w-1]=='/') w--;
    out[w]=0;
}

const char* disk_subpath(const char* abs){
    if (!abs) return 0;
    if (abs[0]=='/' && abs[1]=='d' && abs[2]=='i' && abs[3]=='s' && abs[4]=='k'){
        const char* p=abs+5; if (*p==0) return "/";
        if (*p!='/') return 0;
        return p+1;
    }
    return 0;
}
