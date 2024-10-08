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


#ifndef MAESTRO_DSE_CSV_WRITER_HPP_
#define MAESTRO_DSE_CSV_WRITER_HPP_

#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
#include <string>

#include "DSE_design_point.hpp"
#include "API_configuration.hpp"

namespace maestro {
    namespace DSE {
        class CSVWriter {
        public:
            long num_partial_sums_ = 1;

            CSVWriter(std::shared_ptr<ConfigurationV2> maestro_config, std::string file_name_):
                    num_partial_sums_(1) {
                bool file_exists = boost::filesystem::exists(file_name_);

                outfile_.open(file_name_,std::fstream::in | std::fstream::out | std::fstream::app);

                if(!file_exists) {
                    outfile_ << "Neural Network Name, Layer Number, NumPEs, Runtime (Cycles), Activity count-based Energy (nJ), Throughput (MACs/Cycle), Throughput Per Energy (GMACs/s*J), " <<
                             "Area, Power, NoC BW Req (Elements/cycle), Avg BW Req, Peak BW Req, Vector Width, Offchip BW Req (Elements/cycle),  L2 SRAM Size Req (Bytes), L1 SRAM Size Req (Bytes), Multicasting Factor (Weight), Multicasting Factor (Input), "
                             << "Num Total Input Pixels, Num Total Weight Pixels, Ops/J, Num MACs,MACs energy,L1 energy,L2 energy,NoC energy, ";

                    for(auto& tensor : *maestro_config->tensors_->at(0)) { // TODO: fix this hard-coded part ( at(0) )
                        auto dataclass = tensor->GetDataClass();
                        auto tensor_name = tensor->GetTensorName();
                        outfile_ << tensor_name << " l1 read, " << tensor_name << " l1 write, " << tensor_name << " l2 read, " << tensor_name << " l2 write, " << tensor_name << " reuse factor" << ",";
                    }

                    outfile_ << "Ingress Delay (Min), Ingress Delay (Max), Ingress Delay (Avg), Egress Delay (Min), Egress Delay (Max),  Egress Delay (Avg),"
                             << "Compute Delay (Min), Compute Delay (Min), Compute Delay (Avg),";
                    outfile_ << "Avg number of utilized PEs, Arithmetic Intensity";

                    outfile_ << std::endl;
                }

            }

            void WriteDesignPoint(
                    std::shared_ptr<ConfigurationV2> maestro_config,
                    int tensor_idx,
                    std::shared_ptr<DesignPoint> dp,
                    std::string nn_name,
                    std::string layer_name, long num_partial_sums, long num_inputs, long num_weights,
                    double ops_per_joule, double mac_energy, double l1_energy, double l2_energy,
                    double noc_energy, std::shared_ptr<CA::CostAnalysisResults> cost_analysis_results, LayerQuantizationType quantizationType) {
                double throughput =  static_cast<double>(num_partial_sums)/ static_cast<double>(dp->runtime_);

                outfile_ << nn_name << "," << layer_name << "," << dp->num_pes_ << "," << dp->runtime_ << "," << dp->energy_ << "," << throughput << "," << dp->performance_per_energy_
                         << "," << dp->area_ << "," << dp->power_ << "," << cost_analysis_results->GetPeakBWReq() << "," << cost_analysis_results->GetAvgBWReq() << "," << cost_analysis_results->GetPeakBWReq() << "," << dp->vector_width_ << "," << cost_analysis_results->GetOffchipBWReq() << "," << dp->l2_sram_sz << ","
                         << dp->l1_sram_sz << "," << dp->GetMulticastingFactor("weight") << "," << dp->GetMulticastingFactor("input")
                         << "," << num_inputs << "," << num_weights << "," << ops_per_joule << "," << num_partial_sums
                         << "," << mac_energy << "," << l1_energy << "," << l2_energy << "," << noc_energy << ",";


                for(auto& tensor : *maestro_config->tensors_->at(tensor_idx)) {
                    auto dataclass = tensor->GetDataClass();

                    long l1_read = (cost_analysis_results->GetBufferAccessCount(CA::BufferType::Downstream, CA::BufferAccessType::Read, dataclass))/
                                   quantizationFactor(quantizationType);
                    long l1_write = (cost_analysis_results->GetBufferAccessCount(CA::BufferType::Downstream, CA::BufferAccessType::Write, dataclass))/
                    quantizationFactor(quantizationType);
                    long l2_read = (cost_analysis_results->GetBufferAccessCount(CA::BufferType::Upstream, CA::BufferAccessType::Read, dataclass))/
                                   quantizationFactor(quantizationType);
                    long l2_write = (cost_analysis_results->GetBufferAccessCount(CA::BufferType::Upstream, CA::BufferAccessType::Write, dataclass))/
                                    quantizationFactor(quantizationType);
                    outfile_ << l1_read << ", " << l1_write << ", " << l2_read << ", " << l2_write << ", ";
                    long double reuse_factor = static_cast<long double>(l1_read) / static_cast<long double>(l1_write);
                    outfile_ << reuse_factor << ", ";
                }

                long ingress_delay_[static_cast<int>(CA::ValueType::NumValTypes)];
                long egress_delay_[static_cast<int>(CA::ValueType::NumValTypes)];
                long compute_delay_[static_cast<int>(CA::ValueType::NumValTypes)];

                ingress_delay_[static_cast<int>(CA::ValueType::Min)] = cost_analysis_results->GetDelay(CA::DelayType::Ingress, CA::ValueType::Min);
                ingress_delay_[static_cast<int>(CA::ValueType::Max)] = cost_analysis_results->GetDelay(CA::DelayType::Ingress, CA::ValueType::Max);
                ingress_delay_[static_cast<int>(CA::ValueType::Avg)] = cost_analysis_results->GetDelay(CA::DelayType::Ingress, CA::ValueType::Avg);

                egress_delay_[static_cast<int>(CA::ValueType::Min)] = cost_analysis_results->GetDelay(CA::DelayType::Egress, CA::ValueType::Min);
                egress_delay_[static_cast<int>(CA::ValueType::Max)] = cost_analysis_results->GetDelay(CA::DelayType::Egress, CA::ValueType::Max);
                egress_delay_[static_cast<int>(CA::ValueType::Avg)] = cost_analysis_results->GetDelay(CA::DelayType::Egress, CA::ValueType::Avg);

                compute_delay_[static_cast<int>(CA::ValueType::Min)] = cost_analysis_results->GetDelay(CA::DelayType::Computation, CA::ValueType::Min);
                compute_delay_[static_cast<int>(CA::ValueType::Max)] = cost_analysis_results->GetDelay(CA::DelayType::Computation, CA::ValueType::Max);
                compute_delay_[static_cast<int>(CA::ValueType::Avg)] = cost_analysis_results->GetDelay(CA::DelayType::Computation, CA::ValueType::Avg);


                outfile_<< ingress_delay_[static_cast<int>(CA::ValueType::Min)]
                        << ", " << ingress_delay_[static_cast<int>(CA::ValueType::Max)]
                        << ", " << ingress_delay_[static_cast<int>(CA::ValueType::Avg)] << ", "
                        << egress_delay_[static_cast<int>(CA::ValueType::Min)]
                        << ", " << egress_delay_[static_cast<int>(CA::ValueType::Max)]
                        << ", " << egress_delay_[static_cast<int>(CA::ValueType::Avg)] << ", "
                        << compute_delay_[static_cast<int>(CA::ValueType::Min)]
                        << ", " << compute_delay_[static_cast<int>(CA::ValueType::Max)]
                        << ", " << compute_delay_[static_cast<int>(CA::ValueType::Avg)];

                outfile_ << ", " << cost_analysis_results->GetNumAvgActiveClusters();
                outfile_ << ", " << cost_analysis_results->GetArithmeticIntensity();

                outfile_ << std::endl;
            }

            /*
             * This function permit to packed the number of access to the memory. It is assumed that 1 access can take 1 FP32 element, 2 FP16 element and so on
             */
            static int quantizationFactor(LayerQuantizationType quantizationType){

                switch (quantizationType) {
                    case LayerQuantizationType::FP32:
                    case LayerQuantizationType::INT32:
                        return 1;
                    case LayerQuantizationType::FP16:
                    case LayerQuantizationType::INT16:
                        return 2;
                    case LayerQuantizationType::FP8:
                    case LayerQuantizationType::INT8:
                        return 4;
                    case LayerQuantizationType::FP4:
                    case LayerQuantizationType::INT4:
                        return 8;
                    case LayerQuantizationType::FP2:
                    case LayerQuantizationType::INT2:
                        return 16;
                    default:
                        std::cerr << "Unsupported quantization type" << std::endl;
                        exit(EXIT_FAILURE);
                }

            }

        protected:
            std::ofstream outfile_;

        }; // End of class CSVWriter
    }; // End of namespace DSE
}; // End of namespace maestro

#endif
