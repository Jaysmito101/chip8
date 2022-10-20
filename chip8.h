#ifndef CHIP8_H
#define CHIP8_H

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

// types
typedef unsigned char chip8_u8;
typedef unsigned short chip8_u16;

struct chip8
{    
    chip8_u8 memory[4096];
    chip8_u8 display[64 * 32];
    chip8_u16 stack[16];
    chip8_u8 regs[16];
    chip8_u16 opcode;
    chip8_u16 I;
    chip8_u16 PC; // program counter
    chip8_u8 SP; // stack pointer
    chip8_u8 DT; // delay timer
    chip8_u8 ST; // sound timer
};

void chip8_init(struct chip8* vm);
chip8_u8 chip8_load_rom(struct chip8* vm, const chip8_u8* data, chip8_u16 data_size);
void chip8_update_timer(struct chip8* vm);
chip8_u8 chip8_cycle(struct chip8* vm, const chip8_u8* input);


#ifdef CHIP8_IMPLEMENTATION

#ifndef chip8_log
#define chip8_log(...)
#endif

#ifndef chip8_rand
#include <stdlib.h>
#define chip8_rand rand
#endif

static const chip8_u8 font[16 * 5] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80 // F
};


static void chip8__memset(chip8_u8* mem, chip8_u16 size, chip8_u8 value)
{
    for(chip8_u16 i = 0 ; i < size ; i++) mem[i] = value;
}

static void chip8__memcpy(chip8_u8* dst, const chip8_u8* src, chip8_u16 size)
{
    for(chip8_u16 i = 0 ; i < size ; i++) dst[i] = src[i];
}

static char chip8__to_hex(chip8_u8 dec)
{
    if(dec <= 9) return '0' + dec;
    return 'A' + (dec - 10);
}

static chip8_u8 chip8__to_u8(char hex)
{
    if(hex < 65) return hex - '0';
    return hex - '0' + 10;
}

void chip8_init(struct chip8* vm)
{
    chip8__memset(vm->memory, 4096, 0);
    chip8__memset(vm->display, 64 * 32, 0);
    chip8__memset((chip8_u8*)vm->stack, 2 * 16, 0);
    chip8__memset(vm->regs, 16, 0);
    chip8__memcpy(vm->memory, font, 16 * 5);
    vm->I = 0;
    vm->PC = 0x200; // 512 (0 - 511 is the interpreter data)
    vm->SP = 0;
    vm->DT = 0;
    vm->ST = 0;
}


chip8_u8 chip8_load_rom(struct chip8* vm, const chip8_u8* data, chip8_u16 data_size)
{
    if((data_size - 512) >= 4096) return false;
    chip8_init(vm);
    chip8__memcpy(vm->memory + 0x200, data, data_size);
    return true;
}

void chip8_update_timer(struct chip8* vm)
{
    if(vm->DT > 0) vm->DT--;
    if(vm->ST > 0) vm->ST--;
}


// NOTE: The notes about the instructions are taken from : http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#00Cn

chip8_u8 chip8_cycle(struct chip8* vm, const chip8_u8* input)
{
    chip8_u8 opcode_s = (chip8_u8)((vm->memory[vm->PC] & 0b11110000) >> 4);
    chip8_u8 opcode_x = (chip8_u8)((vm->memory[vm->PC] & 0b00001111));
    chip8_u8 opcode_y = (chip8_u8)((vm->memory[vm->PC + 1] & 0b11110000) >> 4);
    chip8_u8 opcode_n = (chip8_u8)((vm->memory[vm->PC + 1] & 0b00001111));
    chip8_u8 opcode_nn = (chip8_u8)(vm->memory[vm->PC + 1]);
    chip8_u16 opcode_nnn = (chip8_u16)(vm->memory[vm->PC] & 0b00001111);
    opcode_nnn = (opcode_nnn << 8);
    opcode_nnn |= (chip8_u16)(vm->memory[vm->PC + 1]);
    if(opcode_s == 0 && opcode_x == 0 && opcode_y == 0 && opcode_n == 0)  return false;
    vm->PC += 2;


    switch(opcode_s)
    {
        case 0:
        {
            if(opcode_y == 14 && opcode_n == 0) // CLS (0x00E0)
            {
                chip8_log("CLS\n");
                // Clear the display.
                chip8__memset(vm->display, 64 * 32, 0);
            }
            else if(opcode_y == 14 && opcode_n == 14) // RET (0x00EE)
            {
                chip8_log("RET\n");
                // Return from a subroutine.
                // The interpreter sets the program
                // counter to the address at the
                // top of the stack, then subtracts
                // 1 from the stack pointer.
                if(vm->SP == 0)
                {
                    chip8_log("Stack Underflow\n");
                    return false;
                }
                vm->SP -= 1;
                vm->PC = vm->stack[vm->SP];
            }
            else // SYS addr (0x0nnn)
            {
                chip8_log("SYS %d ; (NOT IMPLEMENTED)\n", opcode_nnn);                
                // Jump to a machine code routine at nnn
                // This instruction is only used on the
                // old computers on which Chip-8 was
                // originally implemented. It is ignored
                // by modern interpreters.
            }
            break;
        }
        case 1: // JP addr (0x1nnn)
        {
            chip8_log("JP %d\n", opcode_nnn);
            // Jump to location nnn.
            // The interpreter sets the
            // program counter to nnn.
            vm->PC = opcode_nnn;
            break;
        }
        case 2: //  CALL addr (0x2nnn)
        {
            chip8_log("CALL %d\n", opcode_nnn);
            // Call subroutine at nnn.
            // The interpreter increments
            // the stack pointer, then puts
            // the current PC on the top
            // of the stack. The PC is
            // then set to nnn.
            if(vm->SP == 16)
            {
                chip8_log("Stack Overflow\n");
                return false;
            }
            vm->stack[vm->SP] = vm->PC;
            vm->SP += 1;
            vm->PC = opcode_nnn;
            break;
        }
        case 3: // SE Vx, byte (0x3xnn)
        {
            chip8_log("SE V%c, %d\n", chip8__to_hex(opcode_x), opcode_nn);
            // Skip next instruction if Vx = nn.
            // The interpreter compares register
            // Vx to nn, and if they are equal,
            // increments the program counter by 2.
            if(vm->regs[opcode_x] == opcode_nn) vm->PC += 2;
            break;
        }
        case 4: // SNE Vx, byte (0x4xnn)
        {
            chip8_log("SNE V%c, %d\n", chip8__to_hex(opcode_x), opcode_nn);
            //Skip next instruction if Vx != nn.
            //The interpreter compares register
            // Vx to nn, and if they are not equal,
            // increments the program counter by 2.
            if(vm->regs[opcode_x] != opcode_nn) vm->PC += 2;
            break;
        }
        case 5: // SE Vx, Vy (0x5xy0)
        {
            chip8_log("SE V%c, V%c \n", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
            //Skip next instruction if Vx = Vy.
            //The interpreter compares register
            // Vx to register Vy, and if they are
            // equal, increments the program
            // counter by 2.
            if(vm->regs[opcode_x] == vm->regs[opcode_y]) vm->PC += 2;
            break;
        }
        case 6: // LD Vx, byte (0x6xnn)
        {
            chip8_log("LD V%c, %d\n", chip8__to_hex(opcode_x), opcode_nn);
            // Set Vx = nn.
            // The interpreter puts the
            // value nn into register Vx.
            vm->regs[opcode_x] = opcode_nn;
            break;
        }
        case 7: // ADD Vx, byte (0x7xnn)
        {
            chip8_log("ADD V%c, %d\n", chip8__to_hex(opcode_x), opcode_nn);
            // Set Vx = Vx + nn.
            // Adds the value nn to the
            // value of register Vx, then
            // stores the result in Vx.
            chip8_u16 temp = (chip8_u16)vm->regs[opcode_x] + (chip8_u16)opcode_nn;           
            if(temp > 255) temp = temp - 256;
            vm->regs[opcode_x] = (chip8_u8)temp;
            break;
        }
        case 8:
        {
            switch(opcode_n)
            {
                case 0: // LD Vx, Vy (0x8xy0)
                {
                    chip8_log("LF V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vy.
                    // Stores the value of register
                    // Vy in register Vx.
                    vm->regs[opcode_x] = vm->regs[opcode_y];
                    break;
                }
                case 1: // OR Vx, Vy (0x8xy1)
                {
                    chip8_log("OR V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vx OR Vy.
                    // Performs a bitwise OR on the values
                    // of Vx and Vy, then stores the result
                    // in Vx. A bitwise OR compares the
                    // corrseponding bits from two values,
                    // and if either bit is 1, then the same
                    // bit in the result is also 1. Otherwise,
                    // it is 0.
                    vm->regs[opcode_x] = vm->regs[opcode_x] | vm->regs[opcode_y];
                    break;
                }
                case 2: // AND Vx, Vy (0x8xy2)
                {
                    chip8_log("AND V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vx AND Vy.
                    // Performs a bitwise AND on the values
                    // of Vx and Vy, then stores the result
                    // in Vx. A bitwise AND compares the
                    // corrseponding bits from two values,
                    // and if both bits are 1, then the same
                    // bit in the result is also 1. Otherwise,
                    // it is 0.
                    vm->regs[opcode_x] = vm->regs[opcode_x] & vm->regs[opcode_y];
                    break;
                }
                case 3: // XOR Vx, Vy (0x8xy3)
                {
                    chip8_log("XOR V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vx XOR Vy.
                    // Performs a bitwise exclusive OR
                    // on the values of Vx and Vy,
                    // then stores the result in Vx.
                    // An exclusive OR compares the
                    // corrseponding bits from two values,
                    // and if the bits are not both the same,
                    // then the corresponding bit in the
                    // result is set to 1. Otherwise, it is 0.
                    vm->regs[opcode_x] = vm->regs[opcode_x] ^ vm->regs[opcode_y];
                    break;
                }
                case 4: // ADD Vx, Vy (0x8xy4)
                {
                    chip8_log("ADD V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vx + Vy, set VF = carry.
                    // The values of Vx and Vy are added
                    // together. If the result is greater
                    // than 8 bits (i.e., > 255,) VF is set
                    // to 1, otherwise 0. Only the lowest
                    // 8 bits of the result are kept,
                    // and stored in Vx.
                    chip8_u16 temp = (chip8_u16)vm->regs[opcode_x] + (chip8_u16)vm->regs[opcode_y];
                    if(temp > 255) { vm->regs[15] = 1; temp = temp - 256; }
                    else vm->regs[15] = 0;                     
                    vm->regs[opcode_x] = (chip8_u8)temp;
                    break;
                }
                case 5: // SUB Vx, Vy (0x8xy5)
                {
                    chip8_log("SUB V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vx - Vy, set VF = NOT borrow.
                    // If Vx > Vy, then VF is set to 1,
                    // otherwise 0. Then Vy is subtracted
                    // from Vx, and the results stored in Vx.
                    vm->regs[opcode_x] = vm->regs[opcode_x] - vm->regs[opcode_y];
                    if(vm->regs[opcode_x] > vm->regs[opcode_y]) vm->regs[15] = 1;
                    else vm->regs[15] = 0;
                    break;
                }
                case 6: // SHR Vx, Vy (0x8xy6)
                {
                    chip8_log("SHR V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vx SHR 1.
                    // If the least-significant bit of Vx
                    // is 1, then VF is set to 1, otherwise 0.
                    // Then Vx is divided by 2.
                    if(vm->regs[opcode_x] & 0b00000001) vm->regs[15] = 1;
                    else vm->regs[15] = 0;
                    break;
                }
                case 7: // SUBN Vx, Vy (0x8xy7)
                {
                    chip8_log("SUBN V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vy - Vx, set VF = NOT borrow.
                    // If Vy > Vx, then VF is set to 1,
                    // otherwise 0. Then Vx is subtracted
                    // from Vy, and the results stored in Vx.
                    vm->regs[opcode_x] = vm->regs[opcode_x] - vm->regs[opcode_y];
                    if(vm->regs[opcode_x] < vm->regs[opcode_y]) vm->regs[15] = 1;
                    else vm->regs[15] = 0;
                    break;
                }
                case 14: // SHL Vx, Vy (0x8xyE)
                {
                    chip8_log("SHL V%c, V%c", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
                    // Set Vx = Vx SHL 1.
                    // If the most-significant bit of
                    // Vx is 1, then VF is set to 1,
                    // otherwise to 0. Then Vx is multiplied
                    // by 2.               
                    if(vm->regs[opcode_x] & 0b10000000) vm->regs[15] = 1;
                    else vm->regs[15] = 0;
                    break;
                }
                default:
                {
                    chip8_log("; Unknown Instruction 0xE%c%c%c \n", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y), chip8__to_hex(opcode_n));
                    //return false;
                }
            }
            break;
        }
        case 9: // SNE Vx, Vy (0x9xy0)
        {
            chip8_log("SE V%c, V%c \n", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y));
            // Skip next instruction if Vx != Vy.
            // The values of Vx and Vy are compared,
            // and if they are not equal, the
            // program counter is increased by 2.
            if(vm->regs[opcode_x] != vm->regs[opcode_y]) vm->PC += 2;
            break;
        }
        case 10: // LD I, addr (0xAnnn)
        {
            chip8_log("LD I, %d \n", opcode_nnn);
            // Set I = nnn.
            // The value of register I is set to nnn.
            vm->I = opcode_nnn;
            break;
        }
        case 11: // JP V0, addr (0xBnnn)
        {
            chip8_log("JP V0, %d \n", opcode_nnn);
            // Jump to location nnn + V0.
            // The program counter is set
            // to nnn plus the value of V0
            vm->PC = vm->regs[0] + opcode_nnn;
            break;
        }
        case 12: // RND Vx, byte (0xCxnn)
        {
            chip8_log("RND V%c, %d \n", chip8__to_hex(opcode_x), opcode_nn);
            // Set Vx = random byte AND nn.
            // The interpreter generates a
            // random number from 0 to 255,
            // which is then ANDed with the
            // value nn. The results are stored
            // in Vx. See instruction 8xy2
            // for more information on AND.
            vm->regs[opcode_x] = (chip8_u8)(rand() % 255) & opcode_nn;
            break;
        }
        case 13: // DRW Vx, Vy, nibble (0xDxyn)
        {
            chip8_log("DRW V%c, V%c, %d \n", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y), opcode_n);
            // Display n-byte sprite starting at memory
            // location I at (Vx, Vy), set VF = collision.
            // The interpreter reads n bytes from memory,
            // starting at the address stored in I.
            // These bytes are then displayed as
            // sprites on screen at coordinates (Vx, Vy).
            // Sprites are XORed onto the existing screen.
            // If this causes any pixels to be erased,
            // VF is set to 1, otherwise it is set to 0.
            // If the sprite is positioned so part of it
            // is outside the coordinates of the display,
            // it wraps around to the opposite side of the screen.

            chip8_u8 x_loc = vm->regs[opcode_x];
            chip8_u8 y_loc = vm->regs[opcode_y];
            chip8_u8 height = opcode_n;
            chip8_u8 pixel = 0;
            vm->regs[15] = 0;
            for(chip8_u16 y = 0 ; y < height ; y++)
            {
                pixel = vm->memory[vm->I + y];
                for(chip8_u16 x = 0 ; x < 8 ; x++ )
                {
                    if((pixel & (0x80 >> x)) != 0)
                    {
                        chip8_u16 yf = (y_loc + y) % 32;
                        chip8_u16 xf = (x_loc + x) % 64;
                        if(vm->display[yf * 64 + xf] == 1) vm->regs[15] = 1;
                        vm->display[yf * 64 + xf] ^= 1;
                    }
                }
            }
            break;
        }
        case 14: // E
        {
            if(opcode_n == 14) // SKP Vx (0xEx9E)
            {
                chip8_log("SKP V%c \n", chip8__to_hex(opcode_x));
                // Skip next instruction if key
                // with the value of Vx is pressed.
                // Checks the keyboard, and if the key
                // corresponding to the value of Vx is
                // currently in the down position, PC
                // is increased by 2.

                if(input[vm->regs[opcode_x]]) vm->PC += 2;
            }
            else if(opcode_n == 1) // SKNP Vx (0xExA1)
            {
                chip8_log("SKNP V%c \n", chip8__to_hex(opcode_x));
                // Skip next instruction if key with
                // the value of Vx is not pressed.
                // Checks the keyboard, and if the key
                // corresponding to the value of Vx is
                // currently in the up position,
                // PC is increased by 2.

                if(!input[vm->regs[opcode_x]]) vm->PC += 2;
            }
            else
            {
                chip8_log("; Unknown Instruction 0xE%c%c%c \n", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y), chip8__to_hex(opcode_n));
               // return false;
            }
            break;
        }
        case 15: // F
        {
            if(opcode_n == 7) // LD Vx, DT (0xFx07)
            {
                chip8_log("LD V%c, DT\n", chip8__to_hex(opcode_x));
                // Set Vx = delay timer value.
                // The value of DT is placed into Vx.
                vm->regs[opcode_x] = vm->DT;
            }
            else if(opcode_n == 10) // LD Vx, K (0xFx0A)
            {
                chip8_log("LD V%c, K\n", chip8__to_hex(opcode_x));
                // Wait for a key press, store
                // the value of the key in Vx.
                // All execution stops until a
                // key is pressed, then the value
                // of that key is stored in Vx.
                
                if(input[vm->regs[opcode_x]]) vm->PC -= 2;
            }
            else if(opcode_y == 1 && opcode_n == 5) // LD DT, Vx (0xFx15)
            {
                chip8_log("LD DT, V%c\n", chip8__to_hex(opcode_x));
                // Set delay timer = Vx.
                // DT is set equal to the value of Vx.
                vm->DT = vm->regs[opcode_x];
            }
            else if(opcode_n == 8) // LD ST, Vx (0xFx18)
            {
                chip8_log("LD ST, V%c\n", chip8__to_hex(opcode_x));
                // Set sound timer = Vx.
                // ST is set equal to the value of Vx.
                vm->ST = vm->regs[opcode_x];
            }
            else if(opcode_n == 14) // ADD I, Vx (0xFx1E)
            {
                chip8_log("ADD I, V%c\n", chip8__to_hex(opcode_x));
                // Set I = I + Vx.
                // The values of I and Vx are
                // added, and the results are
                // stored in I.
                vm->I = vm->I + vm->regs[opcode_x];
            }
            else if(opcode_n == 9) // LD F, Vx (0xFx29)
            {
                chip8_log("LD F, V%c\n", chip8__to_hex(opcode_x));
                // Set I = location of sprite for digit Vx.
                // The value of I is set to the location
                // for the hexadecimal sprite corresponding
                // to the value of Vx.
                vm->I = 5 * vm->regs[opcode_x];
            }
            else if(opcode_n == 3) // LD B, Vx (0xFx33)
            {
                chip8_log("LD B, V%c\n", chip8__to_hex(opcode_x));
                // Store BCD representation of Vx
                // in memory locations I, I+1, and I+2.
                // The interpreter takes the decimal
                // value of Vx, and places the hundreds
                // digit in memory at location in I,
                // the tens digit at location I+1,
                // and the ones digit at location I+2.
                chip8_u8 temp = vm->regs[opcode_x];
                chip8_u8 ones_digit = temp % 10; temp /= 10;
                chip8_u8 tens_digit = temp % 10; temp /= 10;
                chip8_u8 hund_digit = temp % 10;
                vm->memory[vm->I + 0] = hund_digit;
                vm->memory[vm->I + 1] = tens_digit;
                vm->memory[vm->I + 2] = ones_digit;
            }
            else if(opcode_y == 5 && opcode_n == 5) // LD [I], Vx (0xFx55)
            {
                chip8_log("LD [I], V%c\n", chip8__to_hex(opcode_x));
                // Store registers V0 through Vx
                // in memory starting at location I.
                // The interpreter copies the values
                // of registers V0 through Vx into
                // memory, starting at the address in I.
                chip8__memcpy(vm->memory + vm->I, vm->regs, opcode_x);
            }
            else if(opcode_y == 6 && opcode_n == 5) // LD Vx, [I] (0xFx65)
            {
                chip8_log("LD V%c, [I]\n", chip8__to_hex(opcode_x));
                // Read registers V0 through Vx
                // from memory starting at location I.
                // The interpreter reads values from
                // memory starting at location I
                // into registers V0 through Vx.
                chip8__memcpy(vm->regs, vm->memory + vm->I, opcode_x);                
            }            
            else
            {
                chip8_log("; Unknown Instruction 0xE%c%c%c \n", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y), chip8__to_hex(opcode_n));
                //return false;
            }
            break;
        }
        default:
        {
            chip8_log("; Unknown Instruction 0xE%c%c%c \n", chip8__to_hex(opcode_x), chip8__to_hex(opcode_y), chip8__to_hex(opcode_n));
            //return false;
        }
    }
    if(vm->PC >= 4096) return false;
    return true;
}

#endif

#endif // CHIP8_H