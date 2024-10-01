/******************************************************************************
Copyright (c) 2019 Georgia Instititue of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : Hyoukjun Kwon (hyoukjun@gatech.edu)
*******************************************************************************/


#ifndef BASE_CONSTANTS_HPP_
#define BASE_CONSTANTS_HPP_


#include <string>
#include "DFA_layer.hpp"

namespace maestro{

    enum class DataClass{Input, Weight, Output, NumDataClasses};
    enum class Operation {Read, Write};
    const double l1_energy_multiplier = 1.68;
    const double l2_energy_multiplier = 18.61;
    struct MemoryParams {
        double wr_dyn_energy; // Write Dynamic Energy (nJ/access)
        double rd_dyn_energy; // Read Dynamic Energy (nJ/access)
        double leakage_power; // Leakage Power (mW)
    };

    const double energy_cost_per_bit = 0.1143e-12;

    // Define the available memory sizes and their parameters
    const std::map<size_t, MemoryParams> memory_data = {
            {512, {2.73e-4, 2.91e-4, 5.81e-5}},
            {1024, {3.35e-4, 4.18e-4, 1.30558e-4}},
            {2048, {5.06e-4, 5.89e-4, 3.00498e-4}},
            {4096, {9.51e-4, 9.73e-4, 5.18707e-4}},
            {8192, {1.51e-3, 1.53e-3, 1.01401e-3}},
            {16384, {2.17e-3, 2.24e-3, 2.10329e-3}},
            {32768, {3.39e-3, 3.45e-3, 4.00746e-3}},
            {65536, {5.19e-3, 5.30e-3, 8.65728e-3}},
            {131072, {7.94e-3, 8.06e-3, 1.69947e-2}},
            {262144, {1.08e-2, 1.09e-2, 3.26368e-2}},
            {524288, {1.62e-2, 1.63e-2, 6.42241e-2}},
            {1048576, {2.22e-2, 2.23e-2, 1.25151e-1}},
            {2097152, {3.33e-2, 3.34e-2, 2.47646e-1}},
            {4194304, {4.59e-2, 4.60e-2, 4.87825e-1}},
            {8388608, {9.18e-2, 9.20e-2, 9.7565e-1}},
            {16777216, {1.836e-1, 1.84e-1, 1.9513}},
            {33554432, {3.672e-1, 3.68e-1, 3.9026}},
            {67108864, {7.344e-1, 7.36e-1, 7.8052}},
            {134217728, {1.4688e+0, 1.472e+0, 15.6104}},
            {268435456, {2.937e+0, 2.944e+0, 31.2208}},
            {536870912, {5.8752e+0, 5.88e+0, 62.4416}},
            {1073741824, {1.175e+1, 1.176e+1, 124.8832}},
            {2147483648, {2.35e+1, 2.352e+1, 249.7664}}
    };

// Function to get the bit size based on quantization type
    size_t getBitSize(LayerQuantizationType quantizationType) {
        switch (quantizationType) {
            case LayerQuantizationType::FP32:
            case LayerQuantizationType::INT32:
                return 32;
            case LayerQuantizationType::FP16:
            case LayerQuantizationType::INT16:
                return 16;
            case LayerQuantizationType::FP8:
            case LayerQuantizationType::INT8:
                return 8;
            case LayerQuantizationType::FP4:
            case LayerQuantizationType::INT4:
                return 4;
            case LayerQuantizationType::FP2:
            case LayerQuantizationType::INT2:
                return 2;
            default:
                std::cerr << "Unsupported quantization type" << std::endl;
                exit(EXIT_FAILURE);
        }
    }

// Function to find the appropriate memory size
    size_t getMemorySize(size_t byteSize) {
        for (const auto& [size, params] : memory_data) {
            if (size >= byteSize) return size;
        }
        return 2147483648;
    }

    int return_hop_number(LayerQuantizationType quantizationType){
        switch (quantizationType) {
            case LayerQuantizationType::FP32:
            case LayerQuantizationType::INT32:
            case LayerQuantizationType::FP16:
            case LayerQuantizationType::INT16:
            case LayerQuantizationType::FP8:
            case LayerQuantizationType::INT8:
            case LayerQuantizationType::FP4:
            case LayerQuantizationType::INT4:
            case LayerQuantizationType::FP2:
            case LayerQuantizationType::INT2:
                return 2;
            default:
                std::cerr << "Unsupported quantization type" << std::endl;
                exit(EXIT_FAILURE);
        }
    }

    double getMemoryEnergyMultiplier(size_t numElements, LayerQuantizationType quantizationType, Operation operation) {
        // Get the bit size for the given quantization type
        size_t bitSize = getBitSize(quantizationType);

        // Calculate the total memory size in bytes
        size_t totalBits = numElements * bitSize;
        size_t totalBytes = totalBits / 8;

        // Find the appropriate memory size
        size_t memorySize = getMemorySize(totalBytes);

        // Get the memory parameters for the chosen memory size
        const MemoryParams& params = memory_data.at(memorySize);

        // Return the appropriate dynamic energy based on the operation type
        if (operation == Operation::Read) {
            return params.rd_dyn_energy;
        } else if (operation == Operation::Write) {
            return params.wr_dyn_energy;
        } else {
            std::cerr << "Unsupported operation type" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

}; // End of namespace maestro

#endif
