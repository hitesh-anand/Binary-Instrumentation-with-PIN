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
#include "pin.H"
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <map>
#include <fstream>
using namespace std;

ofstream OutFile;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
typedef long long ll;

#define BILLION 1000000000
#define BTBSIZE 128
#define BTBWIDTH 4
#define LRUNUMINIT 1000000
#define BTBMASK 127
#define BTBACCESSBITS 7
#define GHRWIDTH 7

ll icount = 0;
ll fast_forward = 0;

UINT64 total_branches = 0;
UINT64 correct_branches = 0;
UINT64 total_forward = 0;
UINT64 total_backward = 0;
UINT64 correct_forward = 0;
UINT64 correct_backward = 0;

UINT64 btb_accesses = 0;
UINT64 btb_hits = 0;

UINT64 lru_state[BTBSIZE];

UINT64 ghr = 0;
// Analysis routine to check fast-forward condition

class BTBEntry
{

    int valid, tag, lrunum;
    ADDRINT target;

public:
    BTBEntry(ADDRINT _target, int _lrunum = LRUNUMINIT, int _tag = 0, int _valid = 0)
    {
        this->valid = _valid;
        this->tag = _tag;
        this->target = _target;
        this->lrunum = _lrunum;
    }

    bool isValid()
    {
        return (this->valid == 1);
    }

    ADDRINT getTarget()
    {
        return this->target;
    }

    int getTag()
    {
        return this->tag;
    }

    void setTag(int _tag)
    {
        this->tag = _tag;
    }

    void setTarget(ADDRINT _target)
    {
        this->target = _target;
    }

    void setValid()
    {
        this->valid = 1;
    }

    void setLruNum(int _lrunum)
    {
        this->lrunum = _lrunum;
    }

    void update(ADDRINT _target, int _lrunum, int _tag, int _valid)
    {
        this->target = _target;
        this->lrunum = _lrunum;
        this->tag = _tag;
        this->valid = _valid;
    }

    int getLruNum()
    {
        return this->lrunum;
    }
};

BTBEntry *btb[BTBSIZE][BTBWIDTH];


void InsCount() // Increment the instruction count
{
    icount++;
}

ADDRINT FastForward(void) // Check whether we have reached the fast forward number of instructions
{
    return ((icount >= fast_forward) && (icount < fast_forward + BILLION));
}

void MyPredicatedAnalysis(ADDRINT ins_addr, ADDRINT target_ins_addr, BOOL branch_pred, INT32 ins_cat, UINT32 ins_size) // Increment the value of the passed pointer
{
    total_branches++;
    btb_accesses++;

    int btb_index = (ins_addr & BTBMASK) ^ ghr;
    int ins_tag = ins_addr;
    ADDRINT pred_target;
    bool miss = true;

    for (int i = 0; i < BTBWIDTH; i++)
    {
        if (btb[btb_index][i]->getTag() == ins_tag) //BTB Hit
        {
            btb_hits++;
            miss = false;
            pred_target = btb[btb_index][i]->getTarget();
            btb[btb_index][i]->setTarget(target_ins_addr);
            btb[btb_index][i]->setLruNum(--lru_state[btb_index]);
            break;
        }
    }

    if (miss)
    {
        pred_target = ins_addr + ins_size;
    }

    if (pred_target == target_ins_addr)
    {
        correct_branches++;
    }
    if (miss)
    {
        int lru_a = btb[btb_index][0]->getLruNum();
        int lru_b = btb[btb_index][1]->getLruNum();
        int lru_c = btb[btb_index][2]->getLruNum();
        int lru_d = btb[btb_index][3]->getLruNum();


        // Update the entry with maximum lruNum (least recently used)
        if (lru_a > max(lru_b, max(lru_c, lru_d)))
        {
            btb[btb_index][0]->update(target_ins_addr, --lru_state[btb_index], ins_tag, 1);
        }
        else if (lru_b > max(lru_c, max(lru_a, lru_d)))
        {
            btb[btb_index][1]->update(target_ins_addr, --lru_state[btb_index], ins_tag, 1);
        }
        else if(lru_c > max(lru_b, max(lru_a, lru_d)))
        {
            btb[btb_index][2]->update(target_ins_addr, --lru_state[btb_index], ins_tag, 1);
        }
        else
        {
            btb[btb_index][3]->update(target_ins_addr, --lru_state[btb_index], ins_tag, 1);
        }
    }
}

void UpdateGHR(BOOL branch_pred)
{
    if(branch_pred)
    {
        ghr = (ghr&(1<<(GHRWIDTH-1)))?((ghr-(1<<(GHRWIDTH-1)))<<1)+1:(ghr<<1)+1;  
    }
    else
    {
        ghr = (ghr&(1<<(GHRWIDTH-1)))?((ghr-(1<<(GHRWIDTH-1)))<<1):(ghr<<1);  
    }
}


ADDRINT Terminate(void) // Check whether we have instrumented BILLION instructions successfully
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

    OutFile << "Total BTB Accesses: " << btb_accesses<< endl;
    OutFile << "Number of BTB Misses: " << btb_accesses-btb_hits<< endl;
    OutFile << "BTB Miss rate " << (double)(btb_accesses-btb_hits)* 100 / btb_accesses << endl;

    OutFile << "Number of Correct Target Predictions: " << correct_branches << endl;
    OutFile << "Branch Target Prediction Accuracy (%): " << (double)(correct_branches)* 100 / btb_accesses << endl;

    OutFile.close();
    // Results etc
    exit(0);
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    // Instrumentation routine
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

    // MyExitRoutine() is called only when the last call returns a non-zero value.
    INS_InsertThenCall(ins, IPOINT_BEFORE, MyExitRoutine, IARG_END);

    if (INS_IsIndirectControlFlow(ins))
    {

        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_Category(ins), IARG_UINT32, INS_Size(ins), IARG_END);
    }

    if(INS_IsControlFlow(ins))  //Update the global history for conditional branches
    {
        
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)UpdateGHR, IARG_BRANCH_TAKEN, IARG_END);

    }

    INS_InsertCall(ins, IPOINT_BEFORE, InsCount, IARG_END);
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
                            "o", "inscount.out", "specify output file name");
KNOB<string> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool",
                             "f", "0", "specify fast forward amount");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
    // Do an exit system call to exit the application.
    // As we are calling the exit system call PIN would not be able to instrument application end.
    // Because of this, even if you are instrumenting the application end, the Fini function would not
    // be called. Thus you should report the statistics here, before doing the exit system call.
    exit(0);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl
         << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv))
        return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());
    fast_forward = stoll(KnobFastForward.Value()) * BILLION;
    // OutFile.setf(ios::showbase);
    // OutFile<<"fast forward by: "<<fast_forward<<endl;
    // OutFile.close();
    // Register Instruction to be called to instrument instructions

    for (int i = 0; i < BTBSIZE; i++)
    {
        for (int j = 0; j < BTBWIDTH; j++)
            btb[i][j] = new BTBEntry(0);
        lru_state[i] = LRUNUMINIT;
    }

    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
