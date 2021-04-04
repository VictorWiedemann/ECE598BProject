#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>  // rand(), srand()
#include <time.h>    // time()

#define NUM_OF_SBOXES 4
#define SIZE_OF_SBOX 16
#define NUM_OF_STAGES 4
#define BITS_OF_INPUT 16
#define NUM_OF_CYPHER_PAIRS 5000



struct nibbleList{
    uint16_t currentLocation;
    struct nibblelist * nextNibble;
};

typedef struct nibbleList nibbleList;

//using arrays as a key:value pair to convert 4 bit values. (example 0x4 will convert to 0x2)
const int sbox[SIZE_OF_SBOX] = {0xE, 0x4, 0xD, 0x1, 0x2, 0xF, 0xB, 0x8, 0x3, 0xA, 0x6, 0xC, 0x5, 0x9, 0x0, 0x7};
int reverseSbox[SIZE_OF_SBOX];

//not zero based, we will minus one from all of these in code. 
//I did it this way just so there isn't a human error with off by one issues from copying the values from the PDF
const int permutation[] = {1-1, 5-1, 9-1, 13-1, 2-1, 6-1, 10-1, 14-1, 3-1, 7-1, 11-1, 15-1, 4-1, 8-1, 12-1, 16-1};

//randomly selected xor values on 16 bit values.
const uint16_t keyXor[NUM_OF_STAGES + 1] = {0x4354, 0x23F3, 0x5FEA, 0x9812, 0x0808};

int DiffDistTable[BITS_OF_INPUT][BITS_OF_INPUT];

uint32_t TotalProbabilityNumerator = 1;
uint32_t TotalProbabilityDenominator = 1;


int cyphertextPairs[2][NUM_OF_CYPHER_PAIRS];

bool activeNibbles[BITS_OF_INPUT/4];
uint16_t numOfActiveNibbles = 0;

uint16_t rightPairs[32767] = {0};




// This generates the Diffenece Distribution table. Print the values 6 or greater
void findDiffs()
{
    printf("\nDifference Distribution Table:\n");
    
    
    for(int x1 = 0; x1 < BITS_OF_INPUT; x1++){
        for(int x2 = 0; x2 < BITS_OF_INPUT; x2++){
            int deltax = x1 ^ x2;
            int deltay = sbox[x1] ^ sbox[x2];  
            //Add a note to the difference dist table that we have hit that point. 
            DiffDistTable[deltax][deltay]++;        
        }
    }

    for(int x1 = 0; x1 < BITS_OF_INPUT; x1++)
    {
        for(int x2 = 0; x2 < BITS_OF_INPUT; x2++){
            printf("%3d ", DiffDistTable[x1][x2]);
        }
        printf("\n");
    }
    
    printf("\nDisplaying most probable values (6 or greater only):\n");
    
    printf("num of occurances        Delta X        Delta Y\n");
    for(int deltaX = 1; deltaX < BITS_OF_INPUT; deltaX++){
        for(int deltaY = 1; deltaY < BITS_OF_INPUT; deltaY++){
            if (DiffDistTable[deltaX][deltaY] >= 6 && DiffDistTable[deltaX][deltaY] < BITS_OF_INPUT){           
                printf("  %2d                     %2d                %2d\n",DiffDistTable[deltaX][deltaY], deltaX, deltaY);  
            }
        }
    }
}

//Get the most common DeltaX -> DeltaY combo.
uint8_t getMostCommonDeltasTotal(){
    uint8_t deltaXOut = 0;
    int MostCommon = 0;
    //Dont do zero since 0->0 is always BITS_OF_INPUT times
    for(uint8_t deltaX = 1; deltaX < BITS_OF_INPUT; deltaX++){
        for(uint8_t deltaY = 1; deltaY < BITS_OF_INPUT; deltaY++){
            if(MostCommon < DiffDistTable[deltaX][deltaY]){
                MostCommon = DiffDistTable[deltaX][deltaY];
                deltaXOut = deltaX;
                //*deltaYOut = deltaY;
            }
        }
    }
    return deltaXOut;
}

//Gives the most common deltaY given a deltaX
uint8_t getMostCommonDeltaY(uint8_t deltaX){
    
    //TODO: add tracking of previously selected delta X/Y combos

    int MostCommon = 0;
    uint8_t deltaYOut = 0;
    for(uint8_t deltaY = 1; deltaY < BITS_OF_INPUT; deltaY++){
        if(MostCommon < DiffDistTable[deltaX][deltaY]){
            MostCommon = DiffDistTable[deltaX][deltaY];
            deltaYOut = deltaY;
        }
    }

    TotalProbabilityNumerator *= MostCommon;
    TotalProbabilityDenominator *= BITS_OF_INPUT; 

    return deltaYOut;
}

//Generates the initial input.
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

uint16_t doReverseSbox(uint16_t valueToReverse, int * reverseSbox){
    uint16_t ReverseSboxOut = 0x0;

    //loop through all non-zero values to see which is the most common DeltaY per input.
    for(int i = 0; i < NUM_OF_SBOXES; i++){
        //only deal with the nibbles we care about.
        if(!activeNibbles[i]){
            continue;
        }
        uint16_t sboxInput = ((valueToReverse>>(4*i)&0xF)); //This grabs 4 bits at a time
        
        if(sboxInput > 0xF){
            printf("ERROR THIS CANNOT HAPPEN. input is: %04X\n");
            break;
        }

        uint16_t sboxOutput = reverseSbox[sboxInput];
        ReverseSboxOut += (sboxOutput<<(4*i));
    }
    return ReverseSboxOut; 
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
    for(int i = 0; i < BITS_OF_INPUT ; i++){
        uint16_t invertedIndex = 15 - i;
        if((valueToPermutate >> invertedIndex) & 1U){
            uint16_t permutatedIndex = permutation[invertedIndex];
            permutateOut |= 1UL << permutatedIndex;
        }
    }
    return permutateOut;
}

//Doing page 24 of the tutorial
uint16_t findDeltaU(uint16_t deltaB){

   uint16_t modifiedValue = deltaB;
    //Dont run over the last stage because that is when we will be extracting key bits (section 4.4) 
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
    uint16_t deltaU = modifiedValue;

    return deltaU;
}

uint16_t runEncryption(uint16_t message){
    uint16_t intermediate_val = message;
    //printf("start:    %X\n", intermediate_val);
    for(int i = 0; i <= NUM_OF_STAGES; i++){
        //xor
        intermediate_val = intermediate_val^keyXor[i];
       // printf("xor:      %X\n", intermediate_val);
        //sbox
        intermediate_val = doSboxForEncryption(intermediate_val);
       // printf("sbox:     %X\n", intermediate_val);
        //permutation - don't permutate on round 4 
        if(i != (NUM_OF_STAGES-1)){
            intermediate_val = doPermutation(intermediate_val);
          //  printf("perm:     %X\n", intermediate_val);
        }

    }

    //final XOR
    uint16_t finalxor = keyXor[NUM_OF_STAGES];
    intermediate_val = intermediate_val^finalxor;
    //printf("final:    %X\n", intermediate_val);
    fflush(stdout);

    return intermediate_val;
}

void generatePairs(uint16_t deltaU4, uint16_t deltaB){
    //TODO: fill cyphertextPairs[2][5000]
    int numOfPairs = 0;
    bool isDeltaU4NibbleZero[BITS_OF_INPUT/4];
    
    // Intializes random number generator
    // We could change the seed, but doing this will give us the same execution per run and easier to debug. 
    // We could change this to ensure the system works no matter the input.
    srand((unsigned) time(NULL));

    //Find which nibbles have 0s. We will ensure only active nibbles have a difference in when looking for Crypto Pairs.
    for(int i = 0; i < (BITS_OF_INPUT/4); i++){
        uint16_t currNibbleDeltaU = ((deltaU4 >> (4*i)&0xF)); //This grabs 4 bits at a time
        activeNibbles[i] = (currNibbleDeltaU != 0);
        if(activeNibbles[i]){
            numOfActiveNibbles++;
        }
    }

    while(numOfPairs < NUM_OF_CYPHER_PAIRS){

        //create a pair of inputs that is only different by deltaB.
        uint16_t firstInput = rand() % 0xFFFF;
        uint16_t secondInput = firstInput ^ deltaB;       
        
        //put both through the encryption.
        uint16_t firstEncrypt = runEncryption(firstInput);
        uint16_t secondEncrypt = runEncryption(secondInput);
        
        //check the delta of the 2 outputs only has changes on active nibbles.
        uint16_t deltaC = firstEncrypt ^ secondEncrypt;
        
        bool isInvalidPair = false;
        for(int i = 0; i < (BITS_OF_INPUT/4); i++){
            uint16_t currNibbleDeltaC = ((deltaC >> (4*i)&0xF)); //This grabs 4 bits at a time
            
            // Check for invalid pairs. if there is a difference in a nibble that shouldn't be different, bail out. 
            if(!activeNibbles[i] && (currNibbleDeltaC != 0)){
                isInvalidPair = true;
            }       
        }

        //invalid pair, do not save this pair, skip and go to another randomly generated one.
        if(isInvalidPair){
            //printf("P = first, %04X --- second, %04X\n", firstInput, secondInput);
            //printf("C = first, %04X --- second, %04X\n", firstEncrypt, secondEncrypt);
            //printf("deltaC is %04X\n invalid \n", deltaC);
            continue;
        }

        //printf("P = first, %04X --- second, %04X\n", firstInput, secondInput);
        //printf("C = first, %04X --- second, %04X\n", firstEncrypt, secondEncrypt);
        //printf("deltaC is %04X\n valid \n", deltaC);
        
        
        //if only active nibbles have a difference, save as a pair. 
        cyphertextPairs[0][numOfPairs] = firstEncrypt;
        cyphertextPairs[1][numOfPairs] = secondEncrypt;
        
        numOfPairs++;
    }
}

void generateReverseSboxes(){
    
    for(int i = 0; i < SIZE_OF_SBOX; i++){
        reverseSbox[sbox[i]] = i;
    }
    printf("reverse sbox is:\n");
    for(int i = 0; i < SIZE_OF_SBOX; i++){
        printf("%X, ", reverseSbox[i]);
    }
    printf("\n");

}

void iterateWithPartialKey(uint16_t partialKey, uint16_t deltaU4){
    

    for(int i = 0; i < NUM_OF_CYPHER_PAIRS; i++){
                
        uint16_t xorWithPossibleKey1 = cyphertextPairs[0][i] ^ partialKey;
        uint16_t xorWithPossibleKey2 = cyphertextPairs[1][i] ^ partialKey;

        uint16_t reverseCypher1 = doReverseSbox(xorWithPossibleKey1, reverseSbox);
        uint16_t reverseCypher2 = doReverseSbox(xorWithPossibleKey2, reverseSbox);
        
        uint16_t reverseCypherXor = reverseCypher1 ^ reverseCypher2;


        if (reverseCypherXor == deltaU4){ 
            rightPairs[partialKey]++;
            //printf("C1 %04X, revC1  %04X C2 %04X rev C2  %04X, Partial  %04X\n", cyphertextPairs[0][i], reverseCypher1, cyphertextPairs[1][i], reverseCypher2, possibleKey.total);
        }
            
    }
}

//recursion over all permutations of Z5. Only iterate over all active nibbles.
void loopOverNibbles(nibbleList *nibbleList, uint16_t lastPartialKey, uint16_t deltaU4){
    
    //hit end of recursion system.
    if(nibbleList == NULL){
        iterateWithPartialKey(lastPartialKey, deltaU4);
        return;
    }

    // while(nibbleListCurrent != NULL){
    //     printf("current index is %X and nibble List next is %s\n", nibbleListCurrent->currentLocation, nibbleListCurrent->nextNibble);
    //     nibbleListCurrent = nibbleListCurrent->nextNibble;
    // }

    for(int i = 0; i < 16; i++){
        uint16_t currentPartialKey = lastPartialKey;
        //bitshift the values by currentPartialKey += i << 4*nibbleList
        currentPartialKey += ( i <<(4*nibbleList->currentLocation));

        
        loopOverNibbles(nibbleList->nextNibble, currentPartialKey, deltaU4);
    }
   
    //for overallnibbles starting at for all nibbles starting at lastNibble{
    //}else{
        //return;
    //}
    //for first active nibble, start loop (0-> 16)*4^lastNibble
    //at start of this loop, check if there is another active nibble
    //if yes, call this function (lastNibble++)

 
}


void generateMostLikelyKeyBits(uint16_t deltaU4){
    
    //I am just keeping an array of every possible subkey. I do not know which nibbles we are looking for partial subkeys. Bad for memory space, good for all cases.
    generateReverseSboxes();

    nibbleList *nibbleListHead = (nibbleList*)malloc(sizeof(nibbleList));

    nibbleListHead->currentLocation = 0xFF;
    nibbleListHead->nextNibble = NULL;

    nibbleList *NibbleListCurrent = nibbleListHead;

    //generate the link lists of nibbles
    for(int i = 0; i < 4; i++){
        if(!activeNibbles[i]){
            continue;
        }

        if(NibbleListCurrent->currentLocation == 0xFF){
            NibbleListCurrent->currentLocation = i;
        }else{
            nibbleList *nibbleListNext = (nibbleList*)malloc(sizeof(nibbleList));
            nibbleListNext->currentLocation = i;
            nibbleListNext->nextNibble = NULL;
            NibbleListCurrent->nextNibble = nibbleListNext;
            NibbleListCurrent = NibbleListCurrent->nextNibble;
        }
    }


    loopOverNibbles(nibbleListHead, 0, deltaU4);

    uint16_t mostLikelySubkey = 0;
    uint16_t numOfTimesSubkeyFound = 0;
    
    for (int i = 0; i < 32767; i++) 
    {
        if(rightPairs[i] > 0){
            printf("right pair %X happened %u times\n", i, rightPairs[i]);
        }
        
        if (numOfTimesSubkeyFound < rightPairs[i]){
            numOfTimesSubkeyFound = rightPairs[i];
            
            mostLikelySubkey = i;
        }
    }

    printf("most Likely Key: %04X with probability %d\n", mostLikelySubkey, numOfTimesSubkeyFound);
}


uint16_t FindK5(uint16_t deltaU4, uint16_t deltaB){
    
    generatePairs(deltaU4, deltaB);
    generateMostLikelyKeyBits(deltaU4);

    return 0;
}



void main(){

    findDiffs();

    //same as deltaU1
    uint8_t mostCommonNibble = getMostCommonDeltasTotal(); //TODO: check if we need to grab this delta y
    uint16_t deltaB = generateInputPlainText(mostCommonNibble);

    uint16_t deltaU4 = findDeltaU(deltaB);
    printf("DeltaU is %04X\n", deltaU4);
    FindK5(deltaU4, deltaB);



    float Probabilityfloat =  (float)TotalProbabilityNumerator/(float)TotalProbabilityDenominator;
    printf("theoretical chance is: %u/%u or %f\n", TotalProbabilityNumerator, TotalProbabilityDenominator, Probabilityfloat);
    fflush(stdout);
}






//REPLICATION TESTING
    // printf("B -> %d\n", getMostCommonDeltaY(0xB));
    // printf("4 -> %d\n", getMostCommonDeltaY(0x4));
    // printf("2 -> %d\n", getMostCommonDeltaY(0x2));
    // int deltax, deltay;
    // getMostCommonDeltasTotal(&deltax, &deltay);
    // printf("%X -> %x\n", deltax, deltay);
    