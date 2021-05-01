#include "bp_api.h"
#include <vector>
#include <cassert>
#include <math.h>

const int INFORMATIVE_PC_BITS=30;
const int THIRD_BIT=3;
const int LOW_SHARE_BIT=3;
const int MID_SHARE_BIT=17;
const int VALID_BIT=1;


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
    prediction* fsm_table; // <----------------------------------an array that is not allocated by BtbEntry
    bool is_empty;
    uint32_t tag;
    uint32_t target;

    BtbEntry(uint32_t* history, prediction* fsm_table,bool is_empty=true,uint32_t tag=0,uint32_t target=0):
            history_register(history),fsm_table(fsm_table), is_empty(is_empty),tag(tag), target(target) {}
};
//========================================================================================================
struct Btb{
    vector<BtbEntry> entries;
    vector<uint32_t> histories;
    vector<prediction*> fsm_tables;
    
    unsigned tag_size;
    unsigned history_size;
    int shared;
    prediction initial_fsm_state;

    bool isGloballHistory;
    bool isGlobalTable;

    ~Btb(){
        vector<prediction*>::iterator table_i;
        for( table_i=fsm_tables.begin() ; table_i!=fsm_tables.end() ; table_i++ ) delete[] (*table_i);
    }
};
//========================================================================================================



//Static struct with info for all 4 functions of the BTB data structure
static struct Btb BTB;


//Static variables for statistics
static int number_of_branches;
static int number_of_flushes;
static unsigned int global_index=0;




/* extracts the predicftion of entry */
uint32_t getPredictionIndex(BtbEntry& entry, uint32_t pc){
    
    if( BTB.shared == 0 || BTB.isGlobalTable==false ){// is local table or no share
       return *(entry.history_register);
    }

    /*else*/ assert(BTB.fsm_tables.size() == 1);// is global table and sharing lsb/msb
    int bit_i = (BTB.shared==1)?(LOW_SHARE_BIT):(MID_SHARE_BIT); 
    return bitExtracted(pc,BTB.history_size,bit_i)^(*(entry.history_register));
}
    




int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){

    number_of_branches = 0;
    number_of_flushes = 0;

    BTB.isGloballHistory = isGlobalHist;
    BTB.isGlobalTable = isGlobalTable;
    
    BTB.tag_size = tagSize;
    BTB.history_size = historySize;
    BTB.shared = Shared;
    BTB.initial_fsm_state = prediction(fsmState);     
    assert( BTB.initial_fsm_state==ST || BTB.initial_fsm_state==WT || BTB.initial_fsm_state==WNT || BTB.initial_fsm_state==SNT );
    
    try{    
        //handle histories
        if(isGlobalHist){
            BTB.histories.resize(1);
        }else{
            BTB.histories.resize(btbSize);
        }
        for(unsigned int i=0;i<BTB.histories.size();i++) BTB.histories[i]=0;


        //handle tables
        if(isGlobalTable){
            BTB.fsm_tables.resize(1);
        }else{
            BTB.fsm_tables.resize(btbSize);
        }
        for(unsigned int i=0; i<BTB.fsm_tables.size();i++){
            int size_of_array = pow(2,historySize);
            BTB.fsm_tables[i] = new prediction[size_of_array];
            for(int j=0; j<pow(2,historySize); j++){
                //initial state of the FSM's is requiered
                BTB.fsm_tables[i][j] =prediction(fsmState); 
            }  
        }  
        
        //handle btb
        for( unsigned i=0 ; i<btbSize ; i++ ){
            unsigned int& fsm_index= (BTB.isGlobalTable)?(global_index):(i);
            unsigned int& bhr_index= (BTB.isGloballHistory)?(global_index):(i);
            BTB.entries.push_back(BtbEntry( &BTB.histories[bhr_index],BTB.fsm_tables[fsm_index] ));
        }
        assert (BTB.entries.size() == btbSize);
        return 0;
    }catch(...){}
    return -1;
}




bool BP_predict(uint32_t pc, uint32_t *dst){
    ++number_of_branches;

    unsigned int entry_num = bitExtracted(pc,log2(BTB.entries.size()),THIRD_BIT);
    unsigned int pc_tag = bitExtracted(pc,BTB.tag_size,log2(BTB.entries.size())+THIRD_BIT);
    BtbEntry& entry = BTB.entries[entry_num];

    if(entry.is_empty  ||  entry.tag != pc_tag){//branch not found
        *dst = pc + 4;
        return false;
    } 
     // else /////////////////////////////////// we found our branch in the btb
    uint32_t index = getPredictionIndex(entry,pc);
    if( entry.fsm_table[index] == WNT  ||  entry.fsm_table[index] == SNT ){
        *dst = pc + 4;
        return false;
    }
    assert(entry.fsm_table[index] == ST || entry.fsm_table[index] == WT);
    *dst = entry.target;
	return true;
}




void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
    if( ( !taken && pred_dst!= pc+4 ) || ( taken && pred_dst!= targetPc ) )  ++number_of_flushes;

    unsigned int entry_num = bitExtracted(pc,log2(BTB.entries.size()),THIRD_BIT);
    unsigned int pc_tag = bitExtracted(pc,BTB.tag_size,log2(BTB.entries.size())+THIRD_BIT);
    BtbEntry& entry = BTB.entries[entry_num];
     
    /**************** should reset the fsm-tables in case of: miss AND local_table **********************/
    if( (entry.tag!=pc_tag /*|| entry.is_empty*/)  &&  (BTB.isGlobalTable==false) ){
        for(int i=0;i< pow(2,BTB.history_size); i++) entry.fsm_table[i] = BTB.initial_fsm_state;
    }
    /***************** should reset the history-register in case of: miss AND local_history ************/
    if( (entry.tag!=pc_tag /*|| entry.is_empty*/)  &&  (BTB.isGloballHistory==false)  ){
        *entry.history_register=0;
    }
    /***************************** update the relevant state machine ***********************************/
    uint32_t index = getPredictionIndex(entry,pc);
    prediction& pred = entry.fsm_table[index];
    if( taken && pred!=ST ){
        pred = (pred==SNT)?(WNT):( (pred==WNT)?(WT):(ST) ) ;
    }
    if( !taken && pred!=SNT ){
        pred = (pred==ST)?(WT):( (pred==WT)?(WNT):(SNT) ) ;
    }
    /**************************** shift and update history-register ********************************/
    *entry.history_register = 2*bitExtracted(*entry.history_register,BTB.history_size-1,1);
    *entry.history_register = (taken)?(*entry.history_register+1):(*entry.history_register);


    //update the rest of entry's values
    entry.target = targetPc;
    entry.tag = pc_tag;
    entry.is_empty = false;
    
    return;
}



void BP_GetStats(SIM_stats *curStats){
    //calculate the size in bits
    int btb_table_size = BTB.entries.size()*(VALID_BIT+BTB.tag_size+INFORMATIVE_PC_BITS);
    int fsm_tables_size = 2*BTB.fsm_tables.size()*pow(2,BTB.history_size);
    int bhr_size=BTB.histories.size()*BTB.history_size;
    int size_in_bits = btb_table_size + fsm_tables_size + bhr_size;
     //return the required values
    curStats->size = size_in_bits;
    curStats->br_num = number_of_branches;
    curStats->flush_num = number_of_flushes;
	return;
}