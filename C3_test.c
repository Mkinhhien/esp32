#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"

#define CHAN_I2C_SDA        4
#define CHAN_I2C_SCL        5
#define CONG_I2C            I2C_NUM_0
#define DIA_CHI_OLED        0x3C
#define KENH_ADC            ADC_CHANNEL_2

static adc_oneshot_unit_handle_t bo_doc_adc;

static const uint8_t FONT_CO_BAN[][5] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['0'] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
    ['1'] = {0x00, 0x42, 0x7F, 0x40, 0x00},
    ['2'] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3'] = {0x21, 0x41, 0x45, 0x4B, 0x31},
    ['4'] = {0x18, 0x14, 0x12, 0x7F, 0x10},
    ['5'] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6'] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
    ['7'] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8'] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9'] = {0x06, 0x49, 0x49, 0x29, 0x1E},
    [':'] = {0x00, 0x36, 0x36, 0x00, 0x00},
    ['A'] = {0x7E, 0x11, 0x11, 0x11, 0x7E},
    ['C'] = {0x3E, 0x41, 0x41, 0x41, 0x22},
    ['D'] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
    ['V'] = {0x1F, 0x20, 0x40, 0x20, 0x1F},
    ['m'] = {0x7C, 0x04, 0x18, 0x04, 0x78},
};


// Gui 1 byte ma lenh cau hinh xuong SSD1306
static esp_err_t oled_gui_lenh(uint8_t lenh)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DIA_CHI_OLED << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true); // Byte 0x00: Bao hieu du lieu sau la LENH
    i2c_master_write_byte(cmd, lenh, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(CONG_I2C, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Gui 1 byte du lieu diem anh (pixel) de ve len man hinh
static esp_err_t oled_gui_du_lieu(uint8_t *du_lieu, size_t do_dai)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DIA_CHI_OLED << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x40, true); // Byte 0x40: Bao hieu du lieu sau la DU LIEU HIEN THI
    i2c_master_write(cmd, du_lieu, do_dai, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(CONG_I2C, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Dat con tro ve trang (hang doc 0..7) va cot (0..127)
void oled_dat_vi_tri(uint8_t trang, uint8_t cot)
{
    oled_gui_lenh(0xB0 + trang);               // Set Page Start Address
    oled_gui_lenh(0x00 | (cot & 0x0F));        // Set Lower Column
    oled_gui_lenh(0x10 | ((cot >> 4) & 0x0F)); // Set Higher Column
}

// Xoa sach man hinh OLED
void oled_xoa_man_hinh(void)
{
    uint8_t bo_dem_rong[128] = {0};
    for (uint8_t trang = 0; trang < 8; trang++) {
        oled_dat_vi_tri(trang, 0);
        oled_gui_du_lieu(bo_dem_rong, sizeof(bo_dem_rong));
    }
}

// In mot chuoi ky tu don gian len OLED tai mot trang nhat dinh
void oled_in_chuoi(uint8_t trang, uint8_t cot, const char *chuoi)
{
    oled_dat_vi_tri(trang, cot);
    while (*chuoi) {
        char c = *chuoi++;
        if (c < sizeof(FONT_CO_BAN) / sizeof(FONT_CO_BAN[0]) && FONT_CO_BAN[(uint8_t)c][0] != 0 || c == ' ') {
            oled_gui_du_lieu((uint8_t *)FONT_CO_BAN[(uint8_t)c], 5);
            uint8_t khoang_cach = 0x00;
            oled_gui_du_lieu(&khoang_cach, 1); // Cach 1 cot pixel giua cac chu
        }
    }
}

// Khoi dong OLED SSD1306 bang tap lenh chuan nha san xuat
void khoi_tao_oled(void)
{
    oled_gui_lenh(0xAE); // Tat man hinh (Display OFF)
    oled_gui_lenh(0x20); oled_gui_lenh(0x02); // Che do dinh dia chi trang (Page Addressing Mode)
    oled_gui_lenh(0x8D); oled_gui_lenh(0x14); // Bat mach bom ap noi bo (Charge Pump ON)
    oled_gui_lenh(0xAF); // Bat man hinh (Display ON)
    oled_xoa_man_hinh();
}


void khoi_tao_phan_cung(void)
{
    // --- Khoi tao I2C Master ---
    i2c_config_t i2c_conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = CHAN_I2C_SDA,
        .scl_io_num       = CHAN_I2C_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    i2c_param_config(CONG_I2C, &i2c_conf);
    i2c_driver_install(CONG_I2C, i2c_conf.mode, 0, 0, 0);

    // --- Khoi tao ADC1 (Kenh 2 - GPIO 2) ---
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&init_config, &bo_doc_adc);

    // Cau hinh do phan giai 12-bit (0..4095) va do suy giam 11dB (do dien ap den ~3.1V)
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(bo_doc_adc, KENH_ADC, &chan_config);
}

// =========================================================================
// 3. VONG LAP CHINH
// =========================================================================

void app_main(void)
{
    khoi_tao_phan_cung();
    khoi_tao_oled();

    // In tieu de co dinh len hang dau
    oled_in_chuoi(0, 0, "DO ADC ESP32-C3");

    while (1) {
        int gia_tri_raw = 0;
        // Doc gia tri so nguyen tho tu bo ADC (0 - 4095)
        adc_oneshot_read(bo_doc_adc, KENH_ADC, &gia_tri_raw);

        // Uoc luong dien ap ra mili-volt (mV): V = raw * 3300 / 4095
        int dien_ap_mv = (gia_tri_raw * 3300) / 4095;

        // In ket qua ra Serial PC de doi chieu
        printf("ADC Raw: %4d | Dien ap: %4d mV\r\n", gia_tri_raw, dien_ap_mv);

        // Dinh dang chuoi hien thi len man hinh OLED
        char chuoi_raw[32];
        char chuoi_dien_ap[32];
        snprintf(chuoi_raw, sizeof(chuoi_raw), "RAW : %04d", gia_tri_raw);
        snprintf(chuoi_dien_ap, sizeof(chuoi_dien_ap), "VOLT: %04d mV", dien_ap_mv);

        // Hien thi len Page 3 va Page 5 tren OLED
        oled_in_chuoi(3, 0, chuoi_raw);
        oled_in_chuoi(5, 0, chuoi_dien_ap);

        // Cap nhat 200ms mot lan (du nhanh de quan sat, khong lam chop giat man hinh)
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}