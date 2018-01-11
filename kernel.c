
//I TOOK BASE CODE FROM WIKI.OSDEV (THANKS!) , then I developed some stuff

#if !defined(__cplusplus) //We are not using c++
#include <stdbool.h> // C doesn't boolean
#endif
#include <stddef.h>
#include <stdint.h>
 
/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

//My library with useful codes for keyboard mini driver. I took information in a lot of websites (THANKS), and
//I use this to write a little driver and also to give you an entry point for future development.
#include "codes.h" 

//--------------------------------------------INLINE ASM----------------------------------------------------
//Low level port input/output. Read/write ONE byte from/to a port
static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
    /* There's an outb %al, $imm8  encoding, for compile-time constant port numbers that fit in 8b.  (N constraint).
     * Wider immediate constants would be truncated at assemble-time (e.g. "i" constraint).
     * The  outb  %al, %dx  encoding is the only option for all other cases.
     * %1 expands to %dx because  port  is a uint16_t.  %w1 could be used if we had the port number a wider C type */
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}
//----------------------------------------END INLINE ASM----------------------------------------------------


//---------------------------------------DRIVER KEYBOARD------------------------------------------------------------------
//All codes are in my codes.h library

//read controller status
uint8_t kybrd_ctrl_read_status(){
  return inb(KYBRD_CTRL_STATS_REG);
}
//send command byte to controller
void kybrd_ctrl_send_cmd (uint8_t cmd) {
  //wait for controller input buffer to be clear
  while (1)
    if ( (kybrd_ctrl_read_status () & KYBRD_CTRL_STATS_MASK_IN_BUF) == 0)
      break;
  outb(KYBRD_CTRL_CMD_REG, cmd);
}

//read encoder buffer
uint8_t kybrd_enc_read_buf () {
  return inb(KYBRD_ENC_INPUT_BUF);
}

//write command byte to encoder
void kybrd_enc_send_cmd (uint8_t cmd) {
  //wait for controller input buffer to be clear
  while (1)
    if ( (kybrd_ctrl_read_status () & KYBRD_CTRL_STATS_MASK_IN_BUF) == 0)
      break;
  // send command byte to encoder
  outb(KYBRD_ENC_CMD_REG, cmd);
}

//----------------------------------------FINE KEYBORD--------------------------------------------------------------------

size_t strlen(const char* str) 
{
  size_t len = 0;
  while (str[len])
    len++;
  return len;
}

//--------------------------------------------SCREEN STUFF------------------------------------------------------------------
/* Hardware text mode color constants. */
enum vga_color {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
};
 
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
  return fg | bg << 4;
}
 
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
  return (uint16_t) uc | (uint16_t) color << 8;
}
 
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

//group of useful variables ;)
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t *terminal_buffer;

void terminal_initialize(void) 
{
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal_buffer = (uint16_t*) 0xB8000;
  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
  }
}

void terminal_setcolor(uint8_t color) 
{
  terminal_color = color;
}
 
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
}

void terminal_clear_line(size_t y)   //clear gived line
{ 
  size_t x = 0;
  while(x < VGA_WIDTH){
    terminal_putentryat(' ', terminal_color, x, y);
    x++;
  }
}

void terminal_clearscreen(void)    //clear all screen and set prompt to up left corner
{
  size_t y = 0;
  while(y < VGA_HEIGHT){
    terminal_clear_line(y);
    y++;
  }
  terminal_row = 0;
  terminal_column = 0;
}
 
void terminal_scroll(void)         //scroll losing the first line written, need history? Have fun! :)
{ 
  for (size_t y = 0; y < VGA_HEIGHT; y++){
    for (size_t x = 0; x < VGA_WIDTH; x++){
      const size_t index = y * VGA_WIDTH + x;
      const size_t next_index = (y + 1) * VGA_WIDTH + x;
      terminal_buffer[index] = terminal_buffer[next_index];
    }
  }
  terminal_clear_line(VGA_HEIGHT-1);
  terminal_row = VGA_HEIGHT-1;
  terminal_column = 0;
}

void terminal_putchar(char c)     //putchar with integrated scroll, NOT optimized
{
  if(c == '\n'){
    terminal_column = 0;
    if(terminal_row == VGA_HEIGHT-1){
      terminal_scroll();
    }else {
      terminal_row++;
    }

  }else {
    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
      terminal_column = 0;
      if (++terminal_row == VGA_HEIGHT)
        terminal_row = 0;
    }
  }
}
 
void terminal_write(const char* data, size_t size) 
{
  for (size_t i = 0; i < size; i++)
    terminal_putchar(data[i]);
}
 
void terminal_writestring(const char* data) 
{
  terminal_write(data, strlen(data));
}

//----------------------------------------------------UTILITY (or survival?)-----------------------------------

void memset(char* buffer, char ch, int len){    //I really need this!!
  for(int i=0;i<len;i++){
    buffer[i]=ch;
  }
}

//------------------------------------------------------END UTILITY--------------------------------------------


//------------------------------------------------STRING FUNCTIONS--------------------------------------------
char* itoa(int i, char b[]){          //From integer to ascii, thanks to SENDRAY
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0){
        *p++ = '-';
        i *= -1;
    }
    int shifter = i;
    do{ //Move to where representation ends
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{ //Move back, inserting digits as u go
        *--p = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}

int strcmp(const char *str1, const char *str2){
  int res = 1;
  int i = 0;
  while(str1[i] != 0x0A && str2[i] != 0x0A){
    if(str1[i] != str2[i]){
      res = 0;
    }
    i++;
  }
  return res;
}

bool isascii(int c){
  return c >= 0 && c<128;
}
//------------------------------------------------END STRING FUNCTIONS----------------------------------------


//--------------------------------------------KEYBOARD BAD STUFF----------------------------------------------------
//getchar and getline have been the most difficult parts!
//WARNING: If you press extended or special button, you will print/insert them! Have fun :)
//The trick is: Don't type wrong char lol
char getchar(){              
    uint8_t code = 0;
    uint8_t key = 0;
    while(1){
      if (kybrd_ctrl_read_status () & KYBRD_CTRL_STATS_MASK_OUT_BUF) {
        code = kybrd_enc_read_buf ();
        if(code <= 0x58){
          key = _kkybrd_scancode_std [code];
          break;
        }
      }
    }
    return key;
}
void getline(char* string, int len){
uint8_t i=0;
  int flag = 0;
  char temp = 0;
  memset(string,0,len);
  while(i<len && temp != 0x0D){
    temp = getchar();
    if(isascii(temp) && temp != 0x0D){
      terminal_putchar(temp);
      string[i] = temp;
      i++;
    }
  }
  string[i] = 0x0A;
}
//------------------------------------------------END BAD STUFF----------------------------------------------


//------------------------------------------------INTERFACE UTILITY-------------------------------------------
void spawn_prompt(){
  terminal_writestring("neetx@myos:> ");
}

void hello_user(char* user){
  terminal_writestring("*** WELCOME TO NEETX MINI OS ***\n");
  terminal_writestring("Hello ");
  terminal_writestring(user);
  terminal_writestring("\n");
}
//-------------------------------------------------END INTERFACE UTILITY------------------------------------------

//--------------------------------------------------------COMMAND FUNCTIONS--------------------------------------
//clear is in the vga driver

void help(){
  terminal_writestring("---- HELP MENU ----\n");
  terminal_writestring("Commands:\n");
  terminal_writestring("  help     this menu\n");
  terminal_writestring("  shutdown power of machine\n");
  terminal_writestring("  echo     type and receive a response\n");
  terminal_writestring("  clear    clear all screen\n");
}

void echo(){
  char string[50];
  terminal_writestring("Enter string: ");
  getline(string, 50);
  terminal_writestring("\n");
  terminal_writestring(string);
}

void shutdown(){
  terminal_clearscreen();
  terminal_writestring("You can shutdown");
  __asm__ __volatile__("outw %1, %0" : : "dN" ((uint16_t)0xB004), "a" ((uint16_t)0x2000));
}

int get_command(){
  int cmd = 1;
  char string[10];
  getline(string, 10);
  if(strcmp(string,"help")==1){
    cmd = 1;
  }else{
    if(strcmp(string,"shutdown")==1){
      cmd = 2;
    }else{
      if(strcmp(string,"echo")==1){
        cmd = 3;
      }else{
        if(strcmp(string,"clear")==1){
          cmd = 4;
        }else{
          cmd = 5;
        }
      }
    }
  }
  memset(string,0,10);
  terminal_writestring("\n");
  return cmd;
}

void execute_command(int cmd){
  switch(cmd){
    case 1:
      help();
      break;
    case 2:
      shutdown();
      break;
    case 3:
      echo();
      break;
    case 4:
      terminal_clearscreen();
      break;
    default: 
      terminal_writestring("[!]nsh: Command not found\n");
  }
}
//------------------------------------------------------------END COMMANDS----------------------------------
//-------------------------------------MAIN---------------------------------------------
#if defined(__cplusplus)
extern "C" /* Use C*/
#endif
void kernel_main(void) 
{
  terminal_initialize();

  int cmd = 0;
  hello_user("Neetx");
  while(1){       //-------------Command loop
    spawn_prompt();
    cmd = get_command();
    execute_command(cmd);
    if(cmd == 2){
      break;
    }
    cmd = 0;
  }
}