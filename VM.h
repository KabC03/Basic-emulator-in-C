#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>



typedef struct VM VM;



bool initialise_VM(int numberOfRegisters, int sizeOfRam, int stackSize);
bool get_tokens_VM(char *IRfileName);
bool run_VM(void);











 
