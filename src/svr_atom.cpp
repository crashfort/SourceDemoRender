#include "svr_atom.h"
#include <Windows.h>
#include <intrin0.h>

// Following implementation in MSVC STL xatomic.

void svr_atom_set(SvrAtom32* atom, s32 value)
{
    atom->v = value;
}

void svr_atom_store(SvrAtom32* atom, s32 value)
{
    _ReadWriteBarrier();
    atom->v = value;
}

s32 svr_atom_load(SvrAtom32* atom)
{
    s32 ret = atom->v;
    _ReadWriteBarrier();
    return ret;
}

s32 svr_atom_read(SvrAtom32* atom)
{
    return atom->v;
}

void svr_atom_and(SvrAtom32* atom, s32 value)
{
    _InterlockedAnd((LONG*)&atom->v, (LONG)value);
}

void svr_atom_or(SvrAtom32* atom, s32 value)
{
    _InterlockedOr((LONG*)&atom->v, (LONG)value);
}

bool svr_atom_cmpxchg(SvrAtom32* atom, s32* expr, s32 value)
{
    s32 old_expr = *expr;

    s32 prev = (s32)_InterlockedCompareExchange((LONG*)&atom->v, (LONG)value, (LONG)old_expr);

    if (prev == old_expr)
    {
        return true;
    }

    *expr = prev;
    return false;
}

s32 svr_atom_add(SvrAtom32* atom, s32 num)
{
    return (s32)_InterlockedExchangeAdd((LONG*)&atom->v, (LONG)num);
}

s32 svr_atom_sub(SvrAtom32* atom, s32 num)
{
    return svr_atom_add(atom, 0 - num);
}
