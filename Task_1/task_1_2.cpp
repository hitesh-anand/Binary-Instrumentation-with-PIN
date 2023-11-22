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

#define NOP 0
#define DIRECT_CALL 1
#define INDIRECT_CALL 2
#define RET 3
#define UNCOND_BR 4
#define COND_BR 5
#define LOGICAL 6
#define ROTATE 7
#define SHIFT 7
#define FLAGOP 8
#define AVX 9
#define AVX2 9
#define AVX2GATHER 9
#define AVX512 9
#define CMOV 10
#define MMX 11
#define SSE 11
#define SYSCALL 12
#define X87_ALU 13
#define LOAD 14
#define STORE 15
#define OTHER 16

#define TOTAL OTHER + 1

#define BILLION 1000000000

ll icount = 0;
ll fast_forward = 0;

double ins_counts[TOTAL + 4];
double cycle_counts[TOTAL + 4];
map<int, string> mp;

// Analysis routine to check fast-forward condition

void InsCount()     //Increment the instruction count
{
    icount++;
}

ADDRINT FastForward(void)   //Check whether we have reached the fast forward number of instructions
{
    return ((icount >= fast_forward) && (icount < fast_forward + BILLION));
}

void MyPredicatedAnalysis(void *cnt)    //Increment the value of the passed pointer
{
    double *cntr = (double *)cnt;
    (*cntr)++;
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

    mp[0] = "NOP";
    mp[1] = "DIRECT_CALL";
    mp[2] = "INDIRECT_CALL";
    mp[3] = "RET";
    mp[4] = "UNCOND_BR";
    mp[5] = "COND_BR";
    mp[6] = "LOGICAL";
    mp[7] = "ROTATE and SHIFT";
    mp[8] = "FLAGOP";
    mp[9] = "Vector";
    mp[10] = "CMOV";
    mp[11] = "MMX and SSE";
    mp[12] = "SYSCALL";
    mp[13] = "X87_ALU";
    mp[14] = "LOAD";
    mp[15] = "STORE";
    mp[16] = "OTHER";

    // LOG("Entered the Exit Routine with TOTAL: "+to_string(TOTAL)+"\n");
    // for(int i=0; i<TOTAL; i++)
    // {
    //     LOG("Number of instructions of type ["+to_string(i)+"] are: "+to_string(ins_counts[i])+"\n");
    // }
    OutFile.setf(ios::showbase);

    //Computation of CPI
    double total_cnt = 0, weighted_cycles = 0;
    for (int is = 0; is < TOTAL; is++)
    {
        total_cnt += ins_counts[is];
        weighted_cycles += ins_counts[is] * cycle_counts[is];
    }

    OutFile << "Total Count: " << total_cnt << endl;
    for (int is = 0; is < TOTAL; is++)
    {
        double pcntage = ((ins_counts[is]) / total_cnt) * 100;
        OutFile << "Number of " << mp[is] << " instructions: " << ins_counts[is] << " contributing a total of " << pcntage << "%" << endl;
    }
    OutFile << "CPI for this application: " << weighted_cycles / total_cnt << endl;
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

    // FastForward() is called for every instruction executed

    // MyPredicatedAnalysis() is called only when the last FastForward() returns a non-zero value.
    int ind = OTHER;

    UINT32 memOperands = INS_MemoryOperandCount(ins);
    double load_cnt = 0, store_cnt = 0;

    // Iterate over each memory operand of the instruction.
    if (memOperands > 0)
    {
        // LOG("Found mem Ops with #operands: "+to_string(memOperands)+"\n");
        for (UINT32 memOp = 0; memOp < memOperands; memOp++)
        {
            double refSize = (double)INS_MemoryOperandSize(ins, memOp);
            if (INS_MemoryOperandIsRead(ins, memOp))
            {
                load_cnt += ceil(refSize / 4);
            }
            if (INS_MemoryOperandIsWritten(ins, memOp))
            {
                store_cnt += ceil(refSize / 4);
            }
        }
    }

    // if(load_cnt || store_cnt)
    // {
    //     LOG("load_cnt is: "+to_string(load_cnt)+" and store_cnt : "+to_string(store_cnt)+"\n");
    // }

    if (icount >= fast_forward && icount < fast_forward + 1000000)
    {
        LOG("We are inside "+to_string(icount)+"\n");
        load_cnt = max((double)10, load_cnt);
        store_cnt = max((double)10, store_cnt);
        while (load_cnt--)
        {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis, IARG_PTR, &(ins_counts[LOAD]), IARG_END);

            INS_InsertCall(ins, IPOINT_BEFORE, InsCount, IARG_END);

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

            // MyExitRoutine() is called only when the last call returns a non-zero value.
            INS_InsertThenCall(ins, IPOINT_BEFORE, MyExitRoutine, IARG_END);
        }

        while (store_cnt--)
        {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis, IARG_PTR, &(ins_counts[STORE]), IARG_END);

            INS_InsertCall(ins, IPOINT_BEFORE, InsCount, IARG_END);

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

            // MyExitRoutine() is called only when the last call returns a non-zero value.
            INS_InsertThenCall(ins, IPOINT_BEFORE, MyExitRoutine, IARG_END);
        }

        if (INS_Category(ins) == XED_CATEGORY_NOP)
        {
            ind = NOP;
        }
        else if (INS_Category(ins) == XED_CATEGORY_CALL || INS_Category(ins) == XED_CATEGORY_CALL)
        {
            if (INS_IsDirectCall(ins))
            {
                ind = DIRECT_CALL;
            }
            else
            {
                ind = INDIRECT_CALL;
            }
        }
        else if (INS_Category(ins) == XED_CATEGORY_RET)
        {
            ind = RET;
        }
        else if (INS_Category(ins) == XED_CATEGORY_UNCOND_BR)
        {
            ind = UNCOND_BR;
        }
        else if (INS_Category(ins) == XED_CATEGORY_COND_BR)
        {
            ind = COND_BR;
        }
        else if (INS_Category(ins) == XED_CATEGORY_LOGICAL)
        {
            ind = LOGICAL;
        }
        else if (INS_Category(ins) == XED_CATEGORY_ROTATE || INS_Category(ins) == XED_CATEGORY_SHIFT)
        {
            ind = ROTATE;
        }
        else if (INS_Category(ins) == XED_CATEGORY_FLAGOP)
        {
            ind = FLAGOP;
        }
        else if (INS_Category(ins) == XED_CATEGORY_AVX || INS_Category(ins) == XED_CATEGORY_AVX2 || INS_Category(ins) == XED_CATEGORY_AVX2GATHER || INS_Category(ins) == XED_CATEGORY_AVX512)
        {
            ind = AVX;
        }
        else if (INS_Category(ins) == XED_CATEGORY_CMOV)
        {
            ind = CMOV;
        }
        else if (INS_Category(ins) == XED_CATEGORY_MMX || INS_Category(ins) == XED_CATEGORY_SSE)
        {
            ind = MMX;
        }
        else if (INS_Category(ins) == XED_CATEGORY_SYSCALL)
        {
            ind = SYSCALL;
        }
        else if (INS_Category(ins) == XED_CATEGORY_X87_ALU)
        {
            ind = X87_ALU;
        }
    }

    // OutFile.setf(ios::showbase);
    // OutFile<<"ind : "<<ind<<endl;
    // OutFile.close();
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MyPredicatedAnalysis, IARG_PTR, &(ins_counts[ind]), IARG_END);

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

    for (int i = 0; i < TOTAL; i++)
        ins_counts[i] = 0;

    for (int i = 0; i < TOTAL; i++)
    {
        if (i == LOAD || i == STORE)
            cycle_counts[i] = 70;
        else
            cycle_counts[i] = 1;
    }

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
