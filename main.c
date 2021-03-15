#include <stdio.h>
#include <stdint.h>

int numOfSboxes = 4;
int numberOfStages = 4;

int sbox[] = {0xE, 0x4, 0xD, 0x1, 0x2, 0xF, 0xB, 0x8, 0x3, 0xA, 0x6, 0xC, 0x5, 0x9, 0x0, 0x7};

//not zero based
int permutation[] = {1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16};

int DiffDistTable[16][16];

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

    return deltaYOut;
}


//Generates the initial input.
//TODO: FIND OUT WHY we shift by 8 bits instead of another value (4 or 12)
uint32_t generateInputPlainText(uint8_t inputByte){
    uint32_t plaintext = inputByte << 8; //shift 2 bytes  over
    printf("plaintext is: %04X\n", plaintext);
    return plaintext;
}

uint32_t doSbox(uint32_t valueToDoSboxOn){

    uint32_t sboxOutTotal = 0x0;

    for(int i = 0; i < numOfSboxes; i++){
        uint32_t sboxInput = ((valueToDoSboxOn>>(4*i)&0xF)); //This grabs 4 bits at a time
        if(sboxInput == 0){
            //don't deal with zeros
            continue;
        }

        uint32_t sboxOutput = getMostCommonDeltaY(sboxInput);
        sboxOutTotal += (sboxOutput<<(4*i));
    }
    return sboxOutTotal;   
}


uint32_t doPermutation(uint32_t valueToPermutate){

    uint32_t permutateOut = 0;
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
uint32_t runThrough(){
    uint8_t deltax, deltay;
    //same as deltaU1
    getMostCommonDeltasTotal(&deltax, &deltay); //TODO: check if we need to grab this delta y
    uint32_t modifiedValue = generateInputPlainText(deltax);

    //Dont run over the last stage yet for some reason. //TODO: figure out why
    for (int i = 0; i < numberOfStages-1; i++){
        //do sbox
        modifiedValue = doSbox(modifiedValue);
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


//reduce all of the permutation values by 1, Makes it easier to work with later.
void initPermutationArray(){
    for(int i = 0; i < sizeof(permutation); i++){
        permutation[i]--;
    }
}


void main(){

    findDiffs();
    initPermutationArray();
    //REPLICATION TESTING
    // printf("B -> %d\n", getMostCommonDeltaY(0xB));
    // printf("4 -> %d\n", getMostCommonDeltaY(0x4));
    // printf("2 -> %d\n", getMostCommonDeltaY(0x2));
    // int deltax, deltay;
    // getMostCommonDeltasTotal(&deltax, &deltay);
    // printf("%X -> %x\n", deltax, deltay);
    

    uint32_t nMinus1Stages = runThrough();
}