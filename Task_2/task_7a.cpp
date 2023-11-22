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
#include <unordered_map>
using namespace std;


ofstream OutFile;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
typedef long long ll;

#define BILLION 1000000000
#define PHTSIZE 512
#define BHTSIZE 1024
#define BHTWIDTH 9
#define GHRWIDTH 9
#define MAXCNTRSAG 4
#define MAXCNTRGAG 8
#define MAXCNTRGSHARE 8
#define BIMSIZE 512
#define MAXCNTRBIM 4

ll icount = 0;
ll fast_forward = 0;

UINT64 total_branches=0;
UINT64 correct_branches = 0;
UINT64 total_forward = 0;
UINT64 total_backward = 0;
UINT64 correct_forward = 0;
UINT64 correct_backward = 0;

UINT64 pht_index_gag = 0, pht_index_sag=0, pht_index_gshare=0, bht_index=0;
UINT64 mid_sag = (MAXCNTRSAG-1)>>1;
UINT64 mid_gag = (MAXCNTRGAG-1)>>1;
UINT64 mid_gshare = (MAXCNTRGSHARE-1)>>1;


unordered_map<ADDRINT, UINT64> ghr_to_pht, bht_to_pht, pc_to_bht, gshare_to_pht;
UINT64 pht_gag[PHTSIZE];
UINT64 pht_sag[PHTSIZE];
UINT64 pht_gshare[PHTSIZE];
UINT64 bht[BHTSIZE];
UINT64 ghr=0;


// Analysis routine to check fast-forward condition

void InsCount()     //Increment the instruction count
{
    icount++;
}

ADDRINT FastForward(void)   //Check whether we have reached the fast forward number of instructions
{
    return ((icount >= fast_forward) && (icount < fast_forward + BILLION));
}

void MyPredicatedAnalysis(ADDRINT ins_addr, ADDRINT target_ins_addr, BOOL branch_pred, INT32 ins_cat)
{
    total_branches++;

    if(pc_to_bht.find(ins_addr)==pc_to_bht.end())
    {
        bht_index = (bht_index + 1)%BHTSIZE;
        bht[bht_index] = 0;
        pc_to_bht[ins_addr] = bht_index;
    }
    UINT64 bht_ind = pc_to_bht[ins_addr];

    if(bht_to_pht.find(bht_ind)==bht_to_pht.end())
    {
        pht_index_sag = (pht_index_sag + 1)%PHTSIZE;
        pht_sag[pht_index_sag] = mid_sag;
        bht_to_pht[bht_ind] = pht_index_sag;
    }
    UINT64 pht_ind_sag = bht_to_pht[bht_ind];

    if(ghr_to_pht.find(ghr)==ghr_to_pht.end())
    {
        pht_index_gag = (pht_index_gag + 1)%PHTSIZE;
        pht_gag[pht_index_gag] = mid_gag;
        ghr_to_pht[ghr] = pht_index_gag;
    }
    UINT64 pht_ind_gag = ghr_to_pht[ghr];

    UINT64 gshare_ind = ghr^ins_addr;
    if(gshare_to_pht.find(gshare_ind)==gshare_to_pht.end())
    {
        pht_index_gshare = (pht_index_gshare + 1)%PHTSIZE;
        pht_gshare[pht_index_gshare] = mid_gshare;
        gshare_to_pht[gshare_ind] = pht_index_gshare;
    }
    UINT64 pht_ind_gshare = gshare_to_pht[gshare_ind];

    int taken_vote = 0, not_taken_vote = 0;

    if(pht_sag[pht_ind_sag]>mid_sag)
    {
        taken_vote++;
    }
    else{
        not_taken_vote++;
    }

    if(pht_gag[pht_ind_gag]>mid_gag){
        taken_vote++;
    }
    else{
        not_taken_vote++;
    }

    if(pht_gshare[pht_ind_gshare]>mid_gshare){
        taken_vote++;
    }
    else{
        not_taken_vote++;
    }

    if((taken_vote>not_taken_vote && branch_pred) || (taken_vote<not_taken_vote && !branch_pred))
    {
        correct_branches++;
        if(ins_cat == XED_CATEGORY_COND_BR)
        {
            if(target_ins_addr>ins_addr)
            {
                total_forward++;
                correct_forward++;
            }
            else
            {
                total_backward++;
                correct_backward++;
            }
        }
    }
    else if(ins_cat == XED_CATEGORY_COND_BR)
    {
        if(target_ins_addr>ins_addr)
            total_forward++;
        else
            total_backward++;
    }


    if(branch_pred)
    {
        pht_sag[pht_ind_sag] = min(pht_sag[pht_ind_sag]+1, (UINT64)MAXCNTRSAG-1);
        pht_gag[pht_ind_gag] = min(pht_sag[pht_ind_gag]+1, (UINT64)MAXCNTRGAG-1);
        pht_gshare[pht_ind_gshare] = min(pht_gshare[pht_ind_gshare]+1, (UINT64)MAXCNTRGSHARE-1);
        ghr = (ghr&(1<<(GHRWIDTH-1)))?((ghr-(1<<(GHRWIDTH-1)))<<1)+1:(ghr<<1)+1; 
        bht[bht_ind] = (bht[bht_ind]&(1<<(BHTWIDTH-1)))?((bht[bht_ind]-(1<<(BHTWIDTH-1)))<<1)+1:(bht[bht_ind]<<1)+1;     
    }
    else
    {
        pht_sag[pht_ind_sag] = max(pht_sag[pht_ind_sag]-1, (UINT64)0);
        pht_gag[pht_ind_gag] = max(pht_gag[pht_ind_gag]-1, (UINT64)0);
        pht_gshare[pht_ind_gshare] = max(pht_gshare[pht_ind_gshare]-1, (UINT64)0);
        ghr = (ghr&(1<<(GHRWIDTH-1)))?((ghr-(1<<(GHRWIDTH-1)))<<1):(ghr<<1);  
        bht[bht_ind] = (bht[bht_ind]&(1<<(BHTWIDTH-1)))?((bht[bht_ind]-(1<<(BHTWIDTH-1)))<<1):(bht[bht_ind]<<1);
    }
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

    OutFile<<"Total forward conditional branch instructions: "<<total_forward<<endl;
    OutFile<<"Correct forward conditional branch predictions: "<<correct_forward<<endl;
    OutFile<<"Prediction Accuracy: "<<(double)correct_forward*100/total_forward<<endl;

    OutFile<<"Total backward conditional branch instructions: "<<total_backward<<endl;
    OutFile<<"Correct forward conditional branch predictions: "<<correct_backward<<endl;
    OutFile<<"Prediction Accuracy: "<<(double)correct_backward*100/total_backward<<endl;

    OutFile<<"Total branch instructions: "<<total_branches<<endl;
    OutFile<<"Correct branch predictions: "<<correct_branches<<endl;
    OutFile<<"Prediction Accuracy: "<<(double)correct_branches*100/total_branches<<endl;

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


    if(INS_IsControlFlow(ins))
    {
        
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_Category(ins), IARG_END);
    }
    
    INS_InsertCall(ins, IPOINT_BEFORE, InsCount, IARG_END);
    // if(icount%BILLION==0)
    //     LOG("Done with "+to_string(icount)+" instructions"+"\n");
    // if(icount>=fast_forward && icount<fast_forward+BILLION)
    //     LOG("Number of instructions is now: "+to_string(icount)+" and this ind: "+to_string(ind)+"\n");
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
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
