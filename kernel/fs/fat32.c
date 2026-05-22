#include "fat32.h"
#include "drivers/serial/serial.h"
#include "mm/kmalloc.h"
#include <stddef.h>

static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void wr32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

static uint32_t fat_next_cluster(struct fat32_fs *fs, uint32_t cluster)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start + (fat_offset / fs->bpb.bytes_per_sector);
    uint32_t byte_off   = fat_offset % fs->bpb.bytes_per_sector;

    uint8_t buf[512];
    if (block_read(fs->bdev, fat_sector, 1, buf) != 0)
        return FAT32_EOC;

    return rd32(buf + byte_off) & FAT_MASK;
}

int fat32_read_cluster(struct fat32_fs *fs, uint32_t cluster, void *buf)
{
    uint32_t first_sector = fs->data_start + (cluster - 2) * fs->bpb.sectors_per_cluster;
    uint8_t count = fs->bpb.sectors_per_cluster;
    return block_read(fs->bdev, first_sector, count, buf);
}

/* ── FAT write helpers ── */

static int fat_set_cluster(struct fat32_fs *fs, uint32_t cluster, uint32_t value)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start + (fat_offset / fs->bpb.bytes_per_sector);
    uint32_t byte_off   = fat_offset % fs->bpb.bytes_per_sector;

    uint8_t buf[512];
    if (block_read(fs->bdev, fat_sector, 1, buf) != 0)
        return -1;

    uint32_t old = rd32(buf + byte_off);
    wr32(buf + byte_off, (old & ~FAT_MASK) | (value & FAT_MASK));

    return block_write(fs->bdev, fat_sector, 1, buf);
}

static uint32_t fat_alloc_cluster(struct fat32_fs *fs)
{
    uint32_t bps = fs->bpb.bytes_per_sector;
    uint8_t buf[512];
    uint32_t total = fs->total_clusters + 2;

    for (uint32_t s = 0; s < fs->sectors_per_fat; s++)
    {
        if (block_read(fs->bdev, fs->fat_start + s, 1, buf) != 0)
            return FAT32_EOC;

        for (uint32_t i = 0; i < bps / 4; i++)
        {
            uint32_t cluster = (s * (bps / 4)) + i;
            if (cluster < 2) continue;
            if (cluster >= total) return FAT32_EOC;

            if ((((uint32_t *)buf)[i] & FAT_MASK) == FAT32_FREE)
            {
                ((uint32_t *)buf)[i] = FAT32_EOC;
                if (block_write(fs->bdev, fs->fat_start + s, 1, buf) != 0)
                    return FAT32_EOC;
                return cluster;
            }
        }
    }
    return FAT32_EOC;
}

static void fat_free_chain(struct fat32_fs *fs, uint32_t cluster)
{
    while (cluster >= 2 && cluster < FAT32_EOC)
    {
        uint32_t next = fat_next_cluster(fs, cluster);
        fat_set_cluster(fs, cluster, FAT32_FREE);
        cluster = next;
    }
}

static uint32_t fat_chain_last(struct fat32_fs *fs, uint32_t cluster)
{
    if (cluster < 2 || cluster >= FAT32_EOC)
        return 0;
    uint32_t next;
    while ((next = fat_next_cluster(fs, cluster)) >= 2 && next < FAT32_EOC)
        cluster = next;
    return cluster;
}

static uint32_t fat_extend_chain(struct fat32_fs *fs, uint32_t cluster)
{
    uint32_t last = fat_chain_last(fs, cluster);
    uint32_t new_cl = fat_alloc_cluster(fs);
    if (new_cl >= FAT32_EOC)
        return FAT32_EOC;

    if (last >= 2 && last < FAT32_EOC)
        fat_set_cluster(fs, last, new_cl);

    return new_cl;
}

static int fat_write_fsinfo(struct fat32_fs *fs, uint32_t free_clusters, uint32_t next_free)
{
    uint32_t fsi_sector = fs->bpb.fs_info_sector;
    if (fsi_sector == 0) fsi_sector = 1;

    uint8_t buf[512];
    if (block_read(fs->bdev, fsi_sector, 1, buf) != 0)
        return -1;

    uint32_t sig1 = rd32(buf + 0);
    uint32_t sig2 = rd32(buf + 484);
    if (sig1 != 0x41615252 || sig2 != 0x61417272)
        return 0;

    if (free_clusters != 0xFFFFFFFF)
        wr32(buf + 488, free_clusters);
    if (next_free != 0xFFFFFFFF)
        wr32(buf + 492, next_free);

    return block_write(fs->bdev, fsi_sector, 1, buf);
}

/* ── Short name / LFN helpers ── */

static uint8_t fat_checksum(const char *shortname)
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (sum >> 1) + (sum << 7) + (uint8_t)shortname[i];
    return sum;
}

static int lfn_to_ascii(const uint16_t *utf16, int len, char *out, int out_max)
{
    int o = 0;
    for (int i = 0; i < len && o < out_max - 1; i++)
    {
        uint16_t c = utf16[i];
        if (c == 0xFFFF || c == 0xFFFE)
            break;
        if (c >= 0x20 && c <= 0x7E)
            out[o++] = (char)c;
        else if (c == 0)
            break;
        else
            out[o++] = '?';
    }
    out[o] = '\0';
    return o;
}

static void ascii_to_utf16(const char *in, int in_len, uint16_t *out, int out_len)
{
    int i;
    for (i = 0; i < in_len && i < out_len; i++)
        out[i] = (uint8_t)in[i];
    for (; i < out_len; i++)
        out[i] = 0xFFFF;
}

static int parse_short_name(const char name11[11], char *out, int out_max)
{
    int o = 0;

    for (int i = 0; i < 8 && name11[i] && name11[i] != ' '; i++)
    {
        if (o < out_max - 1)
            out[o++] = (name11[i] >= 'A' && name11[i] <= 'Z') ? name11[i] + 0x20 : name11[i];
    }

    int has_ext = 0;
    for (int i = 8; i < 11 && name11[i] && name11[i] != ' '; i++)
        has_ext = 1;

    if (has_ext)
    {
        if (o < out_max - 1) out[o++] = '.';
        for (int i = 8; i < 11 && name11[i] && name11[i] != ' '; i++)
        {
            if (o < out_max - 1)
                out[o++] = (name11[i] >= 'A' && name11[i] <= 'Z') ? name11[i] + 0x20 : name11[i];
        }
    }

    out[o] = '\0';
    return o;
}

static void make_short_name(const char *long_name, char name11[11])
{
    for (int i = 0; i < 11; i++)
        name11[i] = ' ';

    int len = 0;
    while (long_name[len]) len++;

    int dot_pos = -1;
    for (int i = len - 1; i >= 0; i--)
    {
        if (long_name[i] == '.') { dot_pos = i; break; }
    }

    int base_len = (dot_pos >= 0) ? dot_pos : len;
    int ext_len = (dot_pos >= 0) ? (len - dot_pos - 1) : 0;

    for (int i = 0; i < 6 && i < base_len; i++)
    {
        char c = long_name[i];
        if (c >= 'a' && c <= 'z') c -= 0x20;
        if (c >= 'A' && c <= 'Z') { name11[i] = c; continue; }
        if (c >= '0' && c <= '9') { name11[i] = c; continue; }
        name11[i] = '_';
    }
    name11[6] = '~';
    name11[7] = '1';

    for (int i = 0; i < 3 && i < ext_len; i++)
    {
        char c = long_name[dot_pos + 1 + i];
        if (c >= 'a' && c <= 'z') c -= 0x20;
        if (c >= 'A' && c <= 'Z') { name11[8 + i] = c; continue; }
        if (c >= '0' && c <= '9') { name11[8 + i] = c; continue; }
        name11[8 + i] = '_';
    }
}

static int strcmp_ci(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 0x20 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 0x20 : *b;
        if (ca != cb) return -1;
        a++; b++;
    }
    if (*a != *b) return -1;
    return 0;
}

static void collect_lfn(struct fat32_lfn_part *lfn_entries, int count, char *out, int out_max)
{
    int o = 0;
    for (int i = count - 1; i >= 0; i--)
    {
        uint16_t tmp[13];
        for (int j = 0; j < 5; j++) tmp[j] = lfn_entries[i].name1[j];
        for (int j = 0; j < 6; j++) tmp[5 + j] = lfn_entries[i].name2[j];
        for (int j = 0; j < 2; j++) tmp[11 + j] = lfn_entries[i].name3[j];
        o += lfn_to_ascii(tmp, 13, out + o, out_max - o);
    }
}

/* ── LFN scan state ── */

struct fat_scan_state {
    struct fat32_lfn_part lfn_buf[FAT32_MAX_LFN_ENTRIES];
    int lfn_count;
    int lfn_seq;
    uint8_t lfn_checksum;
    int visible_count;
    int found;
};

static void reset_lfn(struct fat_scan_state *st)
{
    st->lfn_count = 0;
    st->lfn_seq = 0;
    st->lfn_checksum = 0;
}

static int add_lfn_entry(struct fat_scan_state *st, struct fat32_lfn_part *lfn)
{
    int seq = lfn->seq & 0x1F;
    int is_last = (lfn->seq & 0x40) ? 1 : 0;

    if (seq == 0 || seq > FAT32_MAX_LFN_ENTRIES)
    {
        reset_lfn(st);
        return -1;
    }

    if (is_last)
    {
        st->lfn_count = seq;
        st->lfn_seq = seq;
        st->lfn_checksum = lfn->checksum;
    }
    else
    {
        if (seq != st->lfn_seq - 1)
        {
            reset_lfn(st);
            return -1;
        }
        st->lfn_seq = seq;
    }

    if (seq > 0 && seq <= FAT32_MAX_LFN_ENTRIES)
        st->lfn_buf[seq - 1] = *lfn;

    return 0;
}

int fat32_read_fsinfo(struct fat32_fs *fs, uint32_t *free_clusters, uint32_t *next_free)
{
    uint32_t fsi_sector = fs->bpb.fs_info_sector;
    if (fsi_sector == 0) fsi_sector = 1;

    uint8_t buf[512];
    if (block_read(fs->bdev, fsi_sector, 1, buf) != 0)
        return -1;

    uint32_t sig1 = rd32(buf + 0);
    uint32_t sig2 = rd32(buf + 484);

    if (sig1 != 0x41615252 || sig2 != 0x61417272)
        return -1;

    if (free_clusters)
        *free_clusters = rd32(buf + 488);
    if (next_free)
        *next_free = rd32(buf + 492);

    return 0;
}

static int is_pow2(uint32_t v)
{
    return v && (v & (v - 1)) == 0;
}

int fat32_mount(struct block_device *bdev, struct fat32_fs *fs)
{
    if (!bdev || !fs)
        return -1;

    uint8_t buf[512];
    if (block_read(bdev, 0, 1, buf) != 0)
        return -1;

    struct fat32_bpb *bpb = (struct fat32_bpb *)buf;

    if (buf[510] != 0x55 || buf[511] != 0xAA)
        return -1;

    if (bpb->sectors_per_fat_32 == 0)
        return -1;

    if (bpb->bytes_per_sector != 512)
        return -1;

    if (!is_pow2(bpb->sectors_per_cluster) || bpb->sectors_per_cluster > 128)
        return -1;

    if (bpb->reserved_sectors == 0 || bpb->reserved_sectors > 1024)
        return -1;

    if (bpb->num_fats != 1 && bpb->num_fats != 2)
        return -1;

    if (bpb->root_cluster < 2)
        return -1;

    uint32_t total_sectors = bpb->total_sectors_32 ? bpb->total_sectors_32 : bpb->total_sectors_16;
    if (total_sectors == 0)
        return -1;

    uint32_t fat_end = bpb->reserved_sectors + (uint32_t)bpb->num_fats * bpb->sectors_per_fat_32;
    if (fat_end >= total_sectors)
        return -1;

    fs->bdev = bdev;
    __builtin_memcpy(&fs->bpb, buf, sizeof(struct fat32_bpb));

    fs->sectors_per_fat = bpb->sectors_per_fat_32;
    fs->fat_start = bpb->reserved_sectors;
    fs->data_start = fat_end;

    uint32_t data_sectors = total_sectors - fs->data_start;
    fs->total_clusters = data_sectors / bpb->sectors_per_cluster;

    return 0;
}

/* ── Directory sector scanning ── */

static int scan_sector(struct fat32_fs *fs, uint32_t sector,
                       uint32_t target_index, struct vfs_dentry *entry,
                       struct fat_scan_state *st)
{
    uint8_t buf[512];
    if (block_read(fs->bdev, sector, 1, buf) != 0)
        return -1;

    int entries_per_sector = 512 / 32;
    for (int i = 0; i < entries_per_sector; i++)
    {
        struct fat32_dent *de = (struct fat32_dent *)(buf + i * 32);
        uint8_t first = (uint8_t)de->name[0];

        if (first == 0x00)
            return -1;

        if (first == 0xE5)
        {
            reset_lfn(st);
            continue;
        }

        if (de->attr == ATTR_LFN)
        {
            add_lfn_entry(st, (struct fat32_lfn_part *)de);
            continue;
        }

        if (de->attr & ATTR_VOLUME_ID)
        {
            reset_lfn(st);
            continue;
        }

        int use_lfn = 0;
        if (st->lfn_count > 0 && st->lfn_seq == 1)
        {
            uint8_t csum = fat_checksum(de->name);
            use_lfn = (csum == st->lfn_checksum);
        }

        if (st->visible_count == (int)target_index)
        {
            uint32_t cluster = ((uint32_t)de->cluster_hi << 16) | de->cluster_lo;

            if (use_lfn)
                collect_lfn(st->lfn_buf, st->lfn_count, entry->name, VFS_NAME_MAX);
            else
                parse_short_name(de->name, entry->name, VFS_NAME_MAX);

            entry->ino  = cluster;
            entry->type = (de->attr & ATTR_DIRECTORY) ? VFS_DIR : VFS_FILE;
            entry->size = de->size;
            st->found = 1;
            return 0;
        }

        st->visible_count++;
        reset_lfn(st);
    }

    return 1;
}

int fat32_read_dir(struct fat32_fs *fs, uint32_t cluster, uint32_t index, struct vfs_dentry *entry)
{
    struct fat_scan_state st;
    st.visible_count = 0;
    st.found = 0;
    reset_lfn(&st);

    uint32_t cl = cluster;

    while (cl >= 2 && cl < FAT32_EOC)
    {
        uint32_t first_sector = fs->data_start + (cl - 2) * fs->bpb.sectors_per_cluster;
        uint32_t sectors_per_cluster = fs->bpb.sectors_per_cluster;

        for (uint32_t s = 0; s < sectors_per_cluster; s++)
        {
            int r = scan_sector(fs, first_sector + s, index, entry, &st);
            if (r == 0) return 0;
            if (r < 0) return (st.found) ? 0 : -1;
        }

        cl = fat_next_cluster(fs, cl);
    }
    return -1;
}

int fat32_lookup(struct fat32_fs *fs, uint32_t cluster, const char *name,
                 uint32_t *out_cluster, uint32_t *out_size, int *out_dir)
{
    if (!name || !name[0])
        return -1;

    struct fat_scan_state st;
    st.visible_count = 0;
    st.found = 0;
    reset_lfn(&st);

    uint32_t cl = cluster;

    while (cl >= 2 && cl < FAT32_EOC)
    {
        uint32_t first_sector = fs->data_start + (cl - 2) * fs->bpb.sectors_per_cluster;
        uint32_t sectors_per_cluster = fs->bpb.sectors_per_cluster;

        for (uint32_t s = 0; s < sectors_per_cluster; s++)
        {
            uint8_t buf[512];
            if (block_read(fs->bdev, first_sector + s, 1, buf) != 0)
                return -1;

            int entries = 512 / 32;
            for (int ei = 0; ei < entries; ei++)
            {
                struct fat32_dent *de = (struct fat32_dent *)(buf + ei * 32);
                uint8_t first = (uint8_t)de->name[0];

                if (first == 0x00)
                    return -1;

                if (first == 0xE5)
                {
                    reset_lfn(&st);
                    continue;
                }

                if (de->attr == ATTR_LFN)
                {
                    add_lfn_entry(&st, (struct fat32_lfn_part *)de);
                    continue;
                }

                if (de->attr & ATTR_VOLUME_ID)
                {
                    reset_lfn(&st);
                    continue;
                }

                int use_lfn = 0;
                if (st.lfn_count > 0 && st.lfn_seq == 1)
                {
                    uint8_t csum = fat_checksum(de->name);
                    use_lfn = (csum == st.lfn_checksum);
                }

                int match = 0;
                if (use_lfn)
                {
                    char lfn_name[VFS_NAME_MAX];
                    collect_lfn(st.lfn_buf, st.lfn_count, lfn_name, VFS_NAME_MAX);
                    if (strcmp_ci(name, lfn_name) == 0)
                        match = 1;
                }

                if (!match)
                {
                    char shortname[13];
                    parse_short_name(de->name, shortname, sizeof(shortname));
                    if (strcmp_ci(name, shortname) == 0)
                        match = 1;
                }

                if (match)
                {
                    *out_cluster = ((uint32_t)de->cluster_hi << 16) | de->cluster_lo;
                    *out_size    = de->size;
                    *out_dir     = (de->attr & ATTR_DIRECTORY) ? 1 : 0;
                    return 0;
                }

                reset_lfn(&st);
            }
        }

        cl = fat_next_cluster(fs, cl);
    }
    return -1;
}

int fat32_read_file(struct fat32_fs *fs, uint32_t cluster, uint32_t size,
                    uint64_t offset, uint64_t count, void *buf)
{
    if (count == 0) return 0;
    if (offset >= size) return 0;
    if (offset + count > size)
        count = size - offset;

    uint32_t cluster_size = fs->bpb.bytes_per_sector * fs->bpb.sectors_per_cluster;
    uint32_t start_cluster_idx = (uint32_t)(offset / cluster_size);
    uint32_t start_byte_in_cluster = offset % cluster_size;

    uint32_t cl = cluster;
    for (uint32_t i = 0; i < start_cluster_idx; i++)
    {
        cl = fat_next_cluster(fs, cl);
        if (cl < 2 || cl >= FAT32_EOC)
            return -1;
    }

    uint8_t tmp[512];
    uint64_t done = 0;
    uint64_t remaining = count;

    while (cl >= 2 && cl < FAT32_EOC && remaining > 0)
    {
        uint32_t first_sector = fs->data_start + (cl - 2) * fs->bpb.sectors_per_cluster;

        while (start_byte_in_cluster < cluster_size && remaining > 0)
        {
            uint32_t sector_in_cluster = start_byte_in_cluster / fs->bpb.bytes_per_sector;
            uint32_t byte_in_sector = start_byte_in_cluster % fs->bpb.bytes_per_sector;

            if (block_read(fs->bdev, first_sector + sector_in_cluster, 1, tmp) != 0)
                return (int)done;

            uint32_t chunk = fs->bpb.bytes_per_sector - byte_in_sector;
            if (chunk > remaining) chunk = (uint32_t)remaining;

            for (uint32_t i = 0; i < chunk; i++)
                ((uint8_t *)buf)[done++] = tmp[byte_in_sector + i];

            remaining -= chunk;
            start_byte_in_cluster += chunk;
        }

        start_byte_in_cluster = 0;
        cl = fat_next_cluster(fs, cl);
    }

    return (int)done;
}

/* ── File write / truncate ── */

int fat32_write_file(struct fat32_fs *fs, uint32_t *cluster, uint32_t *size,
                     uint64_t offset, uint64_t count, const void *buf)
{
    if (count == 0) return 0;

    uint32_t bps = fs->bpb.bytes_per_sector;
    uint32_t spc = fs->bpb.sectors_per_cluster;
    uint32_t cluster_size = bps * spc;
    uint64_t end = offset + count;

    /* grow file if needed */
    if (end > *size)
    {
        uint64_t needed_clusters = (end + cluster_size - 1) / cluster_size;
        uint32_t cur = *cluster;

        /* count current clusters */
        uint32_t cur_count = 0;
        if (cur >= 2 && cur < FAT32_EOC)
        {
            cur_count = 1;
            uint32_t next;
            while ((next = fat_next_cluster(fs, cur)) >= 2 && next < FAT32_EOC)
            {
                cur_count++;
                cur = next;
            }
        }

        /* allocate more clusters */
        while (cur_count < needed_clusters)
        {
            if (*cluster == 0 || *cluster >= FAT32_EOC)
            {
                *cluster = fat_alloc_cluster(fs);
                if (*cluster >= FAT32_EOC) return -1;
                cur_count++;
            }
            else
            {
                uint32_t new_cl = fat_extend_chain(fs, *cluster);
                if (new_cl >= FAT32_EOC) return -1;
                cur_count++;
            }
        }

        *size = (uint32_t)end;
    }

    /* write data */
    uint32_t start_cluster_idx = (uint32_t)(offset / cluster_size);
    uint32_t start_byte = offset % cluster_size;

    uint32_t cl = *cluster;
    for (uint32_t i = 0; i < start_cluster_idx; i++)
    {
        if (cl < 2 || cl >= FAT32_EOC) return -1;
        cl = fat_next_cluster(fs, cl);
    }

    uint64_t done = 0;
    uint64_t remaining = count;

    while (cl >= 2 && cl < FAT32_EOC && remaining > 0)
    {
        uint32_t first_sector = fs->data_start + (cl - 2) * spc;

        while (start_byte < cluster_size && remaining > 0)
        {
            uint32_t sector_in = start_byte / bps;
            uint32_t byte_in  = start_byte % bps;

            /* read-modify-write if not sector-aligned */
            if (byte_in != 0 || (remaining < bps && byte_in == 0))
            {
                uint8_t tmp[512];
                if (block_read(fs->bdev, first_sector + sector_in, 1, tmp) != 0)
                    return (int)done;

                uint32_t chunk = bps - byte_in;
                if (chunk > remaining) chunk = (uint32_t)remaining;

                for (uint32_t i = 0; i < chunk; i++)
                    tmp[byte_in + i] = ((const uint8_t *)buf)[done + i];

                if (block_write(fs->bdev, first_sector + sector_in, 1, tmp) != 0)
                    return (int)done;

                done += chunk;
                remaining -= chunk;
                start_byte += chunk;
            }
            else
            {
                uint8_t cnt = (uint8_t)(remaining / bps);
                if (cnt > 1) cnt = 1;
                if (cnt == 0) cnt = 1;

                uint32_t wsize = cnt * bps;
                if (wsize > remaining) wsize = (uint32_t)remaining;

                if (block_write(fs->bdev, first_sector + sector_in, cnt, &((const uint8_t *)buf)[done]) != 0)
                    return (int)done;

                done += wsize;
                remaining -= wsize;
                start_byte += wsize;
            }
        }

        start_byte = 0;
        cl = fat_next_cluster(fs, cl);
    }

    return (int)done;
}

int fat32_truncate(struct fat32_fs *fs, uint32_t *cluster, uint32_t *size, uint64_t new_size)
{
    if (new_size >= *size) return 0;

    uint32_t bps = fs->bpb.bytes_per_sector;
    uint32_t spc = fs->bpb.sectors_per_cluster;
    uint32_t cluster_size = bps * spc;

    if (new_size == 0)
    {
        fat_free_chain(fs, *cluster);
        *cluster = 0;
        *size = 0;
        return 0;
    }

    uint32_t keep_clusters = (uint32_t)((new_size + cluster_size - 1) / cluster_size);
    uint32_t cl = *cluster;

    /* walk to the keep point */
    uint32_t prev = 0;
    for (uint32_t i = 0; i < keep_clusters; i++)
    {
        if (cl < 2 || cl >= FAT32_EOC) return -1;
        prev = cl;
        cl = fat_next_cluster(fs, cl);
    }

    /* free the rest */
    fat_free_chain(fs, cl);

    /* mark prev as EOC */
    if (prev >= 2 && prev < FAT32_EOC)
        fat_set_cluster(fs, prev, FAT32_EOC);

    *size = (uint32_t)new_size;
    return 0;
}

/* ── Directory entry find / update ── */

/* find a directory entry by its cluster number */
static int find_entry_by_cluster(struct fat32_fs *fs, uint32_t dir_cluster,
                                 uint32_t target_cluster,
                                 uint32_t *out_sector, uint32_t *out_offset)
{
    uint32_t cl = dir_cluster;

    while (cl >= 2 && cl < FAT32_EOC)
    {
        uint32_t first_sector = fs->data_start + (cl - 2) * fs->bpb.sectors_per_cluster;

        for (uint32_t s = 0; s < fs->bpb.sectors_per_cluster; s++)
        {
            uint8_t buf[512];
            if (block_read(fs->bdev, first_sector + s, 1, buf) != 0)
                continue;

            for (int ei = 0; ei < 512 / 32; ei++)
            {
                struct fat32_dent *de = (struct fat32_dent *)(buf + ei * 32);
                uint8_t first = (uint8_t)de->name[0];
                if (first == 0x00 || first == 0xE5) continue;
                if (de->attr == ATTR_LFN || (de->attr & ATTR_VOLUME_ID)) continue;

                uint32_t ec = ((uint32_t)de->cluster_hi << 16) | de->cluster_lo;
                if (ec == target_cluster)
                {
                    *out_sector = first_sector + s;
                    *out_offset = ei * 32;
                    return 0;
                }
            }
        }

        cl = fat_next_cluster(fs, cl);
    }
    return -1;
}

static int update_entry_size(struct fat32_fs *fs, uint32_t dir_cluster,
                             uint32_t file_cluster, uint32_t new_size)
{
    uint32_t sector, offset;
    if (find_entry_by_cluster(fs, dir_cluster, file_cluster, &sector, &offset) != 0)
        return -1;

    uint8_t buf[512];
    if (block_read(fs->bdev, sector, 1, buf) != 0)
        return -1;

    struct fat32_dent *de = (struct fat32_dent *)(buf + offset);
    de->size = new_size;

    return block_write(fs->bdev, sector, 1, buf);
}

static int update_entry_cluster(struct fat32_fs *fs, uint32_t dir_cluster,
                                uint32_t old_cluster, uint32_t new_cluster)
{
    uint32_t sector, offset;
    if (find_entry_by_cluster(fs, dir_cluster, old_cluster, &sector, &offset) != 0)
        return -1;

    uint8_t buf[512];
    if (block_read(fs->bdev, sector, 1, buf) != 0)
        return -1;

    struct fat32_dent *de = (struct fat32_dent *)(buf + offset);
    de->cluster_lo = new_cluster & 0xFFFF;
    de->cluster_hi = (new_cluster >> 16) & 0xFFFF;

    return block_write(fs->bdev, sector, 1, buf);
}

/* ── Directory entry creation / deletion ── */

static int dir_write_entries(struct fat32_fs *fs, uint32_t dir_cluster,
                             int lfn_count, struct fat32_lfn_part *lfns,
                             struct fat32_dent *entry,
                             uint8_t checksum)
{
    int total_slots = lfn_count + 1;
    int slots_found = 0;

    /* absolute entry index where the free run starts */
    int run_start = -1;
    uint32_t run_base_sector = 0;

    uint32_t cl = dir_cluster;
    int abs_entry = 0;

    while (cl >= 2 && cl < FAT32_EOC)
    {
        uint32_t first_sector = fs->data_start + (cl - 2) * fs->bpb.sectors_per_cluster;

        for (uint32_t s = 0; s < fs->bpb.sectors_per_cluster; s++)
        {
            uint8_t buf[512];
            if (block_read(fs->bdev, first_sector + s, 1, buf) != 0)
                return -1;

            for (int ei = 0; ei < 512 / 32; ei++, abs_entry++)
            {
                struct fat32_dent *de = (struct fat32_dent *)(buf + ei * 32);
                uint8_t first = (uint8_t)de->name[0];

                if (first == 0x00 || first == 0xE5)
                {
                    if (slots_found == 0)
                    {
                        run_start = abs_entry;
                        run_base_sector = first_sector;
                    }
                    slots_found++;
                    if (slots_found >= total_slots)
                    {
                        /* write LFN entries (last LFN first, then backwards) */
                        for (int li = lfn_count - 1; li >= 0; li--)
                        {
                            int ae = run_start + (lfn_count - 1 - li);
                            int sec_off = ae / 16;
                            int ent_off = ae % 16;
                            uint32_t ws = run_base_sector + sec_off;

                            uint8_t wbuf[512];
                            if (block_read(fs->bdev, ws, 1, wbuf) != 0)
                                return -1;

                            struct fat32_lfn_part *lp = (struct fat32_lfn_part *)(wbuf + ent_off * 32);
                            *lp = lfns[li];
                            if (li == lfn_count - 1)
                                lp->seq |= 0x40;
                            lp->checksum = checksum;

                            if (block_write(fs->bdev, ws, 1, wbuf) != 0)
                                return -1;
                        }

                        /* write the short name entry (last slot) */
                        {
                            int ae = run_start + total_slots - 1;
                            int sec_off = ae / 16;
                            int ent_off = ae % 16;
                            uint32_t ws = run_base_sector + sec_off;

                            uint8_t wbuf[512];
                            if (block_read(fs->bdev, ws, 1, wbuf) != 0)
                                return -1;

                            struct fat32_dent *de2 = (struct fat32_dent *)(wbuf + ent_off * 32);
                            __builtin_memcpy(de2, entry, sizeof(struct fat32_dent));

                            if (block_write(fs->bdev, ws, 1, wbuf) != 0)
                                return -1;
                        }

                        return 0;
                    }
                }
                else
                {
                    slots_found = 0;
                }
            }
        }

        cl = fat_next_cluster(fs, cl);
    }

    return -1;
}

static int build_lfn_entries(const char *name, int name_len,
                             struct fat32_lfn_part *lfns, int *out_count)
{
    int total_chars = name_len;
    int entries_needed = (total_chars + 12) / 13;
    if (entries_needed > FAT32_MAX_LFN_ENTRIES)
        entries_needed = FAT32_MAX_LFN_ENTRIES;

    for (int i = 0; i < entries_needed; i++)
    {
        int base = i * 13;
        int remain = total_chars - base;
        if (remain > 13) remain = 13;

        uint16_t buf[13];

        for (int j = 0; j < remain; j++)
            buf[j] = (uint8_t)name[base + j];
        for (int j = remain; j < 13; j++)
            buf[j] = 0xFFFF;

        lfns[i].seq = (i + 1);
        for (int j = 0; j < 5; j++) lfns[i].name1[j] = buf[j];
        lfns[i].attr = ATTR_LFN;
        lfns[i].type = 0;
        lfns[i].checksum = 0;
        for (int j = 0; j < 6; j++) lfns[i].name2[j] = buf[5 + j];
        lfns[i].reserved = 0;
        for (int j = 0; j < 2; j++) lfns[i].name3[j] = buf[11 + j];
    }

    *out_count = entries_needed;
    return 0;
}

int fat32_create_file(struct fat32_fs *fs, uint32_t dir_cluster,
                      const char *name, uint32_t *out_cluster)
{
    if (!name || !name[0])
        return -1;

    /* check if file already exists */
    uint32_t dummy_cluster, dummy_size;
    int dummy_dir;
    if (fat32_lookup(fs, dir_cluster, name, &dummy_cluster, &dummy_size, &dummy_dir) == 0)
        return -1;

    int name_len = 0;
    while (name[name_len]) name_len++;

    /* generate short name */
    char name11[11];
    make_short_name(name, name11);
    uint8_t checksum = fat_checksum(name11);

    /* build LFN entries */
    struct fat32_lfn_part lfns[FAT32_MAX_LFN_ENTRIES];
    int lfn_count = 0;
    build_lfn_entries(name, name_len, lfns, &lfn_count);

    /* allocate first cluster (0 means empty file) */
    uint32_t first_cluster = fat_alloc_cluster(fs);
    if (first_cluster >= FAT32_EOC)
        return -1;

    /* zero out the new cluster */
    uint8_t zero[512];
    for (int i = 0; i < 512; i++) zero[i] = 0;
    uint32_t first_sector = fs->data_start + (first_cluster - 2) * fs->bpb.sectors_per_cluster;
    for (uint32_t s = 0; s < fs->bpb.sectors_per_cluster; s++)
        block_write(fs->bdev, first_sector + s, 1, zero);

    /* build the directory entry */
    struct fat32_dent dent;
    __builtin_memcpy(dent.name, name11, 11);
    dent.attr = ATTR_ARCHIVE;
    dent.nt_res = 0;
    dent.ctime_tenth = 0;
    dent.ctime = 0;
    dent.cdate = 0;
    dent.adate = 0;
    dent.cluster_hi = (first_cluster >> 16) & 0xFFFF;
    dent.wtime = 0;
    dent.wdate = 0;
    dent.cluster_lo = first_cluster & 0xFFFF;
    dent.size = 0;

    /* write entries to directory */
    if (dir_write_entries(fs, dir_cluster, lfn_count, lfns, &dent, checksum) != 0)
    {
        fat_free_chain(fs, first_cluster);
        return -1;
    }

    if (out_cluster)
        *out_cluster = first_cluster;

    return 0;
}

int fat32_unlink_file(struct fat32_fs *fs, uint32_t dir_cluster, const char *name)
{
    if (!name || !name[0]) return -1;

    uint32_t file_cluster, file_size;
    int file_is_dir;
    if (fat32_lookup(fs, dir_cluster, name, &file_cluster, &file_size, &file_is_dir) != 0)
        return -1;

    if (file_is_dir)
    {
        /* check if directory is empty (only . and ..) */
        struct vfs_dentry de;
        int count = 0;
        for (int i = 0; fat32_read_dir(fs, file_cluster, i, &de) == 0; i++)
        {
            if (de.name[0] == '.' && (de.name[1] == '\0' || (de.name[1] == '.' && de.name[2] == '\0')))
                continue;
            count++;
        }
        if (count > 0)
            return -1;
    }

    /* free clusters */
    if (file_cluster >= 2 && file_cluster < FAT32_EOC)
        fat_free_chain(fs, file_cluster);

    /* mark directory entries as free */
    uint32_t cl = dir_cluster;
    while (cl >= 2 && cl < FAT32_EOC)
    {
        uint32_t first_sector = fs->data_start + (cl - 2) * fs->bpb.sectors_per_cluster;

        for (uint32_t s = 0; s < fs->bpb.sectors_per_cluster; s++)
        {
            uint8_t buf[512];
            if (block_read(fs->bdev, first_sector + s, 1, buf) != 0)
                continue;

            int entries = 512 / 32;
            for (int ei = 0; ei < entries; ei++)
            {
                struct fat32_dent *de = (struct fat32_dent *)(buf + ei * 32);
                uint8_t first = (uint8_t)de->name[0];
                if (first == 0x00) return 0;
                if (first == 0xE5) continue;

                if (de->attr == ATTR_LFN)
                {
                    de->name[0] = 0xE5;
                    block_write(fs->bdev, first_sector + s, 1, buf);
                    continue;
                }

                if (de->attr & ATTR_VOLUME_ID) continue;

                uint32_t ec = ((uint32_t)de->cluster_hi << 16) | de->cluster_lo;
                if (ec == file_cluster)
                {
                    de->name[0] = 0xE5;
                    block_write(fs->bdev, first_sector + s, 1, buf);
                    return 0;
                }

                /* also check LFN checksum match */
                /* skip - already handled above by matching cluster */
            }
        }

        cl = fat_next_cluster(fs, cl);
    }

    return -1;
}

/* ── mkdir ── */

int fat32_mkdir(struct fat32_fs *fs, uint32_t dir_cluster,
                const char *name, uint32_t *out_cluster)
{
    if (!name || !name[0]) return -1;

    /* check not existing */
    uint32_t dummy;
    int dummy_dir;
    if (fat32_lookup(fs, dir_cluster, name, &dummy, &dummy, &dummy_dir) == 0)
        return -1;

    /* allocate cluster */
    uint32_t cl = fat_alloc_cluster(fs);
    if (cl >= FAT32_EOC) return -1;

    /* zero it */
    uint8_t zero[512];
    for (int i = 0; i < 512; i++) zero[i] = 0;
    uint32_t first_sector = fs->data_start + (cl - 2) * fs->bpb.sectors_per_cluster;
    for (uint32_t s = 0; s < fs->bpb.sectors_per_cluster; s++)
        block_write(fs->bdev, first_sector + s, 1, zero);

    /* write "." entry */
    {
        uint8_t buf[512];
        block_read(fs->bdev, first_sector, 1, buf);
        struct fat32_dent *dot = (struct fat32_dent *)buf;
        __builtin_memcpy(dot->name, ".          ", 11);
        dot->attr = ATTR_DIRECTORY;
        dot->cluster_lo = cl & 0xFFFF;
        dot->cluster_hi = (cl >> 16) & 0xFFFF;
        dot->size = 0;
        block_write(fs->bdev, first_sector, 1, buf);
    }

    /* write ".." entry */
    {
        uint8_t buf[512];
        block_read(fs->bdev, first_sector, 1, buf);
        struct fat32_dent *dotdot = (struct fat32_dent *)(buf + 32);
        __builtin_memcpy(dotdot->name, "..         ", 11);
        dotdot->attr = ATTR_DIRECTORY;
        dotdot->cluster_lo = dir_cluster & 0xFFFF;
        dotdot->cluster_hi = (dir_cluster >> 16) & 0xFFFF;
        dotdot->size = 0;
        block_write(fs->bdev, first_sector, 1, buf);
    }

    /* create parent directory entry */
    int name_len = 0;
    while (name[name_len]) name_len++;

    char name11[11];
    make_short_name(name, name11);
    uint8_t checksum = fat_checksum(name11);

    struct fat32_lfn_part lfns[FAT32_MAX_LFN_ENTRIES];
    int lfn_count = 0;
    build_lfn_entries(name, name_len, lfns, &lfn_count);

    struct fat32_dent dent;
    __builtin_memcpy(dent.name, name11, 11);
    dent.attr = ATTR_DIRECTORY;
    dent.nt_res = 0;
    dent.ctime_tenth = 0;
    dent.ctime = 0;
    dent.cdate = 0;
    dent.adate = 0;
    dent.cluster_hi = (cl >> 16) & 0xFFFF;
    dent.wtime = 0;
    dent.wdate = 0;
    dent.cluster_lo = cl & 0xFFFF;
    dent.size = 0;

    if (dir_write_entries(fs, dir_cluster, lfn_count, lfns, &dent, checksum) != 0)
    {
        fat_free_chain(fs, cl);
        return -1;
    }

    if (out_cluster)
        *out_cluster = cl;

    return 0;
}

/* ── VFS integration ── */

struct fat32_inode_private {
    uint32_t cluster;
    uint32_t size;
    int      is_dir;
    uint32_t parent_cluster;
};

static struct fat32_fs *fat32_sb_to_fs(struct vfs_superblock *sb)
{
    return (struct fat32_fs *)sb->private;
}

static int fat32_vfs_readdir(struct vfs_inode *dir, uint32_t index, struct vfs_dentry *entry)
{
    struct fat32_inode_private *priv = (struct fat32_inode_private *)dir->private;
    struct fat32_fs *fs = fat32_sb_to_fs(dir->sb);

    if (!priv || !fs)
        return -1;
    if (!priv->is_dir)
        return -1;

    return fat32_read_dir(fs, priv->cluster, index, entry);
}

static int fat32_vfs_lookup(struct vfs_inode *dir, const char *name, struct vfs_inode **child)
{
    struct fat32_inode_private *priv = (struct fat32_inode_private *)dir->private;
    struct fat32_fs *fs = fat32_sb_to_fs(dir->sb);

    if (!priv || !fs)
        return -1;
    if (!priv->is_dir)
        return -1;

    uint32_t child_cluster, child_size;
    int child_is_dir;

    if (fat32_lookup(fs, priv->cluster, name, &child_cluster, &child_size, &child_is_dir) != 0)
        return -1;

    struct vfs_inode *inode = dir->sb->sb_ops->alloc_inode(dir->sb);
    if (!inode) return -1;

    struct fat32_inode_private *child_priv = (struct fat32_inode_private *)kmalloc(sizeof(struct fat32_inode_private));
    if (!child_priv)
    {
        dir->sb->sb_ops->destroy_inode(inode);
        return -1;
    }

    child_priv->cluster = child_cluster;
    child_priv->size    = child_size;
    child_priv->is_dir  = child_is_dir;
    child_priv->parent_cluster = priv->cluster;

    inode->ino     = child_cluster;
    inode->type    = child_is_dir ? VFS_DIR : VFS_FILE;
    inode->size    = child_size;
    inode->sb      = dir->sb;
    inode->ops     = dir->ops;
    inode->private = child_priv;
    inode->refcount = 1;

    *child = inode;
    return 0;
}

static int fat32_vfs_read(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf)
{
    struct fat32_inode_private *priv = (struct fat32_inode_private *)inode->private;
    struct fat32_fs *fs = fat32_sb_to_fs(inode->sb);

    if (!priv || !fs)
        return -1;
    if (priv->is_dir)
        return -1;

    return fat32_read_file(fs, priv->cluster, priv->size, offset, size, buf);
}

static int fat32_vfs_write(struct vfs_inode *inode, uint64_t offset, uint64_t size, const void *buf)
{
    struct fat32_inode_private *priv = (struct fat32_inode_private *)inode->private;
    struct fat32_fs *fs = fat32_sb_to_fs(inode->sb);

    if (!priv || !fs)
        return -1;
    if (priv->is_dir)
        return -1;

    uint32_t old_size = priv->size;

    int r = fat32_write_file(fs, &priv->cluster, &priv->size, offset, size, buf);
    if (r < 0) return r;

    /* update inode metadata */
    inode->size = priv->size;

    /* if size changed, update directory entry on disk */
    if (priv->size != old_size)
    {
        if (priv->cluster != inode->ino)
        {
            /* first cluster changed (was 0, now allocated) */
            inode->ino = priv->cluster;
            if (priv->parent_cluster)
                update_entry_cluster(fs, priv->parent_cluster, 0, priv->cluster);
        }

        if (priv->parent_cluster)
            update_entry_size(fs, priv->parent_cluster, priv->cluster, priv->size);
    }

    return r;
}

static int fat32_vfs_truncate(struct vfs_inode *inode, uint64_t size)
{
    struct fat32_inode_private *priv = (struct fat32_inode_private *)inode->private;
    struct fat32_fs *fs = fat32_sb_to_fs(inode->sb);

    if (!priv || !fs)
        return -1;

    if (fat32_truncate(fs, &priv->cluster, &priv->size, size) != 0)
        return -1;

    inode->size = priv->size;
    inode->ino  = priv->cluster ? priv->cluster : 0;

    if (priv->parent_cluster)
        update_entry_size(fs, priv->parent_cluster, priv->cluster, priv->size);

    return 0;
}

static int fat32_vfs_create(struct vfs_inode *dir, const char *name, struct vfs_inode **child)
{
    struct fat32_inode_private *priv = (struct fat32_inode_private *)dir->private;
    struct fat32_fs *fs = fat32_sb_to_fs(dir->sb);

    if (!priv || !fs || !priv->is_dir)
        return -1;

    uint32_t new_cluster;
    if (fat32_create_file(fs, priv->cluster, name, &new_cluster) != 0)
        return -1;

    if (child)
    {
        struct vfs_inode *inode = dir->sb->sb_ops->alloc_inode(dir->sb);
        if (!inode) return 0;

        struct fat32_inode_private *cp = (struct fat32_inode_private *)kmalloc(sizeof(struct fat32_inode_private));
        if (!cp) { dir->sb->sb_ops->destroy_inode(inode); return 0; }

        cp->cluster = new_cluster;
        cp->size    = 0;
        cp->is_dir  = 0;
        cp->parent_cluster = priv->cluster;

        inode->ino      = new_cluster;
        inode->type     = VFS_FILE;
        inode->size     = 0;
        inode->sb       = dir->sb;
        inode->ops      = dir->ops;
        inode->private  = cp;
        inode->refcount = 1;

        *child = inode;
    }

    return 0;
}

static int fat32_vfs_unlink(struct vfs_inode *dir, const char *name)
{
    struct fat32_inode_private *priv = (struct fat32_inode_private *)dir->private;
    struct fat32_fs *fs = fat32_sb_to_fs(dir->sb);

    if (!priv || !fs || !priv->is_dir)
        return -1;

    return fat32_unlink_file(fs, priv->cluster, name);
}

static int fat32_vfs_mkdir(struct vfs_inode *dir, const char *name)
{
    struct fat32_inode_private *priv = (struct fat32_inode_private *)dir->private;
    struct fat32_fs *fs = fat32_sb_to_fs(dir->sb);

    if (!priv || !fs || !priv->is_dir)
        return -1;

    uint32_t new_cluster;
    return fat32_mkdir(fs, priv->cluster, name, &new_cluster);
}

static struct vfs_inode_ops fat32_inode_ops = {
    .read     = fat32_vfs_read,
    .write    = fat32_vfs_write,
    .truncate = fat32_vfs_truncate,
    .readdir  = fat32_vfs_readdir,
    .lookup   = fat32_vfs_lookup,
    .create   = fat32_vfs_create,
    .unlink   = fat32_vfs_unlink,
    .mkdir    = fat32_vfs_mkdir,
};

static struct vfs_inode *fat32_alloc_inode(struct vfs_superblock *sb)
{
    (void)sb;
    return (struct vfs_inode *)kmalloc(sizeof(struct vfs_inode));
}

static void fat32_destroy_inode(struct vfs_inode *inode)
{
    if (!inode) return;
    if (inode->private)
        kfree(inode->private);
    kfree(inode);
}

static struct vfs_inode *fat32_get_root(struct vfs_superblock *sb)
{
    return sb->root;
}

static struct vfs_sb_ops fat32_sb_ops = {
    .alloc_inode   = fat32_alloc_inode,
    .destroy_inode = fat32_destroy_inode,
    .get_root      = fat32_get_root,
};

int vfs_mount_fat32(const char *path, struct block_device *bdev)
{
    if (!path || !bdev)
        return -1;

    struct fat32_fs *fs = (struct fat32_fs *)kmalloc(sizeof(struct fat32_fs));
    if (!fs) return -1;

    if (fat32_mount(bdev, fs) != 0)
    {
        kfree(fs);
        return -1;
    }

    uint32_t free_clusters = 0, next_free = 0;
    if (fat32_read_fsinfo(fs, &free_clusters, &next_free) == 0)
        serial_printf("fat32: FSInfo free_clusters=%u next_free=%u\n", free_clusters, next_free);
    else
        serial_printf("fat32: no FSInfo\n");

    serial_printf("fat32: mounted, BPB: bps=%d spc=%d reserved=%d fats=%d spf=%d root_cluster=%d\n",
                  fs->bpb.bytes_per_sector, fs->bpb.sectors_per_cluster,
                  fs->bpb.reserved_sectors, fs->bpb.num_fats,
                  fs->sectors_per_fat, fs->bpb.root_cluster);

    struct fat32_inode_private *root_priv = (struct fat32_inode_private *)kmalloc(sizeof(struct fat32_inode_private));
    if (!root_priv)
    {
        kfree(fs);
        return -1;
    }

    root_priv->cluster = fs->bpb.root_cluster;
    root_priv->size    = 0;
    root_priv->is_dir  = 1;
    root_priv->parent_cluster = fs->bpb.root_cluster;

    struct vfs_inode *root_inode = fat32_alloc_inode(NULL);
    if (!root_inode)
    {
        kfree(root_priv);
        kfree(fs);
        return -1;
    }

    root_inode->ino      = fs->bpb.root_cluster;
    root_inode->type     = VFS_DIR;
    root_inode->size     = 0;
    root_inode->ops      = &fat32_inode_ops;
    root_inode->private  = root_priv;
    root_inode->refcount = 1;

    struct vfs_superblock *sb = (struct vfs_superblock *)kmalloc(sizeof(struct vfs_superblock));
    if (!sb)
    {
        kfree(root_priv);
        kfree(root_inode);
        kfree(fs);
        return -1;
    }

    root_inode->sb = sb;

    sb->mounted  = 1;
    sb->sb_ops   = &fat32_sb_ops;
    sb->root     = root_inode;
    sb->private  = fs;

    return vfs_mount(path, sb);
}
