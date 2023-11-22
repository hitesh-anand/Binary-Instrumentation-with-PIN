/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */
 
/*
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
 */
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <fstream>
using namespace std;
#include "pin.H"

#define GRANULARITY 32
#define BILLION 1000000000
typedef long long ll;

ll icount = 0;
ll fast_forward = 0;
FILE* trace;

ofstream OutFile;

set<ll> ins_st, mem_st;

void InsCount() {
    icount++;
}

ADDRINT FastForward (void) {
    return ((icount >= fast_forward) && (icount < fast_forward + BILLION));
}

ADDRINT Terminate(void)
{
    return (icount >= fast_forward + BILLION);
}

// Analysis routine to exit the application
void MyExitRoutine() {
	// Do an exit system call to exit the application.
	// As we are calling the exit system call PIN would not be able to instrument application end.
	// Because of this, even if you are instrumenting the application end, the Fini function would not
	// be called. Thus you should report the statistics here, before doing the exit system call.
	// Results etc
    OutFile.setf(ios::showbase);
    OutFile<<"Number of Unique Memory Addresses Accessed: "<<mem_st.size()<<"\n\n";
    int cnt=0;
    for(auto it:mem_st)
    {
        if(cnt==32)
            break;
        cnt++;
        OutFile<<it<<" ";
    }
    OutFile<<"\n\n";
    OutFile<<"Number of Unique Instruction Addresses Accessed: "<<ins_st.size()<<"\n\n";
    cnt=0;
    for(auto it:ins_st)
    {
        if(cnt==32)
            break;
        cnt++;
        OutFile<<it<<" ";
    }
    OutFile<<"\n\n";
    OutFile.close();
	exit(0);
}
 
// Insert the Memory Address acccessed
VOID MemoryTrace(VOID* sz, VOID* addr) { 
    mem_st.insert(((*(double*)addr)/GRANULARITY)*GRANULARITY);
}
 
// Insert the Instruction Adddress accessed
VOID InstructionTrace(VOID* sz, VOID* addr) { 
    ins_st.insert(((*(double*)addr)/GRANULARITY)*GRANULARITY);
}
 
// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP
    // prefixed instructions appear as predicated instructions in Pin.

    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) Terminate, IARG_END);

// MyExitRoutine() is called only when the last call returns a non-zero value.
    INS_InsertThenCall(ins, IPOINT_BEFORE, MyExitRoutine, IARG_END);

    UINT32 memOperands = INS_MemoryOperandCount(ins);
 
    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        double refSize = INS_MemoryOperandSize(ins, memOp);
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MemoryTrace, IARG_PTR, &refSize, IARG_MEMORYOP_EA, memOp,
                                     IARG_END);
        }
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MemoryTrace, IARG_PTR, &refSize, IARG_MEMORYOP_EA, memOp,
                                     IARG_END);
        }
    }

    int ins_size = INS_Size(ins);
    
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)InstructionTrace, IARG_PTR, &ins_size, IARG_INST_PTR, IARG_END);

    INS_InsertCall(ins, IPOINT_BEFORE, InsCount, IARG_END);
}
 
VOID Fini(INT32 code, VOID* v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}
 
/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
 
INT32 Usage()
{
    PIN_ERROR("This Pintool prints a trace of memory addresses\n" + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}
 
/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "inscount.out", "specify output file name");
KNOB<string> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool",
    "f", "0", "specify fast forward amount");
 
int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv)) return Usage();
    OutFile.open(KnobOutputFile.Value().c_str());
    fast_forward = stoll(KnobFastForward.Value())*BILLION;
    // trace = fopen("pinatrace.out", "w");
 
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
 
    // Never returns
    PIN_StartProgram();
 
    return 0;
}
