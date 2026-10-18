#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define CHAN_LED        8
#define CHAN_TX         21
#define CHAN_RX         20
#define CONG_UART       UART_NUM_0

int trang_thai_led = 0;

int khoi_tao_phan_cung(void)
{

    gpio_config_t cau_hinh_led = {
        .pin_bit_mask = (1ULL << CHAN_LED),        
        .mode         = GPIO_MODE_OUTPUT,         
        .pull_up_en   = GPIO_PULLUP_DISABLE,       
        .pull_down_en = GPIO_PULLDOWN_DISABLE,     
        .intr_type    = GPIO_INTR_DISABLE         
    };

    if (gpio_config(&cau_hinh_led) != 0) {
        printf("Loi: Khong the cau hinh chan LED!\r\n");
        return -1;
    }
    // Mac dinh ban dau tat den
    gpio_set_level(CHAN_LED, 0);

    uart_config_t cau_hinh_uart = {
        .baud_rate = 115200,                      
        .data_bits = UART_DATA_8_BITS,             
        .parity    = UART_PARITY_DISABLE,          
        .stop_bits = UART_STOP_BITS_1,            
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,     
        .source_clk = UART_SCLK_DEFAULT           
    };

    // Nap struct thong so UART
    if (uart_param_config(CONG_UART, &cau_hinh_uart) != 0) {
        printf("Loi: Khong the cau hinh thong so UART!\r\n");
        return -1;
    }

    // Gan chan TX = 21, RX = 20 vao khoi UART0
    if (uart_set_pin(CONG_UART, CHAN_TX, CHAN_RX, -1, -1) != 0) {
        printf("Loi: Khong the dinh tuyen chan UART!\r\n");
        return -1;
    }

    // Cai dat driver UART voi bo dem nhan phan cung 256 bytes
    if (uart_driver_install(CONG_UART, 256, 0, 0, NULL, 0) != 0) {
        printf("Loi: Khong the cai dat bo dem UART!\r\n");
        return -1;
    }

    return 0; // Khoi tao thanh cong toan bo
}

void app_main(void)
{
    if (khoi_tao_phan_cung() != 0) {
        printf("Khoi tao phan cung that bai. Dung chuong trinh!\r\n");
        return;
    }

    printf("\r\nESP32-C3 Da San Sang!\r\n");
    printf("Cac lenh hop le: LED ON | LED OFF | STATUS\r\n");

    while (1) {
        char bo_dem[32];

        // Doc du lieu tu PC, cho toi da 100ms
        int so_byte = uart_read_bytes(CONG_UART, bo_dem, sizeof(bo_dem) - 1, pdMS_TO_TICKS(100));

        if (so_byte > 0) {
            // Chot ky tu ket thuc chuoi de bien thanh string chuan C
            bo_dem[so_byte] = '\0';

            // Cat bo ky tu xuong dong (\r, \n) hoac dau cach thua o cuoi
            while (so_byte > 0 && 
                  (bo_dem[so_byte - 1] == '\r' || 
                   bo_dem[so_byte - 1] == '\n' || 
                   bo_dem[so_byte - 1] == ' ')) {
                bo_dem[--so_byte] = '\0';
            }

            // So sanh chuoi va dieu khien den LED
            if (strcmp(bo_dem, "LED ON") == 0) {
                trang_thai_led = 1;
                gpio_set_level(CHAN_LED, 1);
                printf("PHAN HOI: DA BAT LED\r\n");
            }
            else if (strcmp(bo_dem, "LED OFF") == 0) {
                trang_thai_led = 0;
                gpio_set_level(CHAN_LED, 0);
                printf("PHAN HOI: DA TAT LED\r\n");
            }
            else if (strcmp(bo_dem, "STATUS") == 0) {
                if (trang_thai_led == 1) {
                    printf("TRANG THAI: LED DANG BAT\r\n");
                } else {
                    printf("TRANG THAI: LED DANG TAT\r\n");
                }
            }
            else {
                printf("LOI: LENH KHONG HOP LE (\"%s\")\r\n", bo_dem);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
