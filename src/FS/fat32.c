#include <stdint.h>
#include <stddef.h>
#include "fat32.h"
#include "../Memory/kmem.h"

extern void serial_write(const char*); extern void serial_puthex64(uint64_t);

typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t BytsPerSec;      // 11
    uint8_t  SecPerClus;      // 13
    uint16_t RsvdSecCnt;      // 14
    uint8_t  NumFATs;         // 16
    uint16_t RootEntCnt;      // 17 (FAT12/16)
    uint16_t TotSec16;        // 19
    uint8_t  Media;           // 21
    uint16_t FATSz16;         // 22
    uint16_t SecPerTrk;       // 24
    uint16_t NumHeads;        // 26
    uint32_t HiddSec;         // 28
    uint32_t TotSec32;        // 32

    uint32_t FATSz32;         // 36
    uint16_t ExtFlags;        // 40
    uint16_t FSVer;           // 42
    uint32_t RootClus;        // 44
    uint16_t FSInfo;          // 48
    uint16_t BkBootSec;       // 50
    uint8_t  Reserved[12];    // 52..63
} __attribute__((packed)) bpb32_t;

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

static uint64_t clus_lba(fat32_t* fs, uint32_t cl){
    return fs->data_lba + (uint64_t)(cl - 2) * fs->sec_per_clus;
}
static int read_sector(fat32_t* fs, uint64_t lba, void* buf){ return fs->b->read(fs->part_lba + lba, 1, buf); }
static int write_sector(fat32_t* fs, uint64_t lba, const void* buf){ return fs->b->write(fs->part_lba + lba, 1, buf); }

int fat32_mount(fat32_t* fs, blockdev_t* b, uint64_t part_lba){
    if (!fs || !b || !b->read) return -1;

    serial_write("fat32: byps="); serial_puthex64(fs->bytes_per_sec);
    serial_write(" spc="); serial_puthex64(fs->sec_per_clus);
    serial_write(" rsvd="); serial_puthex64(fs->rsvd_secs);
    serial_write(" nfats="); serial_puthex64(fs->num_fats);
    serial_write(" fatsz="); serial_puthex64(fs->fat_sz);
    serial_write(" root="); serial_puthex64(fs->root_clus);
    serial_write("\n");

    uint8_t sec[512];
    if (b->read(part_lba,1,sec)) return -1;

    const bpb32_t* bpb = (const bpb32_t*)sec;
    uint16_t byps = bpb->BytsPerSec;
    if (byps == 0 || (byps & (byps-1))) return -1;      // must be power-of-two

    fs->b             = b;
    fs->bytes_per_sec = byps;
    fs->sec_per_clus  = bpb->SecPerClus;
    fs->rsvd_secs     = bpb->RsvdSecCnt;
    fs->num_fats      = bpb->NumFATs;

    uint32_t fatsz = bpb->FATSz32 ? bpb->FATSz32 : bpb->FATSz16;
    if (fatsz == 0) return -1;
    fs->fat_sz    = fatsz;
    fs->root_clus = bpb->RootClus ? bpb->RootClus : 2;

    fs->part_lba  = part_lba;
    fs->fat_lba   = fs->rsvd_secs;                          // within partition
    fs->data_lba  = fs->rsvd_secs + (uint64_t)fs->num_fats * fs->fat_sz;

    return 0;
}

static void upcase11(char out[11], const char* name83){
    for (int i=0;i<11;i++) out[i]=' ';
    int i=0,j=0;
    while (name83[i] && j<11){
        char c = name83[i++];
        if (c=='.'){ j=8; continue; }
        if (c>='a'&&c<='z') c-=32;
        out[j++]=c;
    }
}

static int fat_get(fat32_t* fs, uint32_t cl, uint32_t* next){
    uint8_t sec[512];
    uint64_t idx = cl*4;
    uint64_t lba = fs->fat_lba + (idx/512);
    uint32_t off = (uint32_t)(idx%512);
    if (read_sector(fs,lba,sec)) return -1;
    *next = *(uint32_t*)(sec+off) & 0x0FFFFFFF;
    return 0;
}

static int fat_set(fat32_t* fs, uint32_t cl, uint32_t val){
    uint8_t sec[512];
    uint64_t idx = cl*4;
    uint64_t lba = fs->fat_lba + (idx/512);
    uint32_t off = (uint32_t)(idx%512);
    if (read_sector(fs,lba,sec)) return -1;
    uint32_t v = *(uint32_t*)(sec+off);
    v = (v & 0xF0000000) | (val & 0x0FFFFFFF);
    *(uint32_t*)(sec+off) = v;
    if (write_sector(fs,lba,sec)) return -1;
    return 0;
}

static int read_clus(fat32_t* fs, uint32_t cl, uint8_t* buf){
    uint32_t bps = fs->bytes_per_sec;
    for (uint32_t i=0;i<fs->sec_per_clus;i++){
        if (read_sector(fs, clus_lba(fs,cl)+i, buf + i*bps)) return -1;
    }
    return 0;
}

static int write_clus(fat32_t* fs, uint32_t cl, const uint8_t* buf){
    uint32_t bps = fs->bytes_per_sec;
    for (uint32_t i=0;i<fs->sec_per_clus;i++){
        if (write_sector(fs, clus_lba(fs,cl)+i, buf + i*bps)) return -1;
    }
    return 0;
}

static int find_in_dir(fat32_t* fs, uint32_t cl, const char name11[11], dirent_t* out, uint32_t* out_cl, uint32_t* out_off){
    uint8_t buf[4096];
    for(;;){
        if (read_clus(fs, cl, buf)) return -1;
        dirent_t* e=(dirent_t*)buf;
        for (uint32_t i=0;i<fs->bytes_per_sec*fs->sec_per_clus/sizeof(dirent_t); i++,e++){
            if (e->name[0]==0x00) return -2;
            if ((uint8_t)e->name[0]==0xE5) continue;
            if (e->attr==0x0F) continue;
            int match=1; for (int k=0;k<11;k++) if (e->name[k]!=name11[k]) {match=0;break;}
            if (match){ if(out) *out=*e; if(out_cl) *out_cl=cl; if(out_off) *out_off=i; return 0; }
        }
        uint32_t nx; if (fat_get(fs, cl, &nx)) return -1;
        if (nx>=0x0FFFFFF8) return -2;
        cl=nx;
    }
}

int fat32_read_all(fat32_t* fs, const char* path, uint8_t** out, uint32_t* sz){
    if (!fs || !fs->b || !fs->b->read || !path || !out || !sz) return -1;
    char name11[11]; upcase11(name11, path);
    dirent_t e; uint32_t dcl, doff;
    if (find_in_dir(fs, fs->root_clus, name11, &e, &dcl, &doff)) return -1;
    uint32_t cl = ((uint32_t)e.clus_hi<<16) | e.clus_lo;
    uint32_t left = e.size;
    uint32_t cps = fs->sec_per_clus * fs->bytes_per_sec;
    uint8_t* buf = (uint8_t*)kmalloc(left ? left : 1);
    uint32_t pos = 0;
    while (left){
        uint8_t tmp[4096];
        if (read_clus(fs, cl, tmp)) { kfree(buf); return -1; }
        uint32_t take = (left>cps)? cps : left;
        for (uint32_t i=0;i<take;i++) buf[pos+i]=tmp[i];
        pos+=take; left-=take;
        uint32_t nx; if (fat_get(fs,cl,&nx)) { kfree(buf); return -1; }
        if (left && nx>=0x0FFFFFF8) { kfree(buf); return -1; }
        cl=nx;
    }
    *out = buf; *sz = e.size;
    return 0;
}

static uint32_t find_free_cluster(fat32_t* fs, uint32_t start){
    uint8_t sec[512];
    uint64_t ents = (uint64_t)fs->fat_sz * 512 / 4;
    for (uint64_t cl = (start?start:2); cl < ents; cl++){
        uint64_t idx = cl*4;
        uint64_t lba = fs->fat_lba + (idx/512);
        uint32_t off = (uint32_t)(idx%512);
        if (read_sector(fs,lba,sec)) return 0;
        uint32_t v = *(uint32_t*)(sec+off) & 0x0FFFFFFF;
        if (v==0) return (uint32_t)cl;
    }
    return 0;
}

static int dir_write_entry(fat32_t* fs, uint32_t dcl, uint32_t slot, const dirent_t* e){
    uint8_t buf[4096];
    while (dcl){
        if (read_clus(fs,dcl,buf)) return -1;
        dirent_t* de=(dirent_t*)buf;
        if (slot < fs->bytes_per_sec*fs->sec_per_clus/sizeof(dirent_t)){
            de[slot]=*e;
            return write_clus(fs,dcl,buf);
        }
        uint32_t nx; if (fat_get(fs,dcl,&nx)) return -1;
        if (nx>=0x0FFFFFF8) return -1;
        dcl=nx; slot -= fs->bytes_per_sec*fs->sec_per_clus/sizeof(dirent_t);
    }
    return -1;
}

static int dir_find_free_slot(fat32_t* fs, uint32_t dcl, uint32_t* out_cl, uint32_t* out_idx){
    uint8_t buf[4096];
    for(;;){
        if (read_clus(fs,dcl,buf)) return -1;
        dirent_t* de=(dirent_t*)buf;
        for (uint32_t i=0;i<fs->bytes_per_sec*fs->sec_per_clus/sizeof(dirent_t); i++){
            if (de[i].name[0]==0x00 || (uint8_t)de[i].name[0]==0xE5){ *out_cl=dcl; *out_idx=i; return 0; }
        }
        uint32_t nx; if (fat_get(fs,dcl,&nx)) return -1;
        if (nx>=0x0FFFFFF8) return -1;
        dcl=nx;
    }
}

int fat32_write_all_root(fat32_t* fs, const char* name83, const uint8_t* data, uint32_t sz){
    if (!fs || !fs->b || !fs->b->read || !name83 || (!data && sz)) return -1;
    char name11[11]; upcase11(name11, name83);
    dirent_t e; uint32_t exist_cl, exist_off;
    int exists = (find_in_dir(fs, fs->root_clus, name11, &e, &exist_cl, &exist_off) == 0);
    uint32_t need = (sz + (fs->sec_per_clus*512 - 1)) / (fs->sec_per_clus*512);
    uint32_t first=0, prev=0;
    for (uint32_t i=0;i<need;i++){
        uint32_t cl = find_free_cluster(fs, prev?prev+1:2);
        if (!cl) return -1;
        if (!first) first=cl;
        if (prev) { if (fat_set(fs,prev,cl)) return -1; }
        prev=cl;
    }
    if (prev) if (fat_set(fs,prev,0x0FFFFFFF)) return -1;

    uint32_t left=sz; uint32_t pos=0; uint32_t cl=first;
    while(left){
        uint8_t buf[4096];
        uint32_t cps = fs->sec_per_clus*512;
        uint32_t take = (left>cps)? cps : left;
        for(uint32_t i=0;i<take;i++) buf[i]=data[pos+i];
        for(uint32_t i=take;i<cps;i++) buf[i]=0;
        if (write_clus(fs,cl,buf)) return -1;
        pos+=take; left-=take;
        if (left){ uint32_t nx; if (fat_get(fs,cl,&nx)) return -1; cl=nx; }
    }

    dirent_t ne;
    for (int i=0;i<11;i++) ne.name[i]=name11[i];
    ne.attr=0x20;
    ne.ntres=0; ne.crt_tenth=0; ne.crt_time=0; ne.crt_date=0; ne.lst_acc=0; ne.wrt_time=0; ne.wrt_date=0;
    ne.clus_hi=(uint16_t)(first>>16);
    ne.clus_lo=(uint16_t)(first&0xFFFF);
    ne.size=sz;

    if (exists){
        if (dir_write_entry(fs, exist_cl, exist_off, &ne)) return -1;
    } else {
        uint32_t dcl, idx;
        if (dir_find_free_slot(fs, fs->root_clus, &dcl, &idx)) return -1;
        if (dir_write_entry(fs, dcl, idx, &ne)) return -1;
    }
    return 0;
}

int fat32_list_root(fat32_t* fs, void (*cb)(const char* name, uint32_t size, int is_dir)){
    if (!fs || !fs->b || !fs->b->read || !cb) return -1;
    uint8_t buf[4096];
    uint32_t cl = fs->root_clus;
    for(;;){
        if (read_clus(fs,cl,buf)) return -1;
        dirent_t* e=(dirent_t*)buf;
        for (uint32_t i=0;i<fs->bytes_per_sec*fs->sec_per_clus/sizeof(dirent_t); i++,e++){
            if (e->name[0]==0x00) return 0;
            if ((uint8_t)e->name[0]==0xE5 || e->attr==0x0F) continue;
            char n[13]; for (int k=0;k<8;k++) n[k]=e->name[k];
            int p=8; while(p>0 && n[p-1]==' ') p--;
            n[p++]='.';
            for (int k=0;k<3;k++) n[p++]=e->name[8+k];
            while(p>0 && n[p-1]==' ') p--;
            if (n[p-1]=='.') p--;
            n[p]=0;
            cb(n, e->size, (e->attr&0x10)?1:0);
        }
        uint32_t nx; if (fat_get(fs,cl,&nx)) return -1;
        if (nx>=0x0FFFFFF8) return 0;
        cl=nx;
    }
}
