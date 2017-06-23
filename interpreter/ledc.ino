// #define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DEBUG(m) Serial.println (m);
#include <SoftwareSerial.h>
#else
#define DEBUG(m)
#endif

#define LED_STRIPS        12
#define LED_REGISTERS      4
#define LED_GROUPS         4

#define LEDS_PER_STRIP    16

#define ENABLE_BRIGHTNESS  0
#define DEFAULT_BRIGHTNESS 1
#define LED_BANKS          2
#define LED_BANK0          3
#define LED_BANK1         14
#define LED_CLOCK          2

// VM Opcodes
#define OP_DEST            0
#define OP_SETS            1
#define OP_SETI            2
#define OP_COPY            5
#define OP_MIRRORS         6
#define OP_SHIFTR          8
#define OP_GROUPADDS      10
#define OP_GROUPADDI      11
#define OP_FRAME          12
#define OP_GOTO           13
#define OP_COLOR1         14
#define OP_COLOR2         15

struct led {
#if ENABLE_BRIGHTNESS
  uint8_t brightness;
#endif
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

led strips [LED_STRIPS][LEDS_PER_STRIP];
led regs [LED_REGISTERS][LEDS_PER_STRIP];
uint32_t groups [LED_GROUPS];

uint16_t ps = 0;
uint16_t pc = 0;
uint16_t lpc = 0;
uint8_t dest = 0;
led color;
char *p;

#if ENABLE_BRIGHTNESS
#define BLACK { 0, 0, 0, 0 }
#else
#define BLACK { 0, 0, 0 }
#endif

void setup () {
#ifdef ENABLE_DEBUG
  // Enable serial
  Serial.begin (19200);
#endif
  
  // Setup GPIO-Pins
  DEBUG ("Setting up LED-Pins");
  
  pinMode (LED_CLOCK, OUTPUT);
  digitalWrite (LED_CLOCK, LOW);

  uint8_t strip = LED_STRIPS, bank;
  
  while (strip--) {
    bank = strip / (LED_STRIPS / LED_BANKS);

    if (bank == 0)
      dest = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK0;
#if LED_BANKS > 1
    else if (bank == 1)
      dest = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK1;
#  if LED_BANKS > 2
    else if (bank == 2)
      dest = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK2;
#    if LED_BANKS > 3
    else if (bank == 3)
      dest = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK3;
#    endif
#  endif
#endif
    else
      continue;

    DEBUG ("  Enable PIN D" + String (dest) + " as Output for " + String (strip));
    
    pinMode (dest, OUTPUT);
    digitalWrite (dest, LOW);
  }
  
  // Set programm and reset VM
  p = "\xe2\xff\xff\x00\x84\x0c\x00\x00\x10\x00\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x10\x10\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x10\x20\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x10\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x64\x00\x00\x00\x11\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x12\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x13\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x14\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x15\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x16\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x17\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x18\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x10\x00\x00\x00\x19\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x10\x10\x00\x00\x1a\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x10\x20\x00\x00\x1b\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x10\x30\x00\x00\x1c\x30\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x11\x30\x00\x00\x1d\x20\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x12\x30\x00\x00\x1e\x10\x00\x00\xc0\x02\x00\x00\x84\x0c\x00\x00\x13\x30\x00\x00\x1f\x00\x00\x00\xc0\x02\x00\x00\xd0\x05\xc0\x00";
  ps = 268;
  pc = 0;
  lpc = 0;
  dest = 0;
  color = BLACK;
  
  memset (&strips, 0, LED_STRIPS * LEDS_PER_STRIP * sizeof (led));
  memset (&regs, 0, LED_REGISTERS * LEDS_PER_STRIP * sizeof (led));
  memset (&groups, 0, LED_GROUPS * sizeof (uint32_t));

  DEBUG ("Setup done");
}

void setPixel (uint8_t addr, uint8_t offset, led color) {
  // Make sure the offset is valid
  if (offset >= LEDS_PER_STRIP)
    return;
  
  // Set a group of pixels
  if (addr > 0x1F) {
    // Truncate to real group-address
    addr -= 0x20;

    // Check if the group really exists
    if (addr >= LED_GROUPS)
      return;

    uint32_t group = groups [addr];
    addr = 0;

    while (group) {
      if (group & 0x01)
        setPixel (addr, offset, color);

      addr++;
      group >>= 1;
    }
    
    return;
  }

  // Set a register
  if (addr > 0x0F) {
    // Truncate to real register-address
    addr -= 0x10;

    // Check if the register is existant
    if (addr >= LED_REGISTERS)
      return;
    
    regs [addr][offset] = color;
     
    return;
  }

  // Check if the strip exists
  if (addr >= LED_STRIPS)
    return;
  
  strips [addr][offset] = color;
}

void copyPixels (uint8_t src, uint8_t dst, uint8_t start, uint8_t len, uint8_t offset) {
  uint8_t pos;
  
  // Convert group-address to a real one
  if (src > 0x1F) {
    // Rewrite source to real group-address
    src -= 0x20;

    // Make sure the group exists
    if (src >= LED_GROUPS) {
      // Set everything to black
      while (len--)
        setPixel (dst, offset + len, BLACK);
      
      return;
    }

    // Find something real on the group
    uint32_t group = groups [src];
    src = 0;

    while (group) {
      if (group & 0x01)
        break;

      src++;
      group >>= 1;
    }

    // Check if a real source was found
    if (group & 0x01 == 0x00) {
      // Set everything to black
      while (len--)
        setPixel (dst, offset + len, BLACK);
      
      return;
    }
  }
  
  // Copy pixels from a register
  if (src > 0x0F) {
    src -= 0x10;
    
    while (len--)
      if ((src < LED_REGISTERS) && (start + len < LEDS_PER_STRIP))
        setPixel (dst, offset + len, regs [src][start + len]);
      else
        setPixel (dst, offset + len, BLACK);
    
    return;
  }

  // Copy from a strip
  while (len--)
    if ((src < LED_STRIPS) && (start + len < LEDS_PER_STRIP))
      setPixel (dst, offset + len, strips [src][start + len]);
    else
      setPixel (dst, offset + len, BLACK);
}
void loop () {
  // Check if there is something to execute
  if (pc >= ps)
    return delay (500);
  
  // Process current opcode
  uint8_t p1, p2;
  
  switch ((p [pc] >> 4) & 0x0F) {
    case OP_COLOR2:
    case OP_COLOR1:
#if ENABLE_BRIGHTNESS
      memcpy (&color, p + pc, 4);
      DEBUG ("OP_COLOR: Bright " + String (color.brigthness) + " Red " + String (color.red) + " Green " + String (color.green) + " Blue " + String (color.blue));
#else
      memcpy (&color, p + pc + 1, 3);
      DEBUG ("OP_COLOR: Red " + String (color.red) + " Green " + String (color.green) + " Blue " + String (color.blue));
#endif
      
      break;
    case OP_DEST:
      dest = ((p [pc] & 0x0F) << 2) | ((p [pc + 1] >> 6) & 0x03);
      DEBUG ("OP_DEST: New Destination " + String (dest));
      
      break;
    case OP_SETS:
      p1 = (p [pc] & 0x0F);
      p2 = ((p [pc + 1] >> 4) & 0x0F) + 1;

      while (p2--)
        setPixel (dest, p1++, color);

      DEBUG ("OP_SETS");
      
      break;
    case OP_SETI:
      p1 = 0;

      while (p1++ <= (p [pc] & 0x0F)) // TODO: Really <= or <?!
        setPixel (dest, (p [pc + (p1 + 1) / 2] >> ((p1 % 2) == 1 ? 4 : 0)) & 0x0F, color);

      DEBUG ("OP_SETI");
      
      break;
    case OP_COPY:
      copyPixels (
        ((p [pc] & 0x0F) << 2) | (p [pc + 1] >> 6),
        (p [pc + 1] & 0x3F),
        (p [pc + 2] >> 4),
        (p [pc + 2] & 0x0F) + 1,
        (p [pc + 3] >> 4)
      );

      DEBUG ("OP_COPY");
      
      break;
    case OP_MIRRORS:
      p1 = ((p [pc] & 0x0F) << 2) | (p [pc + 1] >> 6);
      p2 = (p [pc + 2] >> 2) + 1;

      while (p2-- > 0)
        copyPixels (
          p1,
          (p [pc + 1] & 0x3F) + p2,
          0,
          LEDS_PER_STRIP,
          0
        );

      DEBUG ("OP_MIRRORS");
      
      break;
    case OP_SHIFTR:
      if (p [pc] & 0x0C) {
        p1 = ((p [pc] & 0x03) << 4) | (p [pc + 1] >> 4);
        p2 = (p [pc + 1] & 0x0F);
  
        while (p2-- > 0)
          copyPixels (
            p1 + p2,
            p1 + p2 + 1,
            0,
            LEDS_PER_STRIP,
            0
          );

        p2 = 16;
        
        while (p2--)
          setPixel (p1, p2, BLACK);
      }

      if (p [pc] & 0x0C > 0x04) {
        p1 = (p [pc + 2] >> 2);
        p2 = ((p [pc + 2] & 0x03) << 2) | (p [pc + 3] >> 6);
  
        while (p2-- > 0)
          copyPixels (
            p1 + p2,
            p1 + p2 + 1,
            0,
            LEDS_PER_STRIP,
            0
          );

        p2 = 16;
        
        while (p2--)
          setPixel (p1, p2, BLACK);
      }

      DEBUG ("OP_SHIFTR");
      
      break;
    case OP_GROUPADDS:
      p1 = (((p [pc] & 0x0F) << 2) | (p [pc + 1] >> 6)) - 0x20;
      p2 = p [pc + 2] >> 2;
      
      // Make sure the group is valid
      if (p1 >= LED_GROUPS) {
        DEBUG ("OP_GROUPADDS: Nothing done");
        break;
      }

      // Join the group
      while (p2--) {
        groups [p1] |= (1 << (p [pc + 1] & 0x3F) + p2);
        DEBUG ("OP_GROUPADDS: Add " + String ((p [pc + 1] & 0x3F) + p2) + " to group " + String (p1));
      }

      break;
    case OP_GROUPADDI:
      p1 = (((p [pc] & 0x0F) << 2) | (p [pc + 1] >> 6)) - 0x20;
      p2 = ((p [pc + 1] >> 4) & 0x03);

      // Make sure there is something to do and group is valid
      if (!p2 || (p1 >= LED_GROUPS))
        break;

      // Join the group
      groups [p1] |= (1 << (((p [pc + 1] & 0x0F) << 4) | (p [pc + 2] >> 4)));

      if (p2 > 1) {
        groups [p1] |= (1 << (p [pc + 2] & 0x3F));

        if (p2 > 2)
          groups [p1] |= (1 << (p [pc + 3] >> 2));
      }

      DEBUG ("OP_GROUPADDI");

      break;
    case OP_FRAME:
      uint8_t strip = LED_STRIPS;
      uint8_t led = LEDS_PER_STRIP;
      uint8_t bit;

      // p1 bank
      // p2 physical pin

      // Say Hello (\x00\x00\x00\x00)
      for (bit = 0; bit < 32; bit++) {
        while (strip--) {
          p1 = strip / (LED_STRIPS / LED_BANKS);
          
          if (p1 == 0)
            p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK0;
#if LED_BANKS > 1
          else if (p1 == 1)
            p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK1;
#  if LED_BANKS > 2
          else if (p1 == 2)
            p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK2;
#    if LED_BANKS > 3
          else if (p1 == 3)
            p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK3;
#    endif
#  endif
#endif
          else
            continue;
          
          digitalWrite (p2, LOW);
        }
        
        // Commit current bit
        digitalWrite (LED_CLOCK, HIGH);
        digitalWrite (LED_CLOCK, LOW);
      }
      
      // Process each LED
      while (led--) {
        // Process each bit of LED-Data
        for (bit = 0; bit < 32; bit++) {
          // Set bits for each strip
          strip = LED_STRIPS;
          
          while (strip--) {
            p1 = strip / (LED_STRIPS / LED_BANKS);
            
            if (p1 == 0)
              p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK0;
#if LED_BANKS > 1
            else if (p1 == 1)
              p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK1;
#  if LED_BANKS > 2
            else if (p1 == 2)
              p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK2;
#    if LED_BANKS > 3
            else if (p1 == 3)
              p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK3;
#    endif
#  endif
#endif
            else
              continue;
            
            digitalWrite (
              p2,
              (bit < 3  ? HIGH : // Signature
#if ENABLE_BRIGHTNESS
              (bit < 8  ? (strips [strip][led].brightness & (1 << (7 - bit)) ? HIGH : LOW) : // Brightness
#else
              (bit < 8  ? (DEFAULT_BRIGHTNESS & (1 << (7 - bit)) ? HIGH : LOW) :             // Default Brightness (if compiled without support for individual brightness)
#endif
              (bit < 16 ? (strips [strip][led].blue  & (1 << (15 - bit)) ? HIGH : LOW) :     // Blue
              (bit < 24 ? (strips [strip][led].green & (1 << (23 - bit)) ? HIGH : LOW) :     // Green
                          (strips [strip][led].red   & (1 << (31 - bit)) ? HIGH : LOW)       // Red
            )))));
          }
          
          // Commit current bit
          digitalWrite (LED_CLOCK, HIGH);
          digitalWrite (LED_CLOCK, LOW);
        }
      }

      // Say Goodbye (\xFF\xFF\xFF\xFF)
      for (bit = 0; bit < 32; bit++) {
        strip = LED_STRIPS;
        
        while (strip--) {
          p1 = strip / (LED_STRIPS / LED_BANKS);
          
          if (p1 == 0)
            p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK0;
#if LED_BANKS > 1
          else if (p1 == 1)
            p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK1;
#  if LED_BANKS > 2
          else if (p1 == 2)
            p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK2;
#    if LED_BANKS > 3
          else if (p1 == 3)
            p2 = (strip % (LED_STRIPS / LED_BANKS)) + LED_BANK3;
#    endif
#  endif
#endif
          else
            continue;
          
          digitalWrite (p2, HIGH);
        }
        
        // Commit current bit
        digitalWrite (LED_CLOCK, HIGH);
        digitalWrite (LED_CLOCK, LOW);
      }
      
      // Delay current execution
      DEBUG ("OP_FRAME: Delay for " + String (((p [pc] & 0x0F) << 12) | (p [pc + 1] << 4) | ((p [pc + 2] >> 4) & 0x0F)));
      
      delay ((((p [pc] & 0x0F) << 12) | (p [pc + 1] << 4) | ((p [pc + 2] >> 4) & 0x0F)));
      
      break;
    case OP_GOTO:
      // OP_GOTO is handled below when pc is increased/changed
      DEBUG ("OP_GOTO");
      
      break;
    default:
      DEBUG ("Unknown OP: " + String ((p [pc] >> 4) & 0x0F));
  }

  // Update Programm-Counter
  lpc = pc;
  
  if (((p [pc] >> 4) & 0x0F) == OP_GOTO)
    pc = ((p [pc] & 0x0F) << 12) | (p [pc + 1] << 4) | ((p [pc + 2] >> 4) & 0x0F);
  else
    pc += 4;

  DEBUG ("  Done. OP " + String ((p [lpc] >> 4) & 0x0F) + ", old PC " + String (lpc) + ", new PC " + String (pc));
}
