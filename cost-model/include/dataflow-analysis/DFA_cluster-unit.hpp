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

#ifndef MAESTRO_DFA_CLUSTER_UNIT_HPP_
#define MAESTRO_DFA_CLUSTER_UNIT_HPP_

//#define DEBUG_CLUSTER_UNIT

#include <memory>
#include <vector>
#include <list>
#include <map>
#include <string>

#include "BASE_maestro-class.hpp"
#include "TL_error-handler.hpp"

#include "AHW_noc-model.hpp"
#include "DFA_dimension-table.hpp"
#include "DFA_directives.hpp"
#include "DFA_directive-table.hpp"

#include "DFA_tensor.hpp"
#include "DFA_tensor-table.hpp"


namespace maestro {
    namespace DFA {

        const int invalid_map_pos = -1;

        class ClusterUnit : public MAESTROClass {
        public:
            ClusterUnit(int cluster_level, int cluster_size,
                        std::shared_ptr<DFA::DirectiveTable> dataflow,
                        std::shared_ptr<DFA::DimensionTable> dimensions,
                        std::shared_ptr<DFA::TensorTable> tensors,
                        std::shared_ptr<AHW::NetworkOnChipModel> noc) :
                    cluster_level_(cluster_level),
                    cluster_size_(cluster_size),
                    noc_(noc),
                    dimensions_(dimensions),
                    dataflow_(dataflow),
                    tensors_(tensors),
                    MAESTROClass("ClusterUnitAnalysis_Lv"+ std::to_string(cluster_level))
            {

                num_mapped_elements_ = std::make_unique<std::map<std::string, int>>();
                dataflow->ConvertToInputCentric();
                Preprocess();
            }

            int GetClusterLevel() {
                return cluster_level_;
            }

            std::shared_ptr<AHW::NetworkOnChipModel> GetNoCModel() {
                return noc_;
            }

            std::shared_ptr<DFA::DimensionTable> GetDimensions() {
                return dimensions_;
            }

            std::shared_ptr<DFA::DirectiveTable> GetDataflow() {
                return dataflow_;
            }

            long GetNumTotalIterations() {
                long ret = 1;

                for(int idx = 0; idx < dataflow_->size();idx ++) {

                    auto directive = dataflow_->at(idx);
                    auto directive_dim = directive->GetVariable();
                    auto dim_size = dimensions_->GetSize(directive_dim);
                    int num_iter_this_dim = 1;

                    if(dimensions_->IsOverlapped(directive_dim)) {
                        if(!dimensions_->IsSlidingDim(directive_dim)) {
                            auto sliding_dim = dimensions_->GetOverlappingDim(directive_dim);
                            auto sliding_dim_directive = dataflow_->FindDirective(directive_dim);
                            assert(sliding_dim_directive != nullptr);
                            auto sliding_dim_map_size = sliding_dim_directive->GetSize();
                            auto sliding_dim_size = dimensions_->GetSize(sliding_dim);

                            if(sliding_dim_map_size == sliding_dim_size) {
                                dim_size = dim_size - sliding_dim_size + 1;
                            }
                        }
                    }

                    if (directive->GetClass() == directive::DirectiveClass::TemporalMap) {
                        num_iter_this_dim = dim_size/directive->GetOfs();
                        if(dim_size % directive->GetOfs() != 0) {
                            num_iter_this_dim++;
                        }
                    }
                    else if(directive->GetClass() == directive::DirectiveClass::SpatialMap) {
                        num_iter_this_dim = dim_size/(directive->GetOfs() * cluster_size_);
                        if(dim_size % (directive->GetOfs() * cluster_size_) != 0) {
                            num_iter_this_dim++;
                        }
                    }
                    else {
                        //TODO: Handle this error
                    }

                    ret *= num_iter_this_dim;
                } //End of for(idx)
                return ret;
            }


            long GetNumClusters(bool is_spatial_edge = false) {
                if(!is_spatial_edge) {
                    return cluster_size_;
                }
                else {
                    return num_spatial_edge_clusters_;
                }
            }


        protected:

            int cluster_level_ = -1;
            int cluster_size_ = 1;

            int upper_spatial_map_idx_ = invalid_map_pos;
            int lower_spatial_map_idx_ = invalid_map_pos;
            int outer_temporal_map_idx_ = invalid_map_pos;
            int inner_temporal_map_idx_ = invalid_map_pos;

            int num_spatial_iterations_ = 1;

            int num_spatial_edge_clusters_ = 1;
            int num_steady_spatial_iterations_ = 1;
            int num_edge_spatial_iterations_ = 0;

            std::shared_ptr<DFA::DimensionTable> dimensions_;
            std::shared_ptr<DFA::DirectiveTable> dataflow_;
            std::shared_ptr<AHW::NetworkOnChipModel> noc_;

            std::unique_ptr<std::map<std::string, int>> num_mapped_elements_; //TSz

            std::shared_ptr<DFA::TensorTable> tensors_;

        private:
            //Get the index of the inner-most spatial map
            void AnalyzeSpatialMapIdx() {
                int idx = 0;

                for(auto& it : *dataflow_) {
                    if(it->GetClass() == directive::DirectiveClass::SpatialMap) {
                        if(upper_spatial_map_idx_ == invalid_map_pos) { // First spatial map
                            upper_spatial_map_idx_ = idx;
                        }
                        else if(lower_spatial_map_idx_ == invalid_map_pos) { // Second spatial map
                            lower_spatial_map_idx_ = idx;
                        }
                        else { // Third spatial map == Error!
                            this->error_handler_->PrintErrorMsg(TL::ErrorCode::MultiParallelismInSingleCluster, std::to_string(cluster_level_), this->GetName());
                            this->error_handler_->TerminateProgram();
                        }
                    } // End of if(this directive == SpatialMap)
                    idx++;
                }

                if(upper_spatial_map_idx_ == invalid_map_pos) { // Error: No spatial map!
                    this->error_handler_->PrintErrorMsg(TL::ErrorCode::NoSpatialMap, std::to_string(cluster_level_), this->GetName());
                    this->error_handler_->TerminateProgram();
                }

                //TODO: Add another error check for invalid double spatial map

            }

            //Get the index of the inner-most temporal map under the inner-most spatial map.
            // If no temporal map is under the inner-most spatial map, it returns the index to the inner-most spatial map
            // TODO: FLAG! Needs to check the correctness
            void AnalyzeInnerTemporalMapIdx() {
                int inner_temporal_map_index = -1;
                int idx = 0;

                inner_temporal_map_index = upper_spatial_map_idx_;

                for(int idx = upper_spatial_map_idx_; idx < dataflow_->size() ; idx++) {
                    if(dataflow_->at(idx)->GetClass() == directive::DirectiveClass::TemporalMap) {
                        bool isUnrolled = dataflow_->at(idx)->GetSize() >= dimensions_->GetSize(dataflow_->at(idx)->GetVariable());
                        if(!isUnrolled) {
                            inner_temporal_map_index = idx;
                        }
                    }
                }

                inner_temporal_map_idx_ = inner_temporal_map_index;
            }



            /*
             * - num_spatial_edge_clusters_ = number of clusters which are in spatial edge (not all clusters operative) (Init or Edge status)
             * - num_edge_spatial_iterations_ = equal to 1 or zero, depending on the existence of edge cases (Init or Edge status)
             *   used in function DFA_cluster-unit.hpp::HasSpatialEdgeCase(),
             *   called by CA_cost-analysis-engine.hpp::GetNumCaseOccurrence(),
             *   called by std::shared_ptr<CostAnalyisResults> AnalyzeSingleCluster(),
             *   which is NEVER USED
             *   - num_steady_spatial_iterations_ = used only inside AnalyzeSpatialEdgeCase()
             */
            // #include <cmath>
            void AnalyzeSpatialEdgeCase() {
                auto sp_map_directive = dataflow_->at(upper_spatial_map_idx_);
                auto sp_var = sp_map_directive->GetVariable();
                /* LF: this value is inherited from the upper level cluster: if SpatialMap(a,a) X,
                * and in the upper cluster X tile is z, GetSize() will give z */
                auto sp_dim_sz = dimensions_->GetSize(sp_var);
                int map_size = sp_map_directive->GetSize();
                int map_ofs = sp_map_directive->GetOfs();

                num_spatial_edge_clusters_ = 0;
                num_edge_spatial_iterations_ = 0;
                int remaining_items = 0;

                int each_sp_iter_full_coverage = (map_ofs * (cluster_size_-1)) + map_size;

                // > or >=?
                if(sp_dim_sz >= each_sp_iter_full_coverage) {

                    num_steady_spatial_iterations_ = ((sp_dim_sz - map_size) / map_ofs + 1) / cluster_size_ - 1;
                    int tot_coverage = ((num_steady_spatial_iterations_ + 1) * cluster_size_ - 1) * map_ofs + map_size;

                    if (sp_dim_sz == tot_coverage) { remaining_items = 0; }
                    //the remaining items are the dimension - total coverage??
                    else { remaining_items = sp_dim_sz - ((num_steady_spatial_iterations_ + 1) * cluster_size_) * map_ofs; }
                    //else { remaining_items = sp_dim_sz - ((num_steady_spatial_iterations_ + 1) * cluster_size_ -1) * map_ofs + map_size; }

                    if (remaining_items == 0) {
                        num_spatial_edge_clusters_ = 0;
                        num_edge_spatial_iterations_ = 0;
                    } else {
                        num_edge_spatial_iterations_ = 1;

                        if (remaining_items < map_size) { num_spatial_edge_clusters_ = 1; } // LF: first element ... is_first_pe
                        else { num_spatial_edge_clusters_ = ceil((double) (remaining_items - map_size) / (double) map_ofs + 1); }
                    }
                }
                else {
                    num_edge_spatial_iterations_ = 1;
                    if(sp_dim_sz > map_size) {
                        num_spatial_edge_clusters_ = ceil((double)(sp_dim_sz - map_size) / (double)map_ofs + 1);
                        if(sp_dim_sz<=(num_spatial_edge_clusters_-1)*map_ofs) {num_spatial_edge_clusters_ = num_spatial_edge_clusters_ - 1;}
                    }
                    else {num_spatial_edge_clusters_ = 1;}
                }

#ifdef DEBUG_CLUSTER_UNIT
                std::cout << "Cluster lv: " << cluster_level_ << std::endl;
          std::cout << "Cluster size: " << cluster_size_ << std::endl;
          std::cout << "num_steady_spatial_iterations_ size: " << num_steady_spatial_iterations_ << std::endl;
          std::cout << "Cluster num_edge_spatial_iterations_: " << num_edge_spatial_iterations_ << std::endl;
#endif
            }

            void Preprocess() {
                //Functions regarding spatial mapping always need to be called first
                AnalyzeSpatialMapIdx();
                AnalyzeInnerTemporalMapIdx();
                AnalyzeSpatialEdgeCase();
            }
        }; // End of class ClusterUnit
    } // End of namespace DFA
} // End of namespace maestro


#endif
