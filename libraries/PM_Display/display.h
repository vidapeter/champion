#include "GD.h"

#include "digits.h"     // sprites - font for large digits
#include "cal.h"        // sprites - the text for calories
#include "meters.h"     // sprites - the text for meters
#include "per500.h"     // sprites - the text for /500
#include "watts.h"      // sprites - the text for watts
#include "logo.h"       // character background for the concePt2 logo

#ifndef _Display_h_
#define _Display_h_

class Display
{
  #define DIGITS_FRAME0 (0)
  #define CAL_FRAME0    (DIGITS_FRAME0 + DIGITS_FRAMES)
  #define METERS_FRAME0 (CAL_FRAME0 + CAL_FRAMES)
  #define PER500_FRAME0 (METERS_FRAME0 + METERS_FRAMES)
  #define WATTS_FRAME0  (PER500_FRAME0 + PER500_FRAMES)
  
  #define DIGIT_FRAME0(digit) (4 * (digit))

  // This function is based on the 4-color (2-bit) sprite scheme
  // Each 'sprite image' in RAM_SPRIMG actually contains 4 images (frames).
  // In the 'sprite control' 32-bit word for each of the 256 sprites,
  // the sprite image field (6 bits) and the palette field (4 bits)
  // combine to select a single 4-color sprite frame.
  //
  // The 4-bit palette field is interpreted like this:
  //    M m n P   where M   = 1 = 2-bit (4-color) mode
  //                    m n = selects one of 4 frames within the image
  //                    P   = palette B
  //    1 0 0 1 =  9
  //    1 0 1 1 = 11
  //    1 1 0 1 = 13
  //    1 1 1 1 = 15
  //
  // So, 4 concecutive frames are define by the image/palette pairs:
  //   (0, 9) (0, 11) (0, 13) (0, 15)
  //
  // Note that with this configuration, there are 256 sprite frames
  // to choose from.
  //
  void draw4ColorSpriteAt(byte spr, int frame, int x, int y) {
    int image = frame / 4;
    int palette = 9 + 2 * (frame % 4);
    GD.sprite(spr, x, y, image, palette, 0, 0);
  }

  // The 'units' text is assumed to consist of 4 consecutive sprite
  // frames (left to right).  
  void drawUnits(int x0, int y0, byte frame)
  {
    byte spr = 120; // comes after the 5 digits, at 24 sprites/digit
    for (int ix = 0; ix < 4; ix++)
    {
      draw4ColorSpriteAt(spr, frame, x0 + (16*ix), y0);
      spr++;
      frame++;
    }
  }  
  
  // A digit (0 - 9) is drawn as a 4x6 set of sprite frames.
  // In RAM_SPRIMG, the digits were loaded as a single picture
  // of the digits displayed in order - 0123456789
  // Each digit is 64 bits (4 sprites) wide, and 96 bits (6 sprites)
  // tall.
  //
  // The 256 sprites are allocated like this:
  //    0..23   10000's digit, place = 4
  //   24..47    1000's digit, place = 3
  //   48..71     100's digit, place = 2
  //   72..95      10's digit, place = 1
  //   96..119      1's digit, place = 0
  //  120..123  units text
  //  124+      unused
  //
  void drawDigit(int x0, int y0, byte place, byte digit, byte blank)
  {
    // Draw a blank, but not in the 0's place
    if ((place > 0) && (digit == 0) && (blank))
    {
      x0 = 400; // off-screen
    }
  
    byte spr = 24 * place;
    int frame0 = DIGIT_FRAME0(digit);
    for (int iy = 0; iy < 6; iy++)
    {
      for (int ix = 0; ix < 4; ix++)
      {
        int frame = frame0 + ix + (40 * iy);
        int x = x0 + (16*ix);
        int y = y0 + (16*iy);
        
        draw4ColorSpriteAt(spr, frame, x, y);
        spr++;
      }
    }
  }

  void loadSprites()
  {
    // The sprites share a palette
    GD.copy(PALETTE4B, digits_sprpal, sizeof(digits_sprpal));
    // 64 bytes per sprite frame (for 4-color sprites)
    GD.uncompress(RAM_SPRIMG + (64 * DIGITS_FRAME0), digits_sprimg);
    GD.uncompress(RAM_SPRIMG + (64 * CAL_FRAME0), cal_sprimg);
    GD.uncompress(RAM_SPRIMG + (64 * METERS_FRAME0), meters_sprimg);
    GD.uncompress(RAM_SPRIMG + (64 * PER500_FRAME0), per500_sprimg);
    GD.uncompress(RAM_SPRIMG + (64 * WATTS_FRAME0), watts_sprimg);
  }

  //
  #define CHAR_ID_TRANSPARENT 255
  #define CHAR_ID_DOT         254
  #define CHAR_ID_HOLLOW_DOT  253
  
  void loadCharColor(uint8_t chrId, uint8_t colorId, uint16_t colorValue)
  {
    GD.wr16(RAM_PAL + (8*chrId) + (2*colorId), colorValue);
  }

  // We setup our own special characters
  //   255 - this is a completely transparent character
  //   254 - this is a filled-in dot, with just the 4 corner pixels transparent
  //   253 - this is a hollow dot, with the corner pixels and the center transparent  
  void loadChars()
  {
    // Define the transparent character
    GD.fill(RAM_CHR + 16*CHAR_ID_TRANSPARENT, 0, 16);       // 16 bytes per character definition, 
                                                            // each pixel is color index 0
    GD.wr16(RAM_PAL +  8*CHAR_ID_TRANSPARENT, TRANSPARENT); // make color index 0 transparent

    // Define the dot - 2 bits define the color index for each pixel
    static PROGMEM prog_uchar dot_pic[] =
    {
      0x3f, 0xfc, // Corners are color 0, other pixels are color 3
      0xd5, 0x57, // Edges are color 3, other pixels are color 1
      0xd5, 0x57,
      0xd5, 0x57,
      0xd5, 0x57,
      0xd5, 0x57,
      0xd5, 0x57,
      0x3f, 0xfc // Corners are color 0, other pixels are color 3
    };
    
    GD.copy(RAM_CHR + 16*CHAR_ID_DOT, dot_pic, sizeof(dot_pic));
    GD.copy(RAM_CHR + 16*CHAR_ID_HOLLOW_DOT, dot_pic, sizeof(dot_pic));

    // The palettes are different for 254 and 253
    loadCharColor(CHAR_ID_DOT, 0, TRANSPARENT);
    loadCharColor(CHAR_ID_DOT, 1, RGB(0xff, 0xff, 0xff));
    loadCharColor(CHAR_ID_DOT, 2, RGB(0xff, 0xff, 0xff));
    loadCharColor(CHAR_ID_DOT, 3, RGB(0xff, 0xff, 0xff));

    loadCharColor(CHAR_ID_HOLLOW_DOT, 0, TRANSPARENT);
    loadCharColor(CHAR_ID_HOLLOW_DOT, 1, TRANSPARENT);
    loadCharColor(CHAR_ID_HOLLOW_DOT, 2, RGB(0xff, 0xff, 0xff));
    loadCharColor(CHAR_ID_HOLLOW_DOT, 3, RGB(0xff, 0xff, 0xff));
    
    // Fill the entire char pic with the transparent char
    GD.fill(RAM_PIC, CHAR_ID_TRANSPARENT, 64 * 64);

    // For the background, use the 3rd color from the sprite palette
    //GD.copy(BG_COLOR, digits_sprpal + 4, 2);
    
  }
  
  void loadLogo()
  {
    // The logo is made of characters, not sprites. The logo was generated by the 
    // online Gameduino tool that creates a background from a png file.  This code 
    // puts it at the begining of char memory.
    #define LOGO_WIDTH 50
    #define LOGO_HEIGHT 6
    for (byte y = 0; y < LOGO_HEIGHT; y++)
      GD.copy(RAM_PIC + y * 64, logo_pic + y * LOGO_WIDTH, LOGO_WIDTH);
    GD.uncompress(RAM_CHR, logo_chr);
    GD.uncompress(RAM_PAL, logo_pal);
  }

public:  
  Display()
  {
    cylonDirection = 1;
    cylonDot = 0;
  }

  void begin()
  {  
    GD.begin();
    loadSprites();
    loadChars();
    loadLogo();
  }
  
  // Use the GD.h RGB(r,g,b) macro to form these color values
  void setColors(uint16_t foreground,
                 uint16_t antialias,
                 uint16_t background)
  {
    // Set the background color (note that the special characters make use of
    // transparent pixels, so we need to set the background color);
    GD.wr16(BG_COLOR, background);

    // Set the sprite palette colors
    GD.wr16(PALETTE4B + 0, foreground);
    GD.wr16(PALETTE4B + 2, antialias);
    GD.wr16(PALETTE4B + 4, background);
    
    // Set the 'special' character colors
    loadCharColor(CHAR_ID_DOT, 3, foreground); // Outline of the dot
    loadCharColor(CHAR_ID_DOT, 1, foreground); // Inside of the dot
    loadCharColor(CHAR_ID_HOLLOW_DOT, 3, foreground); // Outline of the hollow dot
    
  }

  // Draw a 5-digit number on the screen, centered left-to-right
  // Also draw the units, below the 1's digit
  // unitsFrame should be:  CAL_FRAME0, METERS_FRAME0, or WATTS_FRAME0
  void drawNumber(uint32_t num, byte unitsFrame)
  {
    clearText();
    GD.wr(SPR_DISABLE, 0);
    // Clip the value to maximum displayable value
    if (num > 99999) num = 99999;
    
    // Start with x such that a single digit is centered left-to-right, then
    // move it to the right by half a digit for each visible digit in the
    // final display.
    int x = 200 - 32; // screen width = 400, digit width = 64
    if (num > 9) x += 32;
    if (num > 99) x += 32;
    if (num > 999) x += 32;
    if (num > 9999) x += 32;
  
    int y = 128; // looks about right - depends on the height of the logo
  
    drawUnits(x - 8, y + (16*6), unitsFrame); // digits are 6 sprites tall
    
    for (int place = 0; place < 5; place++)
    {
      byte digit = num % 10;
      num /= 10;
      drawDigit(x, y, place, digit, num == 0);
      x -= 64;
    }
  }
  
  void drawColon(int x0, int y0)
  {
    int x = (x0 / 8) + 1;
    int y = (y0 / 8);
    GD.wr(RAM_PIC + x + 64*(y + 3), CHAR_ID_DOT);
    GD.wr(RAM_PIC + x + 64*(y + 8), CHAR_ID_DOT);
  }
  
  void drawTime(uint16_t sec)
  {
    clearText();
    GD.wr(SPR_DISABLE, 0);
    // Time is drawn as  mm:ss or m:ss (no leading zero)
    // The digits are made of sprites, but the colon is drawn with dots
    // The space for the colon is two blank columns, a column with 2 dots, and another
    // blank column.
    int seconds = sec % 60;
    int minutes = sec / 60;
    int sec_ones = seconds % 10;
    int sec_tens = seconds / 10;
    int min_ones = minutes % 10;
    int min_tens = minutes / 10;

    int x = 200 + ((64 + (4*8) + 64 + 64) / 2) - 64;;
    if (min_tens > 0)
    {
      x -= 64/2;
    }
    int y = 128;
    
    drawUnits(x - 8, y + (16*6), PER500_FRAME0); // digits are 6 sprites tall
    
    drawDigit(x, y, 0, sec_ones, false);
    x -= 64;
    drawDigit(x, y, 1, sec_tens, false);
    x -= 24;
    drawColon(x, y);
    x -= 64;
    drawDigit(x, y, 2, min_ones, false);
    x -= 64;
    drawDigit(x, y, 3, min_tens, true);
    drawDigit(x, y, 4, 0, true);
  }
  
  int8_t cylonDirection;
  int8_t cylonDot;
  void clearText()
  {
    GD.fill(RAM_PIC + 64 * 13, CHAR_ID_TRANSPARENT, 64*15);
  }
  void drawSearching()
  {
    clearText();
    GD.wr(SPR_DISABLE, 1);
    for (int i = 0; i < 10; i++) 
    {
      int loc = 16 + 2*i + (64 * 20);
      int dot = CHAR_ID_DOT;
      if (i == cylonDot) dot = CHAR_ID_HOLLOW_DOT;
      GD.wr(RAM_PIC + loc, dot);
    }
    if (cylonDirection == 1)
    {
      cylonDot++;
      if (cylonDot > 9)
      {
        cylonDot = 8;
        cylonDirection = -1;
      }
    }
    else
    {
      cylonDot--;
      if (cylonDot < 0)
      {
        cylonDot = 1;
        cylonDirection = 1;
      }
    }
  }

};

#endif

