#include "VM.h"
#define array_len(array) sizeof(array)/sizeof(array[0])
#define current_instruction ((VirtualMachine.instructionMemory)[i])
#define destination_register (VirtualMachine.registers[current_instruction.reg0])
#define source_register1 (VirtualMachine.registers[current_instruction.reg1])
#define source_register2 (VirtualMachine.registers[current_instruction.ARG3.reg2])


#define RAM_TYPE uint8_t
#define INSTRUCTION_MEMORY_EXPANSION 1
#define LABEL_EXPANSION 1
#define INSTRUCTION_INPUT_BUFFER 100



typedef enum OPCODE {

    ADD_I,
    SUB_I,
    MUL_I,
    DIV_I,

    ADD_F,
    SUB_F,
    MUL_F,
    DIV_F,


    LOD, //A because its easier to parse
    STR, //A because its easier to parse


    BEQ_I,
    BLT_I,
    BLE_I,

    BEQ_F,
    BLT_F,
    BLE_F,



    JUM,

    JAL,
    JRT,

    ALLOC, //A type function (basiJRTly malloc)
    FREE,  //A type function (basiJRTly free)
    PRINT, //A type function (print ONE value) - PRINT R0 Rx INT (Print INT in R0 if Rx == 0)
    INPUT, //A type function (input ONE value)

    LABEL, //NOT an instruction - used to declare labels

} OPCODE;
typedef enum INSTRUCTION_TYPE {

    R, //Register
    I, //Immediate
    C, //Compare
    J, //Jump
    A, //Abstract
    L, //Label

} INSTRUCTION_TYPE;
typedef enum OPCODE_DATATYPE {

    INTEGER,
    FLOAT,
    NONE,

} OPCODE_DATATYPE;
typedef enum ABSTRACT_DATATYPE {

    INTEGER_TYPE,
    FLOAT_TYPE,

} ABSTRACT_DATATYPE;

typedef struct Instruction {
    
    INSTRUCTION_TYPE instructionType;


    OPCODE opcode;
    OPCODE_DATATYPE opcodeDatatype;

    int reg0;
    int reg1;

    union ARG3 {
        char *label;
        float floatImmediate;
        int intImmediate;
        int reg2;
        ABSTRACT_DATATYPE abstractDatatype; //allocate float, or int
    } ARG3;

} Instruction;
typedef union Registers {

    int intValue;
    float floatValue;

} Registers;
typedef union IOdata {

    int intVal;
    float floatVal;
    char charVal;

} IOdata;
typedef union RAMElement{

    RAM_TYPE RAM;
    bool isUsed; //If the current byte is in use (for memory allocation)

} RAMElement;




struct VM {

    int programCounter;             //Program counter index into instructionMemory

    Registers *registers;              //Pointer to register array (NOTE - SP is considered to be to R0)
    int registerSize;

    Instruction *instructionMemory;    //Array of instructions
    size_t numInstructions;            //Size of instructionMemory
    RAMElement *RAM;                   //Array of RAM elements
    int RAMSize;
    int stackSize;

};




VM VirtualMachine; //Only ONE should exist
//Dictionary for labels
char **labelKey = NULL;
size_t *labelValue = NULL;
size_t labelDictionarySize = 0;
bool initialise_VM(int numberOfRegisters, int sizeOfRam, int stackSize) {

    if(numberOfRegisters == 0 || sizeOfRam == 0) {
        printf("Number of registers or size of RAM cannot be zero\n");
        return false;
    }
    numberOfRegisters += 2; //Account for ZERO register (R0) and SP (R1)
    //malloc(numberOfRegisters * sizeof(Registers))
    VirtualMachine.registers = calloc(numberOfRegisters, sizeof(Registers));
    VirtualMachine.RAM = malloc(sizeOfRam * sizeof(RAMElement));
    VirtualMachine.instructionMemory = NULL;
    VirtualMachine.programCounter = 0;
    VirtualMachine.numInstructions = 0;
    VirtualMachine.registerSize = numberOfRegisters;
    VirtualMachine.RAMSize = sizeOfRam;
    VirtualMachine.stackSize = stackSize;

    if(VirtualMachine.registers == NULL || VirtualMachine.RAM == NULL) {
        printf("Unable to allocate space for registers or RAM\n");
        return false;
    }

    (VirtualMachine.registers[1]).intValue = sizeOfRam - 1; //Set SP to end of heap
    return true;
}


bool destroy_VM(void) {
    //Free memory associated with VM
    if(VirtualMachine.registers != NULL) {
        free(VirtualMachine.registers);
        VirtualMachine.registers = NULL;
    }
    if(VirtualMachine.RAM != NULL) {
        free(VirtualMachine.RAM);
        VirtualMachine.RAM = NULL;
    }
    if(VirtualMachine.instructionMemory != NULL) {

        for(int i = 0; current_instruction.opcode != VirtualMachine.numInstructions; i++) {
            if(current_instruction.instructionType == I || current_instruction.instructionType == J) {
                if(current_instruction.ARG3.label != NULL) {
                    free(current_instruction.ARG3.label);
                    current_instruction.ARG3.label = NULL;
                }
            }
        }
    }
    return true;
}






















void print_instructions(void) {

    printf("\n\n Instruction     |||     OPCODE     R0     R1     R2\n");
    printf("==========================================================\n");
    for(int i = 0; i < VirtualMachine.numInstructions; i++) {
        printf("%5d            |||     %5d  %5d  %5d  %5d\n",i,current_instruction.opcode, current_instruction.reg0,current_instruction.reg1,current_instruction.ARG3.reg2);

    }

}
void print_dictionary(void) {

    printf("\n\n Label        |||        Address\n");
    printf("=================================\n");
    for(int i = 0; i < labelDictionarySize; i++) {
        printf("%10s    ||| %10zu\n",labelKey[i],labelValue[i]);
    }

    return;
}
bool is_integer(char *str) {

    for(int i = 0; str[i] != '\0'; i++) {
        if(isdigit(str[i]) == 0) {
            return false;
        }
    }
    return true;
}
bool is_float_or_integer(char *str) {

    bool foundDecimal = false;
    bool foundNegative = false;
    for(int i = 0; str[i] != '\0'; i++) {


        if(str[i] == '-' && foundNegative == false) {
            foundNegative = true;
        } else if(str[i] =='.' && foundDecimal == false) {
            foundDecimal = true;
        } else if(isdigit(str[i]) == 0) {
            return false;
        }
    }
    return true;
}


bool get_tokens_VM(char *IRfileName) {

    if(IRfileName == NULL) {
        printf("Filename unexpectedly NULL\n");
        return false;
    }

    FILE *IRfile = fopen(IRfileName, "r");
    if(IRfile == NULL) {
        printf("Unable to open '%s'\n",IRfileName);
        return false;
    }

    char instructionInputBuffer[INSTRUCTION_INPUT_BUFFER];
    char copyInstructionInputBuffer[INSTRUCTION_INPUT_BUFFER]; //Used for displaying errors since strtok is destructuve

    VirtualMachine.numInstructions = 0;
    char *currentToken = NULL;

    int iMax = 0;
    for(int i = 0; fgets(instructionInputBuffer, array_len(instructionInputBuffer), IRfile); i++) {


        strcpy(copyInstructionInputBuffer, instructionInputBuffer);
        if(i == VirtualMachine.numInstructions) { //Reallocate memory
            VirtualMachine.instructionMemory = realloc(VirtualMachine.instructionMemory, (VirtualMachine.numInstructions + INSTRUCTION_MEMORY_EXPANSION + 1) * sizeof(Instruction)); //NEED TO HANDLE IF REALLOC FAILS (DO LATER)
            if(VirtualMachine.instructionMemory == NULL) {
                printf("Memory allocation failure when expanding instruction memory\n");
                return false;
            }
            VirtualMachine.numInstructions += INSTRUCTION_MEMORY_EXPANSION;
        }

        //Get instruction type
        currentToken = strtok(instructionInputBuffer, " \n");
        if(currentToken == NULL) { //Empty line
            i--;
            continue;
        }
        



        if(strcmp(currentToken, "[R]") == 0) {
            current_instruction.instructionType = R; //Register


            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected opcode: '%s'\n",copyInstructionInputBuffer);
                return false;
            }
            if(strcmp(currentToken, "ADD_I") == 0) {
                current_instruction.opcode = ADD_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "ADD_F") == 0) {
                current_instruction.opcode = ADD_F;
                current_instruction.opcodeDatatype = FLOAT;
            } else if(strcmp(currentToken, "SUB_I") == 0) {
                current_instruction.opcode = SUB_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "SUB_F") == 0) {
                current_instruction.opcode = SUB_F;
                current_instruction.opcodeDatatype = FLOAT;
            } else if(strcmp(currentToken, "MUL_I") == 0) {
                current_instruction.opcode = MUL_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "MUL_F") == 0) {
                current_instruction.opcode = MUL_F;
                current_instruction.opcodeDatatype = FLOAT;
            } else if(strcmp(currentToken, "DIV_I") == 0) {
                current_instruction.opcode = DIV_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "DIV_F") == 0) {
                current_instruction.opcode = DIV_F;
                current_instruction.opcodeDatatype = FLOAT;
            } else {
                printf("Unrecognised R type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }

            //Register arguments

            //Destination register
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected destination register in R type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register destination argument: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            if(atoi(currentToken + 1) > 0 && atoi(currentToken + 1) < VirtualMachine.registerSize) {
                current_instruction.reg0 = atoi(currentToken + 1); //Skip the first "R"
            } else {
                printf("OOB register access in R type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            //Source register 1
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected source register in R type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register source argument: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            current_instruction.reg1 = atoi(currentToken + 1); //Skip the first "R"



            //Source register 2
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected source register in R type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register source argument: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            current_instruction.ARG3.reg2 = atoi(currentToken + 1); //Skip the first "R"




            if(strtok(NULL, " \n") != NULL) {
                printf("Too many operands passed to R type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }

        } else if(strcmp(currentToken, "[I]") == 0) {
            current_instruction.instructionType = I; //Immediate

            bool isFloat = false;
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected opcode: '%s'\n",copyInstructionInputBuffer);
                return false;
            }
            if(strcmp(currentToken, "ADD_I") == 0) {
                current_instruction.opcode = ADD_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "ADD_F") == 0) {
                current_instruction.opcode = ADD_F;
                current_instruction.opcodeDatatype = FLOAT;
                isFloat = true;


            } else if(strcmp(currentToken, "SUB_I") == 0) {
                current_instruction.opcode = SUB_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "SUB_F") == 0) {
                current_instruction.opcode = SUB_F;
                current_instruction.opcodeDatatype = FLOAT;
                isFloat = true;


            } else if(strcmp(currentToken, "MUL_I") == 0) {
                current_instruction.opcode = MUL_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "MUL_F") == 0) {
                current_instruction.opcode = MUL_F;
                current_instruction.opcodeDatatype = FLOAT;
                isFloat = true;


            } else if(strcmp(currentToken, "DIV_I") == 0) {
                current_instruction.opcode = DIV_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "DIV_F") == 0) {
                current_instruction.opcode = DIV_F;
                current_instruction.opcodeDatatype = FLOAT;
                isFloat = true;

            } else {
                printf("Unrecognised I type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }

            //Register arguments

            //Destination register
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected destination register in I type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register destination argument: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            if(atoi(currentToken + 1) > 0 && atoi(currentToken + 1) < VirtualMachine.registerSize) {
                current_instruction.reg0 = atoi(currentToken + 1); //Skip the first "R"
            } else {
                printf("OOB register access in I type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            //Source register 1
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected source register in I type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register source argument: '%s'\n",copyInstructionInputBuffer);
                return false;
            }



            current_instruction.reg1 = atoi(currentToken + 1); //Skip the first "R"




            //Immediate
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected immediate in I type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(is_float_or_integer(currentToken) == false) {
                printf("Incorrectly formatted immediate argument: '%s'\n",copyInstructionInputBuffer);
                return false;
            }

            if(isFloat == false) {

                current_instruction.ARG3.intImmediate = atoi(currentToken);



                
            } else {

                current_instruction.ARG3.floatImmediate = atof(currentToken);   



            }


            if(strtok(NULL, " \n") != NULL) {
                printf("Too many operands passed to I type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


        } else if(strcmp(currentToken, "[C]") == 0) {
            current_instruction.instructionType = C; //Compare


            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected opcode: '%s'\n",copyInstructionInputBuffer);
                return false;
            }
            if(strcmp(currentToken, "BEQ_I") == 0) {
                current_instruction.opcode = BEQ_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "BLT_I") == 0) {
                current_instruction.opcode = BLT_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "BLE_I") == 0) {
                current_instruction.opcode = BLE_I;
                current_instruction.opcodeDatatype = INTEGER;
            } else if(strcmp(currentToken, "BEQ_F") == 0) {
                current_instruction.opcode = BEQ_F;
                current_instruction.opcodeDatatype = FLOAT;
            } else if(strcmp(currentToken, "BLT_F") == 0) {
                current_instruction.opcode = BLT_F;
                current_instruction.opcodeDatatype = FLOAT;
            } else if(strcmp(currentToken, "BLE_F") == 0) {
                current_instruction.opcode = BLE_F;
                current_instruction.opcodeDatatype = FLOAT;
            } else {
                printf("Unrecognised C type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }

            //Register arguments

            //Destination register
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected destination register in C type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register destination argument: '%s'\n",copyInstructionInputBuffer);
                return false;
            }

            if(atoi(currentToken + 1) > 0 && atoi(currentToken + 1) < VirtualMachine.registerSize) {
                current_instruction.reg0 = atoi(currentToken + 1); //Skip the first "R"
            } else {
                printf("OOB register access in C type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            //Source register 1
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected source register in C type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register source argument: '%s'\n",copyInstructionInputBuffer);    
                return false;
            }


            current_instruction.reg1 = atoi(currentToken + 1); //Skip the first "R"






            //Label
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected label in C type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            current_instruction.ARG3.label = malloc((strlen(currentToken) + 1) * sizeof(char));
            if(current_instruction.ARG3.label == NULL) {
                printf("Failure to allocate memory for label\n");
                return false;
            }
            strcpy(current_instruction.ARG3.label, currentToken);



            if(strtok(NULL, " \n") != NULL) {
                printf("Too many operands passed to C type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }





        } else if(strcmp(currentToken, "[J]") == 0) {
            current_instruction.instructionType = J; //Jump

            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected opcode: '%s'\n",copyInstructionInputBuffer);
                return false;
            }
            if(strcmp(currentToken, "JUM") == 0) {
                current_instruction.opcode = JUM;
                current_instruction.opcodeDatatype = NONE;
            } else if(strcmp(currentToken, "JAL") == 0) {
                current_instruction.opcode = JAL;
                current_instruction.opcodeDatatype = NONE;
            } else if(strcmp(currentToken, "JRT") == 0) {
                current_instruction.opcode = JRT;
                current_instruction.opcodeDatatype = NONE;
            } else {
                printf("Unrecognised J type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected label in J type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            current_instruction.ARG3.label = malloc((strlen(currentToken) + 1) * sizeof(char));
            if(current_instruction.ARG3.label == NULL) {
                printf("Failure to allocate memory for label\n");
                return false;
            }
            strcpy(current_instruction.ARG3.label, currentToken);




            if(strtok(NULL, " \n") != NULL) {
                printf("Too many operands passed to J type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


        } else if(strcmp(currentToken, "[L]") == 0) {
            current_instruction.instructionType = L; //Label

            //Put into a dictionary



            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected opcode: '%s'\n",copyInstructionInputBuffer);
                return false;
            }
            if(strcmp(currentToken, "LABEL") == 0) {

            } else {
                printf("Unrecognised L type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }




            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected label: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


            labelKey = realloc(labelKey, ((labelDictionarySize + 1) * sizeof(char*)));
            labelValue = realloc(labelValue, ((labelDictionarySize + 1) * sizeof(size_t)));   

            if(labelKey == NULL || labelValue == NULL) {
                printf("Failed to allocate memory for label dictionary\n");
                return false;
            }
            labelKey[labelDictionarySize] = malloc((strlen(currentToken) + 1) * sizeof(char));


            if(labelKey[labelDictionarySize] == NULL) {
                printf("Failed to allocate memory for label dictionary\n");
                return false;
            }




            strcpy(labelKey[labelDictionarySize], currentToken);
            labelValue[labelDictionarySize] = i; //Assign to current insruction address

            labelDictionarySize += 1; 

            //printf("key: %s || value: %d\n",labelKey[labelDictionarySize], labelValue[labelDictionarySize]);

            if(strtok(NULL, " \n") != NULL) {
                printf("Too many operands passed to L type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }

            i--; //Do this because this instruction should not appear in memory

        } else if(strcmp(currentToken, "[A]") == 0) {
            current_instruction.instructionType = A; //Abstract




            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected opcode: '%s'\n",copyInstructionInputBuffer);
                return false;
            }
            if(strcmp(currentToken, "ALLOC") == 0) {
                current_instruction.opcode = ALLOC;
                current_instruction.opcodeDatatype = NONE;
            } else if(strcmp(currentToken, "FREE") == 0) {
                current_instruction.opcode = FREE;
                current_instruction.opcodeDatatype = NONE;
            } else if(strcmp(currentToken, "PRINT") == 0) {
                current_instruction.opcode = PRINT;
                current_instruction.opcodeDatatype = NONE;
            } else if(strcmp(currentToken, "INPUT") == 0) {
                current_instruction.opcode = INPUT;
                current_instruction.opcodeDatatype = NONE;
            } else if(strcmp(currentToken, "LOD") == 0) {
                current_instruction.opcode = LOD;
                current_instruction.opcodeDatatype = NONE;
            } else if(strcmp(currentToken, "STR") == 0) {
                current_instruction.opcode = STR;
                current_instruction.opcodeDatatype = NONE;
            } else {
                printf("Unrecognised A type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }




            //Destination register
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected destination register in A type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register destination argument: '%s'\n",copyInstructionInputBuffer);
                return false;
            }

            if(atoi(currentToken + 1) > 0 && atoi(currentToken + 1) < VirtualMachine.registerSize) {
                current_instruction.reg0 = atoi(currentToken + 1); //Skip the first "R"
            } else {
                printf("OOB register access in A type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }






            //Source register 1
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected source register in A type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }
            if(currentToken[0] != 'R' || is_integer(currentToken + 1) == false) { //Skip the fist letter (which should be 'R')
                printf("Incorrectly formatted register source argument: '%s'\n",copyInstructionInputBuffer);    
                return false;
            }

            current_instruction.reg1 = atoi(currentToken + 1); //Skip the first "R"






            //Datatype for function
            currentToken = strtok(NULL, " \n");
            if(currentToken == NULL) {
                printf("Expected datatype in A type instruciton: '%s'\n", copyInstructionInputBuffer);
                return false;
            }

            if(strcmp(currentToken, "FLOAT") == 0) {
                current_instruction.ARG3.abstractDatatype = FLOAT_TYPE;

            } else if(strcmp(currentToken, "INTEGER") == 0) {
                current_instruction.ARG3.abstractDatatype = INTEGER_TYPE;
            } else {
                printf("Invalid datatype passed to A type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }








            if(strtok(NULL, " \n") != NULL) {
                printf("Too many operands passed to A type instruction: '%s'\n",copyInstructionInputBuffer);
                return false;
            }


        } else if(currentToken[0] == '#') {
            i--;
        } else { //Unrecognised instruction type
            printf("Unrecognised instruction '%s'\n",copyInstructionInputBuffer);
            return false;
        }
        //print_instruction(current_instruction);
        iMax = i;
    }
    VirtualMachine.numInstructions = iMax + 1;
    


    //Cleanup
    fclose(IRfile);
    //print_dictionary();
    //print_instructions();
    return true;
}





bool get_value_label_dict(char *label, size_t *valueOut) {

    if(label == NULL) {
        printf("Label unexpectedly NULL\n");
        return false;
    }

    for(int i = 0; i < labelDictionarySize; i++) {

        if(strcmp(labelKey[i], label) == 0) {
            *(valueOut) = labelValue[i];
            return true;
        }

    }
    printf("Label '%s' not defined\n",label);
    return false; //Should never happen
}
bool run_VM(void) {

    if(VirtualMachine.registers == NULL || VirtualMachine.RAM == NULL || VirtualMachine.instructionMemory == NULL) {
        printf("Virtual machine not initialised\n");
        return false;
    }



    int bytesRequested = 0;
    int memAddress = 0;
    int bytesToFree = 0;
    int baseAddress = 0;
    size_t labelOut = 0;
    //size_t jumpAddress = -1;
    for(int i = 0; i < VirtualMachine.numInstructions; i++) {
        switch (current_instruction.opcode) {


        case ADD_I:


            if(current_instruction.instructionType == R) {

                VirtualMachine.registers[current_instruction.reg0].intValue = 
                VirtualMachine.registers[current_instruction.reg1].intValue +
                VirtualMachine.registers[current_instruction.ARG3.reg2].intValue;

            } else if(current_instruction.instructionType == I) {

                VirtualMachine.registers[current_instruction.reg0].intValue = 
                VirtualMachine.registers[current_instruction.reg1].intValue +
                current_instruction.ARG3.intImmediate;


            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }


            break;
        case ADD_F:



            if(current_instruction.instructionType == R) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = 
                VirtualMachine.registers[current_instruction.reg1].floatValue +
                VirtualMachine.registers[current_instruction.ARG3.reg2].floatValue;

            } else if(current_instruction.instructionType == I) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = 
                VirtualMachine.registers[current_instruction.reg1].floatValue +
                current_instruction.ARG3.floatImmediate;


            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }



            break;

        case SUB_I:


            if(current_instruction.instructionType == R) {

                VirtualMachine.registers[current_instruction.reg0].intValue = 
                VirtualMachine.registers[current_instruction.reg1].intValue -
                VirtualMachine.registers[current_instruction.ARG3.reg2].intValue;

            } else if(current_instruction.instructionType == I) {

                VirtualMachine.registers[current_instruction.reg0].intValue = 
                VirtualMachine.registers[current_instruction.reg1].intValue -
                current_instruction.ARG3.intImmediate;


            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }



            break;
        case SUB_F:

            if(current_instruction.instructionType == R) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = 
                VirtualMachine.registers[current_instruction.reg1].floatValue -
                VirtualMachine.registers[current_instruction.ARG3.reg2].floatValue;

            } else if(current_instruction.instructionType == I) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = 
                VirtualMachine.registers[current_instruction.reg1].floatValue -
                current_instruction.ARG3.floatImmediate;


            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }




            break;

        case MUL_I:


            if(current_instruction.instructionType == R) {

                VirtualMachine.registers[current_instruction.reg0].intValue = 
                VirtualMachine.registers[current_instruction.reg1].intValue *
                VirtualMachine.registers[current_instruction.ARG3.reg2].intValue;

            } else if(current_instruction.instructionType == I) {

                VirtualMachine.registers[current_instruction.reg0].intValue = 
                VirtualMachine.registers[current_instruction.reg1].intValue *
                current_instruction.ARG3.intImmediate;


            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }





            break;
        case MUL_F:


            if(current_instruction.instructionType == R) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = 
                VirtualMachine.registers[current_instruction.reg1].floatValue *
                VirtualMachine.registers[current_instruction.ARG3.reg2].floatValue;

            } else if(current_instruction.instructionType == I) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = 
                VirtualMachine.registers[current_instruction.reg1].floatValue *
                current_instruction.ARG3.floatImmediate;


            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }




            break;
            
        case DIV_I:


            if(current_instruction.instructionType == R) {

                VirtualMachine.registers[current_instruction.reg0].intValue = 
                VirtualMachine.registers[current_instruction.reg1].intValue /
                VirtualMachine.registers[current_instruction.ARG3.reg2].intValue;

            } else if(current_instruction.instructionType == I) {

                VirtualMachine.registers[current_instruction.reg0].intValue = 
                VirtualMachine.registers[current_instruction.reg1].intValue /
                current_instruction.ARG3.intImmediate;


            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }



            break;
        case DIV_F:


            if(current_instruction.instructionType == R) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = 
                VirtualMachine.registers[current_instruction.reg1].floatValue /
                VirtualMachine.registers[current_instruction.ARG3.reg2].floatValue;

            } else if(current_instruction.instructionType == I) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = 
                VirtualMachine.registers[current_instruction.reg1].floatValue /
                current_instruction.ARG3.floatImmediate;


            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }


            break;








        case LOD:

            if(current_instruction.reg1 < 0 || current_instruction.reg1 > VirtualMachine.RAMSize) {
                printf("Segmentation fault while reading from RAM\n");
                return false;
            }

            if(current_instruction.ARG3.abstractDatatype == INTEGER_TYPE) {

                VirtualMachine.registers[current_instruction.reg0].intValue = *((int*)(VirtualMachine.RAM + current_instruction.reg1)); 
                //Add byte offset, cast to int pointer then dereference

            } else if(current_instruction.ARG3.abstractDatatype == FLOAT_TYPE) {

                VirtualMachine.registers[current_instruction.reg0].floatValue = *((float*)(VirtualMachine.RAM + current_instruction.reg1)); 
                //Add byte offset, cast to int pointer then dereference

            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }

            break;
        case STR:

            if(current_instruction.reg1 < 0 || current_instruction.reg1 > VirtualMachine.RAMSize) {
                printf("Segmentation fault while storing into RAM\n");
                return false;
            }

            if(current_instruction.ARG3.abstractDatatype == INTEGER_TYPE) {

                *((int*)(VirtualMachine.RAM + current_instruction.reg1)) = VirtualMachine.registers[current_instruction.reg0].intValue; 
                //Add byte offset, cast to int pointer then dereference

            } else if(current_instruction.ARG3.abstractDatatype == FLOAT_TYPE) {

                *((float*)(VirtualMachine.RAM + current_instruction.reg1)) = VirtualMachine.registers[current_instruction.reg0].floatValue; 
                //Add byte offset, cast to int pointer then dereference

            } else {
                printf("Unexpected datatype in instruction: '%d'\n",current_instruction.opcode);
                return false;
            }

            break;



        case BEQ_I:
            

            if(get_value_label_dict(current_instruction.ARG3.label, &labelOut) == false) {
                printf("Unrecognised label\n");
                return false;
            }

            if((VirtualMachine.registers[current_instruction.reg0]).intValue == 
            (VirtualMachine.registers[current_instruction.reg1]).intValue) {
                i = labelOut - 1; //Set the PC (i) to this value
            }

            break;
        case BLT_I:

            if(get_value_label_dict(current_instruction.ARG3.label, &labelOut) == false) {
                printf("Unrecognised label\n");
                return false;
            }


            if((VirtualMachine.registers[current_instruction.reg0]).intValue < 
            (VirtualMachine.registers[current_instruction.reg1]).intValue) {
                i = labelOut - 1; //Set the PC (i) to this value
            }


            break;
        case BLE_I:


            if(get_value_label_dict(current_instruction.ARG3.label, &labelOut) == false) {
                printf("Unrecognised label\n");
                return false;
            }


            if((VirtualMachine.registers[current_instruction.reg0]).intValue <= 
            (VirtualMachine.registers[current_instruction.reg1]).intValue) {
                i = labelOut - 1; //Set the PC (i) to this value
            }

            break;

        case BEQ_F:

            if(get_value_label_dict(current_instruction.ARG3.label, &labelOut) == false) {
                printf("Unrecognised label\n");
                return false;
            }


            if((VirtualMachine.registers[current_instruction.reg0]).floatValue < 
            (VirtualMachine.registers[current_instruction.reg1]).floatValue) {
                i = labelOut - 1; //Set the PC (i) to this value
            }


            break;
        case BLT_F:

            if(get_value_label_dict(current_instruction.ARG3.label, &labelOut) == false) {
                printf("Unrecognised label\n");
                return false;
            }


            if((VirtualMachine.registers[current_instruction.reg0]).floatValue < 
            (VirtualMachine.registers[current_instruction.reg1]).floatValue) {
                i = labelOut - 1; //Set the PC (i) to this value
            }


            break;
        case BLE_F:

            if(get_value_label_dict(current_instruction.ARG3.label, &labelOut) == false) {
                printf("Unrecognised label\n");
                return false;
            }


            if((VirtualMachine.registers[current_instruction.reg0]).floatValue <= 
            (VirtualMachine.registers[current_instruction.reg1]).floatValue) {
                i = labelOut - 1; //Set the PC (i) to this value
            }


            break;
        case JUM:


            if(get_value_label_dict(current_instruction.ARG3.label, &labelOut) == false) {
                printf("Unrecognised label\n");
                return false;
            }
            i = labelOut - 1; //Set the PC (i) to this value


            break;





        case JAL:

            //Push PC onto stack then jump to label


            (VirtualMachine.registers[1]).intValue -= sizeof(int); //Move back by size of int in bytes

            if(VirtualMachine.stackSize < VirtualMachine.RAMSize - (VirtualMachine.registers[1]).intValue
            || VirtualMachine.registers[1].intValue > VirtualMachine.RAMSize - 1) {

                printf("Stack overflow\n");
                return false;
            }

            ((VirtualMachine.RAM)[VirtualMachine.registers[1].intValue]).RAM = i;
            //Add byte offset, cast to int pointer then dereference
            

            if(get_value_label_dict(current_instruction.ARG3.label, &labelOut) == false) {
                printf("Unrecognised label\n");
                return false;
            }

            i = labelOut - 1; //Set the PC (i) to this value


            break;
        case JRT:

            //Pop address from stack, set PC to it



            if(VirtualMachine.stackSize < VirtualMachine.RAMSize - (VirtualMachine.registers[1]).intValue + sizeof(int)
            || VirtualMachine.registers[1].intValue + sizeof(int) > VirtualMachine.RAMSize - 1) {
                printf("Stack underflow\n");
                return false;
            }

            i = ((VirtualMachine.RAM)[VirtualMachine.registers[1].intValue]).RAM; //R1 has sp
            (VirtualMachine.registers[1]).intValue += sizeof(int); //Move back by size of int in byte 
            //Add byte offset, cast to int pointer then dereference


            break;


        case ALLOC:


            bytesRequested = VirtualMachine.registers[current_instruction.reg1].intValue;
            if(current_instruction.ARG3.abstractDatatype == INTEGER_TYPE) {
                bytesRequested *= sizeof(int);
            } else if(current_instruction.ARG3.abstractDatatype == FLOAT_TYPE) {
                bytesRequested *= sizeof(float);
            } else {
                printf("Unrecognised datatype: '%d'\n",current_instruction.opcode);
            }


            memAddress = -1; //Negative number indicates failure

            if(bytesRequested < VirtualMachine.RAMSize && bytesRequested > 0) { //DO this to prevent underflow
                for(int j = 0; j < VirtualMachine.RAMSize - bytesRequested; j++) {
                    memAddress = j;
                    for(int k = j; k < j + bytesRequested; k++) {
                        if((VirtualMachine.RAM[k]).isUsed == true) {
                            memAddress = -1;
                            break;
                        }
                    }

                    if(memAddress != -1) {
                        break; //Found a free location
                    }
                }

                for(int j = memAddress; j < memAddress + bytesRequested; j++) {

                    (VirtualMachine.RAM[j]).isUsed = true;
                }
            }
            VirtualMachine.registers[current_instruction.reg0].intValue = memAddress; //Assign the new address






            break;
        case FREE:

            
            bytesToFree = VirtualMachine.registers[current_instruction.reg1].intValue;
            baseAddress = current_instruction.reg0;

            if(current_instruction.ARG3.abstractDatatype == INTEGER_TYPE) {
                bytesToFree *= sizeof(int);
            } else if(current_instruction.ARG3.abstractDatatype == FLOAT_TYPE) {
                bytesToFree *= sizeof(float);
            } else {
                printf("Unrecognised datatype: '%d'\n",current_instruction.opcode);
            }


            if(baseAddress + bytesToFree > 0 && baseAddress + bytesToFree < VirtualMachine.RAMSize && baseAddress >= 0) {
                for(int j = baseAddress; j < baseAddress + bytesToFree; j++) {
                    (VirtualMachine.RAM[j].isUsed) = false;
                }
            }

            break;



        //These work
        case PRINT:
            if(current_instruction.ARG3.abstractDatatype == INTEGER_TYPE) {

                if(VirtualMachine.registers[current_instruction.reg1].intValue == 0) { //If equal to zero then input as int

                    printf("%d",VirtualMachine.registers[current_instruction.reg0].intValue);
                } else {
                    printf("%c",VirtualMachine.registers[current_instruction.reg0].intValue);
                }


            } else if(current_instruction.ARG3.abstractDatatype == FLOAT_TYPE) {

                printf("%f",VirtualMachine.registers[current_instruction.reg0].floatValue);

            } else {
                printf("Unrecognised datatype: '%d'\n",current_instruction.opcode);

            }
            break;




        case INPUT:


            
            if(current_instruction.ARG3.abstractDatatype == INTEGER_TYPE) {

                IOdata newDataInt;
                newDataInt.intVal = 0;

                if(VirtualMachine.registers[current_instruction.reg1].intValue == 0) { //If equal to zero then input as int

                    scanf("%d",&(newDataInt.intVal));
                    while(getchar() != '\n');
                    VirtualMachine.registers[current_instruction.reg0].intValue = newDataInt.intVal;
                } else {
                    scanf("%c",&(newDataInt.charVal));
                    while(getchar() != '\n');
                    VirtualMachine.registers[current_instruction.reg0].intValue = newDataInt.charVal;
                }

            } else if(current_instruction.ARG3.abstractDatatype == FLOAT_TYPE) {
                
                IOdata newDataFloat;
                newDataFloat.floatVal = 0;

                scanf("%f",&(newDataFloat.floatVal));
                while(getchar() != '\n');
                VirtualMachine.registers[current_instruction.reg0].floatValue = newDataFloat.floatVal;

            } else {
                printf("Unrecognised datatype: '%d'\n",current_instruction.opcode);
            }



            break;

        default:
            printf("Invalid instruction: '%d'\n",current_instruction.opcode);
            return false;
        }



    }




    return true;
}







