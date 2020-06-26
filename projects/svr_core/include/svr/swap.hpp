#pragma once

namespace svr
{
    template <typename T>
    inline void swap_ptr(T& a, T& b)
    {
        T temp = b;
        b = a;
        a = temp;
    }
}
