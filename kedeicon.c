// kedeicon - userspace daemon that mirrors the Linux text console (tty1) onto a
// KeDei 3.5" v5.0 TFT (ILI9486 clone behind three 74HC595) via /dev/spidev0.1.
//
// WHY USERSPACE: a kernel DRM driver corrupts full-frame writes on this panel
// (bcm2835 SPI + thousands of tiny transfers). The SAME protocol from userspace
// (spidev + ioctl) paints cleanly. So we stay 100% userspace. See KEDEICON.md.
//
// The SPI framing below is copied verbatim from the proven kedei_test.c
// (reverse-engineered by Conjur / l0nley / FREEWING, public domain). Do NOT
// "optimize" the two-transfer-per-pixel framing: it is what the 74HC595 latch
// chain requires and what was verified on hardware.
//
// ADDRESSING: the controller's clean native canvas is 480(col) x 320(page) --
// that is exactly what kedei_test.c fills without corruption. Addressing it as
// 320x480 leaves 160 columns unwritten (a noise band) and rotates/compresses
// content. So the PHYSICAL framebuffer here is always 480x320 and any portrait
// reading is done by rotating the text in SOFTWARE (KEDEI_ROT). MADCTL stays at
// the proven 0xEA. See KEDEICON.md "orientation".
//
// Build: gcc -O2 -o kedeicon kedeicon.c   (needs font8x16.h, see genfont.py)
// Run  : sudo ./kedeicon                  (root: needs /dev/vcsa1 and spidev)
//
// Tunables via environment (the systemd unit sets these; no rebuild to change):
//   KEDEI_DEV         spidev node            (default /dev/spidev0.1)
//   KEDEI_ROT         orientation 0..3       (default 1)
//                       0,2 = landscape 60x20 cols/rows ; 1,3 = portrait 40x30
//   KEDEI_INTERVAL_MS console poll period ms (default 700)
//   KEDEI_VCSA        console source         (default /dev/vcsa1 = tty1)
//   KEDEI_COLOR       1=VGA attr colors, 0=mono white-on-black (default 1)

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "font.h"       // #define FONT_W/FONT_H + static const unsigned char fontdata[256][FONT_H];

// ---- physical panel canvas (proven-clean native addressing) ----
#define PHYS_W 480
#define PHYS_H 320
// Grid sizes are derived from the font: the longest logical axis is 480, so the
// worst-case grid is 480/FONT_W cols by 480/FONT_H rows (covers both orientations).
#define MAX_COLS (480 / FONT_W)
#define MAX_ROWS (480 / FONT_H)

// ---- SPI config (identical to kedei_test.c) ----
static uint32_t spi_mode  = 0;
static uint8_t  spi_bits  = 8;
static uint32_t spi_speed = 32000000;
static int      spih      = -1;

static void delayms(int ms){ usleep(ms * 1000); }

static int lcd_open(const char *dev){
    spih = open(dev, O_WRONLY);
    if(spih < 0){ perror(dev); return -1; }
    if(ioctl(spih, SPI_IOC_WR_MODE32, &spi_mode) < 0){ perror("mode"); return -1; }
    ioctl(spih, SPI_IOC_WR_BITS_PER_WORD, &spi_bits);
    ioctl(spih, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
    return 0;
}
static void spi_tx(uint8_t *data, int len){
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)data;
    tr.len = len;
    tr.speed_hz = spi_speed;
    tr.bits_per_word = spi_bits;
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
    lcd_cmd(0x36); lcd_data(0xEA);               // MADCTL: proven-clean 480x320 canvas
    lcd_cmd(0x3A); lcd_data(0x66);               // RGB666 18-bit
    lcd_cmd(0x35); lcd_data(0x00);
    lcd_cmd(0x29); delayms(200);                 // display ON
    lcd_cmd(0x00);
    lcd_cmd(0x11); delayms(200);
    lcd_cmd(0xEE); lcd_data(0x02);lcd_data(0x01);lcd_data(0x02);lcd_data(0x01);
    lcd_cmd(0xED); lcd_data(0x00);lcd_data(0x00);lcd_data(0x9A);lcd_data(0x9A);lcd_data(0x9B);lcd_data(0x9B);lcd_data(0x00);lcd_data(0x00);lcd_data(0x00);lcd_data(0x00);lcd_data(0xAE);lcd_data(0xAF);lcd_data(0x01);lcd_data(0xA2);lcd_data(0x01);lcd_data(0xBF);lcd_data(0x2A);
}

// Address window (physical): X = column (0x2A, 0..479), Y = page (0x2B, 0..319).
static void lcd_setframe(uint16_t x, uint16_t y, uint16_t w, uint16_t h){
    lcd_cmd(0x2A); lcd_data(x>>8);lcd_data(x&0xFF); lcd_data(((w+x)-1)>>8);lcd_data(((w+x)-1)&0xFF);
    lcd_cmd(0x2B); lcd_data(y>>8);lcd_data(y&0xFF); lcd_data(((h+y)-1)>>8);lcd_data(((h+y)-1)&0xFF);
    lcd_cmd(0x2C);
}

// ---- physical framebuffer (480x320 RGB565) ----
static uint16_t fb[PHYS_W * PHYS_H];

static void flush_rect(int x, int y, int w, int h){
    lcd_setframe(x, y, w, h);
    for(int gy = y; gy < y + h; gy++){
        const uint16_t *row = &fb[gy * PHYS_W];
        for(int gx = x; gx < x + w; gx++)
            lcd_color(row[gx]);
    }
}

// ---- orientation: map a LOGICAL pixel (lx,ly) to a PHYSICAL pixel (px,py) ----
// rot 0/2 landscape (logical 480x320); rot 1/3 portrait (logical 320x480).
static int g_rot = 1;
static int g_use_color = 1;
static int LOGW, LOGH, GCOLS, GROWS;

static void set_orientation(int rot){
    g_rot = rot & 3;
    if(g_rot == 0 || g_rot == 2){ LOGW = 480; LOGH = 320; }  // landscape
    else                        { LOGW = 320; LOGH = 480; }  // portrait
    GCOLS = LOGW / FONT_W;   // 60 or 40
    GROWS = LOGH / FONT_H;   // 20 or 30
}
static inline void map_l2p(int lx, int ly, int *px, int *py){
    switch(g_rot){
        case 0: *px = lx;            *py = ly;            break; // landscape
        case 2: *px = PHYS_W-1-lx;   *py = PHYS_H-1-ly;   break; // landscape 180
        case 1: *px = ly;            *py = PHYS_H-1-lx;   break; // portrait
        default:*px = PHYS_W-1-ly;   *py = lx;            break; // portrait (3)
    }
}

// ---- VGA 16-color palette -> RGB565 ----
static uint16_t pal[16];
static uint16_t rgb565(int r, int g, int b){ return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
static void init_palette(void){
    static const uint8_t v[16][3] = {
        {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
        {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
    };
    for(int i=0;i<16;i++) pal[i] = rgb565(v[i][0], v[i][1], v[i][2]);
}

// Draw one glyph into the physical fb at logical cell (col c, row r), rotated.
static void draw_glyph(int c, int r, unsigned char ch, uint16_t fg, uint16_t bg){
    int lx0 = c * FONT_W, ly0 = r * FONT_H;
    const unsigned char *g = fontdata[ch];
    for(int gy=0; gy<FONT_H; gy++){
        unsigned char bitsrow = g[gy];
        for(int gx=0; gx<FONT_W; gx++){
            int px, py;
            map_l2p(lx0+gx, ly0+gy, &px, &py);
            fb[py * PHYS_W + px] = (bitsrow & (0x80 >> gx)) ? fg : bg;
        }
    }
}

// Flush a logical cell run (row r, cols c0..c1) by mapping to its physical bbox.
static void flush_cells(int r, int c0, int c1){
    int lx0 = c0*FONT_W, ly0 = r*FONT_H;
    int lx1 = (c1+1)*FONT_W - 1, ly1 = (r+1)*FONT_H - 1;
    int ax,ay,bx,by;
    map_l2p(lx0, ly0, &ax, &ay);
    map_l2p(lx1, ly1, &bx, &by);
    int minx = ax<bx?ax:bx, maxx = ax<bx?bx:ax;
    int miny = ay<by?ay:by, maxy = ay<by?by:ay;
    flush_rect(minx, miny, maxx-minx+1, maxy-miny+1);
}

// ---- console mirror state ----
typedef struct { unsigned char ch, attr; } cell_t;
static cell_t prev[MAX_ROWS][MAX_COLS];

// Paint one cell from prev[][]; as_cursor => reverse-video block (blinking cursor).
static void paint_cell(int r, int c, int as_cursor){
    unsigned char ch = prev[r][c].ch, attr = prev[r][c].attr;
    uint16_t fg = g_use_color ? pal[attr & 0x0F] : pal[15];
    uint16_t bg = g_use_color ? pal[(attr >> 4) & 0x0F] : pal[0];
    if(as_cursor) draw_glyph(c, r, ch, bg, fg);   // swap fg/bg = reverse video
    else          draw_glyph(c, r, ch, fg, bg);
    flush_cells(r, c, c);
}

static volatile sig_atomic_t running = 1;
static void on_sig(int s){ (void)s; running = 0; }

static int getenv_int(const char *name, int def){
    const char *s = getenv(name);
    if(!s || !*s) return def;
    char *end; long v = strtol(s, &end, 10);
    return (*end) ? def : (int)v;
}

int main(void){
    const char *dev  = getenv("KEDEI_DEV");   if(!dev || !*dev)   dev  = "/dev/spidev0.1";
    const char *vcsa = getenv("KEDEI_VCSA");  if(!vcsa || !*vcsa) vcsa = "/dev/vcsa1";
    int interval  = getenv_int("KEDEI_INTERVAL_MS", 700);
    g_use_color   = getenv_int("KEDEI_COLOR", 1);
    if(interval < 100) interval = 100;
    set_orientation(getenv_int("KEDEI_ROT", 1));

    fprintf(stderr, "kedeicon: dev=%s vcsa=%s rot=%d interval=%dms color=%d grid=%dx%d\n",
            dev, vcsa, g_rot, interval, g_use_color, GCOLS, GROWS);

    init_palette();
    if(lcd_open(dev) < 0) return 1;
    lcd_init();

    // Initial full clear to black (the only full-frame write; 480x320 = clean).
    for(long i=0; i<PHYS_W*PHYS_H; i++) fb[i] = pal[0];
    flush_rect(0, 0, PHYS_W, PHYS_H);
    memset(prev, 0, sizeof(prev));   // forces first paint of every cell

    signal(SIGTERM, on_sig);
    signal(SIGINT,  on_sig);

    static unsigned char buf[8 + 2*256*256];
    int pcx = -1, pcy = -1, blink = 0;   // blinking-cursor state
    while(running){
        int fd = open(vcsa, O_RDONLY);
        if(fd < 0){
            fprintf(stderr, "kedeicon: open(%s): %s\n", vcsa, strerror(errno));
            delayms(interval);
            continue;
        }
        ssize_t n = read(fd, buf, sizeof(buf));
        close(fd);
        if(n < 4){ delayms(interval); continue; }

        int vrows = buf[0], vcols = buf[1];   // vcsa header: rows, cols, cx, cy
        const unsigned char *data = buf + 4;

        for(int r=0; r<GROWS; r++){
            int run_start = -1;
            for(int c=0; c<GCOLS; c++){
                unsigned char ch = ' ', attr = 0x07;
                if(r < vrows && c < vcols){
                    long idx = (long)(r * vcols + c) * 2;
                    if(idx + 1 < n - 4){ ch = data[idx]; attr = data[idx+1]; }
                }
                if(ch == 0) ch = ' ';
                int changed = (prev[r][c].ch != ch) || (prev[r][c].attr != attr);
                if(changed){
                    prev[r][c].ch = ch; prev[r][c].attr = attr;
                    uint16_t fg = g_use_color ? pal[attr & 0x0F] : pal[15];
                    uint16_t bg = g_use_color ? pal[(attr >> 4) & 0x0F] : pal[0];
                    draw_glyph(c, r, ch, fg, bg);
                    if(run_start < 0) run_start = c;
                } else if(run_start >= 0){
                    flush_cells(r, run_start, c-1);
                    run_start = -1;
                }
            }
            if(run_start >= 0) flush_cells(r, run_start, GCOLS-1);
        }

        // --- blinking cursor overlay (vcsa header: cx=buf[2], cy=buf[3]) ---
        int cx = buf[2], cy = buf[3];
        if(cx >= GCOLS || cy >= GROWS){ cx = -1; cy = -1; }   // off-grid: hide
        blink ^= 1;
        if(pcx != cx || pcy != cy){
            if(pcx >= 0) paint_cell(pcy, pcx, 0);             // restore old cursor cell
            pcx = cx; pcy = cy;
        }
        if(cx >= 0) paint_cell(cy, cx, blink);

        delayms(interval);
    }

    fprintf(stderr, "kedeicon: exiting\n");
    close(spih);
    return 0;
}
