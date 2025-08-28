#include <stdio.h> 
#include <string.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <stdint.h> 
#include <assert.h>
#include <unistd.h>

void processInstructions(FILE* fp);

#define INIT_CAPACITY 32248
#define SMALL_CAPACITY 22
#define max(a, b) ((a) > (b) ? (a) : (b))

/*prebuilt masks for quick bitmasking*/
#define OPCODE(instr)        ((instr) >> 28)
#define REG_A(instr)         (((instr) & 0x000001C0u) >> 6)   /* for ABC ops */
#define REG_B(instr)         (((instr) & 0x00000038u) >> 3)
#define REG_C(instr)         ((instr)  & 0x00000007u)
#define LV_REG_A(instr)      (((instr) & 0x0E000000u) >> 25)  /* for LOADVAL */
#define LV_VALUE(instr)      ((instr)  & 0x01FFFFFFu)

typedef enum {
    CMOV       = 0,
    SEGLOAD    = 1,
    SEGSTORE   = 2,
    ADD        = 3,
    MUL        = 4,
    DIV        = 5,
    NAND       = 6,
    HALT       = 7,
    MAPSEG     = 8,
    UNMAPSEG   = 9,
    OUT        = 10,
    IN         = 11,
    LOADPROG   = 12,
    LOADVAL    = 13
} Instruction;

int main(int argc, char *argv[]) { 
    setvbuf(stdout, NULL, _IONBF, 0);  // disable buffering on stdout
    setvbuf(stdin, NULL, _IONBF, 0);  

    if(argc != 2) { 
        fprintf(stderr, "Error: expected argument count to be 1");
        return EXIT_FAILURE;
    }

    char *fileName = argv[1]; 
    FILE *fp = fopen(fileName, "rb"); 
    assert(fp != NULL); 

    processInstructions(fp);
    fclose(fp); 
    return EXIT_SUCCESS;
}

void processInstructions(FILE *fp) { 
    /*step 1: read memory into an array */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp); 

    uint32_t programLen = (uint32_t)size >> 2; // size / 4
    // malloc EXACTLY enough space ot hold the full contents of the program
    register uint32_t *restrict program = (uint32_t*)malloc(programLen << 2);

    for(uint32_t i = 0; i < programLen; i++) { 
        uint8_t byte1 = getc(fp); 
        uint8_t byte2 = getc(fp); 
        uint8_t byte3 = getc(fp); 
        uint8_t byte4 = getc(fp);

        uint32_t word = ((uint32_t)byte1 << 24) | ((uint32_t)byte2 << 16) | ((uint32_t)byte3 << 8) | ((uint32_t)byte4); 
        program[i] = word;
    }

    // container for unusedIDs
    uint32_t *restrict unusedIDs = (uint32_t*)malloc(INIT_CAPACITY << 2);
    assert(unusedIDs); 
    uint32_t unusedIDCnt = 0;
    uint32_t unusedIDCap = INIT_CAPACITY;
    // container for memory, each element is 1 segment: SoA format
    uint32_t **restrict m = (uint32_t**)malloc(INIT_CAPACITY << 3); // m[i] = data of ith segment
    uint32_t *restrict len = (uint32_t*)malloc(INIT_CAPACITY << 2); // len[i] = # length of actively mapped ith segment
    uint32_t *restrict capacity = (uint32_t*)malloc(INIT_CAPACITY << 2); // capacity[i] = # of words underlying buffer at segment i can hold
    assert(m && len && capacity);
    uint32_t memSize = 1; 
    uint32_t memCap = INIT_CAPACITY;

    // small init inner segments: 
    for(int i = INIT_CAPACITY-1; i >= 0; i--) { 
        m[i] = (uint32_t*)malloc(SMALL_CAPACITY << 2);
        // assert(posix_memalign((void **)&m[i], ALIGNMENT, SMALL_CAPACITY << 2) == 0);
        capacity[i] = SMALL_CAPACITY;
    }

    register uint32_t programCounter = 0; 
    m[0] = program; 
    len[0] = programLen;
    capacity[0] = max(capacity[0], programLen);

    uint32_t r[8];
    memset(r, 0, sizeof(r));
    bool halt = false; 

    while(!halt && programCounter < programLen) { 
        uint32_t instruction = program[programCounter++];
        Instruction opcode = (Instruction)OPCODE(instruction);

        if (opcode == LOADVAL) {
            r[LV_REG_A(instruction)] = LV_VALUE(instruction);
            continue; 
        } else if(opcode == SEGLOAD) {
            r[REG_A(instruction)] = m[r[REG_B(instruction)]][r[REG_C(instruction)]];
            continue;
        } else if(opcode == SEGSTORE) { 
            m[r[REG_A(instruction)]][r[REG_B(instruction)]] = r[REG_C(instruction)];
            continue;
        }
        
        switch(opcode) { 
            case CMOV: 
                if(r[REG_C(instruction)] != 0) r[REG_A(instruction)] = r[REG_B(instruction)]; 
                break;
            case ADD: 
                r[REG_A(instruction)] = r[REG_B(instruction)] + r[REG_C(instruction)]; 
                break;
            case MUL: 
                r[REG_A(instruction)] = r[REG_B(instruction)] * r[REG_C(instruction)];
                break;
            case DIV: 
                r[REG_A(instruction)] = r[REG_B(instruction)] / r[REG_C(instruction)]; 
                break;
            case NAND: 
                r[REG_A(instruction)] = ~(r[REG_B(instruction)] & r[REG_C(instruction)]); 
                break;
            case HALT:
                halt = true;
                break;
            case MAPSEG: {
                uint32_t segID;
                if(unusedIDCnt > 0) { 
                    // if we have unmapped segments, reuse those segments
                    segID = unusedIDs[--unusedIDCnt]; // pop the id 
                } else { // unlikely
                    segID = memSize++; // else we must expand m to fit a new container
                    if(memSize == memCap) { // unlikely
                        memCap <<= 1;
                        m = (uint32_t**)realloc(m, memCap * sizeof *m);
                        len = (uint32_t*)realloc(len, memCap * sizeof *len); 
                        capacity = (uint32_t*)realloc(capacity, memCap * sizeof *capacity); 
                        assert(m && len && capacity);  
                    }

                    if(segID >= INIT_CAPACITY) { 
                        m[segID] = NULL; 
                        capacity[segID] = 0;
                    }
                }

                uint32_t newLen = r[REG_C(instruction)];

                if(newLen > capacity[segID]) { // newLen > old, we must now update len -- hopefully unlikely
                    uint32_t newCap = newLen << 1;
                    m[segID] = (uint32_t*)realloc(m[segID], newCap << 2); 
                    capacity[segID] = newCap; 
                }

                assert(m[segID] || newLen == 0);
                memset(m[segID], 0, newLen << 2);

                r[REG_B(instruction)] = segID;
                len[segID] = newLen;
                break;
            }
            case UNMAPSEG: 
                unusedIDs[unusedIDCnt++] = r[REG_C(instruction)];
                if(unusedIDCnt == unusedIDCap) { 
                    unusedIDCap <<= 1;
                    unusedIDs = (uint32_t*)realloc(unusedIDs, unusedIDCap * sizeof *unusedIDs);
                    assert(unusedIDs);
                }
                break;
            case OUT: {
                uint32_t outputValue = r[REG_C(instruction)]; 
                if(outputValue > 255) return; 
                unsigned char ch = (unsigned char)outputValue; 
                write(1, &ch, 1);
                break;
            }
            case IN: {
                unsigned char byte; 
                ssize_t n = read(0, &byte, 1); 
                if(n <= 0) { 
                    r[REG_C(instruction)] = ~0;
                } else { 
                    r[REG_C(instruction)] = (uint32_t)byte;
                }
                break;
            }
            case LOADPROG: {
                uint32_t dupSegID = r[REG_B(instruction)];
                programCounter = r[REG_C(instruction)];
                if(dupSegID == 0) {
                    break; 
                }
                uint32_t dupLen = len[dupSegID];
                if(dupLen > capacity[0]) { // create more buffer space if necessary
                    uint32_t newCap = dupLen << 1;
                    program = (uint32_t*)realloc(program, newCap * sizeof *program);
                    assert(program);
                    capacity[0] = newCap;
                }
                memcpy(program, m[dupSegID], dupLen << 2); // duplicate the dupSeg to seg0

                // update len and counter
                programLen = dupLen;
                len[0] = programLen; 
                m[0] = program; // update
                break;
            }
            default: 
                break;
        }
    }

    for(uint32_t i = 0; i < max(memSize, INIT_CAPACITY); i++) { 
        if(m[i]) { 
            free(m[i]);
        }
    }
    if(m) free(m); // free outter container
    if(unusedIDs) free(unusedIDs); // free unusedIDs
    if(len) free(len); 
    if(capacity) free(capacity); 
}

/*
todo preallocate each segment to the exact max size that it will need throughout
the totallity of sandmark and midmark
*/