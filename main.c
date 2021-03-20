#include <stdio.h>
#include <stdint.h>

#define NUM_OF_SBOXES 4
#define NUM_OF_STAGES 4

//using arrays as a key:value pair to convert 4 bit values. (example 0x4 will convert to 0x2)
const int sbox[] = {0xE, 0x4, 0xD, 0x1, 0x2, 0xF, 0xB, 0x8, 0x3, 0xA, 0x6, 0xC, 0x5, 0x9, 0x0, 0x7};

//not zero based, we will minus one from all of these in code. 
//I did it this way just so there isn't a human error with off by one issues from copying the values from the PDF
const int permutation[] = {1-1, 5-1, 9-1, 13-1, 2-1, 6-1, 10-1, 14-1, 3-1, 7-1, 11-1, 15-1, 4-1, 8-1, 12-1, 16-1};

//randomly selected xor values on 16 bit values.
const uint16_t keyXor[NUM_OF_STAGES + 1] = {0x4354, 0x23F3, 0x5FEA, 0x9812, 0x2328};

int DiffDistTable[16][16];

uint32_t TotalProbabilityNumerator = 1;
uint32_t TotalProbabilityDenominator = 1;

// typedef struct deltaMostLikely{
//   int value;
//   int Chance;
// } Point;

// This generates the Diffenece Distribution table. Print the values 6 or greater
void findDiffs()
{
    printf("\nDifference Distribution Table:\n");
    
    
    for(int x1 = 0; x1 < 16; x1++){
        for(int x2 = 0; x2 < 16; x2++){
            int deltax = x1 ^ x2;
            int deltay = sbox[x1] ^ sbox[x2];  
            //Add a note to the difference dist table that we have hit that point. 
            DiffDistTable[deltax][deltay]++;        
        }
    }

    for(int x1 = 0; x1 < 16; x1++)
    {
        for(int x2 = 0; x2 < 16; x2++){
            printf("%3d ", DiffDistTable[x1][x2]);
        }
        printf("\n");
    }
    
    printf("\nDisplaying most probable values (6 or greater only):\n");
    
    printf("num of occurances        Delta X        Delta Y\n");
    for(int deltaX = 1; deltaX < 16; deltaX++){
        for(int deltaY = 1; deltaY < 16; deltaY++){
            if (DiffDistTable[deltaX][deltaY] >= 6 && DiffDistTable[deltaX][deltaY] < 16){           
                printf("  %2d                     %2d                %2d\n",DiffDistTable[deltaX][deltaY], deltaX, deltaY);  
            }
        }
    }
}

//Get the most common DeltaX -> DeltaY combo.
void getMostCommonDeltasTotal(uint8_t * deltaXOut, uint8_t * deltaYOut){

    //TODO: Add tracking for previously selected deltaX/Y combos.

    int MostCommon = 0;
    //Dont do zero since 0->0 is always 16 times
    for(uint8_t deltaX = 1; deltaX < 16; deltaX++){
        for(uint8_t deltaY = 1; deltaY < 16; deltaY++){
            if(MostCommon < DiffDistTable[deltaX][deltaY]){
                MostCommon = DiffDistTable[deltaX][deltaY];
                *deltaXOut = deltaX;
                *deltaYOut = deltaY;
            }
        }
    }
}

//Gives the most common deltaY given a deltaX
uint8_t getMostCommonDeltaY(uint8_t deltaX){
    
    //TODO: add tracking of previously selected delta X/Y combos

    int MostCommon = 0;
    uint8_t deltaYOut = 0;
    for(uint8_t deltaY = 1; deltaY < 16; deltaY++){
        if(MostCommon < DiffDistTable[deltaX][deltaY]){
            MostCommon = DiffDistTable[deltaX][deltaY];
            deltaYOut = deltaY;
        }
    }

    TotalProbabilityNumerator *= MostCommon;
    TotalProbabilityDenominator *= 16; 

    return deltaYOut;
}


//Generates the initial input.
//TODO: FIND OUT WHY we shift by 8 bits instead of another value (4 or 12)
uint16_t generateInputPlainText(uint8_t inputByte){
    uint16_t plaintext = inputByte << 8; //shift 2 bytes  over
    printf("plaintext is: %04X\n", plaintext);
    return plaintext;
}

uint16_t doSboxForAttack(uint16_t valueToDoSboxOn){

    uint16_t sboxOutTotal = 0x0;

    //loop through all non-zero values to see which is the most common DeltaY per input.
    for(int i = 0; i < NUM_OF_SBOXES; i++){
        uint16_t sboxInput = ((valueToDoSboxOn>>(4*i)&0xF)); //This grabs 4 bits at a time
        if(sboxInput == 0){
            //don't deal with zeros
            continue;
        }

        uint16_t sboxOutput = getMostCommonDeltaY(sboxInput);
        sboxOutTotal += (sboxOutput<<(4*i));
    }
    return sboxOutTotal;   
}

uint16_t doSboxForEncryption(uint16_t valueToDoSboxOn){
    uint16_t sboxOutTotal = 0x0;

    for(int i = 0; i < NUM_OF_SBOXES; i++){
        //grab 4 bits at a time
        uint16_t sboxInput = ((valueToDoSboxOn>>(4*i)&0xF)); //This grabs 4 bits at a time
        //use sboxInput as the key for the key:value pair
        uint16_t sboxOutput = sbox[sboxInput];
        //recombine the 16 bits
        sboxOutTotal += (sboxOutput<<(4*i));
    }
    return sboxOutTotal;   
}

uint16_t doPermutation(uint16_t valueToPermutate){

    uint16_t permutateOut = 0;
    //TODO: make this dynamic later
    for(int i = 0; i < 16 ; i++){
        uint16_t invertedIndex = 15 - i;
        if((valueToPermutate >> invertedIndex) & 1U){
            uint16_t permutatedIndex = permutation[invertedIndex];
            permutateOut |= 1UL << permutatedIndex;
        }
    }
    return permutateOut;
}


//Doing page 24 of the tutorial
uint16_t runAttack(){
    uint8_t deltax, deltay;
    //same as deltaU1
    getMostCommonDeltasTotal(&deltax, &deltay); //TODO: check if we need to grab this delta y
    uint16_t modifiedValue = generateInputPlainText(deltax);

    //Dont run over the last stage yet for some reason. //TODO: figure out why
    for (int i = 0; i < NUM_OF_STAGES-1; i++){
        //do sbox
        modifiedValue = doSboxForAttack(modifiedValue);
        printf("end of sbox iteration %d is %04X\n", i, modifiedValue);
        //do permutation
        modifiedValue = doPermutation(modifiedValue);
        printf("end of permutation %d is %04X\n", i, modifiedValue);
    }
    fflush(stdout);
    //At last stage here.

    //TODO: better name, this is bad victor
    return modifiedValue;
}



uint16_t runEncryption(uint16_t message){
    uint16_t intermediate_val = message;
    printf("start:    %X\n", intermediate_val);
    for(int i = 0; i <= NUM_OF_STAGES; i++){
        //xor
        intermediate_val = intermediate_val^keyXor[i];
        printf("xor:      %X\n", intermediate_val);
        //sbox
        intermediate_val = doSboxForEncryption(intermediate_val);
        printf("sbox:     %X\n", intermediate_val);
        //permutation - don't permutate on round 4 
        if(i != (NUM_OF_STAGES-1)){
            intermediate_val = doPermutation(intermediate_val);
            printf("perm:     %X\n", intermediate_val);
        }

    }

    //final XOR
    intermediate_val = intermediate_val^keyXor[NUM_OF_STAGES];
    printf("final:    %X\n", intermediate_val);
    fflush(stdout);

    return intermediate_val;
}

void main(){

    findDiffs();

    uint16_t output = runEncryption(0x3211);
    //REPLICATION TESTING
    // printf("B -> %d\n", getMostCommonDeltaY(0xB));
    // printf("4 -> %d\n", getMostCommonDeltaY(0x4));
    // printf("2 -> %d\n", getMostCommonDeltaY(0x2));
    // int deltax, deltay;
    // getMostCommonDeltasTotal(&deltax, &deltay);
    // printf("%X -> %x\n", deltax, deltay);
    

    uint16_t nMinus1Stages = runAttack();
    float Probabilityfloat =  (float)TotalProbabilityNumerator/(float)TotalProbabilityDenominator;
    printf("chance is: %u/%u or %f\n", TotalProbabilityNumerator, TotalProbabilityDenominator, Probabilityfloat);
    fflush(stdout);
}