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
#define MAXCNTR2 4

#define BHTSIZE 1024
#define BHTWIDTH 9
#define MAXCNTR3 4

#define GHRWIDTH 9
#define MAXCNTR4 8

ll icount = 0;
ll fast_forward = 0;

UINT64 total_branches_1=0;
UINT64 correct_branches_1 = 0;
UINT64 total_forward_1 = 0;
UINT64 total_backward_1 = 0;
UINT64 correct_forward_1 = 0;
UINT64 correct_backward_1 = 0;

//////////////////////////////

UINT64 total_branches_2=0;
UINT64 correct_branches_2 = 0;
UINT64 total_forward_2 = 0;
UINT64 total_backward_2 = 0;
UINT64 correct_forward_2 = 0;
UINT64 correct_backward_2 = 0;

UINT64 pht_index_2 = 0;
const UINT64 mid_2 = (MAXCNTR2-1)>>1;

unordered_map<ADDRINT, UINT64> pc_hash_2;
UINT64 pht_2[PHTSIZE];

///////////////////////////////////

UINT64 total_branches_3 = 0;
UINT64 correct_branches_3 = 0;
UINT64 total_forward_3 = 0;
UINT64 total_backward_3 = 0;
UINT64 correct_forward_3 = 0;
UINT64 correct_backward_3 = 0;

UINT64 pht_index_3 = 0, bht_index_3=0;
const UINT64 mid_3 = (MAXCNTR3-1)>>1;

unordered_map<ADDRINT, UINT64> pc_to_bht_3, bht_to_pht_3;
UINT64 pht_3[PHTSIZE];
UINT64 bht_3[BHTSIZE];
///////////////////////////////////////

UINT64 total_branches_4=0;
UINT64 correct_branches_4 = 0;
UINT64 total_forward_4 = 0;
UINT64 total_backward_4 = 0;
UINT64 correct_forward_4 = 0;
UINT64 correct_backward_4 = 0;

UINT64 pht_index_4 = 0;
const UINT64 mid_4 = (MAXCNTR4-1)>>1;

unordered_map<ADDRINT, UINT64> ghr_to_pht_4;
UINT64 pht_4[PHTSIZE];
UINT64 ghr_4=0;

///////////////////////////////////////

UINT64 total_branches_5=0;
UINT64 correct_branches_5 = 0;
UINT64 total_forward_5 = 0;
UINT64 total_backward_5 = 0;
UINT64 correct_forward_5 = 0;
UINT64 correct_backward_5 = 0;

UINT64 pht_index_5 = 0;
const UINT64 mid_5 = (MAXCNTR4-1)>>1;

unordered_map<ADDRINT, UINT64> ghr_to_pht_5;
UINT64 pht_5[PHTSIZE];
UINT64 ghr_5=0;

///////////////////////////////////////

// Analysis routine to check fast-forward condition


void InsCount()     //Increment the instruction count
{
    icount++;
}

ADDRINT FastForward(void)   //Check whether we have reached the fast forward number of instructions
{
    return ((icount >= fast_forward) && (icount < fast_forward + BILLION));
}

void MyPredicatedAnalysis_1(ADDRINT ins_addr, ADDRINT target_ins_addr, BOOL branch_pred, INT32 ins_cat)    
{
    total_branches_1++;
    if((branch_pred && target_ins_addr<ins_addr) || (!branch_pred && target_ins_addr>ins_addr))
    {
        correct_branches_1++;
        if(ins_cat == XED_CATEGORY_COND_BR)
        {
            if(target_ins_addr>ins_addr)
            {
                total_forward_1++;
                correct_forward_1++;
            }
            else
            {
                total_backward_1++;
                correct_backward_1++;
            }
        }
    }
    else if(ins_cat == XED_CATEGORY_COND_BR)
    {
        if(target_ins_addr>ins_addr)
            total_forward_1++;
        else
            total_backward_1++;
    }
}

void MyPredicatedAnalysis_2(ADDRINT ins_addr, ADDRINT target_ins_addr, BOOL branch_pred, INT32 ins_cat)    
{
    total_branches_2++;
    if(pc_hash_2.find(ins_addr)==pc_hash_2.end())
    {
        pht_index_2 = (pht_index_2 + 1)%PHTSIZE;
        pht_2[pht_index_2] = mid_2;
        pc_hash_2[ins_addr] = pht_index_2;
    }
    UINT64 ind = pc_hash_2[ins_addr];
    if((pht_2[ind]>mid_2 && branch_pred) || (pht_2[ind]<=mid_2 && !branch_pred))
    {
        correct_branches_2++;
        if(ins_cat == XED_CATEGORY_COND_BR)
        {
            if(target_ins_addr>ins_addr)
            {
                total_forward_2++;
                correct_forward_2++;
            }
            else
            {
                total_backward_2++;
                correct_backward_2++;
            }
        }
    }
    else if(ins_cat == XED_CATEGORY_COND_BR)
    {
        if(target_ins_addr>ins_addr)
            total_forward_2++;
        else
            total_backward_2++;
    }
    if(branch_pred)
        pht_2[ind] = min(pht_2[ind]+1, (UINT64)MAXCNTR2-1);
    else
        pht_2[ind] = max((UINT64)0, pht_2[ind]-1);
}

void MyPredicatedAnalysis_3(ADDRINT ins_addr, ADDRINT target_ins_addr, BOOL branch_pred, INT32 ins_cat)    
{
    total_branches_3++;
    if(pc_to_bht_3.find(ins_addr)==pc_to_bht_3.end())
    {
        bht_index_3 = (bht_index_3 + 1)%BHTSIZE;
        bht_3[bht_index_3] = 0;
        pc_to_bht_3[ins_addr] = bht_index_3;
    }
    UINT64 bht_ind = pc_to_bht_3[ins_addr];
    if(bht_to_pht_3.find(bht_ind)==bht_to_pht_3.end())
    {
        pht_index_3 = (pht_index_3 + 1)%PHTSIZE;
        pht_3[pht_index_3] = mid_3;
        bht_to_pht_3[bht_ind] = pht_index_3;
    }
    UINT64 pht_ind = bht_to_pht_3[bht_ind];
    if((pht_3[pht_ind]>mid_3 && branch_pred) || (pht_3[pht_ind]<=mid_3 && !branch_pred))
    {
        correct_branches_3++;
        if(ins_cat == XED_CATEGORY_COND_BR)
        {
            if(target_ins_addr>ins_addr)
            {
                total_forward_3++;
                correct_forward_3++;
            }
            else
            {
                total_backward_3++;
                correct_backward_3++;
            }
        }
    }
    else if(ins_cat == XED_CATEGORY_COND_BR)
    {
        if(target_ins_addr>ins_addr)
            total_forward_3++;
        else
            total_backward_3++;
    }

    if(branch_pred)
    {
        pht_3[pht_ind] = min(pht_3[pht_ind]+1, (UINT64)MAXCNTR3-1);
        bht_3[bht_ind] = (bht_3[bht_ind]&(1<<(BHTWIDTH-1)))?((bht_3[bht_ind]-(1<<(BHTWIDTH-1)))<<1)+1:(bht_3[bht_ind]<<1)+1;
    }
    else
    {
        pht_3[pht_ind] = max(pht_3[pht_ind]-1, (UINT64)0);
        bht_3[bht_ind] = (bht_3[bht_ind]&(1<<(BHTWIDTH-1)))?((bht_3[bht_ind]-(1<<(BHTWIDTH-1)))<<1):(bht_3[bht_ind]<<1);
    }
}

void MyPredicatedAnalysis_4(ADDRINT ins_addr, ADDRINT target_ins_addr, BOOL branch_pred, INT32 ins_cat)    
{
    total_branches_4++;
    UINT64 bht_ind = ghr_4;
    if(ghr_to_pht_4.find(bht_ind)==ghr_to_pht_4.end())
    {
        pht_index_4 = (pht_index_4 + 1)%PHTSIZE;
        pht_4[pht_index_4] = mid_4;
        ghr_to_pht_4[bht_ind] = pht_index_4;
    }
    UINT64 pht_ind = ghr_to_pht_4[bht_ind];
    if((pht_4[pht_ind]>mid_4 && branch_pred) || (pht_4[pht_ind]<=mid_4 && !branch_pred))
    {
        correct_branches_4++;
        if(ins_cat == XED_CATEGORY_COND_BR)
        {
            if(target_ins_addr>ins_addr)
            {
                total_forward_4++;
                correct_forward_4++;
            }
            else
            {
                total_backward_4++;
                correct_backward_4++;
            }
        }
    }
    else if(ins_cat == XED_CATEGORY_COND_BR)
    {
        if(target_ins_addr>ins_addr)
            total_forward_4++;
        else
            total_backward_4++;
    }

    if(branch_pred) 
    {
        pht_4[pht_ind] = min(pht_4[pht_ind]+1, (UINT64)MAXCNTR4-1);
        ghr_4 = (ghr_4&(1<<(GHRWIDTH-1)))?((ghr_4-(1<<(GHRWIDTH-1)))<<1)+1:(ghr_4<<1)+1;
    }
    else
    {
        pht_4[pht_ind] = max(pht_4[pht_ind]-1, (UINT64)0);
        ghr_4 = (ghr_4&(1<<(GHRWIDTH-1)))?((ghr_4-(1<<(GHRWIDTH-1)))<<1):(ghr_4<<1);
    }
}

void MyPredicatedAnalysis_5(ADDRINT ins_addr, ADDRINT target_ins_addr, BOOL branch_pred, INT32 ins_cat)    
{
    total_branches_5++;
    UINT64 bht_ind = ghr_5^ins_addr;
    if(ghr_to_pht_5.find(bht_ind)==ghr_to_pht_5.end())
    {
        pht_index_5 = (pht_index_5 + 1)%PHTSIZE;
        pht_5[pht_index_5] = mid_5;
        ghr_to_pht_5[bht_ind] = pht_index_5;
    }
    UINT64 pht_ind = ghr_to_pht_5[bht_ind];
    if((pht_5[pht_ind]>mid_5&& branch_pred) || (pht_5[pht_ind]<=mid_5 && !branch_pred))
    {
        correct_branches_5++;
        if(ins_cat == XED_CATEGORY_COND_BR)
        {
            if(target_ins_addr>ins_addr)
            {
                total_forward_5++;
                correct_forward_5++;
            }
            else
            {
                total_backward_5++;
                correct_backward_5++;
            }
        }
    }
    else if(ins_cat == XED_CATEGORY_COND_BR)
    {
        if(target_ins_addr>ins_addr)
            total_forward_5++;
        else
            total_backward_5++;
    }

    if(branch_pred) 
    {
        pht_5[pht_ind] = min(pht_5[pht_ind]+1, (UINT64)MAXCNTR4-1);
        ghr_5 = (ghr_5&(1<<(GHRWIDTH-1)))?((ghr_5-(1<<(GHRWIDTH-1)))<<1)+1:(ghr_5<<1)+1;
    }
    else
    {
        pht_5[pht_ind] = max(pht_5[pht_ind]-1, (UINT64)0);
        ghr_5 = (ghr_5&(1<<(GHRWIDTH-1)))?((ghr_5-(1<<(GHRWIDTH-1)))<<1):(ghr_5<<1);
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

    OutFile<<"[1] Total forward conditional branch instructions: "<<total_forward_1<<endl;
    OutFile<<"[1] Correct forward conditional branch predictions: "<<correct_forward_1<<endl;
    OutFile<<"[1] Prediction Accuracy: "<<(double)correct_forward_1*100/total_forward_1<<endl;

    OutFile<<"[1] Total backward conditional branch instructions: "<<total_backward_1<<endl;
    OutFile<<"[1] Correct forward conditional branch predictions: "<<correct_backward_1<<endl;
    OutFile<<"[1] Prediction Accuracy: "<<(double)correct_backward_1*100/total_backward_1<<endl;

    OutFile<<"[1] Total branch instructions: "<<total_branches_1<<endl;
    OutFile<<"[1] Correct branch predictions: "<<correct_branches_1<<endl;
    OutFile<<"[1] Prediction Accuracy: "<<(double)correct_branches_1*100/total_branches_1<<endl;
    cout<<"\n\n";

    ///////////////////////////////////////////////////////////////

    OutFile<<"[2] Total forward conditional branch instructions: "<<total_forward_2<<endl;
    OutFile<<"[2] Correct forward conditional branch predictions: "<<correct_forward_2<<endl;
    OutFile<<"[2] Prediction Accuracy: "<<(double)correct_forward_2*100/total_forward_2<<endl;

    OutFile<<"[2] Total backward conditional branch instructions: "<<total_backward_2<<endl;
    OutFile<<"[2] Correct forward conditional branch predictions: "<<correct_backward_2<<endl;
    OutFile<<"[2] Prediction Accuracy: "<<(double)correct_backward_2*100/total_backward_2<<endl;

    OutFile<<"[2] Total branch instructions: "<<total_branches_2<<endl;
    OutFile<<"[2] Correct branch predictions: "<<correct_branches_2<<endl;
    OutFile<<"[2] Prediction Accuracy: "<<(double)correct_branches_2*100/total_branches_2<<endl;
    cout<<"\n\n";


    OutFile<<"[3] Total forward conditional branch instructions: "<<total_forward_3<<endl;
    OutFile<<"[3] Correct forward conditional branch predictions: "<<correct_forward_3<<endl;
    OutFile<<"[3] Prediction Accuracy: "<<(double)correct_forward_3*100/total_forward_3<<endl;

    OutFile<<"[3] Total backward conditional branch instructions: "<<total_backward_3<<endl;
    OutFile<<"[3] Correct forward conditional branch predictions: "<<correct_backward_3<<endl;
    OutFile<<"[3] Prediction Accuracy: "<<(double)correct_backward_3*100/total_backward_3<<endl;

    OutFile<<"[3] Total branch instructions: "<<total_branches_3<<endl;
    OutFile<<"[3] Correct branch predictions: "<<correct_branches_3<<endl;
    OutFile<<"[3] Prediction Accuracy: "<<(double)correct_branches_3*100/total_branches_3<<endl;
    cout<<"\n\n";


    OutFile<<"[4] Total forward conditional branch instructions: "<<total_forward_4<<endl;
    OutFile<<"[4] Correct forward conditional branch predictions: "<<correct_forward_4<<endl;
    OutFile<<"[4] Prediction Accuracy: "<<(double)correct_forward_4*100/total_forward_4<<endl;

    OutFile<<"[4] Total backward conditional branch instructions: "<<total_backward_4<<endl;
    OutFile<<"[4] Correct forward conditional branch predictions: "<<correct_backward_4<<endl;
    OutFile<<"[4] Prediction Accuracy: "<<(double)correct_backward_4*100/total_backward_4<<endl;

    OutFile<<"[4] Total branch instructions: "<<total_branches_4<<endl;
    OutFile<<"[4] Correct branch predictions: "<<correct_branches_4<<endl;
    OutFile<<"[4] Prediction Accuracy: "<<(double)correct_branches_4*100/total_branches_4<<endl;
    cout<<"\n\n";


    OutFile<<"[5] Total forward conditional branch instructions: "<<total_forward_5<<endl;
    OutFile<<"[5] Correct forward conditional branch predictions: "<<correct_forward_5<<endl;
    OutFile<<"[5] Prediction Accuracy: "<<(double)correct_forward_2*100/total_forward_5<<endl;

    OutFile<<"[5] Total backward conditional branch instructions: "<<total_backward_5<<endl;
    OutFile<<"[5] Correct forward conditional branch predictions: "<<correct_backward_5<<endl;
    OutFile<<"[5] Prediction Accuracy: "<<(double)correct_backward_5*100/total_backward_5<<endl;

    OutFile<<"[5] Total branch instructions: "<<total_branches_5<<endl;
    OutFile<<"[5] Correct branch predictions: "<<correct_branches_5<<endl;
    OutFile<<"[5] Prediction Accuracy: "<<(double)correct_branches_5*100/total_branches_5<<endl;
    cout<<"\n\n";


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
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis_1, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_Category(ins) ,IARG_END);

        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis_2, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_Category(ins), IARG_END);

        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis_3, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_Category(ins), IARG_END);

        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis_4, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_Category(ins), IARG_END);

        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis_5, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_Category(ins), IARG_END);

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

    for(UINT64 i=0; i<PHTSIZE; i++)
    {
        pht_2[i] = 0;
        pht_3[i] = 0;
        pht_4[i] = 0;
        pht_5[i] = 0;
    }
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
