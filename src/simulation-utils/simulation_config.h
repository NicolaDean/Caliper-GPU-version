#ifndef SIMULATION_CONFIG
#define SIMULATION_CONFIG
//#define CUDA //To enambe cuda_helper functions
#ifdef CUDA
#include "../cuda-utils/cuda_helper.h"
#endif

#include "default_values.h"

#define ALIVE true
#define DEAD  false


//----------------------------------------------------------------------------------
//---------------SIMULATION DATA STRUCTURES-----------------------------------------
//----------------------------------------------------------------------------------

class Core_neighbourhood;


/**
 * Allow to store the current state of a single core
*/
struct core_state{
    float curr_r;   //Current Probability to die
    float temp;     //Temperature of the core
    float load;     //Work load of this core
    float voltage;  //We can add more Death types like Chinese paper
    int real_index; //Real position in the grid
    bool alive;

    bool* top_core;
    bool* bot_core;
    bool* left_core;
    bool* right_core;
};

/**
 * Contain all usefull information about the simulation State
 * All the Working variables used by threads
 * All the results (sumTTF...)
 * All the parameters that evolve during time (left_cores, current_workload)
*/
struct simulation_state{
    float * currR;
    float * temps;   //TODO merge temps and loads array 
    float * loads;
    int   * indexes;
    int   * real_pos;
    bool  * alives;  //TODO remove alives array

    Core_neighbourhood *neighbour_state;
    
    bool  * false_register;
    core_state * core_states;
    float* times;   //times to death

    float   current_workload;
    int     left_cores;
#ifdef CUDA
    curandState_t * rand_states;
#endif

    float * sumTTF;
    float * sumTTFx2;
};


/**
 * Contain all the static configuration values initialized by user using args or initialized by default
*/
struct configuration_description{
    int     rows;           //Rows of cores in the grid
    int     cols;           //Cols of cores in the grid
    int     min_cores;      //Min num of cores alive
    int     max_cores;
    int     num_of_tests;   //Num of iteration in montecarlo
    //GPU
    int     gpu_version;    //GPU algorithm version selector (0-> redux, 1-> dynamic,2...)  
    int     block_dim;      //Blocks dimension chosen
    bool    isGPU;

    //Confidence intervall
    bool    useNumOfTest;   //True use confidence, False use numOfTest
    float   threshold;      //Threshold
    float   confInt;        //Intervallo confidenza


    float   initial_work_load;  //Initial cores workload
};


#ifdef CUDA

#define __Host_Device__  __host__ __device__

class Core_neighbourhood{
    public:

    int my_position;
    int offset;
    bool me;
    bool top_core;
    bool bot_core;
    bool left_core;
    bool right_core;

    __Host_Device__ Core_neighbourhood(){
        me = ALIVE;
    }

    __Host_Device__ void initialize(int my_pos,int off,configuration_description config){
        my_position = my_pos;
        offset      = off;

        int r = my_position / config.cols;
        int c = my_position % config.cols;

        bool out_of_range;

        //Top
        out_of_range = ((r - 1) < 0);
        bot_core = out_of_range ? DEAD : ALIVE; 
        //Bot
        out_of_range = ((r + 1) > config.rows);
        top_core = out_of_range ? DEAD : ALIVE; 
        //Left
        out_of_range = ((c - 1) < 0);
        left_core = out_of_range ? DEAD : ALIVE; 
        //Right
        out_of_range = ((c + 1) > config.cols);
        right_core = out_of_range ? DEAD : ALIVE; 
    }

    __Host_Device__ void set_top_dead(){
        top_core = DEAD;
    }

    __Host_Device__ void set_bot_dead(){
        bot_core = DEAD;
    }

    __Host_Device__ void set_left_dead(){
        left_core = DEAD;
    }

    __Host_Device__ void set_right_dead(){
        right_core = DEAD;
    }

    //COMUNICATE TO THIS CORE NEIGHBOURS THAT IT IS DEAD
    __Host_Device__ void update_state_after_core_die(Core_neighbourhood* neighbours,configuration_description config,simulation_state sim_state){
        
        int r = my_position / config.cols;
        int c = my_position % config.cols;

        //CUDA_DEBUG_MSG("CORE IN [%d][%d] is dead\n",r,c);
        //Border check is not necessary since if "top/bot/left/right" is false then or core is dead or does not exist
        if(top_core){
            neighbours[((r+1)*config.cols) + (c)].set_bot_dead();   //I say to my top neighbour to set my dead
        }
        if(bot_core){
            neighbours[((r-1)*config.cols) + (c)].set_top_dead();   //I say to my bot neighbour to set my dead
        }
        if(left_core){
            neighbours[((r)*config.cols) + (c-1)].set_right_dead(); //I say to my left neighbour to set my dead
        }
        if(right_core){
            neighbours[((r)*config.cols) + (c+1)].set_left_dead();  //I say to my right neighbour to set my dead
        }
    }

    __device__ float calculate_temperature(configuration_description config,simulation_state sim_state){

    }
};





/**
 * Allocate Global Memory of gpu to store the simulation state
*/

void allocate_simulation_state_on_device(simulation_state* state,configuration_description config){
    int cells = config.rows*config.cols*config.num_of_tests;

    //STRUCT VERSION
    if(config.gpu_version == VERSION_STRUCT_SHARED || config.gpu_version == VERSION_DYNAMIC || config.gpu_version == VERSION_STRUCT_OPTIMIZED || config.gpu_version == VERSION_2D_GRID || config.gpu_version == VERSION_GRID_LINEARIZED){
        CHECK(cudaMalloc(&state->core_states    , cells*sizeof(core_state)));
    }

    //ALL OTHER VERSION
    CHECK(cudaMalloc(&state->currR    , cells*sizeof(float)));  //CurrR
    CHECK(cudaMalloc(&state->temps    , cells*sizeof(float)));  //temps
    CHECK(cudaMalloc(&state->loads    , cells*sizeof(float)));  //loads
    CHECK(cudaMalloc(&state->indexes  , cells*sizeof(int)));    //indexes
    CHECK(cudaMalloc(&state->real_pos  , cells*sizeof(int)));    //real positions
    CHECK(cudaMalloc(&state->alives   , cells*sizeof(bool)));   //alives
    CHECK(cudaMalloc(&state->times    , cells*sizeof(float)));  //times
    CHECK(cudaMalloc(&state->false_register, sizeof(bool)));

    CHECK(cudaMalloc(&state->neighbour_state, cells*sizeof(Core_neighbourhood)));
}

/**
 * Free all the Global memory allocated for the simulation state
*/
void free_simulation_state(simulation_state* state,configuration_description config){

    //STRUCT VERSION
    if(config.gpu_version == VERSION_STRUCT_SHARED || config.gpu_version == VERSION_DYNAMIC || config.gpu_version == VERSION_STRUCT_OPTIMIZED || config.gpu_version == VERSION_2D_GRID || config.gpu_version == VERSION_GRID_LINEARIZED){
        CHECK(cudaFree(state->core_states));
        return;
    }

    //ALL OTHER VERSIONS
    CHECK(cudaFree(state->currR));
    CHECK(cudaFree(state->temps));
    CHECK(cudaFree(state->loads));
    CHECK(cudaFree(state->indexes));
    CHECK(cudaFree(state->alives));
    CHECK(cudaFree(state->times));
    CHECK(cudaFree(state->false_register));

    CHECK(cudaFree(state->neighbour_state));
}

#endif //CUDA

void setup_config(configuration_description* config,int num_of_tests,int max_cores,int min_cores,int wl,int rows,int cols,int block_dim,int gpu_version){

    config->num_of_tests = num_of_tests;

    config->cols        = cols;
    config->rows        = rows;
    config->min_cores     = min_cores;

    config->block_dim   = block_dim;
    config->gpu_version = gpu_version;
    config->initial_work_load = wl;
    config->max_cores = max_cores;
    config->useNumOfTest = true;
    config->isGPU  = false;
}

/**
 * Data structure is composed like
 * [All data 0][All data 1][..][All data N] N is Num of thread/iteration
 *
 * i represent the data position we want to access
*/
#ifdef CUDA
__device__  int getIndex(int i, int N){

    int tid = threadIdx.x + blockDim.x*blockIdx.x; //Identify

    return tid + N*i; //Get the position of this thread inside the array for "CORE i in the grid"
}
#define GETINDEX(i,tid,N)  (tid + N*(i))

#endif

//----------------------------------------------------------------------------------------------
//----------------------SWAP STATE FUNCTIONS----------------------------------------------------
//-Allow to swap the dead core state with the last alive to optimize simulation cycles----------
//----------------------------------------------------------------------------------------------

#ifdef CUDA
__device__ __host__
#endif
void swap_core_index(int* cores,int dead_index,int size,int offset){
    int tmp = cores[offset+size-1];

    cores[offset+size-1] = cores[dead_index]; //Swap dead index to end
    cores[dead_index] = tmp;
}

#ifdef CUDA
__device__
void swapState(simulation_state sim_state,int dead_index,int left_cores,int num_of_tests)
{
    unsigned int tid = threadIdx.x + blockIdx.x * blockDim.x;

    int* index = sim_state.indexes;
    int* value = sim_state.real_pos;
    core_state* cores = sim_state.core_states;

    //Get some indexes
    int last_elem   = GETINDEX((left_cores-1),tid,num_of_tests);
    int death_i     = GETINDEX(dead_index,tid,num_of_tests);
    

    int temp = value[last_elem];
    value[last_elem] = value[death_i];
    value[death_i] = temp;

    //Save the Alive core at the end of the list into a tmp val  (COALESCED)
    float tmp_currR = sim_state.currR[last_elem];
    float tmp_loads = sim_state.loads[last_elem];
    float tmp_temps = sim_state.temps[last_elem];

    //Swap dead index to end      (COALESCED WRITE, NON COALESCED READ)
    sim_state.currR[last_elem] = sim_state.currR[death_i];
    sim_state.loads[last_elem] = sim_state.loads[death_i];
    sim_state.temps[last_elem] = sim_state.temps[death_i];
    

    //Put Alive core at dead position (NOT COALESCED)
    sim_state.currR[dead_index] = tmp_currR;
    sim_state.loads[dead_index] = tmp_loads;
    sim_state.temps[dead_index] = tmp_temps;

    int temp2 = index[value[last_elem]];
    index[value[last_elem]] = index[value[death_i]];
    index[value[death_i]] = temp2;
    //CUDA_DEBUG_MSG("Swappato core: %d con core %d \n",left_cores-1, dead_index)
}



__device__ 
void swapStateOptimized (simulation_state sim_state,int dead_index,int left_cores,int num_of_tests)
{
    unsigned int tid = threadIdx.x + blockIdx.x * blockDim.x;

    int* index = sim_state.indexes;
    int* value = sim_state.real_pos;
    core_state* cores = sim_state.core_states;

    //Get some indexes
    int last_elem   = GETINDEX((left_cores-1),tid,num_of_tests);
    int death_i     = GETINDEX(dead_index,tid,num_of_tests);

    //Swap dead index to end      (COALESCED WRITE, NON COALESCED READ)
    sim_state.currR[last_elem] = sim_state.currR[death_i];
    sim_state.loads[last_elem] = sim_state.loads[death_i];
    sim_state.temps[last_elem] = sim_state.temps[death_i];

}
template<bool optimized>
__device__
void swapStateStruct(simulation_state sim_state,int dead_index,int left_cores,int num_of_tests){
    unsigned int tid = threadIdx.x + blockIdx.x * blockDim.x;

    int* index = sim_state.indexes;
    int* value = sim_state.real_pos;
    core_state* cores = sim_state.core_states;

    //Get some indexes
    int last_elem   = GETINDEX((left_cores-1),tid,num_of_tests);
    int death_i     = GETINDEX(dead_index,tid,num_of_tests);

    if(optimized){
        cores[death_i] = cores[last_elem];
        return;
    }

    int temp = value[last_elem];
    value[last_elem] = value[death_i];
    value[death_i] = temp;

    core_state t_core = cores[last_elem];
    cores[last_elem] = cores[death_i];
    cores[death_i] = t_core;
    
    temp = index[value[last_elem]];
    index[value[last_elem]] = index[value[death_i]];
    index[value[death_i]] = temp;

    //CUDA_DEBUG_MSG("Swappato core: %d con core %d -> check %d\n",left_cores-1, dead_index,cores[death_i].real_index)
}

__device__
void swapStateDynamic(simulation_state sim_state,int dead_index,int left_cores,int offset){

    int* index = sim_state.indexes;
    int* value = sim_state.real_pos;
    core_state* cores = sim_state.core_states;

    //Get some indexes
    int last_elem   = offset + (left_cores-1);
    int death_i     = offset + (dead_index);

    int temp = value[last_elem];
    value[last_elem] = value[death_i];
    value[death_i] = temp;

    core_state t_core = cores[last_elem];
    cores[last_elem] = cores[death_i];
    cores[death_i] = t_core;
    
    temp = index[value[last_elem]];
    index[value[last_elem]] = index[value[death_i]];
    index[value[death_i]] = temp;

    //CUDA_DEBUG_MSG("Swappato core: %d con core %d -> check %d\n",left_cores-1, dead_index,cores[death_i].real_index)
}

__device__
void swapStateStructOptimized(simulation_state sim_state,int dead_index,int left_cores,int num_of_tests){
    unsigned int tid = threadIdx.x + blockIdx.x * blockDim.x;

    int* index = sim_state.indexes;
    int* value = sim_state.real_pos;
    core_state* cores = sim_state.core_states;


    int last_elem   = GETINDEX((left_cores-1),tid,num_of_tests);
    int death_i     = GETINDEX(dead_index,tid,num_of_tests);

    cores[death_i] = cores[last_elem];
}

#endif


#endif //SIMULATION_CONFIG