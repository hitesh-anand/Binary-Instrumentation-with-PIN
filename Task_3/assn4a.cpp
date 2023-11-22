/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2018 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */

#include "pin.H"
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <unordered_map>
using namespace std;

typedef long long ll;

#define BILLION 1000000000
#define BLKOFFSET 6

#define L1SIZE 16
#define L1INDEX 7
#define L1TAG 17
#define L1WAYS 8

#define L2SIZE 20
#define L2INDEX 10
#define L2TAG 16
#define L2WAYS 16


ll l1_access=0, l2_access=0;
ll l2_filled = 0;
ll l1_miss = 0, l2_miss = 0;
ll l2_no_hit = 0, l2_one_hit = 0, l2_more_hits = 0;

class Block{
    public:
        unsigned data, tag;
        bool valid;
        int num_hits;
        Block(unsigned _tag=0, unsigned _data=0x0){
            this->tag = _tag;
            this->data = _data;
            this->valid = false;
            this->num_hits = 0;
        }
        void invalidate()
        {
            this->valid = false;
            this->num_hits = 0;
        }
        bool isValid()
        {
            return this->valid;
        }
        void setValid()
        {
            this->valid = true;
            this->num_hits = 0;
        }
        void setEntry(unsigned _tag, unsigned _data=0x0)
        {
            this->valid = true;
            this->tag = _tag;
            this->data = _data;
            this->num_hits = 0;
        }
};


class Cache{
    public:
        vector<vector<Block*>> cache_struct;
        int index_bits, tag_bits, num_ways, blk_off;
        ll lru_count;
        int type; // 1->L1, 2->L2
        vector<vector<ll>> lru_num;
        Cache(int index, int tag, int ways, int _type, int blk=6){
            this->index_bits = index;
            this->tag_bits = tag;
            this->blk_off = blk;
            this->num_ways = ways;
            this->type = _type;
            cache_struct.resize((1<<(index_bits))+1, vector<Block*>(num_ways+1, new Block()));
            lru_num.resize((1<<(index_bits))+1, vector<ll>(num_ways+1, 0));
            this->lru_count = 0;
        }
        bool present(unsigned num_set, unsigned tag)
        {
            // auto st = cache_struct[num_set];
            if(type==1)
                l1_access++;
            else if(type==2)
                l2_access++;
            bool res = false;
            int way = 0;
            for(;way<num_ways;way++)
            {
                if(cache_struct[num_set][way]->tag==tag && cache_struct[num_set][way]->isValid())
                {
                    res = true;
                    if(type==2)
                    {
                        int hits = ++cache_struct[num_set][way]->num_hits;
                        if(hits==1)
                        {
                            l2_one_hit++;
                        }
                        if(hits==2)
                        {
                            l2_more_hits++;
                        }
                    }
                    break;
                }
            }
            if(res)
            {
                lru_num[num_set][way] = lru_count++;
            }
            else
            {
                if(type==1)
                    l1_miss++;
                else if(type==2)
                    l2_miss++;
            }
            return res;
        }
        unsigned insertEntry(unsigned num_set, unsigned tag, unsigned data=0x0)
        {
            bool done = false;
            for(int i=0; i<num_ways; i++)
            {
                if(!cache_struct[num_set][i]->isValid())
                {
                    lru_num[num_set][i] = lru_count++;
                    done = true;
                    cache_struct[num_set][i]->setEntry(tag, data);
                    break;
                }
            }
            if(done)
                return 0;
            int mn = 0, val = lru_num[num_set][0];
            for(int i=1; i<num_ways; i++)
            {
                if(lru_num[num_set][i]<val)
                {
                    mn = i;
                    val = lru_num[num_set][i];
                }
            }
            unsigned evicted = 0;
            if(type==2)
            {
                l2_filled++;
                evicted = (cache_struct[num_set][mn]->tag<<L2INDEX) + num_set;
            }
            lru_num[num_set][mn] = lru_count++;
            cache_struct[num_set][mn]->setEntry(tag, data);
            return evicted;
            //TODO : eviction policy
        }
        void invalidateBlock(unsigned num_set, unsigned tag)
        {
            // auto st = cache_struct[num_set];
            for(int i=0; i<num_ways; i++)
            {
                if(cache_struct[num_set][i]->tag==tag)
                {
                    cache_struct[num_set][i]->invalidate();
                    return;
                }
            }
        }
};

Cache* l1 = new Cache(L1INDEX, L1TAG, L1WAYS, 1);
Cache* l2 = new Cache(L2INDEX, L2TAG, L2WAYS, 2);

FILE * trace;
ofstream OutFile;
ll icount = 0;
ll fast_forward = 0;

void InsCount()     //Increment the instruction count
{
    icount++;
}

ADDRINT FastForward(void)   //Check whether we have reached the fast forward number of instructions
{
    return ((icount >= fast_forward) && (icount < fast_forward + BILLION));
}

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr)
{
    fprintf(trace,"%p: R %p\n", ip, addr);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr)
{
    fprintf(trace,"%p: W %p\n", ip, addr);
}

void process(ADDRINT addr)
{
    // fprintf(trace, "Reached %lld instructions\n",icount);
    ADDRINT l1addr = addr, l2addr = addr;
    unsigned num_set_l1 = addr & ((1<<L1INDEX)-1);
    l1addr >>= L1INDEX;
    unsigned tag_l1 = l1addr;

    unsigned num_set_l2 = l2addr & ((1<<L2INDEX)-1);
    l2addr >>= L2INDEX;
    unsigned tag_l2 = l2addr;

    if(l1->present(num_set_l1, tag_l1))
    {
        l1_access++;
    }
    else if(l2->present(num_set_l2, tag_l2))
    {
        l1_access++;
        l1_miss++;
        l2_access++;
        l1->insertEntry(num_set_l1, tag_l1);
    }
    else
    {
        l1_miss++;
        l2_miss++;
        l1_access++;
        l2_access++;
        unsigned evicted = l2->insertEntry(num_set_l2, tag_l2);
        if(evicted)
        {
            unsigned num_set = evicted & ((1<<L1INDEX)-1);
            evicted >>= L1INDEX;
            unsigned tag = evicted;
            l1->invalidateBlock(num_set, tag);
        }
        l1->insertEntry(num_set_l1, tag_l1);
        l2_filled++;
    }
}

VOID addFootPrint(ADDRINT addr, UINT32 size)
{
    // fprintf(trace, "Reached %lld instructions\n",icount);
	ADDRINT block = addr >> BLKOFFSET;
	ADDRINT last_block = (addr + size - 1) >> BLKOFFSET;
    // fprintf(trace, "block: %u, last_block: %u\n", block, last_block);
	do
	{
        // process(block);
        ADDRINT l1addr = block, l2addr = block;
        unsigned num_set_l1 = l1addr & ((1<<L1INDEX)-1);
        l1addr >>= L1INDEX;
        unsigned tag_l1 = l1addr;

        unsigned num_set_l2 = l2addr & ((1<<L2INDEX)-1);
        l2addr >>= L2INDEX;
        unsigned tag_l2 = l2addr;

        if(l1->present(num_set_l1, tag_l1))
        {
            // l1_access++;
            return;
        }
        else if(l2->present(num_set_l2, tag_l2))
        {   
            // l1_access++;
            // l1_miss++;
            // l2_access++;
            l1->insertEntry(num_set_l1, tag_l1);
        }
        else
        {
            // l1_miss++;
            // l2_miss++;
            // l1_access++;
            // l2_access++;
            unsigned evicted = l2->insertEntry(num_set_l2, tag_l2);
            if(evicted)
            {
                unsigned num_set = evicted & ((1<<L1INDEX)-1);
                evicted >>= L1INDEX;
                unsigned tag = evicted;
                l1->invalidateBlock(num_set, tag);
            }
            l1->insertEntry(num_set_l1, tag_l1);
            // l2_filled++;
        }
		block++;
	} while (block <= last_block);
    // fprintf(trace, "Done with icount : %lld\n", icount);
}


ADDRINT Terminate(void)     //Check whether we have instrumented BILLION instructions successfully
{
    return (icount >= fast_forward + BILLION);
}

// Analysis routine to exit the application
void MyExitRoutine()
{
    // Do an exit system call to exit the application.
    // As we are calling the exit system call PIN would not be able to instrument application end.
    // Because of this, even if you are instrumenting the application end, the Fini function would not
    // be called. Thus you should report the statistics here, before doing the exit system call.

    OutFile.setf(ios::showbase);
    OutFile<<"Total L1 accesses: "<<l1_access<<endl;
    OutFile<<"Total L2 accesses: "<<l2_access<<endl;
    OutFile<<"Total L1 misses: "<<l1_miss<<endl;
    OutFile<<"Total L2 misses: "<<l2_miss<<endl;
    OutFile<<"L1_miss / L1_access: "<<(double)l1_miss/l1_access<<endl;
    OutFile<<"L2_miss / L2_access: "<<(double)l2_miss/l2_access<<endl;
    OutFile<<"L1 Misprediction(%): "<<((double)(l1_miss*100)/l1_access)<<endl;
    OutFile<<"L2 Misprediction(%): "<<((double)(l2_miss*100)/l2_access)<<endl;

    OutFile<<"L2 filled blocks: "<<l2_filled<<endl;

    ll dead_on_fill = l2_filled - l2_one_hit;
    OutFile<<"L2 dead_on_fill blocks: "<<dead_on_fill<<endl;
    OutFile<<"L2 one hit blocks: "<<l2_one_hit<<endl;
    OutFile<<"L2 more hit blocks: "<<l2_more_hits<<endl;

    OutFile<<"(%) Dead-on-fill cache blocks: "<<(double)(dead_on_fill*100)/l2_filled<<endl;
    OutFile<<"(%) Cache Blocks with at least two hits: "<<(double)(l2_more_hits*100)/l2_one_hit<<endl;
    OutFile.close();
    exit(0);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.

    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

    // MyExitRoutine() is called only when the last call returns a non-zero value.
    INS_InsertThenCall(ins, IPOINT_BEFORE, MyExitRoutine, IARG_END);

    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenCall(
                ins, IPOINT_BEFORE, (AFUNPTR)addFootPrint,
                IARG_MEMORYOP_EA, memOp,
                IARG_UINT32, INS_MemoryOperandSize(ins, memOp), // memory access size
                IARG_END);
    }
    INS_InsertCall(ins, IPOINT_BEFORE, InsCount, IARG_END);
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
                            "o", "inscount.out", "specify output file name");
KNOB<string> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool",
                             "f", "0", "specify fast forward amount");

VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    trace = fopen("addrtrace.out", "w");

    OutFile.open(KnobOutputFile.Value().c_str());
    fast_forward = stoll(KnobFastForward.Value()) * BILLION;

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}