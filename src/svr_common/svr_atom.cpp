#include "svr_atom.h"
#include <Windows.h>
#include <intrin0.h>

// Following implementation in MSVC STL xatomic.

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

void svr_atom_and(SvrAtom32* atom, s32 value)
{
    InterlockedAnd((LONG*)&atom->v, (LONG)value);
}

void svr_atom_or(SvrAtom32* atom, s32 value)
{
    InterlockedOr((LONG*)&atom->v, (LONG)value);
}

bool svr_atom_cmpxchg(SvrAtom32* atom, s32* expr, s32 value)
{
    s32 old_expr = *expr;

    s32 prev = (s32)InterlockedCompareExchange((LONG*)&atom->v, (LONG)value, (LONG)old_expr);

    if (prev == old_expr)
    {
        return true;
    }

    *expr = prev;
    return false;
}

s32 svr_atom_add(SvrAtom32* atom, s32 num)
{
    return (s32)InterlockedExchangeAdd((LONG*)&atom->v, (LONG)num);
}

s32 svr_atom_sub(SvrAtom32* atom, s32 num)
{
    return svr_atom_add(atom, 0 - num);
}

void svr_atom_store(SvrAtom64* atom, s64 value)
{
    _ReadWriteBarrier();
    atom->v = value;
}

s64 svr_atom_load(SvrAtom64* atom)
{
    s64 ret = atom->v;
    _ReadWriteBarrier();
    return ret;
}

void svr_atom_and(SvrAtom64* atom, s64 value)
{
    InterlockedAnd64(&atom->v, (LONG64)value);
}

void svr_atom_or(SvrAtom64* atom, s64 value)
{
    InterlockedOr64(&atom->v, (LONG64)value);
}

bool svr_atom_cmpxchg(SvrAtom64* atom, s64* expr, s64 value)
{
    s64 old_expr = *expr;

    s64 prev = (s64)InterlockedCompareExchange64((volatile LONG64*)&atom->v, (LONG64)value, (LONG64)old_expr);

    if (prev == old_expr)
    {
        return true;
    }

    *expr = prev;
    return false;
}

s64 svr_atom_add(SvrAtom64* atom, s64 num)
{
    return (s64)InterlockedExchangeAdd64((volatile LONG64*)&atom->v, (LONG64)num);
}

s64 svr_atom_sub(SvrAtom64* atom, s64 num)
{
    return svr_atom_add(atom, 0 - num);
}

void svr_notify_atom_changed(SvrAtom32* atom)
{
    // This creates a full memory barrier. Wake all waiting threads.
    WakeByAddressAll(&atom->v);
}

void svr_notify_atom_changed(SvrAtom64* atom)
{
    // This creates a full memory barrier. Wake all waiting threads.
    WakeByAddressAll(&atom->v);
}

void svr_wait_until_atom_is(SvrAtom32* atom, s32 target_value)
{
    s32 captured_value = svr_atom_load(atom);

    while (captured_value != target_value)
    {
        WaitOnAddress(&atom->v, &captured_value, sizeof(target_value), INFINITE); // Awake when value differs.
        captured_value = svr_atom_load(atom);
    }
}

void svr_wait_until_atom_is(SvrAtom64* atom, s64 target_value)
{
    s64 captured_value = svr_atom_load(atom);

    while (captured_value != target_value)
    {
        WaitOnAddress(&atom->v, &captured_value, sizeof(target_value), INFINITE); // Awake when value differs.
        captured_value = svr_atom_load(atom);
    }
}
