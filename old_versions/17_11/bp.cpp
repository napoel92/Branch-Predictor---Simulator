#include "bp_api.h"
#include <vector>
#include <cassert>
#include <math.h>

using std::vector;

/* helper funtion for binary calcultions */
unsigned int bitExtracted(unsigned int number, int k, int p){
    return  (((1 << k) - 1) & (number >> (p - 1))) ;
}

/* TYPE to represent a prediction state */
enum prediction{SNT, WNT, WT, ST};


//========================================================================================================
class BtbEntry{
public:
    uint32_t* history_register;
    prediction* fsm_table;//an array
    bool is_empty;
    uint32_t tag;
    uint32_t target;

    BtbEntry(uint32_t* history, prediction* fsm_table,bool is_empty=true,uint32_t tag=0,uint32_t target=0):
            history_register(history),fsm_table(fsm_table), is_empty(is_empty),tag(tag), target(target) {}
};
//========================================================================================================
struct Btb{
    static vector<BtbEntry> entries;
    static vector<uint32_t> histories;
    static vector<prediction*> fsm_tables;
    
    static unsigned tag_size;
    static unsigned history_size;
    static int shared;
    static unsigned initial_fsm_state;
};
//========================================================================================================


//Static struct with info for all 4 functions of the BTB data stracture
static struct Btb BTB;


int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
    
    BTB.tag_size = tagSize;
    BTB.history_size = historySize;
    BTB.shared = Shared;
    BTB.initial_fsm_state = fsmState;            
    
    try{    
        //handle histories
        if(isGlobalHist){
            BTB.histories.resize(1);
        }else{
            BTB.histories.resize(btbSize);
        }
        for(int i=0;i<BTB.histories.size();i++) BTB.histories[i]=0;


        //handle tables
        if(isGlobalTable){
            BTB.fsm_tables.resize(1);
        }else{
            BTB.fsm_tables.resize(btbSize);
        }
        for(int i=0; i<BTB.fsm_tables.size();i++){
            BTB.fsm_tables[i] = new prediction[pow(2,historySize)];
            for(int j=0; j<pow(2,historySize); j++){
                //initial state of the FSM's is requiered
                BTB.fsm_tables[i][j] =prediction(fsmState); 
            }  
        }  
        
        //handle btb
        for( unsigned i=1 ; i<=btbSize ; i++ ){
            BTB.entries.push_back(BtbEntry(&BTB.histories[i],BTB.fsm_tables[i]));
        }
        assert (BTB.entries.size() == btbSize);
    }catch(...){
        return -1;
    }
    return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
    int entry_num = bitExtracted(pc,log2(BTB.entries.size()),3);
    int pc_tag = bitExtracted(pc,BTB.tag_size,log2(BTB.entries.size()+3));
    BtbEntry& entry = BTB.entries[entry_num];
   
    if(entry.is_empty  ||  entry.tag != pc_tag){//branch not found
        *dst = *dst + 4;
        return false;
    } 
    // else- we found our branch in the btb
    //todo
	return true;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	return;
}

void BP_GetStats(SIM_stats *curStats){
	return;
}