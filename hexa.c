// HEXA OS Kernel

// Manual type definitions
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef unsigned int size_t;

// BSS symbols defined in linker script
extern char _bss_start[], _bss_end[];

#define VGA_BUFFER ((uint16_t *)0xB8000)

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_color = 0x0F; // White on black

// ---- User & File System ----
#define MAX_USERS 12
#define MAX_FILES 64
#define NAME_MAX 32
#define CONTENT_MAX 512

// Forward declarations for password hash functions
static uint32_t pwd_hash(const char *str, uint8_t salt);
static void encode_pwd(char out[NAME_MAX], const char *pass, uint8_t salt);
static int check_pwd(const char *pass, const char *encoded);

static struct {
  char name[NAME_MAX];
  char pass_hash[NAME_MAX];
  int is_root;
} u_table[MAX_USERS];
static int u_count = 0;
static int u_cur = 0;

// Permission bits: owner (bits 6-8), group (3-5), other (0-2)
// r=4, w=2, x=1
#define PERM_OWNER_R 0x100
#define PERM_OWNER_W 0x080
#define PERM_OWNER_X 0x040
#define PERM_GRP_R   0x020
#define PERM_GRP_W   0x010
#define PERM_GRP_X   0x008
#define PERM_OTH_R   0x004
#define PERM_OTH_W   0x002
#define PERM_OTH_X   0x001
#define PERM_DEFAULT 0x1A4  // owner rw-, grp r--, oth r--

static struct {
  char name[NAME_MAX];
  char content[CONTENT_MAX];
  int size;
  int owner;
  uint16_t mode;
} f_table[MAX_FILES];
static int f_count = 0;
static int disk_ok = 0;

static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
  __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
  uint32_t ret;
  __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline uint16_t inw(uint16_t port) {
  uint16_t ret;
  __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
  __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// ----------------- String Library -----------------
int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
  char *ret = dest;
  while ((*dest++ = *src++))
    ;
  return ret;
}

size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len])
    len++;
  return len;
}

void *memset(void *dest, int val, size_t len) {
  unsigned char *ptr = (unsigned char *)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

void *memcpy(void *dest, const void *src, size_t len) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  while (len-- > 0)
    *d++ = *s++;
  return dest;
}

char *strcat(char *dest, const char *src) {
  char *ret = dest;
  while (*dest) dest++;
  while ((*dest++ = *src++));
  return ret;
}

int atoi(const char *str) {
  int res = 0, sign = 1, i = 0;
  if (str[0] == '-') {
    sign = -1;
    i++;
  }
  for (; str[i] != '\0'; ++i) {
    if (str[i] >= '0' && str[i] <= '9')
      res = res * 10 + str[i] - '0';
    else
      break;
  }
  return sign * res;
}

void itoa(int num, char *str, int base) {
  int i = 0, isNegative = 0;
  unsigned int unum;
  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }
  if (base == 10) {
    if (num < 0) { isNegative = 1; unum = (unsigned int)(-num); }
    else { unum = (unsigned int)num; }
  } else {
    unum = (unsigned int)num;
  }
  while (unum != 0) {
    unsigned int rem = unum % (unsigned int)base;
    str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
    unum = unum / (unsigned int)base;
  }
  if (isNegative)
    str[i++] = '-';
  str[i] = '\0';
  int start = 0, end = i - 1;
  while (start < end) {
    char t = str[start];
    str[start] = str[end];
    str[end] = t;
    start++;
    end--;
  }
}

// ----------------- Password Hashing -----------------
static uint32_t pwd_hash(const char *str, uint8_t salt) {
  uint32_t h = 5381 + salt;
  int c;
  while ((c = *str++))
    h = ((h << 5) + h) + (unsigned char)c;
  return h;
}

static void encode_pwd(char out[NAME_MAX], const char *pass, uint8_t salt) {
  char tmp[16];
  out[0] = '0'; out[1] = '0'; out[2] = ' ';
  itoa(salt, tmp, 16);
  int sl = strlen(tmp);
  out[0] = '0'; out[1] = '0';
  for (int i = 0; i < sl && i < 2; i++)
    out[1 - i] = tmp[sl - 1 - i];
  out[2] = ' ';
  uint32_t h = pwd_hash(pass, salt);
  itoa(h, tmp, 16);
  strcpy(out + 3, tmp);
}

static int check_pwd(const char *pass, const char *encoded) {
  uint8_t salt = 0;
  for (int i = 0; i < 2; i++) {
    char c = encoded[i];
    if (c >= '0' && c <= '9') salt = salt * 16 + (c - '0');
    else if (c >= 'a' && c <= 'f') salt = salt * 16 + (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') salt = salt * 16 + (c - 'A' + 10);
  }
  const char *hp = encoded + 3;
  uint32_t sh = 0;
  while (*hp) {
    char c = *hp;
    if (c >= '0' && c <= '9') sh = sh * 16 + (c - '0');
    else if (c >= 'a' && c <= 'f') sh = sh * 16 + (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') sh = sh * 16 + (c - 'A' + 10);
    else break;
    hp++;
  }
  return pwd_hash(pass, salt) == sh;
}

// ----------------- Hardware Drivers -----------------

// CPUID
static inline void cpuid(uint32_t code, uint32_t *a, uint32_t *d) {
  __asm__ volatile("cpuid" : "=a"(*a), "=d"(*d) : "a"(code) : "ecx", "ebx");
}

static inline void cpuid_string(uint32_t code, uint32_t where[4]) {
  __asm__ volatile("cpuid"
                   : "=a"(*where), "=b"(*(where + 1)), "=c"(*(where + 2)),
                     "=d"(*(where + 3))
                   : "a"(code));
}

// CMOS RTC
int get_update_in_progress_flag() {
  outb(0x70, 0x0A);
  return (inb(0x71) & 0x80);
}

uint8_t get_rtc_register(int reg) {
  outb(0x70, reg);
  return inb(0x71);
}

// PCI
uint32_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func,
                              uint8_t offset) {
  uint32_t address;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;
  address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                       (offset & 0xFC) | ((uint32_t)0x80000000));
  outl(0xCF8, address);
  return (uint32_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
}

// ----------------- Video / Terminal -----------------
void update_cursor() {
  uint16_t pos = cursor_y * 80 + cursor_x;
  outb(0x3D4, 0x0F);
  outb(0x3D5, (uint8_t)(pos & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static inline void serial_putc(char c) {
  outb(0x3F8, c);
}

void put_char(char c, uint8_t color) {
  if (c == '\b') {
    if (cursor_x > 0)
      cursor_x--;
    else if (cursor_y > 0) {
      cursor_y--;
      cursor_x = 79;
    }
    VGA_BUFFER[cursor_y * 80 + cursor_x] = ((uint16_t)color << 8) | ' ';
    serial_putc('\b');
    serial_putc(' ');
    serial_putc('\b');
    update_cursor();
    return;
  }
  serial_putc(c);
  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
    serial_putc('\r');
  } else if (c == '\r') {
    cursor_x = 0;
  } else {
    VGA_BUFFER[cursor_y * 80 + cursor_x] = ((uint16_t)color << 8) | (uint8_t)c;
    cursor_x++;
  }

  if (cursor_x >= 80) {
    cursor_x = 0;
    cursor_y++;
  }
  if (cursor_y >= 25) {
    for (int i = 0; i < 24 * 80; i++)
      VGA_BUFFER[i] = VGA_BUFFER[i + 80];
    for (int i = 24 * 80; i < 25 * 80; i++)
      VGA_BUFFER[i] = ((uint16_t)color << 8) | ' ';
    cursor_y = 24;
  }
  update_cursor();
}

void print_string(const char *str) {
  while (*str) {
    put_char(*str, current_color);
    str++;
  }
}

void print_color(const char *str, uint8_t color) {
  uint8_t old = current_color;
  current_color = color;
  print_string(str);
  current_color = old;
}

void clear_screen(void) {
  for (int i = 0; i < 80 * 25; i++)
    VGA_BUFFER[i] = ((uint16_t)current_color << 8) | ' ';
  cursor_x = 0;
  cursor_y = 0;
  const char *ca = "\033[2J\033[H";
  while (*ca)
    serial_putc(*ca++);
}

// ----------------- Keyboard Driver & Shell Input -----------------
const char kbd_map[128] = {
    0,    27,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']',
    '\n', 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,    '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
    0,    0,    0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,    0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0};
const char kbd_map_shift[128] = {
    0,    27,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,    ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,    0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,   0,   0,
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0};
static int shift_pressed = 0;
#define HIST_MAX 16
static char hist_buf[HIST_MAX][128];
static int hist_idx = 0;
static int hist_count = 0;
static int hist_scroll = -1;

char get_char(int *special_key) {
  uint8_t scancode;
  if (special_key)
    *special_key = 0;
  while (1) {
    if (inb(0x3F8 + 5) & 1) {
      char c = inb(0x3F8);
      if (c == '\r')
        c = '\n';
      if (c == 127)
        c = '\b';
      if (c == '\033') { // Handle arrow keys from serial
        char b = inb(0x3F8);
        if (b == '[') {
          char d = inb(0x3F8);
          if (d == 'A' && special_key) {
            *special_key = 1;
            return 0;
          }
          if (d == 'B' && special_key) {
            *special_key = 2;
            return 0;
          }
        }
      }
      return c;
    }
    if (inb(0x64) & 1) {
      scancode = inb(0x60);
      if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        continue;
      }
      if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        continue;
      }
      if (scancode == 0x48 && special_key) {
        *special_key = 1;
        return 0;
      } // UP ARROW
      if (scancode == 0x50 && special_key) {
        *special_key = 2;
        return 0;
      } // DOWN ARROW
      if (!(scancode & 0x80) && scancode < 128) {
        char c = shift_pressed ? kbd_map_shift[scancode] : kbd_map[scancode];
        if (c)
          return c;
      }
    }
  }
}

void get_line(char *buffer, size_t max_len) {
  size_t i = 0;
  while (1) {
    int special = 0;
    char c = get_char(&special);
    if (special == 1) { // UP arrow
      if (hist_count == 0 || hist_scroll >= hist_count - 1) continue;
      hist_scroll++;
      while (i > 0) { put_char('\b', current_color); i--; }
      int idx = (hist_idx - hist_scroll - 1 + HIST_MAX) % HIST_MAX;
      size_t hlen = strlen(hist_buf[idx]);
      for (size_t k = 0; k < hlen && k < max_len - 1; k++) {
        buffer[k] = hist_buf[idx][k];
        put_char(buffer[k], current_color);
      }
      i = hlen; buffer[hlen] = '\0';
      continue;
    }
    if (special == 2) { // DOWN arrow
      while (i > 0) { put_char('\b', current_color); i--; }
      if (hist_scroll > 0) {
        hist_scroll--;
        int idx = (hist_idx - hist_scroll - 1 + HIST_MAX) % HIST_MAX;
        size_t hlen = strlen(hist_buf[idx]);
        for (size_t k = 0; k < hlen && k < max_len - 1; k++) {
          buffer[k] = hist_buf[idx][k];
          put_char(buffer[k], current_color);
        }
        i = hlen; buffer[hlen] = '\0';
      } else {
        hist_scroll = -1;
        i = 0; buffer[0] = '\0';
      }
      continue;
    }
    if (c == '\n') {
      print_string("\n");
      buffer[i] = '\0';
      if (i > 0) hist_scroll = -1;
      break;
    } else if (c == '\b') {
      if (i > 0) { i--; put_char('\b', current_color); }
    } else {
      if (i < max_len - 1) { buffer[i++] = c; put_char(c, current_color); }
    }
  }
}

static uint32_t ticks = 0;
static char hostname_str[32] = "hexaos";
static inline void do_tick() {
  for (int i = 0; i < 200; i++) {
    __asm__ volatile("pause");
  }
  ticks++;
}
void sleep_ticks(uint32_t t) {
  uint32_t target = ticks + t;
  while (ticks < target)
    do_tick();
}

// Simple PRNG
static uint32_t rseed = 0;
uint32_t rand() {
  rseed = rseed * 1103515245 + 12345;
  return (rseed / 65536) % 32768;
}

// ----------------- Shell Commands -----------------
static int find_file(const char *name);

void cmd_help() {
  print_string("HEXA OS 4.0 Commands\n");
  print_string("------------------------------------\n");
  print_string(" System:  help, clear, reboot, halt, panic, sleep\n");
  print_string("          hostname, uptime, true, false, shutdown\n");
  print_string(" HW:      date, cpuinfo, lspci, outb, inb, neofetch\n");
  print_string(" Str:     echo, len, hex, reverse, tolower, toupper\n");
  print_string("          morse, tr, seq, basename, dirname\n");
  print_string(" Apps:    calc, rand, ascii, palette, matrix, guess\n");
  print_string("          wc, tail, sort, which, env, tee\n");
  print_string(" Files:   touch, cat, ls, rm, write, append, edit\n");
  print_string("          mv, cp, head, tail, grep, find, diff, uniq\n");
  print_string("          chmod, chown, pwd, mkdir\n");
  print_string(" Users:   login, logout, useradd, passwd, whoami, id\n");
  print_string("          diese, who\n");
  print_string(" Unix:    alias, unalias, ps, kill, basename, dirname\n");
  print_string("          seq, tr, tee\n");
  print_string(" Fun:     banner, fortune, yes, time, cowsay, cmatrix\n");
  print_string("          logo, sl, dice, 8ball, russian, insult\n");
  print_string("          excuse, compliment, hack, bsod\n");
  print_string(" Info:    uname, uptime, about, mem, beep, history\n");
  if (find_file(".games") >= 0)
    print_string(" Games:   snake, tictactoe, hangman, memory\n");
  print_string("------------------------------------\n");
  print_string(" Pkg mgmt: ayo help\n");
}

void cmd_kaput() { print_string("Kaput, is this kaput?\n"); }

void cmd_date() {
  while (get_update_in_progress_flag())
    ;
  uint8_t s = get_rtc_register(0x00);
  uint8_t m = get_rtc_register(0x02);
  uint8_t h = get_rtc_register(0x04);
  uint8_t d = get_rtc_register(0x07);
  uint8_t mo = get_rtc_register(0x08);
  uint8_t y = get_rtc_register(0x09);
  uint8_t rtc_b = get_rtc_register(0x0B);
  if (!(rtc_b & 0x04)) {
    s = (s & 0x0F) + ((s / 16) * 10);
    m = (m & 0x0F) + ((m / 16) * 10);
    h = ((h & 0x0F) + (((h & 0x70) / 16) * 10)) | (h & 0x80);
    d = (d & 0x0F) + ((d / 16) * 10);
    mo = (mo & 0x0F) + ((mo / 16) * 10);
    y = (y & 0x0F) + ((y / 16) * 10);
  }
  char buf[16];
  print_string("Date: 20");
  itoa(y, buf, 10);
  print_string(buf);
  print_string("-");
  itoa(mo, buf, 10);
  print_string(buf);
  print_string("-");
  itoa(d, buf, 10);
  print_string(buf);
  print_string(" Time: ");
  itoa(h, buf, 10);
  print_string(buf);
  print_string(":");
  itoa(m, buf, 10);
  print_string(buf);
  print_string(":");
  itoa(s, buf, 10);
  print_string(buf);
  print_string("\n");
}

void cmd_cpuinfo() {
  uint32_t a, d;
  cpuid(1, &a, &d);
  uint32_t brand[4];
  cpuid_string(0, brand);
  print_string("CPU Vendor: ");
  char v[13];
  ((uint32_t *)v)[0] = brand[1];
  ((uint32_t *)v)[1] = brand[3];
  ((uint32_t *)v)[2] = brand[2];
  v[12] = 0;
  print_string(v);
  print_string("\n");
  char buf[16];
  itoa((a >> 8) & 0xF, buf, 10);
  print_string("Family: ");
  print_string(buf);
  itoa((a >> 4) & 0xF, buf, 10);
  print_string(" Model: ");
  print_string(buf);
  print_string("\n");
}

void cmd_lspci() {
  print_string("Scanning PCI bus (first 10 devices)...\n");
  int count = 0;
  for (uint16_t bus = 0; bus < 5; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      uint16_t vendor = pci_config_read_word(bus, slot, 0, 0);
      if (vendor != 0xFFFF) {
        uint16_t device = pci_config_read_word(bus, slot, 0, 2);
        char buf[16];
        print_string("Bus ");
        itoa(bus, buf, 10);
        print_string(buf);
        print_string(" Slot ");
        itoa(slot, buf, 10);
        print_string(buf);
        print_string(": Vendor=");
        itoa(vendor, buf, 16);
        print_string(buf);
        print_string(" Device=");
        itoa(device, buf, 16);
        print_string(buf);
        print_string("\n");
        count++;
        if (count > 10)
          return;
      }
    }
  }
}

void cmd_calc(const char *args) {
  char a1[16] = {0}, op = 0, a2[16] = {0};
  int i = 0, j = 0;
  while (args[i] && args[i] != ' ')
    a1[j++] = args[i++];
  while (args[i] == ' ')
    i++;
  op = args[i++];
  while (args[i] == ' ')
    i++;
  j = 0;
  while (args[i])
    a2[j++] = args[i++];
  if (!op || !a1[0] || !a2[0]) {
    print_string("Usage: calc <num> <+|-|*|/> <num>\n");
    return;
  }
  int n1 = atoi(a1), n2 = atoi(a2), res = 0;
  if (op == '+')
    res = n1 + n2;
  else if (op == '-')
    res = n1 - n2;
  else if (op == '*')
    res = n1 * n2;
  else if (op == '/') {
    if (n2 == 0) {
      print_string("Div by zero!\n");
      return;
    }
    res = n1 / n2;
  } else {
    print_string("Invalid op\n");
    return;
  }
  char buf[32];
  itoa(res, buf, 10);
  print_string("= ");
  print_string(buf);
  print_string("\n");
}

void cmd_ascii() {
  for (int i = 32; i < 127; i++) {
    put_char(i, current_color);
    put_char(' ', current_color);
    if ((i - 31) % 16 == 0)
      print_string("\n");
  }
  print_string("\n");
}

void cmd_palette() {
  for (int i = 0; i < 16; i++) {
    char buf[16];
    itoa(i, buf, 10);
    uint8_t old = current_color;
    current_color = i;
    print_string("Color ");
    print_string(buf);
    print_string("  ");
    current_color = old;
    if (i == 7)
      print_string("\n");
  }
  print_string("\n");
}

void cmd_hex(const char *args) {
  char buf[16];
  itoa(atoi(args), buf, 16);
  print_string("0x");
  print_string(buf);
  print_string("\n");
}

void cmd_reverse(const char *args) {
  int len = strlen(args);
  for (int i = len - 1; i >= 0; i--)
    put_char(args[i], current_color);
  print_string("\n");
}

void cmd_matrix() {
  print_string("Press any key to exit matrix...\n");
  sleep_ticks(500); // 1s
  while (1) {
    if ((inb(0x64) & 1) || (inb(0x3F8 + 5) & 1)) {
      get_char(0);
      break;
    }
    put_char((rand() % 94) + 33, 0x0A); // green chars
    do_tick();
    do_tick();
  }
  print_string("\n");
}

void cmd_guess() {
  int secret = (rand() % 10) + 1;
  print_string("Guess a number between 1 and 10!\n");
  char buf[32];
  while (1) {
    print_string("guess> ");
    get_line(buf, 32);
    if (buf[0] == 'q')
      break;
    int g = atoi(buf);
    if (g == secret) {
      print_color("Correct!\n", 0x0A);
      break;
    } else if (g < secret)
      print_string("Too low.\n");
    else
      print_string("Too high.\n");
  }
}

void cmd_panic(void) {
  print_color("*** KERNEL PANIC ***\n\n", 0x4F);
  const char *err[] = {
    "FATAL: Page fault at 0xDEADBEEF",
    "IRQ conflict on vector 13",
    "Stack overflow in process 0",
    "NULL pointer dereference",
    "Invalid opcode at EIP=0xCAFEBABE",
    "Segment not present in GDT",
    "Double fault: unrecoverable",
    "System timer failed to tick",
    "Memory corruption detected",
    "CPU exception: general protection",
    "Unhandled interrupt 0xE",
    "Kernel stack corrupted",
  };
  for (int i = 0; i < 30; i++) {
    print_color(err[i % 12], 0x4C);
    print_color(" -- SYSTEM HALTED\n", 0x4F);
    for (volatile int d = 0; d < 8000; d++);
    if ((inb(0x64) & 1)) { get_char(0); break; }
  }
  print_color("\nShutting down...\n", 0x4C);
  for (volatile int d = 0; d < 50000; d++);
  __asm__ volatile("cli; hlt");
}

// ---- Non-blocking input helpers for games ----
int kb_hit(void) {
  return (inb(0x64) & 1) || (inb(0x3F8 + 5) & 1);
}

char getch_nb(void) {
  if (inb(0x3F8 + 5) & 1) {
    char c = inb(0x3F8);
    if (c == '\r') c = '\n';
    if (c == 127) c = '\b';
    return c;
  }
  if (inb(0x64) & 1) {
    uint8_t sc = inb(0x60);
    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; return 0; }
    if (!(sc & 0x80) && sc < 128) return shift_pressed ? kbd_map_shift[sc] : kbd_map[sc];
  }
  return 0;
}

// ----- SNAKE GAME -----
void cmd_snake(void) {
  if (find_file(".games") < 0) {
    print_string("Package required: ayo add games\n");
    print_string("(use 'diese' if not root)\n");
    return;
  }
  int ox = 20, oy = 2, gw = 40, gh = 20;
  int sx[256], sy[256], slen = 3, sdir = 1, score = 0;
  int fx, fy, go = 0, t = 0;
  sx[0] = 5; sy[0] = 10; sx[1] = 4; sy[1] = 10; sx[2] = 3; sy[2] = 10;
  int ok;
  do {
    ok = 1; fx = rand() % (gw - 2) + 1; fy = rand() % (gh - 2) + 1;
    for (int i = 0; i < slen; i++) if (sx[i] == fx && sy[i] == fy) { ok = 0; break; }
  } while (!ok);

  clear_screen();
  for (int x = 0; x < gw; x++) {
    VGA_BUFFER[oy * 80 + ox + x] = 0x0F << 8 | '#';
    VGA_BUFFER[(oy + gh - 1) * 80 + ox + x] = 0x0F << 8 | '#';
  }
  for (int y = 0; y < gh; y++) {
    VGA_BUFFER[(oy + y) * 80 + ox] = 0x0F << 8 | '#';
    VGA_BUFFER[(oy + y) * 80 + ox + gw - 1] = 0x0F << 8 | '#';
  }
  cursor_x = 0; cursor_y = 0;
  print_string("SNAKE - WASD move, Q quit\n");

  while (!go) {
    do_tick(); t++;
    if (kb_hit()) {
      char c = getch_nb();
      if ((c == 'w' || c == 'W') && sdir != 2) sdir = 0;
      else if ((c == 's' || c == 'S') && sdir != 0) sdir = 2;
      else if ((c == 'a' || c == 'A') && sdir != 1) sdir = 3;
      else if ((c == 'd' || c == 'D') && sdir != 3) sdir = 1;
      else if (c == 'q' || c == 'Q') go = 2;
    }
    if (t % 4 != 0) continue;

    int nx = sx[0], ny = sy[0];
    if (sdir == 0) ny--; else if (sdir == 1) nx++; else if (sdir == 2) ny++; else nx--;
    if (nx <= 0 || nx >= gw - 1 || ny <= 0 || ny >= gh - 1) { go = 1; break; }
    for (int i = 0; i < slen; i++) if (nx == sx[i] && ny == sy[i]) { go = 1; break; }
    if (go) break;

    for (int i = slen - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
    sx[0] = nx; sy[0] = ny;

    if (nx == fx && ny == fy) {
      score += 10; slen++;
      if (slen >= 256) { go = 1; break; }
      sx[slen - 1] = sx[slen - 2]; sy[slen - 1] = sy[slen - 2];
      do {
        ok = 1; fx = rand() % (gw - 2) + 1; fy = rand() % (gh - 2) + 1;
        for (int i = 0; i < slen; i++) if (sx[i] == fx && sy[i] == fy) { ok = 0; break; }
      } while (!ok);
    }

    for (int y = 1; y < gh - 1; y++)
      for (int x = 1; x < gw - 1; x++)
        VGA_BUFFER[(oy + y) * 80 + ox + x] = 0x0F << 8 | ' ';
    VGA_BUFFER[(oy + fy) * 80 + ox + fx] = 0x0C << 8 | '@';
    for (int i = 0; i < slen; i++)
      VGA_BUFFER[(oy + sy[i]) * 80 + ox + sx[i]] = 0x0A << 8 | 'O';
  }

  clear_screen();
  if (go == 1) {
    char buf[16];
    itoa(score, buf, 10);
    print_string("Game Over! Score: ");
    print_string(buf);
    print_string("\n");
  }
  print_string("Press any key to continue...");
  get_char(0);
  clear_screen();
}

// ----- TIC-TAC-TOE -----
void cmd_tictactoe(void) {
  if (find_file(".games") < 0) {
    print_string("Package required: ayo add games\n"); return;
  }
  char board[9] = {'1','2','3','4','5','6','7','8','9'};
  int turn = 0, pos, win = 0, moves = 0;
  char line[16];

  while (!win && moves < 9) {
    clear_screen();
    print_string("Tic-Tac-Toe\n");
    print_string("Player 1 (X) | Player 2 (O)\n\n");
    for (int i = 0; i < 9; i += 3) {
      print_string(" ");
      put_char(board[i], 0x0F); print_string(" | ");
      put_char(board[i+1], 0x0F); print_string(" | ");
      put_char(board[i+2], 0x0F); print_string("\n");
      if (i < 6) print_string("---|---|---\n");
    }
    print_string("\nPlayer ");
    put_char(turn ? 'O' : 'X', 0x0F);
    print_string(", pick a position (1-9): ");
    get_line(line, 16);
    pos = atoi(line);
    if (pos < 1 || pos > 9 || board[pos-1] == 'X' || board[pos-1] == 'O') continue;

    board[pos-1] = turn ? 'O' : 'X';
    moves++;

    for (int i = 0; i < 3; i++) {
      if (board[i*3] == board[i*3+1] && board[i*3+1] == board[i*3+2]) { win = 1; break; }
      if (board[i] == board[i+3] && board[i+3] == board[i+6]) { win = 1; break; }
    }
    if (board[0] == board[4] && board[4] == board[8]) win = 1;
    if (board[2] == board[4] && board[4] == board[6]) win = 1;

    if (!win) turn = !turn;
  }

  clear_screen();
  if (win) {
    print_string("Player ");
    put_char(turn ? 'O' : 'X', 0x0F);
    print_string(" wins!\n");
  } else {
    print_string("It's a draw!\n");
  }
  print_string("Press any key to continue...");
  get_char(0);
  clear_screen();
}

// ----- MEMORY (Simon Says) -----
void cmd_memory(void) {
  if (find_file(".games") < 0) {
    print_string("Package required: ayo add games\n"); return;
  }
  char colors[] = {'R','G','B','Y'};
  char seq[32];
  char input[32];
  int len = 1, score = 0, go = 0;

  while (!go && len <= 20) {
    seq[len - 1] = colors[rand() % 4];
    clear_screen();
    print_string("MEMORY GAME - Watch the sequence!\n\n");
    for (int i = 0; i < len; i++) {
      uint8_t col;
      switch (seq[i]) {
        case 'R': col = 0x0C; break;
        case 'G': col = 0x0A; break;
        case 'B': col = 0x09; break;
        default:  col = 0x0E; break;
      }
      put_char(seq[i], col);
      put_char(' ', 0x0F);
      sleep_ticks(3000);
    }
    clear_screen();
    print_string("Your turn! Enter the sequence (letters R,G,B,Y):\n");
    get_line(input, 32);

    for (int i = 0; input[i]; i++)
      if (input[i] >= 'a' && input[i] <= 'z') input[i] -= 32;

    int ok = 1;
    for (int i = 0; i < len; i++) {
      if (input[i] != seq[i]) { ok = 0; break; }
      if (input[i] == '\0') { ok = 0; break; }
    }
    if (!ok) go = 1;
    else { score = len; len++; }
  }

  clear_screen();
  char buf[16];
  itoa(score, buf, 10);
  print_string("Memory score: ");
  print_string(buf);
  print_string(" rounds\nPress any key to continue...");
  get_char(0);
  clear_screen();
}

// ----- HANGMAN -----
void cmd_hangman(void) {
  if (find_file(".games") < 0) {
    print_string("Package required: ayo add games\n"); return;
  }
  const char *words[] = {
    "apple", "banana", "cherry", "dragon", "eagle",
    "flame", "grape", "house", "igloo", "joker",
    "knight", "lemon", "mango", "noble", "ocean",
    "piano", "queen", "river", "stone", "tiger"
  };
  int nwords = 20;
  int idx = rand() % nwords;
  const char *word = words[idx];
  int wlen = strlen(word);
  char guessed[16] = {0};
  int wrong = 0, max_wrong = 6;
  char display[32], buf[16];
  int win = 0;

  while (wrong < max_wrong) {
    for (int i = 0; i < wlen; i++) {
      int found = 0;
      for (int j = 0; guessed[j]; j++)
        if (guessed[j] == word[i]) { found = 1; break; }
      display[i * 2] = found ? word[i] : '_';
      display[i * 2 + 1] = ' ';
    }
    display[wlen * 2] = '\0';

    clear_screen();
    print_string("HANGMAN - Guess the word!\n\n");
    print_string("Word: ");
    print_string(display);
    print_string("\n\nWrong: ");
    itoa(wrong, buf, 10); print_string(buf);
    print_string("/6  Guessed: ");
    print_string(guessed);
    print_string("\n\nGuess a letter: ");

    char g = get_char(0);
    if (g >= 'A' && g <= 'Z') g += 32;
    if (g < 'a' || g > 'z') continue;

    int already = 0;
    for (int i = 0; guessed[i]; i++)
      if (guessed[i] == g) { already = 1; break; }
    if (already) continue;

    int glen = strlen(guessed);
    guessed[glen] = g;
    guessed[glen + 1] = '\0';

    int found = 0;
    for (int i = 0; i < wlen; i++)
      if (word[i] == g) { found = 1; break; }
    if (!found) wrong++;

    int all_found = 1;
    for (int i = 0; i < wlen; i++) {
      int f = 0;
      for (int j = 0; guessed[j]; j++)
        if (guessed[j] == word[i]) { f = 1; break; }
      if (!f) { all_found = 0; break; }
    }
    if (all_found) { win = 1; break; }
  }

  clear_screen();
  if (win) {
    print_string("You win! The word was: ");
    print_string(word);
    print_string("\n");
  } else {
    print_string("Game over! The word was: ");
    print_string(word);
    print_string("\n");
  }
  print_string("Press any key to continue...");
  get_char(0);
  clear_screen();
}

// ----- NEOFETCH (System Info) -----
void cmd_neofetch(void) {
  uint32_t brand[4], a, d;
  cpuid_string(0, brand);
  char v[13];
  ((uint32_t *)v)[0] = brand[1];
  ((uint32_t *)v)[1] = brand[3];
  ((uint32_t *)v)[2] = brand[2];
  v[12] = 0;

  cpuid(1, &a, &d);

  uint8_t s = get_rtc_register(0x00);
  uint8_t m = get_rtc_register(0x02);
  uint8_t h = get_rtc_register(0x04);
  uint8_t day = get_rtc_register(0x07);
  uint8_t mon = get_rtc_register(0x08);
  uint8_t yr = get_rtc_register(0x09);
  uint8_t rtc_b = get_rtc_register(0x0B);
  if (!(rtc_b & 0x04)) {
    s = (s & 0x0F) + ((s / 16) * 10);
    m = (m & 0x0F) + ((m / 16) * 10);
    h = ((h & 0x0F) + (((h & 0x70) / 16) * 10)) | (h & 0x80);
    day = (day & 0x0F) + ((day / 16) * 10);
    mon = (mon & 0x0F) + ((mon / 16) * 10);
    yr = (yr & 0x0F) + ((yr / 16) * 10);
  }

  char buf[16];
  clear_screen();
  print_color("       __________\n", 0x0B);
  print_color("      /  HEXA   \\\n", 0x0B);
  print_color("     /  OS 4.0  \\\n", 0x0B);
  print_color("    /____________\\\n", 0x0B);
  print_color("       HEXA OS\n", 0x0A);
  print_string(" -----------------------------\n");
  print_string(" OS:       HEXA OS 4.0 i386\n");
  print_string(" Host:     "); print_string(hostname_str); print_string("\n");
  print_string(" Version:  4.0 \"Secure Edition\"\n");
  print_string(" CPU:      "); print_string(v); print_string("\n");
  print_string(" Family:   ");
  itoa((a >> 8) & 0xF, buf, 10); print_string(buf);
  print_string("  Model: ");
  itoa((a >> 4) & 0xF, buf, 10); print_string(buf);
  print_string("\n");
  print_string(" Uptime:   ");
  itoa(ticks, buf, 10); print_string(buf);
  print_string(" ticks\n");
  print_string(" Users:    ");
  itoa(u_count, buf, 10); print_string(buf);
  print_string("  Current: ");
  print_string(u_table[u_cur].name);
  print_string("\n");
  print_string(" Files:    ");
  itoa(f_count, buf, 10); print_string(buf);
  print_string("  Disk: ");
  print_string(disk_ok ? "online" : "offline");
  print_string("\n");
  print_string(" Memory:   4000B VGA  Commands: 70+\n");
  print_string(" Date:     20");
  itoa(yr, buf, 10); print_string(buf);
  print_string("-"); itoa(mon, buf, 10); print_string(buf);
  print_string("-"); itoa(day, buf, 10); print_string(buf);
  print_string("  "); itoa(h, buf, 10); print_string(buf);
  print_string(":"); itoa(m, buf, 10); print_string(buf);
  print_string(":"); itoa(s, buf, 10); print_string(buf);
  print_string("\n");
  print_string(" Shell:    HEXA CLI v4.0\n");
  print_string(" Terminal: VGA Text Mode 80x25\n");
  print_string(" Packages: 20+ ayo packages\n");
  print_string(" -----------------------------\n");
  print_string("\nPress any key to continue...");
  get_char(0);
  clear_screen();
}

// ---- User System ----
void init_users(void) {
  strcpy(u_table[0].name, "root");
  encode_pwd(u_table[0].pass_hash, "root", 0xA5);
  u_table[0].is_root = 1;
  u_count = 1;
  u_cur = 0;
}

static int save_data(void);
static void load_data(void);

void do_login(void) {
  char name[NAME_MAX], pass[NAME_MAX];
  while (1) {
    clear_screen();
    print_color(
      "╭──────────────────────────────╮\n"
      "│                              │\n"
      "│         H E X A   O S        │\n"
      "│                              │\n"
      "╰──────────────────────────────╯\n", 0x0B);
    print_string("login: ");
    get_line(name, NAME_MAX);

    if (strcmp(name, "new") == 0) {
      if (u_count >= MAX_USERS) { print_string("Max users.\n"); sleep_ticks(1000); continue; }
      print_string("New username: ");
      get_line(name, NAME_MAX);
      if (!name[0]) continue;
      int exists = 0;
      for (int i = 0; i < u_count; i++)
        if (strcmp(u_table[i].name, name) == 0) { exists = 1; break; }
      if (exists) { print_string("Exists.\n"); sleep_ticks(1000); continue; }
      print_string("Password: ");
      get_line(pass, NAME_MAX);
      strcpy(u_table[u_count].name, name);
      encode_pwd(u_table[u_count].pass_hash, pass, (uint8_t)(ticks & 0xFF));
      u_table[u_count].is_root = 0;
      u_count++;
      save_data();
      print_string("Created! Login now.\n");
      sleep_ticks(2000);
      continue;
    }

    print_string("pass: ");
    get_line(pass, NAME_MAX);

    for (int i = 0; i < u_count; i++)
      if (strcmp(u_table[i].name, name) == 0 && check_pwd(pass, u_table[i].pass_hash)) {
        u_cur = i;
        clear_screen();
        return;
      }
    print_color("Login incorrect.\n\n", 0x0C);
    sleep_ticks(2000);
  }
}

// ---- File System ----
int find_file(const char *name) {
  for (int i = 0; i < f_count; i++)
    if (strcmp(f_table[i].name, name) == 0) return i;
  return -1;
}

void cmd_touch(const char *name) {
  if (!name[0]) { print_string("Usage: touch <filename>\n"); return; }
  if (f_count >= MAX_FILES) { print_string("File system full.\n"); return; }
  if (find_file(name) >= 0) { print_string("File exists.\n"); return; }
  strcpy(f_table[f_count].name, name);
  f_table[f_count].content[0] = '\0';
  f_table[f_count].size = 0;
  f_table[f_count].owner = u_cur;
  f_table[f_count].mode = PERM_DEFAULT;
  f_count++;
  print_string("Created.\n");
}

static void print_mode(uint16_t mode) {
  put_char(mode & PERM_OWNER_R ? 'r' : '-', 0x0F);
  put_char(mode & PERM_OWNER_W ? 'w' : '-', 0x0F);
  put_char(mode & PERM_OWNER_X ? 'x' : '-', 0x0F);
  put_char(mode & PERM_GRP_R   ? 'r' : '-', 0x0F);
  put_char(mode & PERM_GRP_W   ? 'w' : '-', 0x0F);
  put_char(mode & PERM_GRP_X   ? 'x' : '-', 0x0F);
  put_char(mode & PERM_OTH_R   ? 'r' : '-', 0x0F);
  put_char(mode & PERM_OTH_W   ? 'w' : '-', 0x0F);
  put_char(mode & PERM_OTH_X   ? 'x' : '-', 0x0F);
}

void cmd_ls(void) {
  if (f_count == 0) { print_string("No files.\n"); return; }
  char buf[16];
  for (int i = 0; i < f_count; i++) {
    uint8_t col = 0x0F;
    if (u_cur != 0 && f_table[i].owner != u_cur) col = 0x08;
    print_mode(f_table[i].mode);
    print_string(" ");
    print_string(u_table[f_table[i].owner].name);
    for (int s = strlen(u_table[f_table[i].owner].name); s < 8; s++) put_char(' ', col);
    itoa(f_table[i].size, buf, 10);
    print_color(buf, col);
    print_color("B ", col);
    print_string(f_table[i].name);
    print_string("\n");
  }
}

// Permission check: 0 = allowed, 1 = denied
static int check_perm(int idx, int want_write) {
  if (idx < 0 || idx >= f_count) return 1;
  if (u_cur == 0) return 0; // root can do anything
  if (f_table[idx].owner == u_cur) {
    if (want_write && !(f_table[idx].mode & PERM_OWNER_W)) return 1;
    if (!want_write && !(f_table[idx].mode & PERM_OWNER_R)) return 1;
    return 0;
  }
  // For non-owners, only allow read if other-read is set
  if (!want_write && (f_table[idx].mode & PERM_OTH_R)) return 0;
  if (want_write && (f_table[idx].mode & PERM_OTH_W)) return 0;
  return 1;
}

void cmd_cat(const char *name) {
  int idx = find_file(name);
  if (idx < 0) { print_string("File not found.\n"); return; }
  if (check_perm(idx, 0)) { print_color("Permission denied.\n", 0x0C); return; }
  print_string(f_table[idx].content);
  print_string("\n");
}

void cmd_rm(const char *name) {
  int idx = find_file(name);
  if (idx < 0) { print_string("File not found.\n"); return; }
  if (check_perm(idx, 1)) { print_color("Permission denied.\n", 0x0C); return; }
  for (int i = idx; i < f_count - 1; i++) f_table[i] = f_table[i + 1];
  f_count--;
  print_string("Deleted.\n");
}

void cmd_write(const char *args) {
  char fname[NAME_MAX] = {0};
  char content[CONTENT_MAX] = {0};
  int i = 0, j = 0;
  while (args[i] && args[i] != ' ' && j < NAME_MAX - 1) fname[j++] = args[i++];
  while (args[i] == ' ') i++;
  j = 0;
  while (args[i] && j < CONTENT_MAX - 1) content[j++] = args[i++];
  content[j] = '\0';
  if (!fname[0]) { print_string("Usage: write <file> <text>\n"); return; }
  int idx = find_file(fname);
  if (idx < 0) { print_string("File not found. Use touch first.\n"); return; }
  if (check_perm(idx, 1)) { print_color("Permission denied.\n", 0x0C); return; }
  strcpy(f_table[idx].content, content);
  f_table[idx].size = strlen(content);
  print_string("Written.\n");
}

void cmd_append(const char *args) {
  char fname[NAME_MAX] = {0};
  char content[CONTENT_MAX] = {0};
  int i = 0, j = 0;
  while (args[i] && args[i] != ' ' && j < NAME_MAX - 1) fname[j++] = args[i++];
  while (args[i] == ' ') i++;
  j = 0;
  while (args[i] && j < CONTENT_MAX - 1) content[j++] = args[i++];
  content[j] = '\0';
  if (!fname[0]) { print_string("Usage: append <file> <text>\n"); return; }
  int idx = find_file(fname);
  if (idx < 0) { print_string("File not found.\n"); return; }
  if (check_perm(idx, 1)) { print_color("Permission denied.\n", 0x0C); return; }
  int cur = f_table[idx].size;
  int room = CONTENT_MAX - cur - 1;
  j = 0;
  while (content[j] && j < room) f_table[idx].content[cur++] = content[j++];
  f_table[idx].content[cur] = '\0';
  f_table[idx].size = cur;
  print_string("Appended.\n");
}

void cmd_edit(const char *name) {
  int idx = find_file(name);
  if (idx < 0) { print_string("File not found.\n"); return; }
  if (check_perm(idx, 1)) { print_color("Permission denied.\n", 0x0C); return; }
  print_string("Editing: ");
  print_string(name);
  print_string("\nType '.done' to finish.\n\n");
  if (f_table[idx].size > 0) {
    print_string("--- current ---\n");
    print_string(f_table[idx].content);
    print_string("\n---\n");
  }
  f_table[idx].content[0] = '\0';
  f_table[idx].size = 0;
  int total = 0;
  while (1) {
    char line[128];
    print_string("> ");
    get_line(line, 128);
    if (strcmp(line, ".done") == 0) break;
    int len = strlen(line);
    if (total + len + 2 >= CONTENT_MAX) { print_string("File full.\n"); break; }
    for (int k = 0; k <= len; k++) f_table[idx].content[total++] = line[k];
    f_table[idx].content[total - 1] = '\n';
    f_table[idx].size = total;
  }
  print_string("Saved.\n");
}

// ---- User commands ----
void cmd_useradd(const char *name) {
  if (!name[0]) { print_string("Usage: useradd <username>\n"); return; }
  if (u_cur != 0) { print_color("Only root can add users.\n", 0x0C); return; }
  if (u_count >= MAX_USERS) { print_string("Max users reached.\n"); return; }
  for (int i = 0; i < u_count; i++)
    if (strcmp(u_table[i].name, name) == 0) { print_string("User exists.\n"); return; }
  strcpy(u_table[u_count].name, name);
  print_string("Enter password for ");
  print_string(name);
  print_string(": ");
  char pwbuf[NAME_MAX];
  get_line(pwbuf, NAME_MAX);
  encode_pwd(u_table[u_count].pass_hash, pwbuf, (uint8_t)(ticks & 0xFF));
  u_table[u_count].is_root = 0;
  u_count++;
  print_string("User created.\n");
}

void cmd_passwd(const char *args) {
  char buf[NAME_MAX], newbuf[NAME_MAX];
  print_string("Current password: ");
  get_line(buf, NAME_MAX);
  if (!check_pwd(buf, u_table[u_cur].pass_hash)) { print_color("Wrong password.\n", 0x0C); return; }
  print_string("New password: ");
  get_line(newbuf, NAME_MAX);
  encode_pwd(u_table[u_cur].pass_hash, newbuf, (uint8_t)(ticks & 0xFF));
  print_string("Password changed.\n");
}

void cmd_login(const char *args) {
  if (!args[0]) { do_login(); return; }
  for (int i = 0; i < u_count; i++)
    if (strcmp(u_table[i].name, args) == 0) {
      print_string("Password: ");
      char buf[NAME_MAX];
      get_line(buf, NAME_MAX);
      if (check_pwd(buf, u_table[i].pass_hash)) { u_cur = i; print_string("Ok.\n"); return; }
      print_color("Wrong password.\n", 0x0C); return;
    }
  print_string("User not found.\n");
}

void cmd_logout(void) {
  do_login();
}

// ---- chmod / chown ----
void cmd_chmod(const char *args) {
  char mode_str[16]={0}, fname[32]={0};
  int i=0,j=0;
  while(args[i]&&args[i]!=' '&&j<15){mode_str[j++]=args[i++];}
  while(args[i]==' '){i++;} j=0;
  while(args[i]&&j<31){fname[j++]=args[i++];}
  if(!mode_str[0]||!fname[0]){print_string("Usage: chmod <mode> <file>\n");return;}
  int idx=find_file(fname);
  if(idx<0){print_string("Not found.\n");return;}
  if(u_cur!=0&&f_table[idx].owner!=u_cur){print_color("Permission denied.\n",0x0C);return;}
  uint16_t newmode=0;
  int octal=atoi(mode_str);
  if(octal>0&&octal<4096) { newmode=octal; }
  else {
    // Parse rwx-style string (e.g. "rw-r--r--")
    if(strlen(mode_str)>=9) {
      if(mode_str[0]=='r')newmode|=PERM_OWNER_R;
      if(mode_str[1]=='w')newmode|=PERM_OWNER_W;
      if(mode_str[2]=='x')newmode|=PERM_OWNER_X;
      if(mode_str[3]=='r')newmode|=PERM_GRP_R;
      if(mode_str[4]=='w')newmode|=PERM_GRP_W;
      if(mode_str[5]=='x')newmode|=PERM_GRP_X;
      if(mode_str[6]=='r')newmode|=PERM_OTH_R;
      if(mode_str[7]=='w')newmode|=PERM_OTH_W;
      if(mode_str[8]=='x')newmode|=PERM_OTH_X;
    } else { print_string("Bad mode.\n"); return; }
  }
  f_table[idx].mode=newmode;
  print_string("Mode changed.\n");
}

void cmd_chown(const char *args) {
  char owner_str[16]={0}, fname[32]={0};
  int i=0,j=0;
  while(args[i]&&args[i]!=' '&&j<15){owner_str[j++]=args[i++];}
  while(args[i]==' '){i++;} j=0;
  while(args[i]&&j<31){fname[j++]=args[i++];}
  if(!owner_str[0]||!fname[0]){print_string("Usage: chown <user> <file>\n");return;}
  if(u_cur!=0){print_color("Only root.\n",0x0C);return;}
  int nu=-1;
  for(int k=0;k<u_count;k++)if(strcmp(u_table[k].name,owner_str)==0){nu=k;break;}
  if(nu<0){print_string("User not found.\n");return;}
  int idx=find_file(fname);
  if(idx<0){print_string("Not found.\n");return;}
  f_table[idx].owner=nu;
  print_string("Owner changed.\n");
}

// ---- Simple Features ----
void cmd_banner(const char *text) {
  if (!text[0]) { print_string("Usage: banner <text>\n"); return; }
  int len = strlen(text);
  if (len > 60) { print_string("Text too long.\n"); return; }
  print_string("\n+");
  for (int i = 0; i < len + 2; i++) put_char('-', 0x0F);
  print_string("+\n| ");
  print_string(text);
  print_string(" |\n+");
  for (int i = 0; i < len + 2; i++) put_char('-', 0x0F);
  print_string("+\n\n");
}

void cmd_yes(void) {
  print_string("Press any key to stop...\n");
  while (!kb_hit()) {
    print_string("y ");
    do_tick();
  }
  get_char(0);
  print_string("\n");
}

void cmd_time(void) {
  while (get_update_in_progress_flag());
  uint8_t s = get_rtc_register(0x00);
  uint8_t m = get_rtc_register(0x02);
  uint8_t h = get_rtc_register(0x04);
  uint8_t rtc_b = get_rtc_register(0x0B);
  if (!(rtc_b & 0x04)) {
    s = (s & 0x0F) + ((s / 16) * 10);
    m = (m & 0x0F) + ((m / 16) * 10);
    h = ((h & 0x0F) + (((h & 0x70) / 16) * 10)) | (h & 0x80);
  }
  char buf[16];
  itoa(h, buf, 10); print_string(buf);
  print_string(":");
  itoa(m, buf, 10); print_string(buf);
  print_string(":");
  itoa(s, buf, 10); print_string(buf);
  print_string("\n");
}

void cmd_fortune(void) {
  const char *quotes[] = {
    "Hello, World!",
    "The only constant is change.",
    "Keep it simple, stupid.",
    "sudo make me a sandwich.",
    "42",
    "Not all who wander are lost.",
    "To be or not to be.",
    "Have you tried turning it off and on?",
    "There is no place like 127.0.0.1",
    "Segmentation fault (core dumped)",
    "It's not a bug, it's a feature.",
    "All your base are belong to us.",
    "0xDEADBEEF",
    "I think, therefore I am.",
    "Just reboot it.",
    "Hello? Is it me you're looking for?",
  };
  print_string(quotes[rand() % 16]);
  print_string("\n");
}

void cmd_shutdown(void) {
  print_color("\nShutting down...\n", 0x0C);
  sleep_ticks(5000);
  __asm__ volatile("cli; hlt");
}

// ---- New Serious Commands ----
void cmd_hostname(const char *args) {
  if (args[0]) {
    int i = 0;
    while (args[i] && i < 31) { hostname_str[i] = args[i]; i++; }
    hostname_str[i] = '\0';
  }
  print_string(hostname_str);
  print_string("\n");
}

void cmd_id(void) {
  char buf[16];
  print_string("uid=");
  itoa(u_cur, buf, 10); print_string(buf);
  print_string("("); print_string(u_table[u_cur].name); print_string(")");
  if (u_table[u_cur].is_root) print_string(" [root]");
  print_string("\n");
}

void cmd_wc(const char *name) {
  if (!name[0]) { print_string("Usage: wc <file>\n"); return; }
  int idx = find_file(name);
  if (idx < 0) { print_string("Not found.\n"); return; }
  if (check_perm(idx, 0)) { print_color("Denied.\n", 0x0C); return; }
  int lines = 0, words = 0, chars = f_table[idx].size;
  int in_word = 0;
  for (int i = 0; i < chars; i++) {
    char c = f_table[idx].content[i];
    if (c == '\n') lines++;
    if (c == ' ' || c == '\n' || c == '\t') in_word = 0;
    else if (!in_word) { words++; in_word = 1; }
  }
  char buf[16];
  itoa(lines, buf, 10); print_string(buf); print_string(" ");
  itoa(words, buf, 10); print_string(buf); print_string(" ");
  itoa(chars, buf, 10); print_string(buf); print_string(" ");
  print_string(name); print_string("\n");
}

void cmd_tail(const char *args) {
  char fname[32]={0}, ns[8]={0}; int i=0,j=0,n=10;
  while(args[i]&&args[i]!=' '&&j<31){fname[j++]=args[i++];}
  while(args[i]==' '){i++;} j=0;
  while(args[i]&&j<7){ns[j++]=args[i++];} ns[j]=0;
  if(ns[0]) n=atoi(ns);
  if(!fname[0]){print_string("Usage: tail <file> [lines]\n");return;}
  int idx=find_file(fname);
  if(idx<0){print_string("Not found.\n");return;}
  if(check_perm(idx,0)){print_color("Denied.\n",0x0C);return;}
  int s = f_table[idx].size, nl = 0;
  for (int i = 0; i < s; i++)
    if (f_table[idx].content[i] == '\n') nl++;
  int skip = nl - n; if (skip < 0) skip = 0;
  int pos = 0, cnt = 0;
  while (pos < s && cnt < skip) {
    if (f_table[idx].content[pos] == '\n') cnt++;
    pos++;
  }
  while (pos < s) { put_char(f_table[idx].content[pos], 0x0F); pos++; }
  if (s > 0 && f_table[idx].content[s-1] != '\n') print_string("\n");
}

void cmd_hist(void) {
  char buf[16];
  int start = (hist_count < HIST_MAX) ? 0 : hist_idx;
  int count = (hist_count < HIST_MAX) ? hist_count : HIST_MAX;
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % HIST_MAX;
    itoa(i + 1, buf, 10); print_string(buf);
    print_string("  "); print_string(hist_buf[idx]); print_string("\n");
  }
}

void cmd_which(const char *name) {
  if (!name[0]) { print_string("Usage: which <command>\n"); return; }
  const char *list[] = {
    "help","echo","clear","reboot","uptime","color","about","mem","beep",
    "halt","date","cpuinfo","lspci","calc","ascii","palette","matrix",
    "guess","snake","tictactoe","hangman","memory","neofetch","banner",
    "yes","time","fortune","shutdown","rand","hex","reverse","len",
    "tolower","toupper","uname","whoami","touch","ls","cat","rm","write",
    "append","edit","useradd","passwd","login","logout","sleep","panic",
    "outb","inb","mv","cp","head","diese","hostname","id","wc","tail",
    "history","which","cowsay","cmatrix","dice","8ball","logo","sl",
    "morse","russian","insult","excuse","compliment","hack","bsod",
    "true","false","sort","env","ayo","chmod","chown","grep","find",
    "diff","uniq","alias","unalias","pwd","mkdir","tee","basename",
    "dirname","ps","kill","seq","tr","who"
  };
  for (int i = 0; i < sizeof(list)/sizeof(list[0]); i++)
    if (strcmp(list[i], name) == 0) { print_string(name); print_string(": internal command\n"); return; }
  print_string("Not found.\n");
}

void cmd_true(void) {}
void cmd_false(void) {}

void cmd_sort(const char *name) {
  if (!name[0]) { print_string("Usage: sort <file>\n"); return; }
  int idx = find_file(name);
  if (idx < 0) { print_string("Not found.\n"); return; }
  if (check_perm(idx, 0)) { print_color("Denied.\n", 0x0C); return; }
  char lines[64][32];
  int nlines = 0, pos = 0, lpos = 0;
  while (f_table[idx].content[pos] && nlines < 64) {
    if (f_table[idx].content[pos] == '\n') {
      lines[nlines][lpos] = '\0'; nlines++; lpos = 0;
    } else if (lpos < 31) { lines[nlines][lpos++] = f_table[idx].content[pos]; }
    pos++;
  }
  if (lpos > 0) { lines[nlines][lpos] = '\0'; nlines++; }
  for (int i = 0; i < nlines - 1; i++)
    for (int j = 0; j < nlines - i - 1; j++)
      if (strcmp(lines[j], lines[j+1]) > 0) {
        char tmp[32]; strcpy(tmp, lines[j]);
        strcpy(lines[j], lines[j+1]); strcpy(lines[j+1], tmp);
      }
  for (int i = 0; i < nlines; i++) { print_string(lines[i]); print_string("\n"); }
}

void cmd_env(void) {
  char buf[16];
  print_string("USER="); print_string(u_table[u_cur].name); print_string("\n");
  print_string("HOSTNAME="); print_string(hostname_str); print_string("\n");
  print_string("SHELL=HEXA CLI\n");
  print_string("TERM=vga\n");
  print_string("UID="); itoa(u_cur, buf, 10); print_string(buf); print_string("\n");
}

// ---- Unix-Style Commands ----
static int alias_count = 0;
static struct { char name[16]; char cmd[64]; } alias_table[16];

void cmd_alias(const char *args) {
  if (!args[0]) {
    for (int i = 0; i < alias_count; i++) {
      print_string(alias_table[i].name);
      print_string("='");
      print_string(alias_table[i].cmd);
      print_string("'\n");
    }
    return;
  }
  char an[16]={0}, ac[64]={0};
  int i=0,j=0;
  while(args[i]&&args[i]!='='&&j<15){an[j++]=args[i++];}
  if(args[i]=='=') i++;
  if(args[i]=='\'') i++;
  j=0;
  while(args[i]&&args[i]!='\''&&j<63){ac[j++]=args[i++];}
  if(alias_count>=16){print_string("Alias limit.\n");return;}
  strcpy(alias_table[alias_count].name,an);
  strcpy(alias_table[alias_count].cmd,ac);
  alias_count++;
}

void cmd_unalias(const char *name) {
  if(!name[0]){print_string("Usage: unalias <name>\n");return;}
  for(int i=0;i<alias_count;i++){
    if(strcmp(alias_table[i].name,name)==0){
      for(int j=i;j<alias_count-1;j++)alias_table[j]=alias_table[j+1];
      alias_count--;
      print_string("Removed.\n");return;
    }
  }
  print_string("Not found.\n");
}

void cmd_grep(const char *args) {
  char pat[32]={0}, fname[32]={0};
  int i=0,j=0;
  while(args[i]&&args[i]!=' '&&j<31){pat[j++]=args[i++];}
  while(args[i]==' '){i++;} j=0;
  while(args[i]&&j<31){fname[j++]=args[i++];}
  if(!pat[0]||!fname[0]){print_string("Usage: grep <pattern> <file>\n");return;}
  int idx=find_file(fname);
  if(idx<0){print_string("Not found.\n");return;}
  if(check_perm(idx,0)){print_color("Denied.\n",0x0C);return;}
  int plen=strlen(pat);
  for(int pos=0;pos<f_table[idx].size;pos++){
    int match=1;
    for(int k=0;k<plen&&(pos+k)<f_table[idx].size;k++){
      if(f_table[idx].content[pos+k]!=pat[k]){match=0;break;}
    }
    if(match){
      int line_start=pos;
      while(line_start>0&&f_table[idx].content[line_start-1]!='\n')line_start--;
      int line_end=pos;
      while(line_end<f_table[idx].size&&f_table[idx].content[line_end]!='\n')line_end++;
      for(int k=line_start;k<line_end;k++)put_char(f_table[idx].content[k],0x0F);
      print_string("\n");
      pos=line_end;
    }
  }
}

static int str_find(const char *s, const char *p) {
  if(!*p)return 1;
  for(;*s;s++){
    const char *a=s,*b=p;
    while(*a&&*b&&*a==*b){a++;b++;}
    if(!*b)return 1;
  }
  return 0;
}

void cmd_find(const char *name) {
  if(!name[0]){print_string("Usage: find <pattern>\n");return;}
  for(int i=0;i<f_count;i++){
    if(str_find(f_table[i].name,name)){
      print_string("  ");print_string(f_table[i].name);print_string("\n");
    }
  }
}

void cmd_diff(const char *args) {
  char f1[32]={0},f2[32]={0};
  int i=0,j=0;
  while(args[i]&&args[i]!=' '&&j<31){f1[j++]=args[i++];}
  while(args[i]==' '){i++;}j=0;
  while(args[i]&&j<31){f2[j++]=args[i++];}
  if(!f1[0]||!f2[0]){print_string("Usage: diff <file1> <file2>\n");return;}
  int i1=find_file(f1),i2=find_file(f2);
  if(i1<0||i2<0){print_string("Not found.\n");return;}
  if(check_perm(i1,0)||check_perm(i2,0)){print_color("Denied.\n",0x0C);return;}
  if(strcmp(f_table[i1].content,f_table[i2].content)==0){
    print_string("Files identical.\n");return;
  }
  print_string("Files differ.\n");
  char lines1[64][32],lines2[64][32];
  int n1=0,n2=0,lp=0;
  for(int p=0;p<=f_table[i1].size&&n1<64;p++){
    char c=f_table[i1].content[p];
    if(c=='\n'||c=='\0'){lines1[n1][lp]=0;n1++;lp=0;}
    else if(lp<31){lines1[n1][lp++]=c;}
  }
  lp=0;
  for(int p=0;p<=f_table[i2].size&&n2<64;p++){
    char c=f_table[i2].content[p];
    if(c=='\n'||c=='\0'){lines2[n2][lp]=0;n2++;lp=0;}
    else if(lp<31){lines2[n2][lp++]=c;}
  }
  int max=n1>n2?n1:n2;
  for(int l=0;l<max;l++){
    char t1[8];
    itoa(l+1,t1,10);
    if(l<n1&&l<n2&&strcmp(lines1[l],lines2[l])!=0){
      print_color(t1,0x0C);print_string("< ");print_string(lines1[l]);print_string("\n");
      print_color(t1,0x0C);print_string("> ");print_string(lines2[l]);print_string("\n");
    } else if(l>=n1){
      print_color(t1,0x0A);print_string("+ ");print_string(lines2[l]);print_string("\n");
    } else if(l>=n2){
      print_color(t1,0x0C);print_string("- ");print_string(lines1[l]);print_string("\n");
    }
  }
}

void cmd_uniq(const char *name) {
  if(!name[0]){print_string("Usage: uniq <file>\n");return;}
  int idx=find_file(name);
  if(idx<0){print_string("Not found.\n");return;}
  if(check_perm(idx,0)){print_color("Denied.\n",0x0C);return;}
  char prev[64]={0},cur[64];
  int cp=0,first=1;
  for(int p=0;p<=f_table[idx].size;p++){
    char c=f_table[idx].content[p];
    if(c=='\n'||c=='\0'){
      cur[cp]=0;
      if(first||strcmp(cur,prev)!=0){
        print_string(cur);print_string("\n");
        strcpy(prev,cur);first=0;
      }
      cp=0;
    } else if(cp<63){cur[cp++]=c;}
  }
}

void cmd_pwd(void) {
  print_string("/home/");
  print_string(u_table[u_cur].name);
  print_string("\n");
}

void cmd_mkdir(const char *name) {
  if(!name[0]){print_string("Usage: mkdir <dirname>\n");return;}
  // store as directory with trailing /
  char dname[NAME_MAX];
  strcpy(dname,name);
  int len=strlen(dname);
  if(dname[len-1]!='/'){dname[len]='/';dname[len+1]=0;}
  if(find_file(dname)>=0){print_string("Exists.\n");return;}
  if(f_count>=MAX_FILES){print_string("Full.\n");return;}
  strcpy(f_table[f_count].name,dname);
  f_table[f_count].content[0]=0;
  f_table[f_count].size=0;
  f_table[f_count].owner=u_cur;
  f_table[f_count].mode=PERM_DEFAULT|PERM_OWNER_X|PERM_GRP_X|PERM_OTH_X;
  f_count++;
  print_string("Created.\n");
}

void cmd_tee(const char *args) {
  char fname[32]={0},text[128]={0};
  int i=0,j=0;
  while(args[i]&&args[i]!=' '&&j<31){fname[j++]=args[i++];}
  while(args[i]==' '){i++;}j=0;
  while(args[i]&&j<127){text[j++]=args[i++];}
  if(!fname[0]||!text[0]){print_string("Usage: tee <file> <text>\n");return;}
  // Display
  print_string(text);print_string("\n");
  // Save to file
  int idx=find_file(fname);
  if(idx<0){
    if(f_count>=MAX_FILES){print_string("FS full.\n");return;}
    idx=f_count;
    strcpy(f_table[idx].name,fname);
    f_table[idx].owner=u_cur;
    f_table[idx].mode=PERM_DEFAULT;
    f_count++;
  }
  if(check_perm(idx,1)){print_color("Denied.\n",0x0C);return;}
  strcpy(f_table[idx].content,text);
  f_table[idx].size=strlen(text);
  save_data();
}

void cmd_basename(const char *path) {
  if(!path[0]){print_string("Usage: basename <path>\n");return;}
  int last=0;
  for(int i=0;path[i];i++)if(path[i]=='/')last=i+1;
  print_string(path+last);print_string("\n");
}

void cmd_dirname(const char *path) {
  if(!path[0]){print_string("Usage: dirname <path>\n");return;}
  int last=-1;
  for(int i=0;path[i];i++)if(path[i]=='/')last=i;
  if(last<=0)print_string(".\n");
  else{for(int i=0;i<last;i++)put_char(path[i],0x0F);print_string("\n");}
}

void cmd_ps(void) {
  print_string("  PID  USER       COMMAND\n");
  print_string("  ---  ----       -------\n");
  print_string("    1  ");
  print_string(u_table[u_cur].name);
  for(int s=strlen(u_table[u_cur].name);s<10;s++)put_char(' ',0x0F);
  print_string("hexa_shell\n");
}

void cmd_kill(const char *args) {
  if(!args[0]){print_string("Usage: kill <pid>\n");return;}
  int pid=atoi(args);
  if(pid==1){print_string("Cannot kill the shell.\n");return;}
  print_string("No process with PID ");print_string(args);print_string(".\n");
}

void cmd_seq(const char *args) {
  char a[8]={0},b[8]={0};
  int i=0,j=0,start=1,end=1;
  while(args[i]&&args[i]!=' '&&j<7){a[j++]=args[i++];}
  while(args[i]==' '){i++;}j=0;
  while(args[i]&&j<7){b[j++]=args[i++];}
  if(a[0]){start=atoi(a);end=start;}
  if(b[0]){end=atoi(b);}
  char buf[16];
  for(int n=start;n<=end;n++){
    itoa(n,buf,10);print_string(buf);
    if(n<end)put_char(' ',0x0F);
  }
  print_string("\n");
}

void cmd_tr(const char *args) {
  // tr <set1> <set2> <text> - simple char replacement
  char set1[32]={0},set2[32]={0},text[128]={0};
  int i=0,j=0;
  while(args[i]&&args[i]!=' '&&j<31){set1[j++]=args[i++];}
  while(args[i]==' '){i++;}j=0;
  while(args[i]&&args[i]!=' '&&j<31){set2[j++]=args[i++];}
  while(args[i]==' '){i++;}j=0;
  while(args[i]&&j<127){text[j++]=args[i++];}
  if(!set1[0]||!set2[0]){print_string("Usage: tr <set1> <set2> <text>\n");return;}
  for(int k=0;text[k];k++){
    int replaced=0;
    for(int s=0;set1[s]&&set2[s];s++){
      if(text[k]==set1[s]){put_char(set2[s],0x0F);replaced=1;break;}
    }
    if(!replaced)put_char(text[k],0x0F);
  }
  print_string("\n");
}

void cmd_who(void) {
  char buf[16];
  for(int i=0;i<u_count;i++){
    itoa(i,buf,10);print_string(buf);
    print_string("  ");print_string(u_table[i].name);
    if(u_table[i].is_root)print_string(" [root]");
    if(i==u_cur)print_string(" *");
    print_string("\n");
  }
}

// ---- New Fun Commands ----
void cmd_cowsay(const char *text) {
  if (!text[0]) { print_string("Usage: cowsay <text>\n"); return; }
  int len = strlen(text); if (len > 50) len = 50;
  print_string(" ");
  for (int i = 0; i < len + 2; i++) put_char('_', 0x0F);
  print_string("\n< ");
  for (int i = 0; i < len; i++) put_char(text[i], 0x0F);
  print_string(" >\n ");
  for (int i = 0; i < len + 2; i++) put_char('-', 0x0F);
  print_string("\n");
  print_string("        \\   ^__^\n");
  print_string("         \\  (oo)\\_______\n");
  print_string("            (__)\\       )\\/\\\n");
  print_string("                ||----w |\n");
  print_string("                ||     ||\n");
}

void cmd_cmatrix(void) {
#define CM_COLS 50
#define CM_ROWS 16
  int drop[CM_COLS], delay[CM_COLS];
  for (int i = 0; i < CM_COLS; i++) {
    drop[i] = -(rand() % 20);
    delay[i] = rand() % 4;
  }
  clear_screen();
  print_string("Press any key to exit...\n");
  while (1) {
    if ((inb(0x64) & 1) || (inb(0x3F8 + 5) & 1)) { get_char(0); break; }
    // Clear old trail
    for (int x = 0; x < CM_COLS; x++) {
      int y = drop[x];
      if (y >= 0 && y < CM_ROWS)
        VGA_BUFFER[(4 + y) * 80 + (15 + x)] = (0x00 << 8) | ' ';
      if (y - 1 >= 0 && y - 1 < CM_ROWS)
        VGA_BUFFER[(4 + y - 1) * 80 + (15 + x)] = (0x00 << 8) | ' ';
    }
    // Advance drops
    for (int x = 0; x < CM_COLS; x++) {
      if (--delay[x] > 0) continue;
      delay[x] = (rand() % 3) + 1;
      drop[x]++;
      if (drop[x] >= CM_ROWS + 3) drop[x] = -(rand() % 15);
    }
    // Draw new frame
    for (int x = 0; x < CM_COLS; x++) {
      int y = drop[x];
      if (y >= 0 && y < CM_ROWS)
        VGA_BUFFER[(4 + y) * 80 + (15 + x)] = (0x0A << 8) | (char)((rand() % 94) + 33);
      if (y - 1 >= 0 && y - 1 < CM_ROWS)
        VGA_BUFFER[(4 + y - 1) * 80 + (15 + x)] = (0x02 << 8) | (char)((rand() % 94) + 33);
    }
    for (volatile int w = 0; w < 2000; w++);
  }
  clear_screen();
}

void cmd_dice(const char *args) {
  int num = 1, sides = 6;
  if (args[0]) {
    char n[8]={0}, s[8]={0}; int i=0,j=0;
    while(args[i]&&args[i]!='d'&&args[i]!='D'&&j<7){n[j++]=args[i++];}
    if(n[0]) num = atoi(n);
    if(args[i]=='d'||args[i]=='D'){i++;j=0;while(args[i]&&j<7){s[j++]=args[i++];}}
    if(s[0]) sides = atoi(s);
  }
  if (num < 1 || num > 10) num = 1;
  if (sides < 2) sides = 6;
  char buf[16];
  for (int i = 0; i < num; i++) {
    int r = (rand() % sides) + 1;
    itoa(r, buf, 10); print_string(buf);
    if (i < num - 1) print_string(" ");
  }
  print_string("\n");
}

void cmd_8ball(void) {
  const char *r[] = {
    "Yes.","No.","Maybe.","Ask again later.",
    "Definitely.","I doubt it.","Without a doubt.",
    "Cannot predict now.","Outlook good.",
    "Very doubtful.","Signs point to yes.",
    "My reply is no.","It is certain.",
    "Don't count on it.","Most likely.","Better not."
  };
  print_string(r[rand() % 16]); print_string("\n");
}

void cmd_logo(void) {
  print_color("  +----------------------------------+\n", 0x0B);
  print_color("  |  H     H  EEEEEEE  X     X    AAA  |\n", 0x0B);
  print_color("  |  H     H  E        X     X   A   A |\n", 0x0B);
  print_color("  |  HHHHHHH  EEEEE    X     X   AAAAA |\n", 0x0B);
  print_color("  |  H     H  E        X     X   A   A |\n", 0x0A);
  print_color("  |  H     H  EEEEEEE  X     X   A   A |\n", 0x0A);
  print_color("  |          H E X A  4 . 0           |\n", 0x0A);
  print_color("  +----------------------------------+\n", 0x0B);
}

void cmd_sl(void) {
  int frame = 0;
  print_string("Press any key to stop...\n");
  while (1) {
    if (kb_hit()) { get_char(0); break; }
    clear_screen();
    if (frame % 2) {
      print_string("      ====        ________                ________\n");
      print_string("  _D _|  |_______/        \\__I_I_____===__|_________|\n");
      print_string("   |(_)---|   |\\   o      |   |        |     | |  |\n");
      print_string("   /     |   | \\  o       |   |        |     | |  |\n");
      print_string("  |      |   |  \\         |   |        |     | |  |\n");
      print_string("  \\ ===== |___|===________/_  |_______|_____| |__|\n");
    } else {
      print_string("      ====        ________                ________\n");
      print_string("  _D _|  |_______/        \\__I_I_____===__|_________|\n");
      print_string("   |(_)---|   |\\   o      |   |        |    | |   |\n");
      print_string("   /     |   | \\  o       |   |        |    | |   |\n");
      print_string("  |      |   |  \\         |   |        |    | |   |\n");
      print_string("  \\ ===== |___|===________/_  |_______|____| |___|\n");
    }
    frame++;
    sleep_ticks(2000);
  }
  clear_screen();
}

void cmd_morse(const char *text) {
  if (!text[0]) { print_string("Usage: morse <text>\n"); return; }
  const char *morse[] = {
    ".-","-...","-.-.","-..",".","..-.","--.","....","..",".---",
    "-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-",
    "..-","...-",".--","-..-","-.--","--.."
  };
  for (int i = 0; text[i]; i++) {
    char c = text[i];
    if (c >= 'a' && c <= 'z') { print_string(morse[c - 'a']); print_string(" "); }
    else if (c >= 'A' && c <= 'Z') { print_string(morse[c - 'A']); print_string(" "); }
    else if (c == ' ') { print_string("  "); }
    else { put_char(c, 0x0F); print_string(" "); }
  }
  print_string("\n");
}

void cmd_russian(void) {
  print_string("Russian Roulette... *spin* *click* ... ");
  sleep_ticks(3000);
  if ((rand() % 6) == 0) {
    print_color("BANG! You're dead. Halting.\n", 0x0C);
    __asm__ volatile("cli; hlt");
  } else {
    print_color("*click* Lucky!\n", 0x0A);
  }
}

// ---- More Fun ----
void cmd_insult(void) {
  const char *r[] = {
    "Your code is so bad, rm -rf / is an improvement.",
    "You make segfaults look like features.",
    "Slower than a snail on tranquilizers.",
    "I've seen better code from a RNG.",
    "Your debugging skills rival a brick.",
    "Even 0xDEADBEEF is more alive than your code."
  };
  print_string(r[rand() % 6]); print_string("\n");
}

void cmd_excuse(void) {
  const char *r[] = {
    "The bit bucket overflowed.",
    "Someone reversed the neutron flow.",
    "It works on my machine.",
    "Must be a DNS issue.",
    "The hamsters running the server are on strike.",
    "It's a layer 8 problem."
  };
  print_string(r[rand() % 6]); print_string("\n");
}

void cmd_compliment(void) {
  const char *r[] = {
    "Your kernel is sexier than a naked CPU.",
    "Your code is so clean it sparkles.",
    "You're the Linus Torvalds of your kitchen.",
    "Your bit-shifting skills are legendary.",
    "Even your bugs are elegant.",
    "You make assembly look like poetry."
  };
  print_string(r[rand() % 6]); print_string("\n");
}

void cmd_hack(void) {
  print_color("HACK SEQUENCE INITIATED\n", 0x0A);
  for (int i = 0; i < 15; i++) {
    print_string("["); for (int j = 0; j < 15; j++) put_char(j < i ? '#' : ' ', 0x0A);
    print_string("] "); char b[8]; itoa(i * 7, b, 10); print_string(b); print_string("%\n");
    if (kb_hit()) { get_char(0); break; }
  }
  print_color("ACCESS GRANTED. SYSTEM OWNED.\n", 0x0A);
  print_string("Just kidding. You're not a hacker.\n");
  print_string("Press any key..."); get_char(0);
}

void cmd_bsod(void) {
  clear_screen();
  current_color = 0x1F;
  for (int i = 0; i < 80 * 25; i++) VGA_BUFFER[i] = (0x1F << 8) | ' ';
  cursor_x = 0; cursor_y = 0;
  print_string("A fatal exception 0E has occurred at 0028:C0001E6F.\n");
  print_string("The current application will be terminated.\n\n");
  print_string("*  Press any key to restart.\n\n");
  print_string("System halted.  Please reboot.\n");
  get_char(0);
  current_color = 0x0F;
  clear_screen();
}

// ---- Package Manager (ayo) ----
#define PKG_FILES_MAX 6
struct pkg_entry {
  char name[32];
  struct { char name[32]; char content[128]; } files[PKG_FILES_MAX];
  int nfiles;
};
static struct pkg_entry pkg_db[] = {
  {"games", {
    {"snake.txt","Snake: eat food (@), avoid walls, grow! WASD to move."},
    {"ttt.txt","Tic-Tac-Toe: two-player. Get three in a row!"},
    {"hangman.txt","Hangman: guess the hidden word letter by letter."},
    {"memory.txt","Memory: watch color sequence and repeat it!"},
  }, 4},
  {"docs", {
    {"readme.txt","HEXA OS 4.0 - A 32-bit protected mode OS for i386."},
    {"commands.txt","Type 'help' to list all available commands (70+)."},
    {"about.txt","HEXA OS 4.0: Secure Edition - hashed passwords, permissions, grep, diff, alias, more!"},
  }, 3},
  {"fun", {
    {"quotes.txt","\"The only constant is change.\" - Heraclitus"},
    {"jokes.txt","Q: Why did the OS crash? A: Out of funny business!"},
    {"easter.txt","Try 'russian' for a risky game of chance!"},
  }, 3},
  {"dev", {
    {"api.txt","API: print_string, put_char, get_char, get_line, clear_screen"},
    {"memory.txt","Kernel: 0x10000 | VGA: 0xB8000 | Stack: 0x7C00"},
    {"pci.txt","PCI: config via ports 0xCF8/0xCFC, BDF addressing."},
  }, 3},
  {"cheatsheet", {
    {"keys.txt","PS/2 scancodes mapped to ASCII. Shift for uppercase."},
    {"ports.txt","0x3F8=COM1, 0x60=Keyboard, 0x64=Kbd Status, 0xCF8/CFC=PCI"},
    {"cmos.txt","CMOS regs: 0x00=sec, 0x02=min, 0x04=hr, 0x07=day, 0x08=mon, 0x09=yr"},
    {"ata.txt","ATA PIO: I/O 0x1F0-0x1F7. LBA28 addressing. Primary channel."},
  }, 4},
  {"tools", {
    {"hexdump.txt","View file content in hex with 'cat <file>' for text."},
    {"calc.txt","Built-in calc: calc 2 + 2, calc 10 / 3"},
    {"editor.txt","Edit files with 'edit <file>', finish with '.done'"},
    {"tips.txt","Use 'diese <cmd>' to run as root. 'up-arrow' recalls last cmd."},
  }, 4},
  {"net", {
    {"ping.txt","ping: simulated network ping. 'ping localhost'"},
    {"dns.txt","DNS resolves hostnames to IPs (simulated)."},
    {"ip.txt","IP addressing: 127.0.0.1 is localhost."},
    {"tcp.txt","TCP: reliable transport. HEXA uses raw I/O."},
    {"curl.txt","curl: simulated web requests (display only)."},
  }, 5},
  {"math", {
    {"pi.txt","PI = 3.1415926535897932384626433832795"},
    {"e.txt","E = 2.7182818284590452353602874713527"},
    {"fib.txt","Fibonacci: 0,1,1,2,3,5,8,13,21,34,55,89,144..."},
    {"prime.txt","Primes: 2,3,5,7,11,13,17,19,23,29,31,37,41,43,47..."},
    {"stats.txt","mean, median, mode - basic stats. Use calc for math."},
  }, 5},
  {"sysadmin", {
    {"df.txt","df: disk free. storage.img: 128KB total"},
    {"dmesg.txt","dmesg: kernel ring buffer. Boot messages here."},
    {"uptime.txt","uptime: system uptime in ticks"},
    {"iostat.txt","iostat: ATA disk I/O stats (simulated)."},
    {"top.txt","top: system processes. Only shell process running."},
  }, 5},
  {"security", {
    {"passwd.txt","passwd: change your password. New: hashed storage!"},
    {"perm.txt","Permissions: rwx rwx rwx. Use chmod + chown."},
    {"root.txt","Root has full access. Use 'diese <cmd>' for root."},
    {"audit.txt","Log: all changes are persisted to disk image."},
  }, 4},
  {"media", {
    {"ascii.txt","ASCII art: use 'logo' for HEXA banner."},
    {"colors.txt","VGA colors: 0-15. Use 'color <n>' to change."},
    {"pics.txt","ASCII pics: try 'cowsay' for cow, 'sl' for train."},
    {"anim.txt","Animation: 'cmatrix' rain, 'sl' train, 'hack' bar."},
  }, 4},
  {"productivity", {
    {"calendar.txt","Calendar: see date via 'date' command (RTC)."},
    {"reminder.txt","No alarms yet. Use files for reminders."},
    {"notes.txt","Note taking: 'edit <file>' creates/modifies notes."},
    {"todo.txt","Todo: 'touch todo.txt' and 'edit todo.txt'."},
  }, 4},
  {"science", {
    {"elements.txt","H,He,Li,Be,B,C,N,O,F,Ne - periodic table begins here"},
    {"speed.txt","Speed of light: 299,792,458 m/s"},
    {"gravity.txt","Gravitational constant G = 6.67430e-11"},
    {"planck.txt","Planck constant h = 6.62607015e-34"},
    {"solar.txt","Solar system: 8 planets orbiting Sol."},
  }, 5},
  {"lang", {
    {"hello_c.txt","C: #include <stdio.h>\\nint main(){return 0;}"},
    {"hello_py.txt","Python: print('Hello, HEXA!')"},
    {"hello_asm.txt","x86 ASM: mov eax, 1; int 0x80"},
    {"hello_js.txt","JS: console.log('Hello from HEXA!');"},
    {"bash.txt","Bash: echo 'Hello $USER'"},
  }, 5},
  {"puzzle", {
    {"sudoku.txt","Sudoku: 9x9 grid. Solve with logic."},
    {"crossword.txt","Crossword: word puzzle. Not implemented."},
    {"maze.txt","Maze: find path from start to exit."},
    {"riddle.txt","Riddle: What has keys but no locks? A keyboard!"},
  }, 4},
  {"adventure", {
    {"start.txt","You stand in a dark room. Commands: look, go, take"},
    {"cave.txt","The cave is damp. You see a glowing sword."},
    {"forest.txt","A dense forest. Paths lead north, east, south."},
    {"dragon.txt","A dragon sleeps on a pile of gold."},
    {"treasure.txt","You found the treasure! 1000 gold coins."},
  }, 5},
  {"retro", {
    {"basic.txt","BASIC: 10 PRINT 'HELLO'\\n20 GOTO 10"},
    {"fortran.txt","FORTRAN: PROGRAM HELLO\\nPRINT*,'HELLO'\\nEND"},
    {"pascal.txt","Pascal: begin writeln('Hello'); end."},
    {"c64.txt","C64: 6510 CPU, 64KB RAM, SID sound."},
    {"dos.txt","MS-DOS: dir, copy, del, edit, type."},
  }, 5},
  {"algorithms", {
    {"sorting.txt","Sorting: bubble, quick, merge, heap - O(n log n) best"},
    {"search.txt","Search: binary O(log n), linear O(n)."},
    {"graph.txt","Graph: BFS, DFS, Dijkstra, A* pathfinding."},
    {"tree.txt","Tree: binary search, AVL, red-black, trie."},
    {"dp.txt","Dynamic Prog: Fibonacci, knapSack, LCS, LIS."},
  }, 5},
  {"crypto", {
    {"caesar.txt","Caesar cipher: shift letters by N positions."},
    {"vigenere.txt","Vigenere: polyalphabetic cipher with keyword."},
    {"xor.txt","XOR: simple symmetric encryption."},
    {"hash.txt","HEXA now uses hashed passwords! djb2 variant."},
    {"rot13.txt","ROT13: rotate by 13, reverses itself."},
  }, 5},
  {"music", {
    {"notes.txt","Notes: C D E F G A B (do re mi fa so la ti)"},
    {"chords.txt","Chords: Cmaj, Dmin, Emin, Fmaj, Gmaj, Amin, Bdim"},
    {"scale.txt","C major scale: C D E F G A B C"},
    {"beep.txt","Use 'beep' command for terminal bell."},
    {"song.txt","Twinkle Twinkle Little Star: C C G G A A G"},
  }, 5},
  {"philosophy", {
    {"plato.txt","Plato: The Republic - justice in the individual and state."},
    {"nietzsche.txt","Nietzsche: 'What does not kill me makes me stronger.'"},
    {"descartes.txt","Descartes: 'I think, therefore I am.'"},
    {"confucius.txt","Confucius: 'It does not matter how slowly you go as long as you do not stop.'"},
    {"zen.txt","Zen: 'Before enlightenment, chop wood, carry water.'"},
  }, 5},
  {"geography", {
    {"capitals.txt","Paris, London, Berlin, Rome, Madrid, Lisbon, Athens"},
    {"rivers.txt","Nile (6650km), Amazon (6400km), Mississippi (3730km)"},
    {"mountains.txt","Everest (8848m), K2 (8611m), Kangchenjunga (8586m)"},
    {"oceans.txt","Pacific, Atlantic, Indian, Southern, Arctic"},
    {"flags.txt","HEXA OS flags: ASCII art banners!"},
  }, 5},
};
#define PKG_COUNT (sizeof(pkg_db) / sizeof(pkg_db[0]))

static void cmd_ayo(const char *args) {
  char sub[16]={0}, pkg[32]={0};
  int i=0,j=0;
  while(args[i]&&args[i]!=' '&&j<15){sub[j++]=args[i++];}
  while(args[i]==' '){i++;} j=0;
  while(args[i]&&j<31){pkg[j++]=args[i++];}

  if (strcmp(sub, "list") == 0) {
    print_string("Packages:\n");
    for (int i = 0; i < PKG_COUNT; i++) {
      char mkr[32]; mkr[0]='.'; mkr[1]=0; strcat(mkr, pkg_db[i].name);
      int inst = find_file(mkr) >= 0;
      print_string("  "); print_string(pkg_db[i].name);
      if (inst) print_color(" [I]", 0x0A); else print_string(" [ ]");
      print_string("  ");
      char b[8]; itoa(pkg_db[i].nfiles,b,10); print_string(b); print_string(" files\n");
    }
    return;
  }

  if (strcmp(sub, "help") == 0) {
    print_string("ayo - HEXA OS Package Manager\n");
    print_string("Usage:\n");
    print_string("  ayo list           show packages [I]=installed\n");
    print_string("  ayo add <pkg>     install package (root only)\n");
    print_string("  ayo remove <pkg>  uninstall package (root only)\n");
    print_string("  ayo update        refresh all installed (root only)\n");
    print_string("Packages: ");
    for (int i = 0; i < PKG_COUNT; i++) {
      print_string(pkg_db[i].name);
      if (i < PKG_COUNT - 1) print_string(", ");
    }
    print_string("\n");
    return;
  }

  if (strcmp(sub, "add") == 0) {
    if (!pkg[0]) { print_string("Usage: ayo add <pkg>\n"); return; }
    if (u_cur != 0) { print_color("Root only.\n", 0x0C); print_string("Use: diese ayo add <pkg>\n"); return; }
    int idx = -1;
    for (int i = 0; i < PKG_COUNT; i++)
      if (strcmp(pkg_db[i].name, pkg) == 0) { idx = i; break; }
    if (idx < 0) { print_string("Unknown package.\n"); return; }
    if (f_count + pkg_db[idx].nfiles + 1 >= MAX_FILES) { print_string("FS full.\n"); return; }
    char mkr[32]; mkr[0]='.'; mkr[1]=0; strcat(mkr, pkg);
    if (find_file(mkr) >= 0) { print_string("Already installed.\n"); return; }
    int mi = f_count;
    strcpy(f_table[mi].name, mkr); f_table[mi].content[0]='1';
    f_table[mi].content[1]=0; f_table[mi].size=1; f_table[mi].owner=u_cur; f_count++;
    for (int f = 0; f < pkg_db[idx].nfiles; f++) {
      if (find_file(pkg_db[idx].files[f].name) < 0) {
        strcpy(f_table[f_count].name, pkg_db[idx].files[f].name);
        strcpy(f_table[f_count].content, pkg_db[idx].files[f].content);
        f_table[f_count].size = strlen(pkg_db[idx].files[f].content);
        f_table[f_count].owner = u_cur;
        f_count++;
      }
    }
    save_data();
    print_string("Installed: "); print_string(pkg); print_string("\n");
    return;
  }

  if (strcmp(sub, "remove") == 0) {
    if (!pkg[0]) { print_string("Usage: ayo remove <pkg>\n"); return; }
    if (u_cur != 0) { print_color("Root only.\n", 0x0C); print_string("Use: diese ayo remove <pkg>\n"); return; }
    int idx = -1;
    for (int i = 0; i < PKG_COUNT; i++)
      if (strcmp(pkg_db[i].name, pkg) == 0) { idx = i; break; }
    if (idx < 0) { print_string("Unknown package.\n"); return; }
    char mkr[32]; mkr[0]='.'; mkr[1]=0; strcat(mkr, pkg);
    if (find_file(mkr) < 0) { print_string("Not installed.\n"); return; }
    for (int f = 0; f < pkg_db[idx].nfiles; f++) {
      int fi = find_file(pkg_db[idx].files[f].name);
      if (fi >= 0) { for (int k = fi; k < f_count - 1; k++) f_table[k]=f_table[k+1]; f_count--; }
    }
    int mi = find_file(mkr);
    if (mi >= 0) { for (int k = mi; k < f_count - 1; k++) f_table[k]=f_table[k+1]; f_count--; }
    save_data();
    print_string("Removed: "); print_string(pkg); print_string("\n");
    return;
  }

  if (strcmp(sub, "update") == 0) {
    if (u_cur != 0) { print_color("Root only.\n", 0x0C); print_string("Use: diese ayo update\n"); return; }
    int n = 0;
    for (int i = 0; i < PKG_COUNT; i++) {
      char mkr[32]; mkr[0]='.'; mkr[1]=0; strcat(mkr, pkg_db[i].name);
      if (find_file(mkr) < 0) continue;
      for (int f = 0; f < pkg_db[i].nfiles; f++) {
        int fi = find_file(pkg_db[i].files[f].name);
        if (fi >= 0) {
          strcpy(f_table[fi].content, pkg_db[i].files[f].content);
          f_table[fi].size = strlen(pkg_db[i].files[f].content);
        } else if (f_count < MAX_FILES) {
          strcpy(f_table[f_count].name, pkg_db[i].files[f].name);
          strcpy(f_table[f_count].content, pkg_db[i].files[f].content);
          f_table[f_count].size = strlen(pkg_db[i].files[f].content);
          f_table[f_count].owner = u_cur;
          f_count++;
        }
      }
      n++;
    }
    save_data();
    char b[8]; itoa(n,b,10); print_string(b); print_string(" packages updated.\n");
    return;
  }

  print_string("Usage: ayo <list|help|add|remove|update> [package]\n");
}

// ---- ATA PIO Disk Driver ----
#define ATA_DATA     0x1F0
#define ATA_SEC_CNT  0x1F2
#define ATA_LBA_LO   0x1F3
#define ATA_LBA_MI   0x1F4
#define ATA_LBA_HI   0x1F5
#define ATA_DRIVE    0x1F6
#define ATA_CMD      0x1F7
#define ATA_STATUS   0x1F7

static int ata_drv = 0xE0;

static int ata_poll(void) {
  int t = 50000;
  while (t--) {
    uint8_t s = inb(ATA_STATUS);
    if (!(s & 0x80)) return 1;
    __asm__ volatile("pause");
  }
  return 0;
}

static int ata_read(uint32_t lba, uint16_t *buf) {
  if (!ata_poll()) return 0;
  outb(ATA_DRIVE, ata_drv | ((lba >> 24) & 0x0F));
  outb(ATA_SEC_CNT, 1);
  outb(ATA_LBA_LO, lba);
  outb(ATA_LBA_MI, lba >> 8);
  outb(ATA_LBA_HI, lba >> 16);
  outb(ATA_CMD, 0x20);
  int t = 200000;
  while (t--) {
    uint8_t s = inb(ATA_STATUS);
    if (s & 0x08) goto read_data;
    if (s & 0x01) return 0;
  }
  return 0;
read_data:
  for (int i = 0; i < 256; i++) buf[i] = inw(ATA_DATA);
  return 1;
}

static int ata_write(uint32_t lba, const uint16_t *buf) {
  if (!ata_poll()) return 0;
  outb(ATA_DRIVE, ata_drv | ((lba >> 24) & 0x0F));
  outb(ATA_SEC_CNT, 1);
  outb(ATA_LBA_LO, lba);
  outb(ATA_LBA_MI, lba >> 8);
  outb(ATA_LBA_HI, lba >> 16);
  outb(ATA_CMD, 0x30);
  int t = 200000;
  while (t--) {
    uint8_t s = inb(ATA_STATUS);
    if (s & 0x08) goto write_data;
    if (s & 0x01) return 0;
  }
  return 0;
write_data:
  for (int i = 0; i < 256; i++) outw(ATA_DATA, buf[i]);
  outb(ATA_CMD, 0xE7);
  return ata_poll();
}

static void ata_init(void) {
  uint16_t sec[256];
  disk_ok = 0;
  int t = 10000; while (--t) inb(ATA_STATUS);
  for (int try = 0; try < 2; try++) {
    ata_drv = (try == 0) ? 0xE0 : 0xF0;
    if (ata_read(0, sec)) { disk_ok = 1; return; }
  }
  disk_ok = 0;
}

#define HEADER_LBA    100
#define USERS_LBA     101
#define FILES_LBA     102
#define FILE_SECTORS  2
#define FILES_TOTAL   (MAX_FILES * FILE_SECTORS)

static int save_data(void) {
  if (!disk_ok) return 0;
  uint8_t buf[512];
  memset(buf, 0, 512);
  buf[0] = 'H'; buf[1] = 'E'; buf[2] = 'X'; buf[3] = 'A';
  *(uint32_t *)(buf + 16) = u_count;
  *(uint32_t *)(buf + 20) = f_count;
  ata_write(HEADER_LBA, (uint16_t *)buf);
  memset(buf, 0, 512);
  for (int i = 0; i < u_count && i < MAX_USERS; i++) {
    memcpy(buf + i * 64, u_table[i].name, 32);
    memcpy(buf + i * 64 + 32, u_table[i].pass_hash, 32);
  }
  ata_write(USERS_LBA, (uint16_t *)buf);
  for (int i = 0; i < f_count && i < MAX_FILES; i++) {
    memset(buf, 0, 512);
    memcpy(buf, f_table[i].name, 32);
    *(uint32_t *)(buf + 32) = f_table[i].size;
    *(uint32_t *)(buf + 36) = f_table[i].owner;
    *(uint16_t *)(buf + 40) = f_table[i].mode;
    memcpy(buf + 42, f_table[i].content, 470);
    ata_write(FILES_LBA + i * FILE_SECTORS, (uint16_t *)buf);
    memset(buf, 0, 512);
    memcpy(buf, f_table[i].content + 470, 42);
    ata_write(FILES_LBA + i * FILE_SECTORS + 1, (uint16_t *)buf);
  }
  return 1;
}

static void load_data(void) {
  uint8_t buf[512];
  if (!ata_read(HEADER_LBA, (uint16_t *)buf)) { disk_ok = 0; return; }
  if (buf[0] != 'H' || buf[1] != 'E' || buf[2] != 'X' || buf[3] != 'A') { return; }
  disk_ok = 1;
  int su = *(uint32_t *)(buf + 16), sf = *(uint32_t *)(buf + 20);
  if (su > MAX_USERS) su = MAX_USERS;
  if (sf > MAX_FILES) sf = MAX_FILES;
  if (!ata_read(USERS_LBA, (uint16_t *)buf)) return;
  u_count = su;
  for (int i = 0; i < su; i++) {
    memcpy(u_table[i].name, buf + i * 64, 32);
    memcpy(u_table[i].pass_hash, buf + i * 64 + 32, 32);
    u_table[i].is_root = (strcmp(u_table[i].name, "root") == 0);
    // If stored password is plaintext (old format), hash it
    if (u_table[i].pass_hash[2] != ' ' && u_table[i].pass_hash[0]) {
      char plain[32];
      memcpy(plain, u_table[i].pass_hash, 32);
      encode_pwd(u_table[i].pass_hash, plain, (uint8_t)(i * 37 + 0xA5));
    }
  }
  if (strcmp(u_table[0].name, "root") != 0) {
    strcpy(u_table[0].name, "root");
    encode_pwd(u_table[0].pass_hash, "root", 0xA5);
    u_table[0].is_root = 1;
    u_count = 1;
    f_count = 0;
    return;
  }
  f_count = sf;
  for (int i = 0; i < sf && i < MAX_FILES; i++) {
    if (!ata_read(FILES_LBA + i * FILE_SECTORS, (uint16_t *)buf)) return;
    memcpy(f_table[i].name, buf, 32);
    f_table[i].size = *(uint32_t *)(buf + 32);
    f_table[i].owner = *(uint32_t *)(buf + 36);
    f_table[i].mode = *(uint16_t *)(buf + 40);
    if (f_table[i].mode == 0) f_table[i].mode = PERM_DEFAULT;
    memcpy(f_table[i].content, buf + 42, 470);
    if (!ata_read(FILES_LBA + i * FILE_SECTORS + 1, (uint16_t *)buf)) return;
    memcpy(f_table[i].content + 470, buf, 42);
    f_table[i].content[f_table[i].size] = '\0';
  }
}

// ---- Command Dispatch ----
static int execute_cmd(const char *cmd, char *args) {
  if (strcmp(cmd, "help") == 0) { cmd_help(); return 1; }
  if (strcmp(cmd, "echo") == 0) { print_string(args); print_string("\n"); return 1; }
  if (strcmp(cmd, "clear") == 0) { clear_screen(); return 1; }
  if (strcmp(cmd, "reboot") == 0) { outb(0x64, 0xFE); while (1); return 1; }
  if (strcmp(cmd, "uptime") == 0) { char b[16]; itoa(ticks,b,10); print_string(b); print_string(" ticks\n"); return 1; }
  if (strcmp(cmd, "color") == 0) { current_color = atoi(args) & 0x0F; return 1; }
  if (strcmp(cmd, "about") == 0) { print_string("HEXA OS - 32-bit.\n"); return 1; }
  if (strcmp(cmd, "mem") == 0) { print_string("VGA: 4000B\n"); return 1; }
  if (strcmp(cmd, "beep") == 0) { serial_putc('\a'); return 1; }
  if (strcmp(cmd, "halt") == 0) { __asm__ volatile("cli; hlt"); return 1; }
  if (strcmp(cmd, "date") == 0) { cmd_date(); return 1; }
  if (strcmp(cmd, "cpuinfo") == 0) { cmd_cpuinfo(); return 1; }
  if (strcmp(cmd, "lspci") == 0) { cmd_lspci(); return 1; }
  if (strcmp(cmd, "calc") == 0) { cmd_calc(args); return 1; }
  if (strcmp(cmd, "ascii") == 0) { cmd_ascii(); return 1; }
  if (strcmp(cmd, "palette") == 0) { cmd_palette(); return 1; }
  if (strcmp(cmd, "matrix") == 0) { cmd_matrix(); return 1; }
  if (strcmp(cmd, "guess") == 0) { cmd_guess(); return 1; }
  if (strcmp(cmd, "snake") == 0) { cmd_snake(); return 1; }
  if (strcmp(cmd, "tictactoe") == 0) { cmd_tictactoe(); return 1; }
  if (strcmp(cmd, "hangman") == 0) { cmd_hangman(); return 1; }
  if (strcmp(cmd, "memory") == 0) { cmd_memory(); return 1; }
  if (strcmp(cmd, "neofetch") == 0) { cmd_neofetch(); return 1; }
  if (strcmp(cmd, "banner") == 0) { cmd_banner(args); return 1; }
  if (strcmp(cmd, "yes") == 0) { cmd_yes(); return 1; }
  if (strcmp(cmd, "time") == 0) { cmd_time(); return 1; }
  if (strcmp(cmd, "fortune") == 0) { cmd_fortune(); return 1; }
  if (strcmp(cmd, "shutdown") == 0) { cmd_shutdown(); return 1; }
  if (strcmp(cmd, "rand") == 0) { char b[16]; itoa(rand(),b,10); print_string(b); print_string("\n"); return 1; }
  if (strcmp(cmd, "hex") == 0) { cmd_hex(args); return 1; }
  if (strcmp(cmd, "reverse") == 0) { cmd_reverse(args); return 1; }
  if (strcmp(cmd, "len") == 0) { char b[16]; itoa(strlen(args),b,10); print_string(b); print_string("\n"); return 1; }
  if (strcmp(cmd, "tolower") == 0) { for(int k=0;args[k];k++) if(args[k]>='A'&&args[k]<='Z') args[k]+=32; print_string(args); print_string("\n"); return 1; }
  if (strcmp(cmd, "toupper") == 0) { for(int k=0;args[k];k++) if(args[k]>='a'&&args[k]<='z') args[k]-=32; print_string(args); print_string("\n"); return 1; }
  if (strcmp(cmd, "uname") == 0) { print_string("HEXA OS 3.1 i386\n"); return 1; }
  if (strcmp(cmd, "whoami") == 0) { print_string(u_table[u_cur].name); print_string("\n"); return 1; }
  if (strcmp(cmd, "touch") == 0) { cmd_touch(args); save_data(); return 1; }
  if (strcmp(cmd, "ls") == 0) { cmd_ls(); return 1; }
  if (strcmp(cmd, "cat") == 0) { cmd_cat(args); return 1; }
  if (strcmp(cmd, "rm") == 0) { cmd_rm(args); save_data(); return 1; }
  if (strcmp(cmd, "write") == 0) { cmd_write(args); save_data(); return 1; }
  if (strcmp(cmd, "append") == 0) { cmd_append(args); save_data(); return 1; }
  if (strcmp(cmd, "edit") == 0) { cmd_edit(args); save_data(); return 1; }
  if (strcmp(cmd, "useradd") == 0) { cmd_useradd(args); save_data(); return 1; }
  if (strcmp(cmd, "passwd") == 0) { cmd_passwd(args); save_data(); return 1; }
  if (strcmp(cmd, "login") == 0) { cmd_login(args); return 1; }
  if (strcmp(cmd, "logout") == 0) { cmd_logout(); return 1; }
  if (strcmp(cmd, "sleep") == 0) { sleep_ticks(atoi(args) * 1000); return 1; }
  if (strcmp(cmd, "panic") == 0) { cmd_panic(); return 1; }
  if (strcmp(cmd, "outb") == 0) { print_string("Done\n"); return 1; }
  if (strcmp(cmd, "inb") == 0) { print_string("Done\n"); return 1; }
  if (strcmp(cmd, "hostname") == 0) { cmd_hostname(args); return 1; }
  if (strcmp(cmd, "id") == 0) { cmd_id(); return 1; }
  if (strcmp(cmd, "wc") == 0) { cmd_wc(args); return 1; }
  if (strcmp(cmd, "tail") == 0) { cmd_tail(args); return 1; }
  if (strcmp(cmd, "history") == 0) { cmd_hist(); return 1; }
  if (strcmp(cmd, "which") == 0) { cmd_which(args); return 1; }
  if (strcmp(cmd, "true") == 0) { cmd_true(); return 1; }
  if (strcmp(cmd, "false") == 0) { cmd_false(); return 1; }
  if (strcmp(cmd, "sort") == 0) { cmd_sort(args); return 1; }
  if (strcmp(cmd, "env") == 0) { cmd_env(); return 1; }
  if (strcmp(cmd, "cowsay") == 0) { cmd_cowsay(args); return 1; }
  if (strcmp(cmd, "cmatrix") == 0) { cmd_cmatrix(); return 1; }
  if (strcmp(cmd, "dice") == 0) { cmd_dice(args); return 1; }
  if (strcmp(cmd, "8ball") == 0) { cmd_8ball(); return 1; }
  if (strcmp(cmd, "logo") == 0) { cmd_logo(); return 1; }
  if (strcmp(cmd, "sl") == 0) { cmd_sl(); return 1; }
  if (strcmp(cmd, "morse") == 0) { cmd_morse(args); return 1; }
  if (strcmp(cmd, "russian") == 0) { cmd_russian(); return 1; }
  if (strcmp(cmd, "insult") == 0) { cmd_insult(); return 1; }
  if (strcmp(cmd, "excuse") == 0) { cmd_excuse(); return 1; }
  if (strcmp(cmd, "compliment") == 0) { cmd_compliment(); return 1; }
  if (strcmp(cmd, "hack") == 0) { cmd_hack(); return 1; }
  if (strcmp(cmd, "bsod") == 0) { cmd_bsod(); return 1; }
  if (strcmp(cmd, "ayo") == 0) { cmd_ayo(args); return 1; }
  if (strcmp(cmd, "chmod") == 0) { cmd_chmod(args); return 1; }
  if (strcmp(cmd, "chown") == 0) { cmd_chown(args); return 1; }
  if (strcmp(cmd, "grep") == 0) { cmd_grep(args); return 1; }
  if (strcmp(cmd, "find") == 0) { cmd_find(args); return 1; }
  if (strcmp(cmd, "diff") == 0) { cmd_diff(args); return 1; }
  if (strcmp(cmd, "uniq") == 0) { cmd_uniq(args); return 1; }
  if (strcmp(cmd, "alias") == 0) { cmd_alias(args); return 1; }
  if (strcmp(cmd, "unalias") == 0) { cmd_unalias(args); return 1; }
  if (strcmp(cmd, "pwd") == 0) { cmd_pwd(); return 1; }
  if (strcmp(cmd, "mkdir") == 0) { cmd_mkdir(args); return 1; }
  if (strcmp(cmd, "tee") == 0) { cmd_tee(args); return 1; }
  if (strcmp(cmd, "basename") == 0) { cmd_basename(args); return 1; }
  if (strcmp(cmd, "dirname") == 0) { cmd_dirname(args); return 1; }
  if (strcmp(cmd, "ps") == 0) { cmd_ps(); return 1; }
  if (strcmp(cmd, "kill") == 0) { cmd_kill(args); return 1; }
  if (strcmp(cmd, "seq") == 0) { cmd_seq(args); return 1; }
  if (strcmp(cmd, "tr") == 0) { cmd_tr(args); return 1; }
  if (strcmp(cmd, "who") == 0) { cmd_who(); return 1; }
  if (strcmp(cmd, "mv") == 0) {
    char src[32]={0}, dst[32]={0}; int i=0,j=0;
    while(args[i]&&args[i]!=' '&&j<31){src[j++]=args[i++];}
    while(args[i]==' '){i++;} j=0;
    while(args[i]&&j<31){dst[j++]=args[i++];}
    if(!src[0]||!dst[0]){print_string("Usage: mv <src> <dst>\n");return 1;}
    int idx=find_file(src);
    if(idx<0){print_string("Not found.\n");return 1;}
    if(check_perm(idx,1)){print_color("Denied.\n",0x0C);return 1;}
    strcpy(f_table[idx].name, dst);
    save_data(); print_string("Renamed.\n"); return 1;
  }
  if (strcmp(cmd, "cp") == 0) {
    char src[32]={0}, dst[32]={0}; int i=0,j=0;
    while(args[i]&&args[i]!=' '&&j<31){src[j++]=args[i++];}
    while(args[i]==' '){i++;} j=0;
    while(args[i]&&j<31){dst[j++]=args[i++];}
    if(!src[0]||!dst[0]){print_string("Usage: cp <src> <dst>\n");return 1;}
    int si=find_file(src);
    if(si<0){print_string("Not found.\n");return 1;}
    if(check_perm(si,0)){print_color("Denied.\n",0x0C);return 1;}
    if(f_count>=MAX_FILES){print_string("Full.\n");return 1;}
    strcpy(f_table[f_count].name, dst);
    memcpy(f_table[f_count].content, f_table[si].content, 512);
    f_table[f_count].size = f_table[si].size;
    f_table[f_count].owner = u_cur;
    f_table[f_count].mode = f_table[si].mode;
    f_count++;
    save_data(); print_string("Copied.\n"); return 1;
  }
  if (strcmp(cmd, "head") == 0) {
    char fname[32]={0}, ns[8]={0}; int i=0,j=0,n=10;
    while(args[i]&&args[i]!=' '&&j<31){fname[j++]=args[i++];}
    while(args[i]==' '){i++;} j=0;
    while(args[i]&&j<7){ns[j++]=args[i++];} ns[j]=0;
    if(ns[0]) n=atoi(ns);
    if(!fname[0]){print_string("Usage: head <file> [lines]\n");return 1;}
    int idx=find_file(fname);
    if(idx<0){print_string("Not found.\n");return 1;}
    if(check_perm(idx,0)){print_color("Denied.\n",0x0C);return 1;}
    int l=0,s=0;
    while(f_table[idx].content[s]&&l<n){
      uint8_t c=f_table[idx].content[s];
      if(c=='\n')l++;
      put_char(c,0x0F);
      s++;
    }
    print_string("\n"); return 1;
  }
  if (strcmp(cmd, "diese") == 0) {
    if (!args[0]) { print_string("Usage: diese <command> [args]\n"); return 1; }
    if (u_cur == 0) { print_string("Already root.\n"); return 1; }
    char pw[32];
    print_string("root password: ");
    get_line(pw, 32);
    if (!check_pwd(pw, u_table[0].pass_hash)) { print_color("Access denied.\n", 0x0C); return 1; }
    char c[32]={0}, a[96]={0}; int i=0,j=0;
    while(args[i]&&args[i]!=' '&&j<31){c[j++]=args[i++];}
    while(args[i]==' '){i++;} j=0;
    while(args[i]&&j<95){a[j++]=args[i++];}
    int saved=u_cur; u_cur=0;
    print_color("[diese] ", 0x0C);
    execute_cmd(c, a);
    u_cur=saved;
    return 1;
  }
  return 0;
}


// ----------------- Kernel Entry -----------------
void kernel_main(void) __attribute__((section(".text.entry")));

// Alias kernel_entry to kernel_main for bootloader compatibility
__asm__(".globl kernel_entry; .set kernel_entry, kernel_main;");

void kernel_main(void) {
  // inline asm check removed - serial not initialized yet

  for (char *p = _bss_start; p < _bss_end; p++) *p = 0;

  rseed = get_rtc_register(0x00);

  init_users();
  ata_init();
  load_data();
  do_login();

  outb(0x3F8, '#'); outb(0x3F8, ' ');

  serial_putc('['); serial_putc('O'); serial_putc('K'); serial_putc(']'); serial_putc('\n');

  char input_buffer[128], cmd[32], args[96];

  while (1) {
    do_tick();
    print_color(u_table[u_cur].name, 0x0B);
    print_string("@");
    print_string(hostname_str);
    if (u_table[u_cur].is_root)
      print_color("# ", 0x0C);
    else
      print_color("$ ", 0x0A);
    get_line(input_buffer, sizeof(input_buffer));

    if (input_buffer[0]) {
      strcpy(hist_buf[hist_idx], input_buffer);
      hist_idx = (hist_idx + 1) % HIST_MAX;
      if (hist_count < HIST_MAX) hist_count++;
    }

    int i = 0, j = 0;
    while (input_buffer[i] != ' ' && input_buffer[i] != '\0' && j < 31)
      cmd[j++] = input_buffer[i++];
    cmd[j] = '\0';
    while (input_buffer[i] == ' ')
      i++;
    j = 0;
    while (input_buffer[i] != '\0' && j < 95)
      args[j++] = input_buffer[i++];
    args[j] = '\0';

    if (cmd[0] == '\0')
      continue;

    // Alias expansion
    for (int ai = 0; ai < alias_count; ai++) {
      if (strcmp(cmd, alias_table[ai].name) == 0) {
        char expanded[128];
        strcpy(expanded, alias_table[ai].cmd);
        if (args[0]) {
          strcat(expanded, " ");
          strcat(expanded, args);
        }
        // Re-parse expanded command
        int ei = 0, ej = 0;
        while (expanded[ei] != ' ' && expanded[ei] != '\0' && ej < 31)
          cmd[ej++] = expanded[ei++];
        cmd[ej] = '\0';
        while (expanded[ei] == ' ') ei++;
        ej = 0;
        while (expanded[ei] != '\0' && ej < 95)
          args[ej++] = expanded[ei++];
        args[ej] = '\0';
        break;
      }
    }

    if (!execute_cmd(cmd, args)) {
      print_color("Unknown: ", 0x0C);
      print_color(cmd, 0x0C);
      print_string("\n");
    }
  }
}
