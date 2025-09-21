#include <string.h>
#include "tusb.h"

// Device descriptor -------------------------------------------------
static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .idVendor = 0xCafe,
    .idProduct = 0x4001,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

// Configuration descriptor with one MSC interface -------------------
// Configuration total length: config(9) + interface(9) + 2 endpoints(2*7) = 32
enum { CONFIG_TOTAL_LEN = 9 + 9 + 7 + 7 };

#define EPNUM_MSC_OUT   0x01
#define EPNUM_MSC_IN    0x81

static uint8_t const desc_fs_configuration[] = {
    // config
    0x09, TUSB_DESC_CONFIGURATION, (CONFIG_TOTAL_LEN & 0xFF), (CONFIG_TOTAL_LEN >> 8),
    0x01, // one interface
    0x01, // configuration value
    0x00, // string index
    0x80, // attributes (bus powered)
    250,  // 500mA (value is in 2mA units)
    // interface (MSC)
    0x09, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI, MSC_PROTOCOL_BOT, 0x00,
    // Endpoint OUT
    0x07, TUSB_DESC_ENDPOINT, EPNUM_MSC_OUT, 0x02, 64, 0x00, 0x00,
    // Endpoint IN
    0x07, TUSB_DESC_ENDPOINT, EPNUM_MSC_IN,  0x02, 64, 0x00, 0x00,
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index; return desc_fs_configuration;
}

// String descriptors -------------------------------------------------
static char const *string_desc[] = {
    (const char[]) {0x09, 0x04}, // 0: supported language (en-US)
    "Pico",                      // 1: Manufacturer
    "Pico RAM Disk",            // 2: Product
    "123456",                   // 3: Serial
};

static uint16_t _desc_str[32];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;
    if (index == 0) {
        memcpy(&_desc_str[1], string_desc[0], 2);
        chr_count = 1; // one LANGID pair
    } else {
        const char *str = string_desc[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) _desc_str[1 + i] = str[i];
    }
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2*chr_count + 2);
    return _desc_str;
}
