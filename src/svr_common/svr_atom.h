#pragma once
#include "svr_common.h"

// Atomic operations for x86.

struct SvrAtom32
{
    s32 v;
};

struct SvrAtom64
{
    s64 v;
};

// svr_atom_store sets with release order.
// svr_atom_load reads with acquire order.

void svr_atom_store(SvrAtom32* atom, s32 value);
s32 svr_atom_load(SvrAtom32* atom);
void svr_atom_and(SvrAtom32* atom, s32 value);
void svr_atom_or(SvrAtom32* atom, s32 value);
bool svr_atom_cmpxchg(SvrAtom32* atom, s32* expr, s32 value);
s32 svr_atom_add(SvrAtom32* atom, s32 num);
s32 svr_atom_sub(SvrAtom32* atom, s32 num);
s32 svr_atom_swap(SvrAtom32* atom, s32 value);

void svr_atom_store(SvrAtom64* atom, s64 value);
s64 svr_atom_load(SvrAtom64* atom);
void svr_atom_and(SvrAtom64* atom, s64 value);
void svr_atom_or(SvrAtom64* atom, s64 value);
bool svr_atom_cmpxchg(SvrAtom64* atom, s64* expr, s64 value);
s64 svr_atom_add(SvrAtom64* atom, s64 num);
s64 svr_atom_sub(SvrAtom64* atom, s64 num);
s64 svr_atom_swap(SvrAtom64* atom, s64 value);
