//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "predictor.h"


//
// TODO:Student Information
//
const char *studentName = "NAME";
const char *studentID   = "PID";
const char *email       = "EMAIL";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits = 16; // Number of bits used for Global History, default: 32, for perceptron
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

int perceptron_pc_width = 9;

#define PERCEPTRON_PC_BITS 9 //How many bits to use from PC
#define PERCEPTRON_THRESHOLD 0 // Threshold to predict taken

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//

int bhr;         // keeps track of global history

// State: What we did last time we saw this history
// Counters: 2-bit predictor
// Each cell is no longer just T/N, it is T/t/n/N
// XOR history with lower 13 bits of PC to index into `state`
// 2^n cells, 1 for each possible history, each 2* bits
// *actually 8 because 1 byte is the smallest size

uint32_t* BHT_index;     // array to keep track of where addr goes to
uint32_t* BHT_global;    // 2^n array to keep track of global t/nt
uint32_t* BHT_local;     // 2^n array to keep track of local t/nt
uint32_t* chooser;       // keeps track of chooser for each addr

// Stuff for perceptron
char * perceptron_f;    // Each F_i is 8-bit char

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

uint32_t gshareIndex(uint32_t pc) {
  // mod out index from pc to get lowest bits
  return (pc ^ bhr) % (uint32_t)pow(2, ghistoryBits);
}

// Initialize the predictor
//
void
init_predictor()
{   
  // the 'ghistoryBits' will be used to size the global and choice predictors
  
  uint32_t global_size = pow(2, ghistoryBits);
  bhr = 0; // index into table

  BHT_global = (uint32_t*)malloc(sizeof(uint32_t) * global_size);
  chooser = (uint32_t*)malloc(sizeof(uint32_t) * global_size);
  
  // the 'lhistoryBits' and 'pcIndexBits' will be used to size the local predictor.
  uint32_t local_size = pow(2, lhistoryBits);
  uint32_t BHT_size = pow(2, pcIndexBits);
  BHT_local = (uint32_t*)malloc(sizeof(uint32_t) * local_size);
  BHT_index = (uint32_t*)malloc(sizeof(uint32_t) * BHT_size);


  // size = |F| * |BHR| * #PC
  // all F_i start at 0
  perceptron_f = (char *)calloc(ghistoryBits * pow(2,perceptron_pc_width), sizeof(char));
  
  // All 2-bit predictors should be initialized to WN (Weakly Not Taken).
  for (uint32_t i = 0; i < global_size; i++){
    BHT_global[i] = WN;
  }

  for (uint32_t i = 0; i < local_size; i++){
    BHT_local[i] = WN;
  }

  // all addresses' indexes to BHT set to be 0 initially
  for (uint32_t i = 0; i < BHT_size; i++){
    BHT_index[i] = 0;
  }

  // The Choice Predictor used to select which predictor 
  // should be initialized to Weakly select the Global Predictor.
  // [0=SG, 1=WG, 2=WL, 3=SL], since 2-bit predictor
  for (uint32_t i = 0; i < global_size; i++){
    chooser[i] = 1;
  }
  
}

// ---------------- BEGIN MAKE PREDICTION ----------------

//Predict using GShare:n
uint8_t make_prediction_gshare(uint32_t pc) {
  // mod out index from pc to get lowest bits
  //uint32_t mod_amt = pow(2, ghistoryBits);
  //uint32_t gshare_index = (pc % mod_amt) ^ bhr;

  uint32_t prediction = BHT_global[gshareIndex(pc)];
  if( prediction > WN) {
    return TAKEN;
  } else {
    return NOTTAKEN;
  }
}

uint8_t make_prediction_tournament(uint32_t pc) {  
  // mod out index from pc, for local prediction
  uint32_t local_size = (int)pow(2, lhistoryBits);
  uint32_t local_index = pc % local_size;
  uint32_t BHT_idx = BHT_index[local_index];
  uint32_t local_choice =  BHT_local[BHT_idx];

  // global choice
  uint32_t global_choice = BHT_global[bhr];

  uint32_t choose_idx = bhr;
  uint32_t choice = chooser[choose_idx];

  if (choice <= 1){
    // choice is SG or WG
    if (global_choice == SN || global_choice == WN){
      // predict not taken, as BHT predicts NT
      return NOTTAKEN;
    } else {
      return TAKEN;
    }
  } else {
    // choice is SL or WL
    if (local_choice == SN || local_choice == WN){
      // predict not taken, as BHT predicts NT
      return NOTTAKEN;
    } else {
      return TAKEN;
    }
  }
}

// Perceptron checks if \Sigma (BHR_i * F_i) > threshold
uint8_t make_prediction_custom(uint32_t pc) {
  // Get truncated pc
  int ind_pc = pc % (int)pow(2, perceptron_pc_width);
  // Get location of start of this row
  char * current_f = perceptron_f + ind_pc;
  // Manipulate bhr to get bhr[i]
  int bhr_i = bhr;
  // Summation
  int sigma = 0;

  int i = 0;
  //bhr[ghistoryBits:0]
  for(; i < ghistoryBits; i++) {
    // F_i * bhr[i], where bhr[i] can only be 0 or 1
    sigma += (int)(current_f[i]  * (bhr_i & 1));
    // Shift right to move to next bhr[i]
    bhr_i = bhr_i >> 1;
  }

  //printf("%d\t", sigma);

  if( sigma > PERCEPTRON_THRESHOLD ) {
    return TAKEN;
  } else {
    return NOTTAKEN;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{
  //
  //TODO: Implement prediction scheme
  //

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      return make_prediction_gshare(pc);
    case TOURNAMENT:
      return make_prediction_tournament(pc);
    case CUSTOM:
      return make_prediction_custom(pc);
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// ----------------- END MAKE PREDICTION -----------------

// ---------------- BEGIN TRAIN PREDICTOR ----------------

void train_predictor_gshare(uint32_t pc, uint8_t outcome) {
  // obtain old location
  uint32_t old_location = gshareIndex(pc);
  if(outcome) { // was taken
    if(BHT_global[old_location] != ST) {
      BHT_global[old_location]++;
    }
  } else { //not taken 
    if(BHT_global[old_location] != SN) {
      BHT_global[old_location]--;
    }
  }
}

void train_predictor_tournament(uint32_t pc, uint32_t outcome) {
  // find local choice
  uint32_t local_size = (int)pow(2, lhistoryBits);
  uint32_t local_index = pc  % local_size; 
  uint32_t BHT_idx = BHT_index[local_index];
  uint32_t local_choice =  BHT_local[BHT_idx];

  // find global choice
  uint32_t global_choice = BHT_global[bhr];

  // determine if global and local choices were correct
  uint8_t global_correct = 0;
  uint8_t local_correct = 0;

  if (( outcome && ((global_choice == WT) || (global_choice == ST))) || 
      (!outcome && ((global_choice == WN) || (global_choice == SN)))){
    // global choice was correct
    global_correct = 1;
  }

  if (( outcome && ((local_choice == WT) || (local_choice == ST))) || 
      (!outcome && ((local_choice == WN) || (local_choice == SN)))){
    // local choice was correct
    local_correct = 1;
  }

  //DEBUG
  //printf("out %d loc %d glo %d\n", outcome, local_correct, global_correct);
  //printf("loc choice %d glob choice %d\n", local_choice, global_choice);
  
  // update local prediction
  // right side of diagram
  if(outcome) { // was taken
    if(BHT_local[BHT_idx] != ST) {
      BHT_local[BHT_idx]++;
    }
  } else { //not taken 
    if(BHT_local[BHT_idx] != SN) {
      BHT_local[BHT_idx]--;
    }
  }

  // update local history
  // shift bits over left and add result to LSB
  // left side of diagram
  BHT_index[local_index] = ((BHT_index[local_index] << 1) + outcome) % local_size;


  // update global prediction
  if(outcome) { // was taken
    if(BHT_global[bhr] != ST) {
      BHT_global[bhr]++;
    }
  } else { //not taken 
    if(BHT_global[bhr] != SN) {
      BHT_global[bhr]--;
    }
  }


  // update chooser/counter, based on whether global or local was right/wrong
  // as per slide in lec 8
  uint32_t choose_idx = bhr;
  int counter_change = local_choice - global_correct;
  if ((0 <= chooser[choose_idx] + counter_change) && 
      (chooser[choose_idx] + counter_change <= 3)){
    chooser[choose_idx] += counter_change;
  } 

  // bhr updated at end of train_predictor
}

void train_predictor_custom(uint32_t pc, uint8_t outcome) {
  // Get truncated pc
  int ind_pc = pc % (int)pow(2, perceptron_pc_width);
  // Get location of start of this row of F_i's
  char * current_f = perceptron_f + ind_pc;
  // Manipulate bhr to get bhr[i]
  int bhr_i = bhr;

  int i = 0;
  //bhr[ghistoryBits:0]
  for(; i < ghistoryBits; i++) {
    // BHR_i == 1 ? F_i++ : F_i––;
    if ((bhr_i & 1) == 1){
      current_f[i] += 1;
    } else {
      current_f[i] -= 1;
    }

    // Shift right to move to next bhr[i]
    bhr_i = bhr_i >> 1;
  }
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void train_predictor(uint32_t pc, uint8_t outcome) {
  //
  //TODO: Implement Predictor training
  //

  // Update based on the bpType
  switch (bpType) {
    case GSHARE:
      train_predictor_gshare(pc, outcome);
    case TOURNAMENT:
      train_predictor_tournament(pc, outcome);
    case CUSTOM:
      train_predictor_custom(pc, outcome);
    case STATIC:
    default:
      break;
  }
  
  // update bhr
  // BHR can be 32-bits long if perceptron
  if( ghistoryBits < 32 ) {
    bhr = ((bhr << 1) | outcome) % (uint32_t)pow(2, ghistoryBits);
  }
  else {
    bhr = ((bhr << 1) | outcome);
  }
}

// ----------------- END TRAIN PREDICTOR -----------------