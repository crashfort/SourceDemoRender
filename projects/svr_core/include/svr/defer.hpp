#pragma once

#define SVR_CONCAT_INTERNAL(x, y) x##y
#define SVR_CONCAT(x, y) SVR_CONCAT_INTERNAL(x, y)

namespace svr
{
    template <typename T>
    class finally
    {
    public:
        finally(T&& func)
            : function(func)
        {

        }

        ~finally()
        {
            function();
        }

    private:
        T function;
    };

    class finally_help
    {
    public:
        template<typename T>
        finally<T> operator+(T t)
        {
            return t;
        }
    };
}

#define defer const auto& SVR_CONCAT(defer__, __LINE__) = svr::finally_help() + [&]()
