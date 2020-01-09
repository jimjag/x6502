#include "emu.h"

#include <stdio.h>
#include "functions.h"
#include "io.h"
#include "opcodes.h"

uint8_t read_byte(cpu *m, uint16_t address) {
    static char trace_entry[80];
    sprintf(trace_entry, "%04X r %02X\n", address, m->mem[address]);
    trace_bus(trace_entry);
    return m->mem[address];
}

uint8_t write_byte(cpu *m, uint16_t address, uint8_t value) {
    static char trace_entry[80];
    sprintf(trace_entry, "%04X W %02X\n", address, value);
    trace_bus(trace_entry);
    return m->mem[address]=value;
}

uint8_t read_next_byte(cpu *m, uint8_t pc_offset) {
    return read_byte(m, m->pc + pc_offset);
}

#define NEXT_BYTE(cpu) (read_next_byte((cpu), pc_offset++))

void main_loop(cpu *m) {
    uint8_t opcode;
    uint8_t arg1, arg2, t1;
    int8_t s1;
    uint16_t r1, r2;

    // pc_offset is used to read from memory like a stream when processing
    // bytecode without modifying the pc. pc_start is the memory address of the
    // currently-executing opcode; if pc == pc_start at the end of a simulation
    // step, we add pc_offset to get the start of the next instruction. if pc !=
    // pc_start, we branched so we don't touch the pc.
    uint8_t pc_offset = 0;
    uint16_t pc_start;

    // branch_offset is an offset that will be added to the program counter
    // after we move to the next instruction
    int8_t branch_offset = 0;

    init_io();

    for (;;) {
        reset_emu_flags(m);

        pc_offset = 0;
        branch_offset = 0;
        pc_start = m->pc;
	m->pc_actual = m->pc;
        opcode = NEXT_BYTE(m);
	m->opcode = opcode;

        switch (opcode) {
            #include "opcode_handlers/arithmetic.h"
            #include "opcode_handlers/branch.h"
            #include "opcode_handlers/compare.h"
            #include "opcode_handlers/flags.h"
            #include "opcode_handlers/incdec.h"
            #include "opcode_handlers/interrupts.h"
            #include "opcode_handlers/jump.h"
            #include "opcode_handlers/load.h"
            #include "opcode_handlers/logical.h"
            #include "opcode_handlers/shift.h"
            #include "opcode_handlers/stack.h"
            #include "opcode_handlers/store.h"
            #include "opcode_handlers/transfer.h"

            case NOP:
            default:
                // Unknown opcodes are a NOP in the 65C02 processor family
                break;
            
            case STP:
                goto end;
        }

        if (m->pc == pc_start) {
            m->pc += pc_offset;
        }
        m->pc += branch_offset;

        do {
            handle_io(m);
            // clear dirty memory flag immediately so that subsequent runs don't
            // redo whatever I/O operation is associated with the dirty memaddr
            m->emu_flags &= ~EMU_FLAG_DIRTY;
        } while ((m->emu_flags & EMU_FLAG_WAIT_FOR_INTERRUPT) &&
                 !m->interrupt_waiting);

        if (m->interrupt_waiting && !get_flag(m, FLAG_INTERRUPT)) {
            STACK_PUSH(m) = (m->pc & 0xFF00) >> 8;
            STACK_PUSH(m) = m->pc & 0xFF;
            STACK_PUSH(m) = m->sr;

            m->interrupt_waiting = 0x00;
            m->pc = mem_abs(m->mem[0xFFFE], m->mem[0xFFFF], 0);
            m->sr |= FLAG_INTERRUPT;
        }
    }
end:
    handle_io(m);
    finish_io();
}
