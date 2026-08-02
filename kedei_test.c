// KeDei 3.5" v5.0 - proof-of-life test (userspace, spidev).
// Protocol reverse-engineered by Conjur / l0nley / FREEWING (public). NOT the KeDei binary driver.
// Fills the panel with solid colors through the 74HC595 shift-register protocol so you can SEE it work.
// Build: gcc -O2 -o kedei_test kedei_test.c
// Run  : sudo ./kedei_test            (LCD default on /dev/spidev0.1 = CE1)
//        sudo ./kedei_test /dev/spidev0.0   (try this if 0.1 shows nothing)
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define LCD_WIDTH  480
#define LCD_HEIGHT 320

static uint32_t mode  = 0;
static uint8_t  bits  = 8;
static uint32_t speed = 32000000;
static int spih = -1;
static uint16_t lcd_w = LCD_WIDTH, lcd_h = LCD_HEIGHT;

static void delayms(int ms){ usleep(ms*1000); }

static int lcd_open(const char *dev){
    spih = open(dev, O_WRONLY);
    if(spih < 0){ perror(dev); return -1; }
    if(ioctl(spih, SPI_IOC_WR_MODE32, &mode) < 0){ perror("mode"); return -1; }
    ioctl(spih, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spih, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    return 0;
}
static void spi_tx(uint8_t *data, int len){
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)data;
    tr.len = len;
    tr.speed_hz = speed;
    tr.bits_per_word = bits;
    ioctl(spih, SPI_IOC_MESSAGE(1), &tr);
}
static void lcd_rst(void){ uint8_t b; b=0x00; spi_tx(&b,1); delayms(150); b=0x01; spi_tx(&b,1); delayms(250); }
static void lcd_cmd(uint8_t c){
    uint8_t b[2];
    b[0]=c>>1; b[1]=((c&1)<<5)|0x11; spi_tx(b,2);
    b[0]=c>>1; b[1]=((c&1)<<5)|0x1B; spi_tx(b,2);
}
static void lcd_data(uint8_t d){
    uint8_t b[2];
    b[0]=d>>1; b[1]=((d&1)<<5)|0x15; spi_tx(b,2);
    b[0]=d>>1; b[1]=((d&1)<<5)|0x1F; spi_tx(b,2);
}
static void lcd_color(uint16_t col){
    uint8_t pseud = ((col>>5)&0x40) | ((col<<5)&0x20);
    uint8_t b[3];
    b[0]=col>>8; b[1]=col&0xFF; b[2]=pseud|0x15; spi_tx(b,3);
    b[0]=col>>8; b[1]=col&0xFF; b[2]=pseud|0x1F; spi_tx(b,3);
}
static const uint8_t rot[4]={0xEA,0x4A,0x2A,0x8A};
static void lcd_setrotation(uint8_t m){
    lcd_cmd(0x36); lcd_data(rot[m]);
    if(m&1){ lcd_h=LCD_WIDTH; lcd_w=LCD_HEIGHT; } else { lcd_h=LCD_HEIGHT; lcd_w=LCD_WIDTH; }
}
static void lcd_init(void){
    lcd_rst();
    lcd_cmd(0x00);
    lcd_cmd(0x11); delayms(200);
    lcd_cmd(0xEE); lcd_data(0x02);lcd_data(0x01);lcd_data(0x02);lcd_data(0x01);
    lcd_cmd(0xED); lcd_data(0x00);lcd_data(0x00);lcd_data(0x9A);lcd_data(0x9A);lcd_data(0x9B);lcd_data(0x9B);lcd_data(0x00);lcd_data(0x00);lcd_data(0x00);lcd_data(0x00);lcd_data(0xAE);lcd_data(0xAE);lcd_data(0x01);lcd_data(0xA2);lcd_data(0x00);
    lcd_cmd(0xB4); lcd_data(0x00);
    lcd_cmd(0xC0); lcd_data(0x10);lcd_data(0x3B);lcd_data(0x00);lcd_data(0x02);lcd_data(0x11);
    lcd_cmd(0xC1); lcd_data(0x10);
    lcd_cmd(0xC8); lcd_data(0x00);lcd_data(0x46);lcd_data(0x12);lcd_data(0x20);lcd_data(0x0C);lcd_data(0x00);lcd_data(0x56);lcd_data(0x12);lcd_data(0x67);lcd_data(0x02);lcd_data(0x00);lcd_data(0x0C);
    lcd_cmd(0xD0); lcd_data(0x44);lcd_data(0x42);lcd_data(0x06);
    lcd_cmd(0xD1); lcd_data(0x43);lcd_data(0x16);
    lcd_cmd(0xD2); lcd_data(0x04);lcd_data(0x22);
    lcd_cmd(0xD3); lcd_data(0x04);lcd_data(0x12);
    lcd_cmd(0xD4); lcd_data(0x07);lcd_data(0x12);
    lcd_cmd(0xE9); lcd_data(0x00);
    lcd_cmd(0xC5); lcd_data(0x08);
    lcd_setrotation(0);
    lcd_cmd(0x3A); lcd_data(0x66);   // RGB666 18-bit
    lcd_cmd(0x35); lcd_data(0x00);
    lcd_cmd(0x29); delayms(200);     // display ON
    lcd_cmd(0x00);
    lcd_cmd(0x11); delayms(200);
    lcd_cmd(0xEE); lcd_data(0x02);lcd_data(0x01);lcd_data(0x02);lcd_data(0x01);
    lcd_cmd(0xED); lcd_data(0x00);lcd_data(0x00);lcd_data(0x9A);lcd_data(0x9A);lcd_data(0x9B);lcd_data(0x9B);lcd_data(0x00);lcd_data(0x00);lcd_data(0x00);lcd_data(0x00);lcd_data(0xAE);lcd_data(0xAF);lcd_data(0x01);lcd_data(0xA2);lcd_data(0x01);lcd_data(0xBF);lcd_data(0x2A);
}
static void lcd_setframe(uint16_t x,uint16_t y,uint16_t w,uint16_t h){
    lcd_cmd(0x2A); lcd_data(x>>8);lcd_data(x&0xFF); lcd_data(((w+x)-1)>>8);lcd_data(((w+x)-1)&0xFF);
    lcd_cmd(0x2B); lcd_data(y>>8);lcd_data(y&0xFF); lcd_data(((h+y)-1)>>8);lcd_data(((h+y)-1)&0xFF);
    lcd_cmd(0x2C);
}
static void lcd_fill(uint16_t col){
    long n=(long)lcd_w*lcd_h, i;
    lcd_setframe(0,0,lcd_w,lcd_h);
    for(i=0;i<n;i++) lcd_color(col);
}

int main(int argc, char **argv){
    const char *dev = (argc>1) ? argv[1] : "/dev/spidev0.1";
    const uint16_t RED=0xF800, GREEN=0x07E0, BLUE=0x001F, GREEN2=0x07E0;
    printf("KeDei v5.0 test on %s\n", dev); fflush(stdout);
    if(lcd_open(dev) < 0) return 1;
    printf("init...\n"); fflush(stdout);
    lcd_init();
    printf("fill RED (screen paints slowly, that is normal for this hardware)\n"); fflush(stdout);
    lcd_fill(RED);   delayms(600);
    printf("fill BLUE\n"); fflush(stdout);
    lcd_fill(BLUE);  delayms(600);
    printf("fill GREEN (leaving this on screen)\n"); fflush(stdout);
    lcd_fill(GREEN2);
    printf("DONE. If you see solid GREEN, the panel WORKS via the KeDei protocol.\n");
    close(spih);
    return 0;
}
