/////////////////////////////////////////////////////////////////////////
// $Id: instrument.cc 12655 2015-02-19 20:23:08Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2006-2015 Stanislav Shwartsman
//          Written by Stanislav Shwartsman [sshwarts at sourceforge net]
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#include <assert.h>

#include "bochs.h"
#include "cpu/cpu.h"
#include "disasm/disasm.h"
#include "bx_debug/debug.h"

// maximum size of an instruction
#define MAX_OPCODE_LENGTH 16

// maximum physical addresses an instruction can generate
#define MAX_DATA_ACCESSES 1024

// the instruction num of every dump log
//#define DUMP_INSTR_NUM 10000

// dump log file name
//#define DUMP_LOG_FILE_NAME "dump_log"

// the dump log file num
//#define DUMP_LOG_FILE_NUM 5

// Use this variable to turn on/off collection of instrumentation data
// If you are not using the debugger to turn this on/off, then possibly
// start this at 1 instead of 0.
#define BX_CPU_APIC(i) (&(BX_CPU(i)->lapic))

static bx_bool active = 1;

static disassembler bx_disassembler;

static struct instruction_t {
    bx_bool  ready;         // is current instruction ready to be printed
    unsigned opcode_length;
    Bit8u    opcode[MAX_OPCODE_LENGTH];
    bx_bool  is32, is64;
    unsigned num_data_accesses;
    unsigned num_data_accesses_phy;
    unsigned num_data_accesses_phy_rmw;
    struct {
        bx_address laddr;     // linear address
        bx_phy_address paddr; // physical address
        unsigned rw;          // BX_READ, BX_WRITE or BX_RW
        unsigned size;        // 1 .. 64
        unsigned memtype;
        //Bit8u* dataptr;
        Bit8u data[64];
    } data_access[MAX_DATA_ACCESSES];
    struct {
         bx_phy_address paddr; // physical address
         unsigned rw;          // BX_READ, BX_WRITE or BX_RW
         unsigned size;        // 1 .. 64
         unsigned memtype;
         unsigned why;
         //Bit8u* dataptr;
         Bit8u data[64];
     } data_access_phy[MAX_DATA_ACCESSES];
     struct {
          bx_phy_address paddr; // physical address
          unsigned rw;          // BX_READ, BX_WRITE or BX_RW
          unsigned size;        // 1 .. 64
          unsigned memtype;
          unsigned why;
          Bit8u* dataptr;
          //Bit8u data[64];
      } data_access_phy_rmw[MAX_DATA_ACCESSES];
    bx_bool is_branch;
    bx_bool is_taken;
    bx_address target_linear;
} *instruction;

static logfunctions *instrument_log = new logfunctions ();
#define LOG_THIS instrument_log->

FILE *dump_log = NULL;
const char* dump_log_dir = NULL;
char* dump_instr_num = new char[20];
char* dump_file_name = new char[20];
char* dump_file_num = new char[20];
char dump_log_file_name[200];
FILE* current_dump_log = NULL;
//FILE* last_dump_log = NULL;
int log_file_num = 0;
unsigned long long instr_cnt = 0;
int rep_flag = 0;
int reload_flag = 0;
int smm_exit_flag = 0;
int DUMP_INSTR_NUM = 0;
int DUMP_FILE_NUM = 0;
int smm_f = 0;
Bit64u last_rip = 0;
Bit64u rip = 0;
struct first_instr{
          unsigned int 	len; // physical address
          char				opcode[32];
          char				addr[9] ;
      }smm_first_instr;

#if BX_INSTRUMENTATION
void bx_dump_apic(unsigned cpu){

		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ID);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_VERSION);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TPR);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ARBITRATION_PRIORITY);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_PPR);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_EOI);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_RRD);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_LDR);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_DESTINATION_FORMAT);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_SPURIOUS_VECTOR);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ISR1);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ISR2);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ISR3);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ISR4);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ISR5);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ISR6);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ISR7);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ISR8);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TMR1);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TMR2);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TMR3);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TMR4);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TMR5);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TMR6);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TMR7);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TMR8);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_IRR1);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_IRR2);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_IRR3);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_IRR4);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_IRR5);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_IRR6);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_IRR7);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_IRR8);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ESR);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_LVT_CMCI);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ICR_LO);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_ICR_HI);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_LVT_TIMER);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_LVT_THERMAL);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_LVT_PERFMON);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_LVT_LINT0);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_LVT_LINT1);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_LVT_ERROR);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TIMER_INITIAL_COUNT);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TIMER_CURRENT_COUNT);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_TIMER_DIVIDE_CFG);
		BX_CPU_APIC(cpu)->read_aligned_dump(BX_LAPIC_SELF_IPI);

}

void bx_dump_msr(unsigned cpu){
    Bit64u val64;
    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_TSC,&val64))
    	dbg_dump_printf("MSR BX_MSR_TSC read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_APICBASE,&val64))
        dbg_dump_printf("MSR BX_MSR_APICBASE read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_TSC_ADJUST,&val64))
        dbg_dump_printf("MSR BX_MSR_TSC_ADJUST read failed!\n");

#if BX_CPU_LEVEL >= 6

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_SYSENTER_CS,&val64))
        dbg_dump_printf("MSR BX_MSR_SYSENTER_CS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_SYSENTER_ESP,&val64))
        dbg_dump_printf("MSR BX_MSR_SYSENTER_ESP read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_SYSENTER_EIP,&val64))
    	dbg_dump_printf("MSR BX_MSR_SYSENTER_EIP read failed!\n");

#endif

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_DEBUGCTLMSR,&val64))
        dbg_dump_printf("MSR BX_MSR_DEBUGCTLMSR read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_LASTBRANCHFROMIP,&val64))
            dbg_dump_printf("MSR BX_MSR_LASTBRANCHFROMIP read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_LASTBRANCHTOIP,&val64))
                dbg_dump_printf("MSR BX_MSR_LASTBRANCHTOIP read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_LASTINTOIP,&val64))
        	dbg_dump_printf("MSR BX_MSR_LASTINTOIP read failed!\n");

//###############################MTRR###########################

#if BX_CPU_LEVEL >= 6

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRCAP,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRCAP read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSBASE0,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSBASE0 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSMASK0,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSMASK0 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSBASE1,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSBASE1 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSMASK1,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSMASK1 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSBASE2,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRPHYSBASE2 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSMASK2,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRPHYSMASK2 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSBASE3,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRPHYSBASE3 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSMASK3,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSMASK3 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSBASE4,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSBASE4 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSMASK4,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSMASK4 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSBASE5,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSBASE5 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSMASK5,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSMASK5 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSBASE6,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSBASE6 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSMASK6,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSMASK6 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSBASE7,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSBASE7 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRPHYSMASK7,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRPHYSMASK7 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX64K_00000,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRFIX64K_00000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX16K_80000,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRFIX16K_80000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX16K_A0000,&val64))
    	dbg_dump_printf("MSR BX_MSR_MTRRFIX16K_A0000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX4K_C0000,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRFIX4K_C0000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX4K_C8000,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRFIX4K_C8000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX4K_D0000,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRFIX4K_D0000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX4K_D8000,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRFIX4K_D8000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX4K_E0000,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRFIX4K_E0000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX4K_E8000,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRFIX4K_E8000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX4K_F0000,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRFIX4K_F0000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRRFIX4K_F8000,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRRFIX4K_F8000 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PAT,&val64))
    	dbg_dump_printf("MSR BX_MSR_PAT read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MTRR_DEFTYPE,&val64))
        dbg_dump_printf("MSR BX_MSR_MTRR_DEFTYPE read failed!\n");

#endif

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_TSC_DEADLINE,&val64))
        dbg_dump_printf("MSR BX_MSR_TSC_DEADLINE read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_MAX_INDEX,&val64))
        dbg_dump_printf("MSR BX_MSR_MAX_INDEX read failed!\n");

#if BX_SUPPORT_PERFMON

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PMC0,&val64))
        dbg_dump_printf("MSR BX_MSR_PMC0 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PMC1,&val64))
        dbg_dump_printf("MSR BX_MSR_PMC1 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PMC2,&val64))
        dbg_dump_printf("MSR BX_MSR_PMC2 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PMC3,&val64))
        dbg_dump_printf("MSR BX_MSR_PMC3 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PMC4,&val64))
        dbg_dump_printf("MSR BX_MSR_PMC4 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PMC5,&val64))
        dbg_dump_printf("MSR BX_MSR_PMC5 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PMC6,&val64))
    	dbg_dump_printf("MSR BX_MSR_PMC6 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PMC7,&val64))
        dbg_dump_printf("MSR BX_MSR_PMC7 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERFEVTSEL0,&val64))
        dbg_dump_printf("MSR BX_MSR_PERFEVTSEL0 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERFEVTSEL1,&val64))
        dbg_dump_printf("MSR BX_MSR_PERFEVTSEL1 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERFEVTSEL2,&val64))
        dbg_dump_printf("MSR BX_MSR_PERFEVTSEL2 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERFEVTSEL3,&val64))
        dbg_dump_printf("MSR BX_MSR_PERFEVTSEL3 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERFEVTSEL4,&val64))
        dbg_dump_printf("MSR BX_MSR_PERFEVTSEL4 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERFEVTSEL5,&val64))
        dbg_dump_printf("MSR BX_MSR_PERFEVTSEL5 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERFEVTSEL6,&val64))
    	dbg_dump_printf("MSR BX_MSR_PERFEVTSEL6 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERFEVTSEL7,&val64))
        dbg_dump_printf("MSR BX_MSR_PERFEVTSEL7 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERF_FIXED_CTR0,&val64))
        dbg_dump_printf("MSR BX_MSR_PERF_FIXED_CTR0 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERF_FIXED_CTR1,&val64))
        dbg_dump_printf("MSR BX_MSR_PERF_FIXED_CTR1 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERF_FIXED_CTR1,&val64))
        dbg_dump_printf("MSR BX_MSR_PERF_FIXED_CTR1 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_FIXED_CTR_CTRL,&val64))
    	dbg_dump_printf("MSR BX_MSR_FIXED_CTR_CTRL read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_PERF_GLOBAL_CTRL,&val64))
        dbg_dump_printf("MSR BX_MSR_PERF_GLOBAL_CTRL read failed!\n");

#endif

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_EFER,&val64))
    	dbg_dump_printf("MSR BX_MSR_EFER read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_STAR,&val64))
        dbg_dump_printf("MSR BX_MSR_STAR read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_LSTAR,&val64))
        dbg_dump_printf("MSR BX_MSR_LSTAR read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_CSTAR,&val64))
        dbg_dump_printf("MSR BX_MSR_CSTAR read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_FMASK,&val64))
        dbg_dump_printf("MSR BX_MSR_FMASK read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_FSBASE,&val64))
    	dbg_dump_printf("MSR BX_MSR_FSBASE read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_GSBASE,&val64))
        dbg_dump_printf("MSR BX_MSR_GSBASE read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_KERNELGSBASE,&val64))
        dbg_dump_printf("MSR BX_MSR_KERNELGSBASE read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_TSC_AUX,&val64))
        dbg_dump_printf("MSR BX_MSR_TSC_AUX read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_SVM_VM_CR_MSR,&val64))
        dbg_dump_printf("MSR BX_SVM_VM_CR_MSR read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_SVM_IGNNE_MSR,&val64))
        dbg_dump_printf("MSR BX_SVM_IGNNE_MSR read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_SVM_SMM_CTL_MSR,&val64))
    	dbg_dump_printf("MSR BX_SVM_SMM_CTL_MSR read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_SVM_HSAVE_PA_MSR,&val64))
        dbg_dump_printf("MSR BX_SVM_HSAVE_PA_MSR read failed!\n");

#if BX_SUPPORT_VMX

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_BASIC,&val64))
    	dbg_dump_printf("MSR BX_MSR_VMX_BASIC read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_PINBASED_CTRLS,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_PINBASED_CTRLS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_PROCBASED_CTRLS,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_PROCBASED_CTRLS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_VMEXIT_CTRLS,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_VMEXIT_CTRLS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_VMENTRY_CTRLS,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_VMENTRY_CTRLS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_MISC,&val64))
    	dbg_dump_printf("MSR BX_MSR_VMX_MISC read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_CR0_FIXED0,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_CR0_FIXED0 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_CR0_FIXED1,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_CR0_FIXED1 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_CR4_FIXED0,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_CR4_FIXED0 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_CR4_FIXED1,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_CR4_FIXED1 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_VMCS_ENUM,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_VMCS_ENUM read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_PROCBASED_CTRLS2,&val64))
    	dbg_dump_printf("MSR BX_MSR_VMX_PROCBASED_CTRLS2 read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_EPT_VPID_CAP,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_EPT_VPID_CAP read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_TRUE_PINBASED_CTRLS,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_TRUE_PINBASED_CTRLS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_TRUE_PROCBASED_CTRLS,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_TRUE_PROCBASED_CTRLS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_TRUE_VMEXIT_CTRLS,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_TRUE_VMEXIT_CTRLS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_TRUE_VMENTRY_CTRLS,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_TRUE_VMENTRY_CTRLS read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_VMX_VMFUNC,&val64))
        dbg_dump_printf("MSR BX_MSR_VMX_VMFUNC read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_IA32_FEATURE_CONTROL,&val64))
    	dbg_dump_printf("MSR BX_MSR_IA32_FEATURE_CONTROL read failed!\n");

    if (!BX_CPU(cpu)->rdmsr_dump(BX_MSR_IA32_SMM_MONITOR_CTL,&val64))
        dbg_dump_printf("MSR BX_MSR_IA32_SMM_MONITOR_CTL read failed!\n");


#endif

}

void check_rep(const Bit8u *opcode){

	char rep[2];
	sprintf(rep,"%02x",opcode[0]);
	if (!strcmp(rep,"f3")||!strcmp(rep,"f2")){
		rep_flag ++;
	}
	else{
		rep_flag=0;
	}
}

void bx_instr_init_env(void) {}
void bx_instr_exit_env(void) {}

void bx_instr_initialize(unsigned cpu) {
    
    assert(cpu < BX_SMP_PROCESSORS);
    int iret = 0;
    if (instruction == NULL)
    	instruction = new struct instruction_t[BX_SMP_PROCESSORS];
    strcpy(smm_first_instr.addr,"00038000");
    smm_first_instr.len = 0x6;
    strcpy(smm_first_instr.opcode,"66bbfcfef0cf");
    memset(dump_log_file_name,0,200);
    bx_dbg_set_dump_events();
    dump_log_dir = bx_dbg_get_dump_log_dir();
    iret = access(dump_log_dir,0);
    if(iret =! 0){
        mkdir(dump_log_dir,0777);
    }
    bx_dbg_get_dump_log_info(dump_instr_num,dump_file_name,dump_file_num);
    //fprintf(stderr,"dump_instr_num %s\n",dump_instr_num);
    //fprintf(stderr,"dump_file_name %s\n",dump_file_name);
    //fprintf(stderr,"dump_file_num %s\n",dump_file_num);
    DUMP_INSTR_NUM = atoi(dump_instr_num);
    DUMP_FILE_NUM = atoi(dump_file_num);
    //fprintf(stderr,"DUMP_INSTR_NUM is %d and DUMP_FILE_NUM is %d",DUMP_INSTR_NUM,DUMP_FILE_NUM);
    //fprintf(stderr, "DIR is %s\n",dump_log_dir);
    sprintf(dump_log_file_name,"%s/%s_%d",dump_log_dir, dump_file_name, log_file_num); //DUMP_LOG_FILE_NAME
    //fprintf(stderr, "FILE is %s\n",dump_log_file_name);
    current_dump_log = bx_dbg_set_dump_log_handler(dump_log_file_name);
   // dbg_dump_printf("Initialize cpu %u\n", cpu);

}

void bx_instr_exit(unsigned cpu) {
	  instruction[cpu].ready = 0;
	  instruction[cpu].num_data_accesses = 0;
	  instruction[cpu].num_data_accesses_phy = 0;
	  instruction[cpu].num_data_accesses_phy_rmw = 0;
	  instruction[cpu].is_branch = 0;
}
void bx_instr_reset(unsigned cpu, unsigned type) {

  instruction[cpu].ready = 0;
  instruction[cpu].num_data_accesses = 0;
  instruction[cpu].num_data_accesses_phy = 0;
  instruction[cpu].num_data_accesses_phy_rmw= 0;
  instruction[cpu].is_branch = 0;

}
void bx_instr_hlt(unsigned cpu) {}
void bx_instr_mwait(unsigned cpu, bx_phy_address addr, unsigned len, Bit32u flags) {}

void bx_instr_debug_promt() {


}
void bx_instr_debug_cmd(const char *cmd) {

	dbg_dump_printf("Debug cmd %s\n",cmd);


}


void bx_instr_tlb_cntrl(unsigned cpu, unsigned what, bx_phy_address new_cr3) {}
void bx_instr_clflush(unsigned cpu, bx_address laddr, bx_phy_address paddr) {}
void bx_instr_cache_cntrl(unsigned cpu, unsigned what) {}
void bx_instr_prefetch_hint(unsigned cpu, unsigned what, unsigned seg, bx_address offset) {}


void bx_print_instruction(unsigned cpu, const instruction_t *i, int t=0)
{
  smm_f = bx_dbg_get_smm_flag();
  if (smm_f == 1){
	  dbg_dump_printf("SMM ENTER INSTR: %d ",instr_cnt);
	  dbg_dump_printf("rip: %08x_%08x\n", GET32H(last_rip), GET32L(last_rip));
	  dbg_dump_printf("LEN %d BYTES: %s [0x0000%s]\n",smm_first_instr.len,smm_first_instr.opcode,smm_first_instr.addr); //bochs bug, it lose the 1st instruction of smm handler,
	  // so we need add it
	  instr_cnt ++;
	  dbg_dump_printf("INSTR: %d\n", instr_cnt);
	  dbg_dump_printf("----------------------------------------------------------\n");
	  bx_dbg_set_smm_flag(0);
	  return;
  }
  char disasm_tbuf[512];        // buffer for instruction disassembly
  unsigned length = i->opcode_length, n, j,k;
  bx_disassembler.disasm(i->is32, i->is64, 0, 0, i->opcode, disasm_tbuf);
  //bool dump_reg = 0;
  last_rip = rip;
  if(length != 0)
  {
	check_rep(i->opcode);
	if (rep_flag<2){
		dbg_dump_printf("CPU %u: %s\n", cpu, disasm_tbuf);
	if(i->is_branch)
    {
    	dbg_dump_printf("BRANCH ");

      if(i->is_taken)
    	  dbg_dump_printf("TARGET " FMT_ADDRX " (TAKEN)\n", i->target_linear);
      else
    	  dbg_dump_printf("(NOT TAKEN)\n");
    }
	dbg_dump_printf("LEN %u BYTES: ",length);
	for(n=0;n < length;n++) dbg_dump_printf("%02x", i->opcode[n]);
  	dbg_dump_printf(" ");
    bx_dbg_disassemble_current_dump(cpu,1);
    dbg_dump_printf("INSTR: %d\n", instr_cnt);
	}
    bx_dbg_check_mini_dump(i->opcode,length,instr_cnt,&reload_flag,&smm_exit_flag);
    if(reload_flag){
    	instr_cnt++;
    	dbg_dump_printf("RELOAD INSTR: %d\n", instr_cnt);

    }

    for(n=0;n < i->num_data_accesses;n++)
    {

    	bx_dbg_lin_memory_access_dump(cpu, i->data_access[n].laddr, i->data_access[n].paddr, i->data_access[n].size, \
    		i->data_access[n].memtype, \
    		i->data_access[n].rw , (Bit8u*)(i->data_access[n].data));

    }

    for(j=0;j < i->num_data_accesses_phy;j++)
      {

    	bx_dbg_phy_memory_access_dump(cpu, i->data_access_phy[j].paddr, i->data_access_phy[j].size, \
    		i->data_access_phy[j].memtype, \
    		i->data_access_phy[j].rw , i->data_access_phy[j].why, (Bit8u*)i->data_access_phy[j].data);
      }

    for(k=0;k < i->num_data_accesses_phy_rmw;k++)
      {

    	bx_dbg_phy_memory_access_dump(cpu, i->data_access_phy_rmw[k].paddr, i->data_access_phy_rmw[k].size, \
    		i->data_access_phy_rmw[k].memtype, \
    		i->data_access_phy_rmw[k].rw , i->data_access_phy_rmw[k].why, i->data_access_phy_rmw[k].dataptr);
      }
    //dbg_dump_printf("\n");

   // if (rep_flag<2){

    //}
    if(smm_exit_flag){
    	dbg_dump_printf("SMM EXIT\n");// rsm instr
    }
    dbg_dump_printf("Next at t=%d\n", bx_pc_system.time_ticks());
	if(instr_cnt == 0){

		dbg_dump_printf("------------------major dump initial start---------------------------\n");
	    bx_dbg_info_registers_command_dump(0xFFFFFFFF);
	    bx_dbg_info_control_regs_command_dump();
	    bx_dbg_info_segment_regs_command_dump();
	    bx_dump_msr(cpu);
	    bx_dump_apic(cpu);
	    dbg_dump_printf("------------------major dump initial end---------------------------\n");
	}
	else if((instr_cnt % DUMP_INSTR_NUM) == 0 ){
		instr_cnt = 0;
		if(t == 0){
			dbg_dump_printf("------------------major dump result start---------------------------\n");
			bx_dbg_info_registers_command_dump(0xFFFFFFFF);
			bx_dbg_info_control_regs_command_dump();
			bx_dbg_info_segment_regs_command_dump();
			bx_dump_msr(cpu);
			bx_dump_apic(cpu);
			dbg_dump_printf("------------------major dump result end---------------------------\n");
			fclose(current_dump_log);
			fprintf(stderr,"Dump file %s done!\n",dump_log_file_name );
			log_file_num++;
			if(log_file_num < DUMP_FILE_NUM){
				sprintf(dump_log_file_name,"%s/%s_%d",dump_log_dir, dump_file_name, log_file_num);
				current_dump_log = bx_dbg_set_dump_log_handler(dump_log_file_name);
				bx_print_instruction(cpu,i,1);

			}
			else{
				active = 0;
				return;
			}
		}
		else{
			dbg_dump_printf("------------------major dump initial start---------------------------\n");
			bx_dbg_info_registers_command_dump(0xFFFFFFFF);
			bx_dbg_info_control_regs_command_dump();
			bx_dbg_info_segment_regs_command_dump();
			bx_dump_msr(cpu);
			bx_dump_apic(cpu);
			dbg_dump_printf("------------------major dump initial end---------------------------\n");
			return;
		}


	}
	else{
		dbg_dump_printf("----------------------------------------------------------\n");
		return;
	}
  }
}


void bx_instr_opcode(unsigned cpu, bxInstruction_c *bx_instr, const Bit8u *opcode, unsigned len, bx_bool is32, bx_bool is64) {


}


void bx_instr_before_execution(unsigned cpu, bxInstruction_c *bx_instr) {
  
  unsigned n;
  if (!active ) return;

  instruction_t *i = &instruction[cpu];
  reload_flag = 0;
  smm_exit_flag = 0;
  if (i->ready) bx_print_instruction(cpu, i);

//  prepare instruction_t structure for new instruction
  i->ready = 1;
  i->num_data_accesses = 0;
  i->num_data_accesses_phy = 0;
  i->num_data_accesses_phy_rmw = 0;
  i->is_branch = 0;
  
  i->is32 = BX_CPU(cpu)->sregs[BX_SEG_REG_CS].cache.u.segment.d_b;
  i->is64 = BX_CPU(cpu)->long64_mode();
  i->opcode_length = bx_instr->ilen();
  memcpy(i->opcode, bx_instr->get_opcode_bytes(), i->opcode_length);
  rip = bx_dbg_get_rip(cpu);
  //fprintf(stderr, "----****-------------------------------------------------****----\n");
  //for(n=0;n < i->opcode_length;n++) fprintf(stderr, "opcode %d is %02x\n", n, i->opcode[n]);
  //fprintf(stderr, "is32 is  %d and is64 is %d\n", i->is32, i->is64);
}
void bx_instr_after_execution(unsigned cpu, bxInstruction_c *bx_instr) {

  if (!active) return;
  Bit64u val64 = 0;
  instruction_t *i = &instruction[cpu];
  if (i->ready) {
    bx_print_instruction(cpu, i);
    if (rep_flag<2){
    		instr_cnt++;
        }
    else
    {
    	rep_flag = 1;
    }
    i->ready = 0;
  }
}

static void branch_taken(unsigned cpu, bx_address new_eip)
{
  if (!active || !instruction[cpu].ready) return;

  instruction[cpu].is_branch = 1;
  instruction[cpu].is_taken = 1;

  // find linear address
  instruction[cpu].target_linear = BX_CPU(cpu)->get_laddr(BX_SEG_REG_CS, new_eip);
}
 
void bx_instr_cnear_branch_taken(unsigned cpu, bx_address branch_eip, bx_address new_eip)
{
     branch_taken(cpu, new_eip);
}
void bx_instr_cnear_branch_not_taken(unsigned cpu, bx_address branch_eip)
{
       if (!active || !instruction[cpu].ready) return;

           instruction[cpu].is_branch = 1;
            instruction[cpu].is_taken = 0;
}
 
void bx_instr_ucnear_branch(unsigned cpu, unsigned what, bx_address branch_eip, bx_address new_eip)
{
               branch_taken(cpu, new_eip);
}
  
void bx_instr_far_branch(unsigned cpu, unsigned what, Bit16u prev_cs, bx_address prev_eip, Bit16u new_cs, bx_address new_eip)
{
                 branch_taken(cpu, new_eip);
}
void bx_instr_interrupt(unsigned cpu, unsigned vector)
{
  	if(active)
        {
  		dbg_dump_printf("CPU %u: interrupt %02xh\n", cpu, vector);
        }
}
void bx_instr_exception(unsigned cpu, unsigned vector, unsigned error_code)
{
  if(active)
  {
	  dbg_dump_printf("CPU %u: exception %02xh, error_code = %x\n", cpu, vector, error_code);
  }
}

void bx_instr_hwinterrupt(unsigned cpu, unsigned vector, Bit16u cs, bx_address eip)
{
  if(active)
  {
	  dbg_dump_printf("CPU %u: hardware interrupt %02xh\n", cpu, vector);
  }
}





void bx_instr_repeat_iteration(unsigned cpu, bxInstruction_c *i) {}

void bx_instr_inp(Bit16u addr, unsigned len) {}
void bx_instr_inp2(Bit16u addr, unsigned len, unsigned val) {}
void bx_instr_outp(Bit16u addr, unsigned len, unsigned val) {}

void bx_instr_lin_access(unsigned cpu, bx_address lin, bx_address phy, unsigned len, unsigned memtype, unsigned rw, Bit8u* dataptr) {

if(!active || !instruction[cpu].ready) return;
  //instruction_t *i = &instruction[cpu];
  unsigned index = instruction[cpu].num_data_accesses;
  //bx_dbg_lin_memory_access_dump(cpu, lin, phy, len, memtype, rw , dataptr);
  //dbg_dump_printf("len is %d\n", len);
  if (index < MAX_DATA_ACCESSES) {
    instruction[cpu].data_access[index].laddr = lin;
    instruction[cpu].data_access[index].paddr = phy;
    instruction[cpu].data_access[index].rw    = rw;
    instruction[cpu].data_access[index].size  = len;
    instruction[cpu].data_access[index].memtype = memtype;
    //instruction[cpu].data_access[index].dataptr = dataptr;
	for(int i=0; i<instruction[cpu].data_access[index].size; i++) {
		instruction[cpu].data_access[index].data[i] = dataptr[i];
	}
    //bx_dbg_store_data_dump(cpu, lin, phy, len, memtype, rw , dataptr,instruction[cpu].data_access[index].data);
    //strncpy((char *)instruction[cpu].data_access[index].data,(const char*)dataptr,len);
    //instruction[cpu].data_access[index].data[0]=dataptr[0];
    //instruction[cpu].data_access[index].data[1]=dataptr[1];
    instruction[cpu].num_data_accesses++;
    index++;
  }
//  if (len == 1) {
//       Bit8u val8 = *dataptr;
//       dbg_dump_printf(":data 0x%02X\n", (unsigned) val8);
//    }
//    else if (len == 2) {
//       Bit16u val16 = *((Bit16u*) dataptr);
//       dbg_dump_printf(":data 0x%04X\n", (unsigned) val16);
//    }
  //dbg_dump_printf("%x%x\n",instruction[cpu].data_access[index-1].data[1],instruction[cpu].data_access[index-1].data[0]);
  //bx_dbg_lin_memory_access_dump(cpu, lin, phy, len, memtype, rw , (Bit8u*)instruction[cpu].data_access[index].data);
  //bx_dbg_lin_memory_access_dump(cpu, lin, phy, len, memtype, rw , dataptr);


}
void bx_instr_phy_access(unsigned cpu,                 bx_address phy, unsigned len, unsigned memtype, unsigned rw, unsigned why, Bit8u* dataptr) {
	if(!active || !instruction[cpu].ready) return;

	  unsigned index = instruction[cpu].num_data_accesses_phy;

	  if (index < MAX_DATA_ACCESSES) {
	    instruction[cpu].data_access_phy[index].paddr = phy;
	    instruction[cpu].data_access_phy[index].rw    = rw;
	    instruction[cpu].data_access_phy[index].size  = len;
	    instruction[cpu].data_access_phy[index].memtype = memtype;
	    instruction[cpu].data_access_phy[index].why = why;
	    //instruction[cpu].data_access_phy[index].dataptr = dataptr;
		for(int i=0; i<instruction[cpu].data_access_phy[index].size; i++) {
			instruction[cpu].data_access_phy[index].data[i] = dataptr[i];
		}
	    instruction[cpu].num_data_accesses_phy++;
	    index++;
	  }
	  //fprintf(stderr,"phy access!\n");
}

void bx_instr_phy_access_rmw(unsigned cpu,                 bx_address phy, unsigned len, unsigned memtype, unsigned rw, unsigned why, Bit8u* dataptr) {
	if(!active || !instruction[cpu].ready) return;

	  unsigned index = instruction[cpu].num_data_accesses_phy_rmw;

	  if (index < MAX_DATA_ACCESSES) {
	    instruction[cpu].data_access_phy_rmw[index].paddr = phy;
	    instruction[cpu].data_access_phy_rmw[index].rw    = rw;
	    instruction[cpu].data_access_phy_rmw[index].size  = len;
	    instruction[cpu].data_access_phy_rmw[index].memtype = memtype;
	    instruction[cpu].data_access_phy_rmw[index].why = why;
	    instruction[cpu].data_access_phy_rmw[index].dataptr = dataptr;
	    instruction[cpu].num_data_accesses_phy_rmw++;
	    index++;
	  }
	  //fprintf(stderr,"phy access!\n");
}


void bx_instr_wrmsr(unsigned cpu, unsigned addr, Bit64u value) {

	if(active)
	  {
		dbg_dump_printf("CPU[%d] WRMSR %x DATA: %x %x",cpu ,addr ,GET32H(value), GET32L(value));
	  }


}

void bx_instr_vmexit(unsigned cpu, Bit32u reason, Bit64u qualification) {}

#endif
