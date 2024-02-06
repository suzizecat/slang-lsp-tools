#pragma once

#include <vector>
#include <numeric>
#include <memory>

constexpr size_t sum_dim_vector(const std::vector<size_t>& vec, const unsigned int start = 0)
{
        return std::reduce(
        vec.cbegin() + start, 
        vec.cend(),
        0);
}
constexpr size_t sum_dim_vector(const std::vector<std::pair<size_t,size_t>>& vec, const unsigned int start = 0)
{
    return std::transform_reduce(
        vec.cbegin() + start, 
        vec.cend(),
        0,
        std::plus{},
        [](std::pair<size_t,size_t> p){return p.first + p.second;}
        );

}