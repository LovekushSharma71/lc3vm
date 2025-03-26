/* 
    main.c
*/

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
/* unix only */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#define MEMORY_MAX (1 << 16)

enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, /* program counter */
    R_CD, /* condition tracker */
    R_CT  /* number of registers */
};
enum
{
    COND_P = 1 << 0, /* Postive condition flag */
    COND_Z = 1 << 1, /* Zero condition flag */
    COND_N = 1 << 2, /* Negative condition flag */
};
enum
{
    OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};
enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};
enum
{
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

uint16_t checkKeys();

/*
    memory 
*/
struct lc3memory
{
    uint16_t memory[MEMORY_MAX];
    uint16_t regstr[R_CT];
};
typedef struct lc3memory vmState;

vmState *initMem(){
    
    vmState *mem = (vmState *)malloc(sizeof(vmState));
    for (int i = 0; i < MEMORY_MAX; i++)
    {
        mem->memory[i]=0;
    }
    for (int i = 0; i < R_CT; i++)
    {
        mem->regstr[i]=0;
    }
    return mem;
}

void stopVm(vmState *vmState){
    free(vmState);
}

void mem_write(vmState *vmState, uint16_t address, uint16_t val){
    vmState->memory[address]=val;
}
uint16_t mem_read(vmState *vmState,uint16_t address){
    if (address == MR_KBSR)
    {
        if (checkKeys())
        {
            vmState->memory[MR_KBSR] = (1 << 15);
            vmState->memory[MR_KBDR] = getchar();
        }
        else
        {
            vmState->memory[MR_KBSR] = 0;
        }
    }
    return vmState->memory[address];
}

/*
    terminal
*/
struct termios originalTio;

void disableInputBuffering(){
    
    //get current terminal attributes and store it in variable
    tcgetattr(STDIN_FILENO, &originalTio);
    struct termios newTio = originalTio;
    /*
        disables both canonical mode (ICANON- terminal doesn't wait for a newline 
        character (like pressing Enter) to complete a line of input) and echoing 
        (ECHO- terminal doesn't display the characters the user types on the 
        screen) for a terminal 
    */ 
   newTio.c_lflag &= ~ICANON & ~ECHO;
   tcsetattr(STDIN_FILENO,TCSANOW,&newTio);
}

void restoreInputBuffering(){

    // set orignal attributes back
    tcsetattr(STDIN_FILENO, TCSANOW, &originalTio);
}

void handleInterrupt(int signal){
    restoreInputBuffering();
    printf("\n");
    exit(-2);
}

uint16_t checkKeys(){
    
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(STDIN_FILENO,&readFds);

    struct timeval timeout;
    timeout.tv_sec=0,timeout.tv_usec=0;
    return select(1,&readFds,NULL,NULL,&timeout)!=0;
}

/*
    helper functions
*/
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}
uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}
void update_flags(vmState *vmState,int regId){
    
    if (vmState->regstr[regId] == 0)
    {
        vmState->regstr[R_CD] = COND_Z;
    }
    else if (vmState->regstr[regId] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        vmState->regstr[R_CD] = COND_N;
    }
    else
    {
        vmState->regstr[R_CD] = COND_P;
    }
    
}
int readImageFile(vmState *vmState,const char* imgPath){
    
    FILE *file = fopen(imgPath,"rb");
    if (!file)return 0;
    
    uint16_t origin;
    fread(&origin,sizeof(uint16_t),1,file);
    origin=swap16(origin);
    // vmState->regstr[R_PC]=origin;

    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t *p = vmState->memory + origin;
    int read = fread(p, sizeof(uint16_t), max_read, file);

    while (read-- >0 )
    {
        *p=swap16(*p);
        p++;
    }
    fclose(file);
    return 1;
}

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        /* show usage string */
        printf("lc3 image-file ...\n");
        exit(2);
    }

    vmState *vmState = initMem();

    for (size_t i = 0; i < argc; i++)
    {
        if (!readImageFile(vmState,argv[i]))
        {
            printf("failed to load image: %s\n", argv[i]);
            exit(1);
        }
    }

    signal(SIGINT, handleInterrupt);
    disableInputBuffering();

    vmState->regstr[R_CD]=COND_Z;
    vmState->regstr[R_PC]=0x3000;

    int isRunning=1;
    while (isRunning)
    {
        uint16_t instr = vmState->memory[vmState->regstr[R_PC]++];
        uint16_t opcode = instr >>12;
        switch (opcode)
        {
        case OP_ADD:
        {
            uint16_t dr = (instr >> 9) & 0b111;
            uint16_t sr1 = (instr >> 6) & 0b111;
            int flag = (instr >> 5) & 0b1;
            if (flag)
            {
                uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                vmState->regstr[dr]=vmState->regstr[sr1]+imm5;
            }else{
                uint16_t sr2 = instr & 0b111;
                vmState->regstr[dr]=vmState->regstr[sr1] + vmState->regstr[sr2];
            }
            update_flags(vmState,dr);
        }
        break;
        case OP_AND:
        {
            uint16_t dr = (instr >> 9) & 0b111;
            uint16_t sr1 = (instr >> 6) & 0b111;
            int flag = (instr >> 5) & 0b1;
            if (flag)
            {
                uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                vmState->regstr[dr]=vmState->regstr[sr1]&imm5;
            }else{
                uint16_t sr2 = instr & 0b111;
                vmState->regstr[dr]=vmState->regstr[sr1] & vmState->regstr[sr2];
            }
            update_flags(vmState,dr);
        }
        break;
        case OP_BR:
        {
            uint16_t pcOffset = sign_extend(instr & 0x1ff,9);
            uint16_t condFlag = (instr>>9) & 0b111;
            if (condFlag & vmState->regstr[R_CD])
            {
                vmState->regstr[R_PC]+=pcOffset;
            }
        }
        break;
        case OP_NOT:
        {    
            uint16_t dr = (instr >> 9) & 0x7;
            uint16_t sr1 = (instr >> 6) & 0x7;
        
            vmState->regstr[dr] = ~vmState->regstr[sr1];
            update_flags(vmState,dr);
        }
        break;
        case OP_JMP:
        {    
            uint16_t r1 = (instr >> 6) & 0x7;
            vmState->regstr[R_PC] = vmState->regstr[r1];
        }
        break;
        case OP_JSR:
        {
            uint16_t lFlag = (instr>>11) & 0b1;
            vmState->regstr[R_R7]=vmState->regstr[R_PC];
            if (lFlag)
            {
                uint16_t lPcOffset = sign_extend(instr&0x7ff,11);
                vmState->regstr[R_PC]+=lPcOffset;
            }else{
                uint16_t tr = (instr>>6)&0b111;
                vmState->regstr[R_PC]=vmState->regstr[tr];
            }
        }
        break;
        case OP_LD:
        {
            uint16_t tr = (instr>>9)&0b111;
            uint16_t pcOffset = sign_extend(instr&0x1ff,9);
            vmState->regstr[tr] = mem_read(vmState,vmState->regstr[R_PC]+pcOffset);
            update_flags(vmState,tr);
        }
        break;
        case OP_LDI:
        {    
            uint16_t tr = (instr>>9) & 0x7;
            uint16_t pcOffset = sign_extend(instr & 0x1ff,9);
            vmState->regstr[tr]=mem_read(vmState,mem_read(vmState,vmState->regstr[R_PC]+pcOffset));
            update_flags(vmState,tr);
        }
        break;
        case OP_LDR:
        {
            uint16_t dr = (instr>>9) & 0b111;
            uint16_t sr1 = (instr>>6) & 0b111;
            uint16_t offset = sign_extend(instr & 0x3f,6);
            vmState->regstr[dr] = mem_read(vmState,vmState->regstr[sr1]+offset);
            update_flags(vmState,dr);
            break;
        }
        case OP_LEA:
        {
            uint16_t tr = (instr>>9) & 0b111;
            uint16_t pcOffset = sign_extend(instr & 0x1ff,9);
            vmState->regstr[tr] = vmState->regstr[R_PC] + pcOffset;
            update_flags(vmState,tr);
        }
        break;
        case OP_ST:
        {
            uint16_t tr = (instr >> 9) & 0b111;
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
            mem_write(vmState,vmState->regstr[R_PC] + pc_offset, vmState->regstr[tr]);
        }
        break;
        case OP_STI:
        {
            uint16_t tr = (instr >> 9) & 0b111;
            uint16_t pcOffset = sign_extend(instr & 0x1FF, 9);
            mem_write(vmState,mem_read(vmState,vmState->regstr[R_PC] + pcOffset), vmState->regstr[tr]);
        }
        break;
        case OP_STR:
        {
            uint16_t tr = (instr >> 9) & 0x7;
            uint16_t tr1 = (instr >> 6) & 0x7;
            uint16_t offset = sign_extend(instr & 0x3F, 6);
            mem_write(vmState,vmState->regstr[tr1] + offset, vmState->regstr[tr]);
        }
        break;
        case OP_TRAP:
        {    
            vmState->regstr[R_R7]=vmState->regstr[R_PC];
            switch (instr & 0xFF)
            {
                case TRAP_GETC:
                {
                    vmState->regstr[R_R0] = (uint16_t)getchar();
                    update_flags(vmState,R_R0);
                }
                    break;
                case TRAP_OUT:
                {    putc((char)vmState->regstr[R_R0], stdout);
                    fflush(stdout);
                }
                    break;
                case TRAP_PUTS:
                    {
                        
                        uint16_t* c = vmState->memory + vmState->regstr[R_R0];
                        while (*c)
                        {
                            putc((char)*c, stdout);
                            ++c;
                        }
                        fflush(stdout);
                    }
                    break;
                case TRAP_IN:
                    {
                        printf("Enter a character: ");
                        char c = getchar();
                        putc(c, stdout);
                        fflush(stdout);
                        vmState->regstr[R_R0] = (uint16_t)c;
                        update_flags(vmState,R_R0);
                    }
                    break;
                case TRAP_PUTSP:
                    {
                        
                        uint16_t* c = vmState->memory + vmState->regstr[R_R0];
                        while (*c)
                        {
                            char char1 = (*c) & 0xFF;
                            putc(char1, stdout);
                            char char2 = (*c) >> 8;
                            if (char2) putc(char2, stdout);
                            ++c;
                        }
                        fflush(stdout);
                    }
                    break;
                case TRAP_HALT:
                {
                    puts("HALT");
                    fflush(stdout);
                    isRunning = 0;
                }
                    break;
            }
        }
        break;
        case OP_RES:
        case OP_RTI:
        default:
            abort();
            break;
        }
    }
    restoreInputBuffering();
    stopVm(vmState);
    return 0;
}
// /*
//     source template
// */
// #include <stdio.h>
// #include <stdint.h>
// #include <signal.h>
// /* unix only */
// #include <stdlib.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <sys/time.h>
// #include <sys/types.h>
// #include <sys/termios.h>
// #include <sys/mman.h>

// enum
// {
//     R_R0 = 0,
//     R_R1,
//     R_R2,
//     R_R3,
//     R_R4,
//     R_R5,
//     R_R6,
//     R_R7,
//     R_PC, /* program counter */
//     R_COND,
//     R_COUNT
// };
// enum
// {
//     FL_POS = 1 << 0, /* P */
//     FL_ZRO = 1 << 1, /* Z */
//     FL_NEG = 1 << 2, /* N */
// };
// enum
// {
//     OP_BR = 0, /* branch */
//     OP_ADD,    /* add  */
//     OP_LD,     /* load */
//     OP_ST,     /* store */
//     OP_JSR,    /* jump register */
//     OP_AND,    /* bitwise and */
//     OP_LDR,    /* load register */
//     OP_STR,    /* store register */
//     OP_RTI,    /* unused */
//     OP_NOT,    /* bitwise not */
//     OP_LDI,    /* load indirect */
//     OP_STI,    /* store indirect */
//     OP_JMP,    /* jump */
//     OP_RES,    /* reserved (unused) */
//     OP_LEA,    /* load effective address */
//     OP_TRAP    /* execute trap */
// };
// enum
// {
//     MR_KBSR = 0xFE00, /* keyboard status */
//     MR_KBDR = 0xFE02  /* keyboard data */
// };
// enum
// {
//     TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
//     TRAP_OUT = 0x21,   /* output a character */
//     TRAP_PUTS = 0x22,  /* output a word string */
//     TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
//     TRAP_PUTSP = 0x24, /* output a byte string */
//     TRAP_HALT = 0x25   /* halt the program */
// };

// #define MEMORY_MAX (1 << 16)
// uint16_t memory[MEMORY_MAX];  /* 65536 locations */
// uint16_t reg[R_COUNT];

// struct termios original_tio;

// void disable_input_buffering()
// {
//     tcgetattr(STDIN_FILENO, &original_tio);
//     struct termios new_tio = original_tio;
//     new_tio.c_lflag &= ~ICANON & ~ECHO;
//     tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
// }

// void restore_input_buffering()
// {
//     tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
// }

// uint16_t check_key()
// {
//     fd_set readfds;
//     FD_ZERO(&readfds);
//     FD_SET(STDIN_FILENO, &readfds);

//     struct timeval timeout;
//     timeout.tv_sec = 0;
//     timeout.tv_usec = 0;
//     return select(1, &readfds, NULL, NULL, &timeout) != 0;
// }
// void handle_interrupt(int signal)
// {
//     restore_input_buffering();
//     printf("\n");
//     exit(-2);
// }
// uint16_t sign_extend(uint16_t x, int bit_count)
// {
//     if ((x >> (bit_count - 1)) & 1) {
//         x |= (0xFFFF << bit_count);
//     }
//     return x;
// }
// uint16_t swap16(uint16_t x)
// {
//     return (x << 8) | (x >> 8);
// }
// void update_flags(uint16_t r)
// {
//     if (reg[r] == 0)
//     {
//         reg[R_COND] = FL_ZRO;
//     }
//     else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
//     {
//         reg[R_COND] = FL_NEG;
//     }
//     else
//     {
//         reg[R_COND] = FL_POS;
//     }
// }
// void read_image_file(FILE* file)
// {
//     /* the origin tells us where in memory to place the image */
//     uint16_t origin;
//     fread(&origin, sizeof(origin), 1, file);
//     origin = swap16(origin);

//     /* we know the maximum file size so we only need one fread */
//     uint16_t max_read = MEMORY_MAX - origin;
//     uint16_t* p = memory + origin;
//     size_t read = fread(p, sizeof(uint16_t), max_read, file);

//     /* swap to little endian */
//     while (read-- > 0)
//     {
//         *p = swap16(*p);
//         ++p;
//     }
// }
// int read_image(const char* image_path)
// {
//     FILE* file = fopen(image_path, "rb");
//     if (!file) { return 0; };
//     read_image_file(file);
//     fclose(file);
//     return 1;
// }
// void mem_write(uint16_t address, uint16_t val)
// {
//     memory[address] = val;
// }

// uint16_t mem_read(uint16_t address)
// {
//     if (address == MR_KBSR)
//     {
//         if (check_key())
//         {
//             memory[MR_KBSR] = (1 << 15);
//             memory[MR_KBDR] = getchar();
//         }
//         else
//         {
//             memory[MR_KBSR] = 0;
//         }
//     }
//     return memory[address];
// }


// int main(int argc, const char* argv[])
// {
//     if (argc < 2)
//     {
//         /* show usage string */
//         printf("lc3 [image-file1] ...\n");
//         exit(2);
//     }
    
//     for (int j = 1; j < argc; ++j)
//     {
//         if (!read_image(argv[j]))
//         {
//             printf("failed to load image: %s\n", argv[j]);
//             exit(1);
//         }
//     }
//     signal(SIGINT, handle_interrupt);
//     disable_input_buffering();

//     /* since exactly one condition flag should be set at any given time, set the Z flag */
//     reg[R_COND] = FL_ZRO;

//     /* set the PC to starting position */
//     /* 0x3000 is the default */
//     enum { PC_START = 0x3000 };
//     reg[R_PC] = PC_START;

//     int running = 1;
//     while (running)
//     {
//         /* FETCH */
//         uint16_t instr = mem_read(reg[R_PC]++);
//         uint16_t op = instr >> 12;

//         switch (op)
//         {
//             case OP_ADD:
//                 {
//                     /* destination register (DR) */
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     /* first operand (SR1) */
//                     uint16_t r1 = (instr >> 6) & 0x7;
//                     /* whether we are in immediate mode */
//                     uint16_t imm_flag = (instr >> 5) & 0x1;
                
//                     if (imm_flag)
//                     {
//                         uint16_t imm5 = sign_extend(instr & 0x1F, 5);
//                         reg[r0] = reg[r1] + imm5;
//                     }
//                     else
//                     {
//                         uint16_t r2 = instr & 0x7;
//                         reg[r0] = reg[r1] + reg[r2];
//                     }
                
//                     update_flags(r0);
//                 }
//                 break;
//             case OP_AND:
//                 {
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     uint16_t r1 = (instr >> 6) & 0x7;
//                     uint16_t imm_flag = (instr >> 5) & 0x1;
                
//                     if (imm_flag)
//                     {
//                         uint16_t imm5 = sign_extend(instr & 0x1F, 5);
//                         reg[r0] = reg[r1] & imm5;
//                     }
//                     else
//                     {
//                         uint16_t r2 = instr & 0x7;
//                         reg[r0] = reg[r1] & reg[r2];
//                     }
//                     update_flags(r0);
//                 }
//                 break;
//             case OP_NOT:
//                 {
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     uint16_t r1 = (instr >> 6) & 0x7;
                
//                     reg[r0] = ~reg[r1];
//                     update_flags(r0);
//                 }
//                 break;
//             case OP_BR:
//                 {
//                     uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
//                     uint16_t cond_flag = (instr >> 9) & 0x7;
//                     if (cond_flag & reg[R_COND])
//                     {
//                         reg[R_PC] += pc_offset;
//                     }
//                 }
//                 break;
//             case OP_JMP:
//                 {
//                     /* Also handles RET */
//                     uint16_t r1 = (instr >> 6) & 0x7;
//                     reg[R_PC] = reg[r1];
//                 }
//                 break;
//             case OP_JSR:
//                 {
//                     uint16_t long_flag = (instr >> 11) & 1;
//                     reg[R_R7] = reg[R_PC];
//                     if (long_flag)
//                     {
//                         uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
//                         reg[R_PC] += long_pc_offset;  /* JSR */
//                     }
//                     else
//                     {
//                         uint16_t r1 = (instr >> 6) & 0x7;
//                         reg[R_PC] = reg[r1]; /* JSRR */
//                     }
//                 }
//                 break;
//             case OP_LD:
//                 {
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
//                     reg[r0] = mem_read(reg[R_PC] + pc_offset);
//                     update_flags(r0);
//                 }
//                 break;
//             case OP_LDI:
//                 {
//                     /* destination register (DR) */
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     /* PCoffset 9*/
//                     uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
//                     /* add pc_offset to the current PC, look at that memory location to get the final address */
//                     reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
//                     update_flags(r0);
//                 }
//                 break;
//             case OP_LDR:
//                 {
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     uint16_t r1 = (instr >> 6) & 0x7;
//                     uint16_t offset = sign_extend(instr & 0x3F, 6);
//                     reg[r0] = mem_read(reg[r1] + offset);
//                     update_flags(r0);
//                 }
//                 break;
//             case OP_LEA:
//                 {
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
//                     reg[r0] = reg[R_PC] + pc_offset;
//                     update_flags(r0);
//                 }
//                 break;
//             case OP_ST:
//                 {
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
//                     mem_write(reg[R_PC] + pc_offset, reg[r0]);
//                 }
//                 break;
//             case OP_STI:
//                 {
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
//                     mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
//                 }
//                 break;
//             case OP_STR:
//                 {
//                     uint16_t r0 = (instr >> 9) & 0x7;
//                     uint16_t r1 = (instr >> 6) & 0x7;
//                     uint16_t offset = sign_extend(instr & 0x3F, 6);
//                     mem_write(reg[r1] + offset, reg[r0]);
//                 }
//                 break;
//             case OP_TRAP:
//                 reg[R_R7] = reg[R_PC];
                
//                 switch (instr & 0xFF)
//                 {
//                     case TRAP_GETC:
//                         /* read a single ASCII char */
//                         reg[R_R0] = (uint16_t)getchar();
//                         update_flags(R_R0);
//                         break;
//                     case TRAP_OUT:
//                         putc((char)reg[R_R0], stdout);
//                         fflush(stdout);
//                         break;
//                     case TRAP_PUTS:
//                         {
//                             /* one char per word */
//                             uint16_t* c = memory + reg[R_R0];
//                             while (*c)
//                             {
//                                 putc((char)*c, stdout);
//                                 ++c;
//                             }
//                             fflush(stdout);
//                         }
//                         break;
//                     case TRAP_IN:
//                         {
//                             printf("Enter a character: ");
//                             char c = getchar();
//                             putc(c, stdout);
//                             fflush(stdout);
//                             reg[R_R0] = (uint16_t)c;
//                             update_flags(R_R0);
//                         }
//                         break;
//                     case TRAP_PUTSP:
//                         {
//                             /* one char per byte (two bytes per word)
//                                here we need to swap back to
//                                big endian format */
//                             uint16_t* c = memory + reg[R_R0];
//                             while (*c)
//                             {
//                                 char char1 = (*c) & 0xFF;
//                                 putc(char1, stdout);
//                                 char char2 = (*c) >> 8;
//                                 if (char2) putc(char2, stdout);
//                                 ++c;
//                             }
//                             fflush(stdout);
//                         }
//                         break;
//                     case TRAP_HALT:
//                         puts("HALT");
//                         fflush(stdout);
//                         running = 0;
//                         break;
//                 }
//                 break;
//             case OP_RES:
//             case OP_RTI:
//             default:
//                 abort();
//                 break;
//         }
//     }
//     restore_input_buffering();
// }
