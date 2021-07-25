#pragma once
#include "svr_common.h"

// Atomic operations for x86.

struct SvrAtom32
{
    s32 v;
};

// svr_atom_set sets directly with no specific order.
// svr_atom_store sets with release order.
// svr_atom_load reads with acquire order.
// svr_atom_read reads with relaxed order.

void svr_atom_set(SvrAtom32* atom, s32 value);
void svr_atom_store(SvrAtom32* atom, s32 value);
s32 svr_atom_load(SvrAtom32* atom);
s32 svr_atom_read(SvrAtom32* atom);
void svr_atom_and(SvrAtom32* atom, s32 value);
void svr_atom_or(SvrAtom32* atom, s32 value);
bool svr_atom_cmpxchg(SvrAtom32* atom, s32* expr, s32 value);
s32 svr_atom_add(SvrAtom32* atom, s32 num);
s32 svr_atom_sub(SvrAtom32* atom, s32 num);
