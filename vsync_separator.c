/*
 * V-Sync Separator - 複合同期信号から垂直同期信号を分離
 * PIOステートマシンを使用した高精度パルス幅測定
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "vsync_separator.h"
#include "vsync_separator.pio.h"

// ピン定義
#define CSYNC_INPUT_PIN 14      // 複合同期信号入力

// デフォルト出力ピン（初期化時に使用）
static const uint8_t DEFAULT_OUTPUT_PINS[] = {15, 16};
static const output_pattern_t DEFAULT_OUTPUT_PATTERNS[] = {OUTPUT_ALWAYS, OUTPUT_ODD};
#define DEFAULT_OUTPUT_COUNT 2

// 設定可能な出力ピン配列
static uint8_t output_pins[MAX_OUTPUT_PINS];
static output_pattern_t output_patterns[MAX_OUTPUT_PINS];
static uint8_t output_pin_count = 0;

// 垂直同期カウンタ（出力パターン判定用）
static uint32_t vsync_counter = 0;

// パルス幅閾値（マイクロ秒単位）
#define HSYNC_PULSE_MIN 2       // 水平同期パルス最小幅
#define HSYNC_PULSE_MAX 6       // 水平同期パルス最大幅
#define EQUALIZING_MIN 2        // 等化パルス最小幅
#define EQUALIZING_MAX 4        // 等化パルス最大幅
#define VSYNC_SERRATION_MIN 20  // 垂直同期鋸歯パルス最小幅
#define VSYNC_SERRATION_MAX 30  // 垂直同期鋸歯パルス最大幅

// ステートマシンの状態
typedef enum {
    STATE_IDLE,              // 待機
    STATE_HSYNC,             // 水平同期検出中
    STATE_EQUALIZING,        // 等化パルス検出中
    STATE_VSYNC_SERRATION,   // 垂直同期鋸歯パルス検出中
    STATE_VSYNC_ACTIVE       // 垂直同期信号出力中
} sync_state_t;

// グローバル変数
static PIO pio = pio0;
static uint sm = 0;
static sync_state_t current_state = STATE_IDLE;
static uint32_t equalizing_count = 0;
static uint32_t serration_count = 0;
static bool vsync_output = false;

// 統計情報
static sync_stats_t stats = {0};

/*
 * V-Sync分離器の初期化
 */
void vsync_separator_init(void) {
    // デフォルト出力ピンを設定
    vsync_separator_set_output_pins(DEFAULT_OUTPUT_PINS, DEFAULT_OUTPUT_PATTERNS, DEFAULT_OUTPUT_COUNT);
    
    // PIOプログラムロード
    uint offset = pio_add_program(pio, &csync_measure_program);
    csync_measure_program_init(pio, sm, offset, CSYNC_INPUT_PIN);
    
    printf("V-Sync Separator initialized\n");
    printf("C-Sync Input: GP%d\n", CSYNC_INPUT_PIN);
    printf("Output Pins:\n");
    for (uint8_t i = 0; i < output_pin_count; i++) {
        const char* pattern_name;
        switch(output_patterns[i]) {
            case OUTPUT_ALWAYS: pattern_name = "ALWAYS(60Hz)"; break;
            case OUTPUT_ODD: pattern_name = "ODD(30Hz)"; break;
            case OUTPUT_EVEN: pattern_name = "EVEN(30Hz)"; break;
            case OUTPUT_QUARTER_01: pattern_name = "QUARTER_01(15Hz)"; break;
            case OUTPUT_QUARTER_23: pattern_name = "QUARTER_23(15Hz)"; break;
            case OUTPUT_2ON_1OFF: pattern_name = "2ON_1OFF(40Hz)"; break;
            case OUTPUT_1ON_2OFF: pattern_name = "1ON_2OFF(20Hz)"; break;
            case OUTPUT_1ON_3OFF: pattern_name = "1ON_3OFF(15Hz)"; break;
            case OUTPUT_1ON_4OFF: pattern_name = "1ON_4OFF(12Hz)"; break;
            case OUTPUT_1ON_5OFF: pattern_name = "1ON_5OFF(10Hz)"; break;
            case OUTPUT_1ON_6OFF: pattern_name = "1ON_6OFF(8.6Hz)"; break;
            default: pattern_name = "UNKNOWN"; break;
        }
        printf("  GP%d: %s\n", output_pins[i], pattern_name);
    }
}

/*
 * パルス幅を分類
 */
static inline bool is_hsync_pulse(uint32_t width_us) {
    return (width_us >= HSYNC_PULSE_MIN && width_us <= HSYNC_PULSE_MAX);
}

static inline bool is_equalizing_pulse(uint32_t width_us) {
    return (width_us >= EQUALIZING_MIN && width_us <= EQUALIZING_MAX);
}

static inline bool is_vsync_serration(uint32_t width_us) {
    return (width_us >= VSYNC_SERRATION_MIN && width_us <= VSYNC_SERRATION_MAX);
}

/*
 * パターンに基づいて出力するかどうかを判定
 */
static inline bool should_output(output_pattern_t pattern, uint32_t counter) {
    uint32_t mod;
    
    switch (pattern) {
        case OUTPUT_ALWAYS:
            return true;
        case OUTPUT_ODD:
            return (counter % 2) == 1;
        case OUTPUT_EVEN:
            return (counter % 2) == 0;
        case OUTPUT_QUARTER_01:
            return (counter % 4) <= 1;
        case OUTPUT_QUARTER_23:
            return (counter % 4) >= 2;
        case OUTPUT_2ON_1OFF:
            // 3回周期: 1,2がオン、3がオフ
            mod = (counter - 1) % 3;
            return mod < 2;
        case OUTPUT_1ON_2OFF:
            // 3回周期: 1がオン、2,3がオフ
            mod = (counter - 1) % 3;
            return mod == 0;
        case OUTPUT_1ON_3OFF:
            // 4回周期: 1がオン、2,3,4がオフ
            mod = (counter - 1) % 4;
            return mod == 0;
        case OUTPUT_1ON_4OFF:
            // 5回周期: 1がオン、2,3,4,5がオフ
            mod = (counter - 1) % 5;
            return mod == 0;
        case OUTPUT_1ON_5OFF:
            // 6回周期: 1がオン、2,3,4,5,6がオフ
            mod = (counter - 1) % 6;
            return mod == 0;
        case OUTPUT_1ON_6OFF:
            // 7回周期: 1がオン、2,3,4,5,6,7がオフ
            mod = (counter - 1) % 7;
            return mod == 0;
        default:
            return false;
    }
}

/*
 * 垂直同期信号を出力
 */
static void set_vsync_output(bool active) {
    vsync_output = active;
    
    if (active) {
        // 垂直同期開始時：カウンタをインクリメント
        vsync_counter++;
        
        // 各ピンのパターンに応じて出力
        for (uint8_t i = 0; i < output_pin_count; i++) {
            bool output = should_output(output_patterns[i], vsync_counter);
            gpio_put(output_pins[i], output ? 1 : 0);
        }
    } else {
        // 垂直同期終了時：すべてのピンをOFF
        for (uint8_t i = 0; i < output_pin_count; i++) {
            gpio_put(output_pins[i], 0);
        }
    }
    
    stats.vsync_active = active;
}

/*
 * ステートマシン処理
 */
static void process_sync_state(uint32_t high_width, uint32_t low_width) {
    stats.last_high_width = high_width;
    stats.last_low_width = low_width;
    stats.current_state = current_state;
    
    switch (current_state) {
        case STATE_IDLE:
            if (is_hsync_pulse(high_width)) {
                current_state = STATE_HSYNC;
                equalizing_count = 0;
                serration_count = 0;
            }
            break;
            
        case STATE_HSYNC:
            if (is_equalizing_pulse(high_width)) {
                // 等化パルス検出開始
                equalizing_count++;
                if (equalizing_count >= 3) {
                    current_state = STATE_EQUALIZING;
                }
            } else if (is_hsync_pulse(high_width)) {
                stats.hsync_count++;
                equalizing_count = 0;
            } else {
                current_state = STATE_IDLE;
            }
            break;
            
        case STATE_EQUALIZING:
            if (is_vsync_serration(high_width)) {
                // 垂直同期鋸歯パルス検出
                serration_count++;
                if (serration_count >= 3) {
                    current_state = STATE_VSYNC_SERRATION;
                    set_vsync_output(true);
                    stats.vsync_count++;
                }
            } else if (!is_equalizing_pulse(high_width)) {
                current_state = STATE_HSYNC;
                equalizing_count = 0;
            }
            break;
            
        case STATE_VSYNC_SERRATION:
            if (is_equalizing_pulse(high_width)) {
                // 垂直同期後の等化パルス検出
                equalizing_count++;
                if (equalizing_count >= 3) {
                    current_state = STATE_VSYNC_ACTIVE;
                }
            } else if (!is_vsync_serration(high_width)) {
                // 予期しないパルス
                set_vsync_output(false);
                current_state = STATE_HSYNC;
            }
            break;
            
        case STATE_VSYNC_ACTIVE:
            if (is_hsync_pulse(high_width)) {
                // 通常の水平同期に戻った
                set_vsync_output(false);
                current_state = STATE_HSYNC;
                equalizing_count = 0;
                serration_count = 0;
            }
            break;
    }
}

/*
 * V-Sync分離タスク（メインループで呼び出し）
 */
void vsync_separator_task(void) {
    // PIO FIFOからデータ読み取り
    while (!pio_sm_is_rx_fifo_empty(pio, sm)) {
        uint32_t high_count = pio_sm_get_blocking(pio, sm);
        
        if (pio_sm_is_rx_fifo_empty(pio, sm)) {
            break; // LOWデータがまだ来ていない
        }
        
        uint32_t low_count = pio_sm_get_blocking(pio, sm);
        
        // カウント値をマイクロ秒に変換（125MHz/125 = 1MHz = 1μs）
        uint32_t high_us = high_count;
        uint32_t low_us = low_count;
        
        // ステートマシンで処理
        process_sync_state(high_us, low_us);
    }
}

/*
 * 出力ピンを設定
 */
void vsync_separator_set_output_pins(const uint8_t* pins, const output_pattern_t* patterns, uint8_t count) {
    // 既存の出力ピンをクリア
    for (uint8_t i = 0; i < output_pin_count; i++) {
        gpio_put(output_pins[i], 0);
        gpio_deinit(output_pins[i]);
    }
    
    // 新しい出力ピンを設定
    output_pin_count = (count > MAX_OUTPUT_PINS) ? MAX_OUTPUT_PINS : count;
    
    for (uint8_t i = 0; i < output_pin_count; i++) {
        output_pins[i] = pins[i];
        output_patterns[i] = patterns[i];
        gpio_init(output_pins[i]);
        gpio_set_dir(output_pins[i], GPIO_OUT);
        gpio_put(output_pins[i], 0);
    }
    
    // カウンタをリセット
    vsync_counter = 0;
}

/*
 * 簡易設定：全ピン同じパターン
 */
void vsync_separator_set_output_pins_simple(const uint8_t* pins, uint8_t count, output_pattern_t pattern) {
    output_pattern_t patterns[MAX_OUTPUT_PINS];
    uint8_t actual_count = (count > MAX_OUTPUT_PINS) ? MAX_OUTPUT_PINS : count;
    
    for (uint8_t i = 0; i < actual_count; i++) {
        patterns[i] = pattern;
    }
    
    vsync_separator_set_output_pins(pins, patterns, actual_count);
}

const char* vsync_separator_get_state_string(void) {
    switch (current_state) {
        case STATE_IDLE: return "IDLE";
        case STATE_HSYNC: return "H-SYNC";
        case STATE_EQUALIZING: return "EQUAL";
        case STATE_VSYNC_SERRATION: return "V-SERR";
        case STATE_VSYNC_ACTIVE: return "V-SYNC";
        default: return "UNKNOWN";
    }
}

const sync_stats_t* vsync_separator_get_stats(void) {
    return &stats;
}


