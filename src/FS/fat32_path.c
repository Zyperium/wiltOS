#include <stdint.h>
#include <stddef.h>
#include "fat32.h"

typedef struct {
    char name[11];
    uint8_t attr;
    uint8_t ntres;
    uint8_t crt_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc;
    uint16_t clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t clus_lo;
    uint32_t size;
} __attribute__((packed)) dirent_t;

static void up11(char out[11], const char* s){
    int i=0,j=0; for (; j<11; j++) out[j]=' ';
    while (s[i] && j<11){ char c=s[i++]; if (c=='.'){ j=8; continue; } if (c>='a'&&c<='z') c-=32; out[j++]=c; }
}

static const char* next_seg(const char* p, size_t* len){
    while (*p=='/') p++;
    const char* s=p; while (*p && *p!='/') p++;
    *len=(size_t)(p-s); return s;
}

static int seg11(char out[11], const char* s, size_t n){
    char tmp[16]; if (n>15) n=15; for (size_t i=0;i<n;i++) tmp[i]=s[i]; tmp[n]=0; up11(out,tmp); return 0;
}

static int eqi(char a, char b){
    if (a>='a'&&a<='z') a = (char)(a - 32);
    if (b>='a'&&b<='z') b = (char)(b - 32);
    return a == b;
}

static void split83_up(const char* in, char* base, int* blen, char* ext, int* elen){
    int i=0, bj=0, ej=0, seen_dot=0;
    while (in[i]){
        char c = in[i++];
        if (c=='.'){ seen_dot=1; continue; }
        if (c>='a'&&c<='z') c = (char)(c-32);
        if (!seen_dot){
            if (bj<8) base[bj++]=c;
        }else{
            if (ej<3) ext[ej++]=c;
        }
    }
    *blen = bj; *elen = ej;
}

static int entry_matches_name83(const dirent_t* e, const char* name83){
    char b[8], x[3]; int bl=0, xl=0;
    split83_up(name83, b, &bl, x, &xl);
    int i;
    for (i=0;i<bl;i++){ char c=e->name[i]; if (c==' ') return 0; if (!eqi(c,b[i])) return 0; }
    for (;i<8;i++){ if (e->name[i] != ' ') return 0; }
    for (i=0;i<xl;i++){ char c=e->name[8+i]; if (c==' ') return 0; if (!eqi(c,x[i])) return 0; }
    for (;i<3;i++){ if (e->name[8+i] != ' ') return 0; }
    return 1;
}

int fat32_read_stream_root83(fat32_t* fs, const char* name83,
                                    int (*sink)(const uint8_t* data, uint32_t len)){
    if (!fs || !name83 || !*name83) return -1;
    uint32_t cl = fs->root_clus;
    uint32_t bps=fs->bytes_per_sec, spc=fs->sec_per_clus;
    dirent_t hit; int found=0;

    for(;;){
        uint64_t lba0 = fs->part_lba + fs->data_lba + (uint64_t)(cl-2)*spc;
        for (uint32_t s=0; s<spc; s++){
            uint8_t sec[512];
            if (fs->b->read(lba0+s,1,sec)) return -1;
            dirent_t* de=(dirent_t*)sec;
            for (uint32_t i=0;i<bps/32;i++,de++){
                if (de->name[0]==0x00) goto end_scan;
                if ((uint8_t)de->name[0]==0xE5) continue;
                if (de->attr==0x0F) continue;
                if (de->attr & 0x10) continue;
                if (entry_matches_name83(de, name83)){ hit=*de; found=1; goto end_scan; }
            }
        }
        uint32_t nx; if (fat_get(fs,cl,&nx)) return -1;
        if (nx>=0x0FFFFFF8) break;
        cl = nx;
    }
end_scan:
    if (!found) return -1;

    uint32_t fcl = ((uint32_t)hit.clus_hi<<16)|hit.clus_lo;
    uint32_t left = hit.size;
    while (left){
        uint64_t base = fs->part_lba + fs->data_lba + (uint64_t)(fcl-2)*spc;
        for (uint32_t s=0; s<spc && left; s++){
            uint8_t sec[512];
            if (fs->b->read(base+s,1,sec)) return -1;
            uint32_t take = left > bps ? bps : left;
            if (sink(sec,take)) return -1;
            left -= take;
        }
        if (!left) break;
        uint32_t nx; if (fat_get(fs,fcl,&nx)) return -1;
        if (nx<2 || nx>=0x0FFFFFF8) return -1;
        fcl = nx;
    }
    return 0;
}

static int find_in_dir11(fat32_t* fs, uint32_t cl, const char name11[11], dirent_t* out, uint32_t* out_cl, uint32_t* out_off){
    uint32_t bps=fs->bytes_per_sec, spc=fs->sec_per_clus;
    for(;;){
        uint64_t lba0 = fs->part_lba + fs->data_lba + (uint64_t)(cl-2)*spc;
        uint32_t ents = (bps*spc)/sizeof(dirent_t);
        for (uint32_t s=0;s<spc;s++){
            uint8_t sec[512];
            if (fs->b->read(lba0+s,1,sec)) return -1;
            dirent_t* e=(dirent_t*)sec;
            for (uint32_t i=0;i<bps/sizeof(dirent_t); i++,e++){
                if (e->name[0]==0x00) return -2;
                if ((uint8_t)e->name[0]==0xE5) continue;
                if (e->attr==0x0F) continue;
                int match=1; for(int k=0;k<11;k++) if (e->name[k]!=name11[k]){ match=0; break; }
                if (match){ if(out) *out=*e; if(out_cl) *out_cl=cl; if(out_off) *out_off = (s*(bps/sizeof(dirent_t)))+i; return 0; }
            }
        }
        uint32_t nx; if (fat_get(fs,cl,&nx)) return -1;
        if (nx>=0x0FFFFFF8) return -2;
        cl=nx;
    }
}

static int walk_path(fat32_t* fs, const char* path, dirent_t* out, uint32_t* out_cl){
    uint32_t cl = fs->root_clus;
    const char* p=path; size_t n=0;
    while (*p){
        size_t seglen; const char* seg = next_seg(p,&seglen);
        if (seglen==0) break;
        char n11[11]; seg11(n11,seg,seglen);
        dirent_t e; uint32_t dcl, off;
        int rc = find_in_dir11(fs, cl, n11, &e, &dcl, &off);
        if (rc) return -1;
        uint32_t fcl = ((uint32_t)e.clus_hi<<16)|e.clus_lo;
        if (e.attr & 0x10){ cl = fcl; }
        else {
            p += seglen; while (*p=='/') p++;
            if (*p) return -1;
            if (out) *out=e; if(out_cl) *out_cl=cl; return 0;
        }
        p += seglen; while (*p=='/') p++;
    }
    if (out){ dirent_t d; for(int i=0;i<11;i++) d.name[i]=' '; d.attr=0x10; d.clus_hi=(uint16_t)(cl>>16); d.clus_lo=(uint16_t)(cl&0xFFFF); d.size=0; *out=d; }
    if (out_cl) *out_cl=cl;
    return 0;
}

void up11_from_seg(char out[11], const char* s, size_t n){
    for (int i=0;i<11;i++) out[i]=' ';
    int j=0,k=0;
    for (; k<(int)n && j<11; k++){
        char c=s[k];
        if (c=='.'){ j=8; continue; }
        if (c>='a'&&c<='z') c-=32;
        out[j++]=c;
    }
}

int fat32_is_dir_path(fat32_t* fs, const char* path){
    if (!fs || !path) return 0;
    while (*path=='/') path++;
    if (!*path) return 1;
    uint32_t cl = fs->root_clus;
    for(;;){
        const char* s = path; while (*path && *path!='/') path++;
        size_t n = (size_t)(path - s);
        char n11[11]; up11_from_seg(n11, s, n);
        dirent_t e; uint32_t dcl, off;
        if (find_in_dir11(fs, cl, n11, &e, &dcl, &off)) return 0;
        if ((e.attr & 0x10)==0){ while (*path=='/') path++; return *path==0 ? 0 : 0; }
        cl = ((uint32_t)e.clus_hi<<16)|e.clus_lo; if (cl<2) cl=fs->root_clus;
        while (*path=='/') path++;
        if (!*path) return 1;
    }
}

int fat32_list_path(fat32_t* fs, const char* path, void (*cb)(const char* name, uint32_t size, int is_dir)){
    dirent_t e; uint32_t cl=0;
    if (walk_path(fs,path,&e,&cl)) return -1;
    if (!(e.attr & 0x10)) return -1;
    uint32_t bps=fs->bytes_per_sec, spc=fs->sec_per_clus;
    for(;;){
        uint64_t lba0 = fs->part_lba + fs->data_lba + (uint64_t)(cl-2)*spc;
        for (uint32_t s=0;s<spc;s++){
            uint8_t sec[512];
            if (fs->b->read(lba0+s,1,sec)) return -1;
            dirent_t* de=(dirent_t*)sec;
            for (uint32_t i=0;i<bps/sizeof(dirent_t); i++,de++){
                if (de->name[0]==0x00) return 0;
                if ((uint8_t)de->name[0]==0xE5) continue;
                if (de->attr==0x0F) continue;
                if (de->attr & 0x08) continue;
                char n[13]; int p2=0; for(int k=0;k<8;k++) n[p2++]=de->name[k];
                while(p2>0 && n[p2-1]==' ') p2--;
                if (!(de->attr & 0x10)){
                    if (p2<12){ n[p2++]='.'; for(int k=0;k<3;k++) n[p2++]=de->name[8+k]; while(p2>0 && n[p2-1]==' ') p2--; if (n[p2-1]=='.') p2--; }
                }
                n[p2]=0;
                cb(n, de->size, (de->attr&0x10)?1:0);
            }
        }
        uint32_t nx; if (fat_get(fs,cl,&nx)) return -1;
        if (nx>=0x0FFFFFF8) return 0;
        cl=nx;
    }
}

int fat32_read_stream_path(fat32_t* fs, const char* path, int (*sink)(const uint8_t* data, uint32_t len)){
    dirent_t e; uint32_t pcl=0;
    if (walk_path(fs,path,&e,&pcl)) return -1;
    if (e.attr & 0x10) return -1;
    uint32_t bps=fs->bytes_per_sec, spc=fs->sec_per_clus;
    uint32_t cl = ((uint32_t)e.clus_hi<<16)|e.clus_lo;
    uint32_t left = e.size;
    while (left){
        uint64_t lba0 = fs->part_lba + fs->data_lba + (uint64_t)(cl-2)*spc;
        for (uint32_t s=0; s<spc && left; s++){
            uint8_t sec[512];
            if (fs->b->read(lba0+s,1,sec)) return -1;
            uint32_t take = left>bps? bps: left;
            if (sink(sec,take)) return -1;
            left -= take;
        }
        if (!left) break;
        uint32_t nx; if (fat_get(fs,cl,&nx)) return -1;
        if (nx<2 || nx>=0x0FFFFFF8) return -1;
        cl=nx;
    }
    return 0;
}

static int fat_set_one(fat32_t* fs, uint64_t fat_lba_base, uint32_t cl, uint32_t val){
    uint8_t sec[512];
    uint64_t idx = (uint64_t)cl * 4;
    uint64_t lba = fs->part_lba + fat_lba_base + (idx / fs->bytes_per_sec);
    uint32_t off = (uint32_t)(idx % fs->bytes_per_sec);
    if (fs->b->read(lba, 1, sec)) return -1;
    uint32_t v = *(uint32_t*)(sec + off);
    v = (v & 0xF0000000) | (val & 0x0FFFFFFF);
    *(uint32_t*)(sec + off) = v;
    if (fs->b->write(lba, 1, sec)) return -1;
    return 0;
}

static int fat_set2(fat32_t* fs, uint32_t cl, uint32_t val){
    if (fat_set_one(fs, fs->fat_lba, cl, val)) return -1;
    if (fs->num_fats > 1)
        if (fat_set_one(fs, fs->fat_lba + fs->fat_sz, cl, val)) return -1;
    return 0;
}

static uint32_t find_free_cluster(fat32_t* fs, uint32_t start){
    uint64_t entries = (uint64_t)fs->fat_sz * fs->bytes_per_sec / 4;
    if (entries < 3) return 0;
    uint32_t first = (start >= 2 && start < entries) ? start : 2;
    uint8_t sec[512];
    for (uint64_t cl = first; cl < entries; cl++){
        uint64_t idx = (uint64_t)cl * 4;
        uint64_t lba = fs->part_lba + fs->fat_lba + (idx / fs->bytes_per_sec);
        uint32_t off = (uint32_t)(idx % fs->bytes_per_sec);
        if (fs->b->read(lba, 1, sec)) return 0;
        uint32_t v = *(uint32_t*)(sec + off) & 0x0FFFFFFF;
        if (v == 0) return (uint32_t)cl;
    }
    return 0;
}

static int dir_write_entry_at_lba(fat32_t* fs, uint64_t lba, uint32_t idx_in_sec, const void* ent){
    uint8_t sec[512];
    if (fs->b->read(lba,1,sec)) return -1;
    uint32_t off = idx_in_sec * 32;
    for (int i=0;i<32;i++) ((uint8_t*)sec)[off+i] = ((const uint8_t*)ent)[i];
    if (fs->b->write(lba,1,sec)) return -1;
    return 0;
}

static int dir_find_free_slot_chain(fat32_t* fs, uint32_t dir_cl, uint64_t* out_lba, uint32_t* out_idx){
    uint32_t bps=fs->bytes_per_sec, spc=fs->sec_per_clus;
    for(;;){
        uint64_t base = fs->part_lba + fs->data_lba + (uint64_t)(dir_cl-2)*spc;
        for (uint32_t s=0;s<spc;s++){
            uint8_t sec[512];
            if (fs->b->read(base+s,1,sec)) return -1;
            for (uint32_t i=0;i<bps/32;i++){
                uint8_t f = sec[i*32];
                if (f==0x00 || f==0xE5){ *out_lba = base+s; *out_idx = i; return 0; }
            }
        }
        uint32_t nx; if (fat_get(fs,dir_cl,&nx)) return -1;
        if (nx>=0x0FFFFFF8) return -1;
        dir_cl = nx;
    }
}

static int last_seg(const char* path, const char** seg, size_t* seglen, int* is_dir){
    const char* p=path; const char* last=p; size_t lastn=0;
    while (*p){
        while (*p=='/') p++;
        if (!*p) break;
        const char* s=p; while (*p && *p!='/') p++;
        last=s; lastn=(size_t)(p-s);
    }
    if (!lastn) return -1;
    *seg=last; *seglen=lastn;
    *is_dir = 0;
    return 0;
}

static const char* skp(const char* p){ while (*p=='/') ++p; return p; }

static int parent_dir_for(fat32_t* fs, const char* path,
                          uint32_t* out_dir_cl, const char** out_base, size_t* out_blen){
    if (!fs || !path){ *out_dir_cl = fs? fs->root_clus : 0; *out_base=""; *out_blen=0; return -1; }

    const char* p = skp(path);
    const char* last = p; size_t lastn = 0;

    while (*p){
        const char* s = p; while (*p && *p!='/') ++p;
        last = s; lastn = (size_t)(p - s);
        p = skp(p);
    }
    if (lastn == 0){ *out_dir_cl = fs->root_clus; *out_base=""; *out_blen=0; return 0; }

    uint32_t cl = fs->root_clus;
    p = skp(path);
    while (*p){
        const char* s = p; while (*p && *p!='/') ++p;
        size_t seglen = (size_t)(p - s);
        if (s == last) break;

        char n11[11]; up11_from_seg(n11, s, seglen);
        dirent_t e; uint32_t dcl, off;
        if (find_in_dir11(fs, cl, n11, &e, &dcl, &off)) return -1;
        if ((e.attr & 0x10) == 0) return -1;

        uint32_t ncl = ((uint32_t)e.clus_hi<<16) | e.clus_lo;
        cl = ncl ? ncl : fs->root_clus;

        p = skp(p);
    }

    *out_dir_cl = cl;
    *out_base   = last;
    *out_blen   = lastn;
    return 0;
}

int fat32_write_file_path(fat32_t* fs, const char* path, const uint8_t* data, uint32_t sz){
    if (!fs || !fs->b || !fs->b->write || !path) return -1;

    uint32_t dir_cl = 0; const char* base = 0; size_t basen = 0;
    if (parent_dir_for(fs, path, &dir_cl, &base, &basen)) return -1;
    if (basen == 0) return -1;

    char name11[11]; up11_from_seg(name11, base, basen);

    dirent_t exist; uint32_t dcl=0, off=0;
    int has = (find_in_dir11(fs, dir_cl, name11, &exist, &dcl, &off) == 0 && (exist.attr & 0x10)==0);

    if (has){
        uint32_t cur = ((uint32_t)exist.clus_hi<<16)|exist.clus_lo;
        while (cur>=2 && cur<0x0FFFFFF8){
            uint32_t nx; if (fat_get(fs,cur,&nx)) return -1;
            if (fat_set2(fs,cur,0)) return -1;
            cur = nx;
        }
    }

    uint32_t bps=fs->bytes_per_sec, spc=fs->sec_per_clus, cps=bps*spc;
    uint32_t need = (sz + (cps-1))/cps; if (!need) need=1;

    uint32_t first=0, prev=0;
    for (uint32_t i=0;i<need;i++){
        uint32_t cl = find_free_cluster(fs, prev?prev+1:2); if (!cl) return -1;
        if (!first) first=cl;
        if (prev && fat_set2(fs,prev,cl)) return -1;
        prev=cl;
    }
    if (prev && fat_set2(fs,prev,0x0FFFFFFF)) return -1;

    uint32_t left=sz, pos=0, cl=first;
    while (1){
        uint64_t base_lba = fs->part_lba + fs->data_lba + (uint64_t)(cl-2)*spc;
        for (uint32_t s=0; s<spc; s++){
            uint8_t sec[512];
            if (left>=bps){
                for (uint32_t i=0;i<bps;i++) sec[i]=data[pos+i];
                if (fs->b->write(base_lba+s,1,sec)) return -1;
                pos+=bps; left-=bps;
            } else {
                for (uint32_t i=0;i<bps;i++) sec[i]=0;
                for (uint32_t i=0;i<left;i++) sec[i]=data[pos+i];
                if (fs->b->write(base_lba+s,1,sec)) return -1;
                pos+=left; left=0;
            }
            if (left==0){ for (uint32_t r=s+1;r<spc;r++){ uint8_t z[512]={0}; if (fs->b->write(base_lba+r,1,z)) return -1; } break; }
        }
        if (left==0) break;
        uint32_t nx; if (fat_get(fs,cl,&nx)) return -1;
        if (nx<2 || nx>=0x0FFFFFF8) return -1;
        cl=nx;
    }

    dirent_t ne;
    for (int i=0;i<11;i++) ne.name[i]=name11[i];
    ne.attr=0x20; ne.ntres=0; ne.crt_tenth=0; ne.crt_time=0; ne.crt_date=0;
    ne.lst_acc=0; ne.wrt_time=0; ne.wrt_date=0;
    ne.clus_hi=(uint16_t)(first>>16); ne.clus_lo=(uint16_t)(first&0xFFFF);
    ne.size=sz;

    if (has){
        uint32_t ents_per_sec = bps/32;
        uint32_t sec_idx = off / ents_per_sec;
        uint32_t ent_idx = off % ents_per_sec;
        uint64_t lba = fs->part_lba + fs->data_lba + (uint64_t)(dcl-2)*spc + sec_idx;
        return dir_write_entry_at_lba(fs, lba, ent_idx, &ne);
    } else {
        uint64_t lba; uint32_t idx;
        if (dir_find_free_slot_chain(fs, dir_cl, &lba, &idx)) return -1;
        return dir_write_entry_at_lba(fs, lba, idx, &ne);
    }
}
