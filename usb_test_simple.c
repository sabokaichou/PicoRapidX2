// Simple USB MSC test - exact copy of picomemory main.c for testing
#include "bsp/board.h"
#include "tusb.h"
#include <string.h>

#define DISK_SECTOR_SIZE 512
#define DISK_SECTOR_COUNT 16
static uint8_t ram_disk[DISK_SECTOR_SIZE * DISK_SECTOR_COUNT];

static void build_fat12_image(void) {
    // Clear
    for (uint i = 0; i < sizeof(ram_disk); ++i) ram_disk[i] = 0;

    // Boot sector (BPB)
    uint8_t *b = ram_disk;
    memset(b, 0, DISK_SECTOR_SIZE);
    b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90; // JMP short
    memcpy(&b[3], "MSDOS5.0", 8);
    uint16_t bytes_per_sector = DISK_SECTOR_SIZE; memcpy(&b[11], &bytes_per_sector, 2);
    b[13] = 1; // sectors per cluster
    uint16_t reserved = 1; memcpy(&b[14], &reserved, 2);
    b[16] = 2; // number of FATs
    uint16_t root_entries = 16; memcpy(&b[17], &root_entries, 2);
    uint16_t total_sectors = DISK_SECTOR_COUNT; memcpy(&b[19], &total_sectors, 2);
    b[21] = 0xF8; // media
    uint16_t sectors_per_fat = 1; memcpy(&b[22], &sectors_per_fat, 2);
    uint16_t sectors_per_track = 1; memcpy(&b[24], &sectors_per_track, 2);
    uint16_t heads = 1; memcpy(&b[26], &heads, 2);
    b[36] = 0x29; // extended boot sig
    memcpy(&b[43], "NO NAME    ", 8);
    memcpy(&b[54], "FAT12   ", 8);

    // FAT12 tables (two copies)
    uint8_t *fat1 = ram_disk + DISK_SECTOR_SIZE * 1;
    uint8_t *fat2 = ram_disk + DISK_SECTOR_SIZE * 2;
    memset(fat1, 0, DISK_SECTOR_SIZE);
    memset(fat2, 0, DISK_SECTOR_SIZE);
    fat1[0] = 0xF8; fat1[1] = 0xFF; fat1[2] = 0xFF;
    fat1[3] = 0xFF; fat1[4] = 0x0F;
    memcpy(fat2, fat1, DISK_SECTOR_SIZE);

    // Root directory entry
    uint8_t *root = ram_disk + DISK_SECTOR_SIZE * 3;
    memset(root, 0, DISK_SECTOR_SIZE);
    memcpy(root + 0, "TEST    TXT", 11);
    root[11] = 0x20; // archive attribute
    uint16_t start_cluster = 2; memcpy(root + 26, &start_cluster, 2);
    uint32_t fsize = 24; memcpy(root + 28, &fsize, 4);
    
    // Data for cluster 2 -> sector 4
    char *text_buffer = (char *)(ram_disk + DISK_SECTOR_SIZE * 4);
    strcpy(text_buffer, "USB MSC Test Working!\r\n");
}

// TinyUSB Callbacks
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;
    memcpy(vendor_id,  "PICO   ", 8);
    const char prod[] = "RAM DISK MSC";
    memset(product_id, ' ', 16);
    memcpy(product_id, prod, sizeof(prod)-1);
    memcpy(product_rev, "1.0", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void) lun;
    *block_count = DISK_SECTOR_COUNT;
    *block_size  = DISK_SECTOR_SIZE;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void) lun; (void) power_condition; (void) start; (void) load_eject; 
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void) lun;
    if (lba >= DISK_SECTOR_COUNT) return -1;
    uint32_t available = DISK_SECTOR_SIZE - offset;
    if (bufsize > available) bufsize = available;
    memcpy(buffer, ram_disk + lba * DISK_SECTOR_SIZE + offset, bufsize);
    return (int32_t) bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void) lun;
    if (lba != 4) {
        return (int32_t) bufsize;
    }
    if (offset >= DISK_SECTOR_SIZE) return 0;
    uint32_t space = DISK_SECTOR_SIZE - offset;
    uint32_t to_copy = bufsize < space ? bufsize : space;
    memcpy(ram_disk + DISK_SECTOR_SIZE * lba + offset, buffer, to_copy);
    return (int32_t) to_copy;
}

bool tud_msc_is_writable_cb (uint8_t lun) {
    (void) lun; 
    return true;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
    return 0;
}

int main(void) {
    board_init();
    build_fat12_image();
    tud_init(0);

    while (true) {
        tud_task();
        tight_loop_contents();
    }
    return 0;
}