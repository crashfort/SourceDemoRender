#pragma once
#include <svr/str.hpp>

#include <string>
#include <vector>

namespace svr
{
    // Splits a string by space.
    // Does not specialize for quotes.
    std::vector<std::string> tokenize(const char* input)
    {
        std::vector<std::string> ret;

        auto view = input;

        while (*view != 0)
        {
            view = str_trim_left(view);

            if (*view == 0)
            {
                break;
            }

            auto token = view;

            while (*view != ' ' && *view != 0)
            {
                view++;
            }

            ret.push_back(std::string(token, token + (view - token)));
        }

        return ret;
    }
}
