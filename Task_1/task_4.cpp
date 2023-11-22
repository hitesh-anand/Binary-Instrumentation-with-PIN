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
#include <climits>
using namespace std;
#include "pin.H"

#define GRANULARITY 32
#define BILLION 1000000000
#define MAXSIZE 25
#define ACCESSVECTOR 1000
typedef UINT64 ll;

ll icount = 0;
ll fast_forward = 0;
FILE* trace;

ofstream OutFile;

set<ll> ins_st, mem_st;

vector<long double>  ins_len(MAXSIZE, 0);
vector<long double> op_count(MAXSIZE, 0);
vector<long double>  reg_read_ops(MAXSIZE, 0);
vector<long double>  reg_write_ops(MAXSIZE, 0);
vector<long double> mem_op_counts(MAXSIZE, 0);
vector<long double>  mem_read_counts(MAXSIZE, 0);
vector<long double> mem_write_counts(MAXSIZE, 0);
vector<long double> mem_accesses(ACCESSVECTOR, 0);

long double icount_pred=0;


INT32 max_imm=0, min_imm = 0;

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

    //Task a
    OutFile<<"Distribution of Instruction Lengths: "<<endl;
    for(int i=0; i<MAXSIZE; i++)
    {
        OutFile<<"Number of Instructions with length "<<i<<": "<<ins_len[i]<<endl;
    }
    OutFile<<"\n\n";

    //Task b op_count
    OutFile<<"Distribution of Number of Operands in an instruction: "<<endl;
    for(int i=0; i<MAXSIZE; i++)
    {
        OutFile<<"Number of Instructions with operand count "<<i<<": "<<op_count[i]<<endl;
    }
    OutFile<<"\n\n";   

    //Task c reg_read_ops
    OutFile<<"Distribution of Number of Register Read Operands in an instruction: "<<endl;
    for(int i=0; i<MAXSIZE; i++)
    {
        OutFile<<"Number of Instructions with Register Read operand count "<<i<<": "<<reg_read_ops[i]<<endl;
    }
    OutFile<<"\n\n";  

    //Task d reg_write_ops
    OutFile<<"Distribution of Number of Register Write Operands in an instruction: "<<endl;
    for(int i=0; i<MAXSIZE; i++)
    {
        OutFile<<"Number of Instructions with Register Write operand count "<<i<<": "<<reg_write_ops[i]<<endl;
    }
    OutFile<<"\n\n";  

    //Task e mem_op_counts
    OutFile<<"Distribution of Number of Memory Operands in an instruction: "<<endl;
    for(int i=0; i<MAXSIZE; i++)
    {
        OutFile<<"Number of Instructions with Memory operand count "<<i<<": "<<mem_op_counts[i]<<endl;
    }
    OutFile<<"\n\n";  

    //Task f mem_read_counts
    OutFile<<"Distribution of Number of Memory Read Operands in an instruction: "<<endl;
    for(int i=0; i<MAXSIZE; i++)
    {
        OutFile<<"Number of Instructions with Memory Read operand count "<<i<<": "<<mem_read_counts[i]<<endl;
    }
    OutFile<<"\n\n";  

    //Task g mem_write_counts
    OutFile<<"Distribution of Number of Memory Write Operands in an instruction: "<<endl;
    for(int i=0; i<MAXSIZE; i++)
    {
        OutFile<<"Number of Instructions with Memory Write operand count "<<i<<": "<<mem_write_counts[i]<<endl;
    }
    OutFile<<"\n\n";  

    long double max_mem_access = 0, total_mem_access=0;
    for(int i=1; i<ACCESSVECTOR; i++)
    {
        if(mem_accesses[i]>0)
            max_mem_access = i;
        total_mem_access += i*mem_accesses[i];
    }

    OutFile<<"Maximum size (in Bytes) of memory access across all instructions: "<<max_mem_access<<endl;
    OutFile<<"Average size (in Bytes) of memory access across all instructions: "<<total_mem_access/icount_pred<<endl;

    OutFile<<"Maximum intermediate value across all instructions: "<<max_imm<<endl;
    OutFile<<"Minimum intermediate value across all instructions: "<<min_imm<<endl;

    OutFile<<"\n\n";

    OutFile.close();
	exit(0);
}
 
//Task a 
VOID Ins_lengths(void* ptr) {
    (*((long double*)ptr))++;
}

//Task b
VOID Op_counts(void* ptr) {
    (*((long double*)ptr))++;
}

//Task c
VOID Regr_ops(void* ptr) {
    (*((long double*)ptr))++;
}

//Task d
VOID Regw_ops(void* ptr) {
    (*((long double*)ptr))++;
}

//Task e
VOID MemOp_counts(void* ptr) {
    (*((long double*)ptr))++;
}

//Task f
VOID MemRead_counts(void* ptr) {
    (*((long double*)ptr))++;
}

//Task g
VOID MemWrite_counts(void* ptr) {
    (*((long double*)ptr))++;
}

//Task h
VOID Mem_access(void* ptr)
{
    (*((long double*)ptr))++;
    icount_pred++;
}

//Task i
VOID Ins_Intermediate(INT32* ptr)
{
    INT32 val = *ptr;
    max_imm = max(max_imm, val);
    min_imm = min(min_imm, val);
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

    // Task (a)
    long double ins_sz = INS_Size(ins);

    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Ins_lengths, IARG_PTR, &(ins_len[ins_sz]), IARG_END);

    // Task (b)
    long double opcount = INS_OperandCount(ins);
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Op_counts, IARG_PTR, &(op_count[opcount]), IARG_END);  

    // Task (c)
    long double regr_op_count = INS_MaxNumRRegs(ins);
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Regr_ops, IARG_PTR, &(reg_read_ops[regr_op_count]), IARG_END); 

    // Task (d)
    long double regw_op_count = INS_MaxNumWRegs(ins);
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Regw_ops, IARG_PTR, &(reg_write_ops[regw_op_count]), IARG_END);   



    UINT32 memOperands = INS_MemoryOperandCount(ins);
    long double memOp_count=0, memRead_count=0, memWrite_count=0, mem_access=0;
 
    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        UINT64 refSize = INS_MemoryOperandSize(ins, memOp);
        // Task (i)
        if(INS_OperandIsImmediate(ins, memOp))
        {
            INT32 val = INS_OperandImmediate(ins, memOp);
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem_access, IARG_ADDRINT, &val, IARG_END);  
        }
        mem_access += refSize;
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            memOp_count++;
            memRead_count++;
        }
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            memOp_count++;
            memWrite_count++;
        }
    }

    // Task (e)
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MemOp_counts, IARG_PTR, &(mem_op_counts[memOp_count]), IARG_END);  

    // Task (f)
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MemRead_counts, IARG_PTR, &(mem_read_counts[memRead_count]), IARG_END);  

    // Task (g)
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MemWrite_counts, IARG_PTR, &(mem_write_counts[memWrite_count]), IARG_END);  

    // Task (h)
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Mem_access, IARG_PTR, &(mem_accesses[mem_access]), IARG_END);  



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
