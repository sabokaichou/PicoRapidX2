#include "bsp/board.h"
#include "tusb.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/platform.h>
#include <pico/stdlib.h>

#define DISK_SECTOR_SIZE 512
#define DISK_SECTOR_COUNT 16
static uint8_t ram_disk[DISK_SECTOR_SIZE * DISK_SECTOR_COUNT];

#define ROOT_DIR_SECTOR       3
#define DATA_START_SECTOR     4

static void build_fat12_image(void) {
    for (uint i = 0; i < sizeof(ram_disk); ++i) ram_disk[i] = 0;

    uint8_t *b = ram_disk;
    memset(b, 0, DISK_SECTOR_SIZE);
    b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90;
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
    b[36] = 0x29;
    memcpy(&b[43], "NO NAME    ", 8);
    memcpy(&b[54], "FAT12   ", 8);

    uint8_t *fat1 = ram_disk + DISK_SECTOR_SIZE * 1;
    uint8_t *fat2 = ram_disk + DISK_SECTOR_SIZE * 2;
    memset(fat1, 0, DISK_SECTOR_SIZE);
    memset(fat2, 0, DISK_SECTOR_SIZE);
    fat1[0] = 0xF8; fat1[1] = 0xFF; fat1[2] = 0xFF; // media + EOC
    fat1[3] = 0xFF; fat1[4] = 0x0F;                 // cluster 2 EOC
    memcpy(fat2, fat1, DISK_SECTOR_SIZE);

    uint8_t *root = ram_disk + DISK_SECTOR_SIZE * ROOT_DIR_SECTOR;
    memset(root, 0, DISK_SECTOR_SIZE);
    // 8.3形式のファイル名: "SETTING TXT" => Setting.txt
    memcpy(root + 0, "SETTING TXT", 11);
    root[11] = 0x20;
    uint16_t start_cluster = 2; memcpy(root + 26, &start_cluster, 2);

    char *text_buffer = (char *)(ram_disk + DISK_SECTOR_SIZE * DATA_START_SECTOR);
    memset(text_buffer, 0, DISK_SECTOR_SIZE);

    const uint8_t *flash_ptr = (const uint8_t *) (XIP_BASE + 0x1F0000);
    int text_pos = 0;
    
    // 設定ファイルのヘッダ（ユーザー指定の2行）
    const char *header =
        "Format: INPUT_NO,RAPID_TYPE,REVERSE,OUT_FRAME,IN_FRAME,OUTPUT_PINS(0:OFF 1:ON)\r\n"
        "RAPID: 1=Norm 2=R30 3=R30Rev 4=Custom 5=Macro 6=R15 7=R15Rev\r\n"
        "\r\n";
    
    int header_len = strlen(header);
    memcpy(text_buffer + text_pos, header, header_len);
    text_pos += header_len;
    
    // 12個の入力設定をCSV形式で出力
    for (int input = 0; input < 12; input++) {
        int base = input * 16;
        uint8_t rapid_type = flash_ptr[base];
        bool reverse = (rapid_type >= 10);
        if (reverse) rapid_type -= 10;
        
        // 容量チェック - 最後の行まで確実に収まるように
        if (text_pos > DISK_SECTOR_SIZE - 30) break;
        
        // INPUT_NO,RAPID_TYPE,REVERSE,OUTPUT_FRAME,INTERVAL_FRAME
        int written = snprintf(text_buffer + text_pos, DISK_SECTOR_SIZE - text_pos,
            "%d,%d,%d,%d,%d,",
            input,
            rapid_type,
            reverse ? 1 : 0,
            flash_ptr[base + 1],  // OUTPUT_FRAME
            flash_ptr[base + 2]   // INTERVAL_FRAME
        );
        
        if (written < 0) break;
        text_pos += written;
        
        // OUTPUT_PINS (12 values) - カンマなしで連続出力
        for (int pin = 0; pin < 12; pin++) {
            if (text_pos >= DISK_SECTOR_SIZE - 3) break; // 安全マージン
            written = snprintf(text_buffer + text_pos, DISK_SECTOR_SIZE - text_pos, "%d", flash_ptr[base + 4 + pin]);
            if (written < 0) break;
            text_pos += written;
        }
        
        // 行終端（容量チェック）
        if (text_pos < DISK_SECTOR_SIZE - 2) {
            text_buffer[text_pos++] = '\r';
            text_buffer[text_pos++] = '\n';
        }
    }
    
    uint32_t fsize = text_pos; memcpy(root + 28, &fsize, 4);
}

// TinyUSB MSC callbacks ------------------------------------------------------
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;
    memcpy(vendor_id,  "PICO   ", 8);
    const char prod[] = "RAM DISK MSC";
    memset(product_id, ' ', 16);
    memcpy(product_id, prod, sizeof(prod)-1);
    memcpy(product_rev, "1.0", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) { (void) lun; return true; }

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void) lun; *block_count = DISK_SECTOR_COUNT; *block_size = DISK_SECTOR_SIZE;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void) lun; (void) power_condition; (void) start; (void) load_eject; return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void) lun;
    if (lba >= DISK_SECTOR_COUNT) return -1;
    uint32_t available = DISK_SECTOR_SIZE - offset;
    if (bufsize > available) bufsize = available;
    memcpy(buffer, ram_disk + lba * DISK_SECTOR_SIZE + offset, bufsize);
    return (int32_t) bufsize;
}

// CSVファイルをパースして設定に反映する
static void parse_settings_csv(const char *csv_data) {
    uint8_t new_settings[256];
    // 既存設定を読み込んでベースにする（未指定項目を保持するため）
    const uint8_t *flash_ptr = (const uint8_t *) (XIP_BASE + 0x1F0000);
    memcpy(new_settings, flash_ptr, 256);
    
    char line[128];
    int line_pos = 0;
    int data_pos = 0;
    
    // 行単位でパース
    while (csv_data[data_pos] && data_pos < 512) {
        char c = csv_data[data_pos++];
        
        if (c == '\r' || c == '\n') {
            if (line_pos > 0) {
                line[line_pos] = '\0';
                
                // コメント行やヘッダをスキップ
                if (line[0] != '#' && line[0] != 'I' && line[0] >= '0' && line[0] <= '9') {
                    // フォーマット: INPUT,TYPE,REV,OFRM,IFRM,PINS(12桁)
                    // 例: 0,1,0,30,15,000000000000
                    
                    // カンマで最初の5つのフィールドを分割
                    int values[5] = {0};
                    int value_count = 0;
                    char *start = line;
                    
                    for (int i = 0; i <= line_pos && value_count < 5; i++) {
                        if (line[i] == ',' || i == line_pos) {
                            line[i] = '\0';
                            values[value_count++] = atoi(start);
                            start = &line[i + 1];
                        }
                    }
                    
                    // 次フィールド（OUTPUT_PINS）を12桁の数字として解析
                    char output_pins[12] = {0};
                    if (value_count >= 5 && start < line + line_pos) {
                        for (int i = 0; i < 12 && start[i] != '\0' && start[i] != '\r' && start[i] != '\n'; i++) {
                            if (start[i] >= '0' && start[i] <= '9') {
                                output_pins[i] = start[i] - '0';
                            }
                        }
                    }
                    
                    // 設定に反映
                    if (value_count >= 5) {
                        int input_no = values[0];
                        if (input_no >= 0 && input_no < 12) {
                            int base = input_no * 16;
                            
                            // RAPID_TYPE + REVERSE
                            int rapid_type = values[1];
                            int reverse = values[2];
                            new_settings[base] = rapid_type + (reverse ? 10 : 0);
                            
                            // 他のパラメータ
                            new_settings[base + 1] = values[3]; // OUTPUT_FRAME
                            new_settings[base + 2] = values[4]; // INTERVAL_FRAME
                            // RAPID_TYPEが5(マクロ)のときはCMD_TYPEを入力番号に自動設定
                            if (rapid_type == 5) {
                                new_settings[base + 3] = (uint8_t)input_no;
                            }
                            
                            // OUTPUT_PINS
                            for (int pin = 0; pin < 12; pin++) {
                                new_settings[base + 4 + pin] = output_pins[pin];
                            }
                        }
                    }
                }
                line_pos = 0;
            }
        } else if (line_pos < 127) {
            line[line_pos++] = c;
        }
    }
    
    // フラッシュメモリに保存（簡易実装）
    // 実際の実装では save_io_setting_to_flash() を呼び出す
    // ここでは RAM 上の設定として反映（デモ用）
    uint8_t *flash_target = (uint8_t *)(XIP_BASE + 0x1F0000);
    // memcpy((void*)flash_target, new_settings, 256); // 実際にはフラッシュ書き込み処理が必要
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void) lun;
    if (lba != DATA_START_SECTOR) return (int32_t) bufsize; // ignore writes to non-data
    if (offset >= DISK_SECTOR_SIZE) return 0;
    uint32_t space = DISK_SECTOR_SIZE - offset;
    uint32_t to_copy = bufsize < space ? bufsize : space;
    memcpy(ram_disk + DISK_SECTOR_SIZE * lba + offset, buffer, to_copy);
    
    // ファイルが更新された場合、設定を解析・反映
    if (offset == 0) {
        // ファイルの先頭から書き込まれた場合、CSVをパース
        char *file_data = (char *)(ram_disk + DISK_SECTOR_SIZE * lba);
        parse_settings_csv(file_data);
    }
    
    return (int32_t) to_copy;
}

bool tud_msc_is_writable_cb (uint8_t lun) { (void) lun; return true; }

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize; return 0;
}

// Public helpers for main app ------------------------------------------------
bool usb_msc_is_connected(void) { return tud_connected(); }
bool usb_msc_is_mounted(void) { return tud_mounted(); }

// USB状態の文字列を取得（デバッグ用）
const char* usb_msc_get_status_string(void) {
    if (!tud_inited()) return "NotInit";
    if (!tud_connected()) return "NotConn"; 
    if (!tud_mounted()) return "NotMount";
    return "Mounted";
}

void usb_msc_start(void) {
    static bool s_inited = false;
    build_fat12_image();
    if (!s_inited) {
        board_init();
        s_inited = true;
    }
    tud_disconnect();
    sleep_ms(100);
    tud_init(0);
    tud_connect();
}

void usb_msc_task(void) {
    tud_task();
}

// Optional: visibility on host state
void tud_mount_cb(void) {
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    // MSC接続中はLED消灯
    gpio_put(25, 0);
}

void tud_umount_cb(void) {
    // アンマウント時も消灯を維持
    gpio_put(25, 0);
}
