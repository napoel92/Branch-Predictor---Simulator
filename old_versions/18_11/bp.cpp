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

    ~Btb(){
        vector<prediction*>::iterator table_i;
        for( table_i=fsm_tables.begin() ; table_i!=fsm_tables.end() ; table_i++ ) delete[] (*table_i);
    }
};
//========================================================================================================




/* extracts the predicftion of entry */
prediction getPrediction(BtbEntry& entry, uint32_t pc){
    unsigned int fsm_index=0;
    
    if(BTB.shared == 0 || BTB.fsm_tables.size() == 1){// is local table or no share
       fsm_index = *(entry.history_register);
        
    }

    //else...
    assert(BTB.fsm_tables.size() > 1);// global table and sharing
    int bit_i = (BTB.shared==1)?(3):(17);
    fsm_index = bitExtracted(pc,BTB.history_size,bit_i)^(*(entry.history_register));
    
    return entry.fsm_table[fsm_index];
}



//Static struct with info for all 4 functions of the BTB data stracture
static struct Btb BTB;




int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
    
    BTB.tag_size = tagSize;
    BTB.history_size = historySize;
    BTB.shared = Shared;
    BTB.initial_fsm_state = prediction(fsmState);     
    assert( BTB.initial_fsm_state==ST || BTB.initial_fsm_state==WT || BTB.initial_fsm_state==WNT || BTB.initial_fsm_state==SNT );
    //BTB.initial_fsm_state = ( fsmState==3 )?(ST):( ( fsmState==2 )?(WT):( (fsmState==1)?(WNT):(SNT) ) ); 
    
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
    unsigned int entry_num = bitExtracted(pc,log2(BTB.entries.size()),3);
    unsigned int pc_tag = bitExtracted(pc,BTB.tag_size,log2(BTB.entries.size()+3));
    BtbEntry& entry = BTB.entries[entry_num];
   
    if(entry.is_empty  ||  entry.tag != pc_tag){//branch not found
        *dst = *dst + 4;
        return false;
    } 
    
    // else- we found our branch in the btb
    prediction pred = getPrediction(entry,pc);
    if( pred == WNT  ||  pred == SNT ){
        *dst = *dst + 4;
        return false;
    }
    assert(pred == ST || pred == WT);
    *dst = entry.target;
	return true;
}




void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
    unsigned int entry_num = bitExtracted(pc,log2(BTB.entries.size()),3);
    unsigned int pc_tag = bitExtracted(pc,BTB.tag_size,log2(BTB.entries.size()+3));
    BtbEntry& entry = BTB.entries[entry_num];
    int updated_history = -1;

    /************************* CAN follow the current history *******************************/
    if(  (entry.tag==pc_tag)&&(!entry.is_empty)  ||  BTB.histories.size()==1){
        //   G-SHARE situation ---------------------------------->> global history
        //         or                                     ------->> global table
        // BTB-hit for incoming op
        updated_history = int( *(entry.history_register) );
        // also works for---------------------------------------->> global history
        //                                                ------->> local table
    }


    
    else{/*********************** CAN-NOT follow the current history *************************/
        if( BTB.histories.size()>1 ){
            if( BTB.fsm_tables.size()==1 ){//                ----->> local history
                // L-SHARE situation------------------------------>> global table
                int bit_i = (BTB.shared==1)?(3):( (BTB.shared==2)?(17):(0) );
                if(bit_i!=0){
                    updated_history=int( updated_history^bitExtracted(pc,BTB.history_size,bit_i) );
                }
            }else{ assert(BTB.fsm_tables.size()>1);//-------------->> local history 
                updated_history = 0;               //          ---->> local table
            }
        }  
    }
    assert( updated_history!= -1 );
    
    

    /**************** should reset the fsm-tables in case of: miss AND local_table ****************/
    if( (entry.tag!=pc_tag)  &&  (BTB.fsm_tables.size()>1) ){
        for(int i=0;i< pow(2,BTB.history_size); i++) entry.fsm_table[i] = BTB.initial_fsm_state;
    }


    /************ should reset the history-register in case of: miss AND local_history ************/
    if( (entry.tag!=pc_tag)  &&  (BTB.histories.size()>1) ){
        entry.history_register=0;
    }

    // TODO - things related to the course:
    //      1. check-out how many FLUSH's were there
    //      2. update BRANCHs number
    //      3. update relevant state-machine
    //      4. shift and update history-register
    //      5. update target
    //      6. make sure entry is valid
    
    return;
}




void BP_GetStats(SIM_stats *curStats){
	return;
}