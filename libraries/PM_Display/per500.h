#define PER500_FRAMES 4
static void draw_per500(int x, int y, byte anim, byte rot, byte jk = 0) {
  switch (anim) {
  case 0:
    GD.xsprite(x, y, -8, -8, 0, 9, rot, jk);
    break;
  case 1:
    GD.xsprite(x, y, -8, -8, 0, 11, rot, jk);
    break;
  case 2:
    GD.xsprite(x, y, -8, -8, 0, 13, rot, jk);
    break;
  case 3:
    GD.xsprite(x, y, -8, -8, 0, 15, rot, jk);
    break;
  }
}

static PROGMEM prog_uchar per500_sprimg[] = {

0xc9,  0x01,  0xc4,  0xa9,  0x0a,  0xe0,  0x41,  0x3e,  0xa4,  0xa8,  0x54,  0xa1,  0x22,  0x6c,  0x29,  0x0a, 
0x40,  0x92,  0x44,  0x05,  0x00,  0x99,  0x4a,  0x14,  0x06,  0x0d,  0x65,  0x2a,  0x82,  0x97,  0x20,  0x41, 
0xc1,  0x53,  0x80,  0x80,  0xb0,  0xa4,  0x40,  0x52,  0x02,  0x90,  0x15,  0x0e,  0x12,  0x85,  0xd2,  0xe0, 
0x21,  0x2b,  0x5a,  0x2a,  0x80,  0xa4,  0x8a,  0x15,  0x2c,  0x50,  0xb4,  0x02,  0x01,  0x52,  0x08,  0x40, 
0x03,  0x85,  0x2a,  0x65,  0x02,  0x00,  0xa1,  0x42,  0x05,  0x0a,  0x54,  0xc0,  0x40,  0x85,  0x00,  0x41, 
0x60,  0x60,  0x95,  0x0d,  0x82,  0x07,  0xa2,  0xf8,  0x40,  0x2c,  0x64,  0xa0,  0x54,  0xa0,  0x80,  0x54, 
0x68,  0x08,  0x5e,  0xc9,  0x09,  0x19,  0x2a,  0x15,  0x0a,  0x14,  0x8a,  0x0a,  0xde,  0xa0,  0x4a,  0x80, 
0x30,  0x41,  0x88,  0x82,  0xa7,  0xa2,  0xe0,  0x2d,  0x28,  0x78,  0x8a,  0x0a,  0x1e,  0x28,  0x83,  0x3f, 
0xac,  0xec,  0x29,  0x08,  0x12,  0x0a,  0x15,  0x4a,  0x25,  0x09,  0x83,  0x15,  0x3e,  0x94,  0x05,  0x2f, 
0x49,  0x82,  0x80,  0x05,  0x8f,  0x20,  0x11,  0x16,  0xa2,  0x87,  0x02,  0x81,  0xe9,  0x21,  0x05,  0x82, 
0x04,  0x21,  0x32,  0x16,  0x24,  0x8a,  0xa0,  0xa1,  0x40,  0x40,  0xf0,  0x54,  0xa6,  0x0a,  0x46,  0xf0, 
0x94,  0xa5,  0x2c,  0x78,  0x2a,  0x15,  0xd1,  0xbd,  0xdb,  0x4a,  0x5f,  0x52,  0x05, 
};
static PROGMEM prog_uchar per500_sprpal[] = {

0x18,  0x63,  0x0d,  0x5f,  0xe4,  0x56,  0x00,  0x80, 
};