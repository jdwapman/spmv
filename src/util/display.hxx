#pragma once

#include <iostream>
#include <cmath>

#include <formats/csr.hxx>

namespace util
{

    template <typename vector_t>
    void display(vector_t v, std::string name,
                 bool verbose = true)
    {
        if (verbose)
        {
            std::cout << name << " = [ ";
            for (size_t i = 0; i < v.size() && (i < 40); i++)
                std::cout << v[i] << " ";

            if(v.size() >= 40) {
                std::cout << "...";
            }
            std::cout << "]" << std::endl;
        }
    }

    template <typename index_t = int, typename value_t = float>
    void display(csr_t<index_t, value_t> &s, std::string name,
                 bool verbose = true)
    {
        if (verbose)
        {
            std::cout << "num rows = " << s.num_rows << std::endl;
            std::cout << "num cols = " << s.num_columns << std::endl;
            std::cout << "num nz = " << s.num_nonzeros << std::endl;

            display(s.nonzero_vals, "values");
            display(s.col_idx, "col_indices");
            display(s.row_offsets, "row_offsets");
        }
    }

} // namespace util