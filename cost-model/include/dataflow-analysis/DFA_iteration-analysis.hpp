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


#ifndef MAESTRO_DFA_ITERATION_ANALYSIS_HPP_
#define MAESTRO_DFA_ITERATION_ANALYSIS_HPP_

//#define DEBUG_ITERATION_ANALYSIS

#include <vector>
#include <map>
#include <memory>
#include <string>

#include "BASE_maestro-class.hpp"
#include "TL_error-handler.hpp"

#include "DFA_iteration-status.hpp"

namespace maestro {
    namespace DFA {

        class IterationAnalysis : public MAESTROClass {
        public:
            IterationAnalysis(
                    std::shared_ptr<DFA::DimensionTable> dimensions,
                    std::shared_ptr<DFA::ClusterUnit> cluster
            ) : dimensions_(dimensions), cluster_(cluster) {

                valid_iteration_states_ = std::make_shared<std::vector<std::shared_ptr<std::vector<std::shared_ptr<DFA::IterationState>>>>>();
                iteration_status_table_ = std::make_shared<std::vector<std::shared_ptr<IterationStatus>>>();
                AnalyzeIterationStates();
                ConstructIterationStatusTable();
            }

            std::string ToString() {
                std::string ret = "";

                for(auto& iter_status : *iteration_status_table_) {
                    ret += iter_status->ToString();
                }

                return ret;
            }

            std::shared_ptr<std::vector<std::shared_ptr<IterationStatus>>> GetAllIterationsStatus() {
                return iteration_status_table_;
            }

        protected:
            std::shared_ptr<DFA::DimensionTable> dimensions_;
            std::shared_ptr<DFA::ClusterUnit> cluster_;

            std::shared_ptr<std::vector<std::shared_ptr<std::vector<std::shared_ptr<DFA::IterationState>>>>> valid_iteration_states_;
            std::shared_ptr<std::vector<std::shared_ptr<IterationStatus>>> iteration_status_table_;

        private:

            void AnalyzeIterationStates() {
                auto dataflow = cluster_->GetDataflow();
                for(auto& directive : *dataflow) {
#ifdef DEBUG_ITERATION_ANALYSIS
                    std::cout << "Directive: " << directive->ToString() << std::endl;
#endif
                    auto directive_var = directive->GetVariable();
                    auto directive_class = directive->GetClass();

                    int dim_size = dimensions_->GetSize(directive_var);
                    int map_size = directive->GetSize();
                    int map_ofs = directive->GetOfs();

                    /* LF: list of iteration states */
                    std::shared_ptr<std::vector<std::shared_ptr<DFA::IterationState>>> iter_state_list = std::make_shared<std::vector<std::shared_ptr<DFA::IterationState>>>();

                    if(directive_class == DFA::directive::DirectiveClass::TemporalMap) {
                        // 1. Init case
                        bool is_init_unroll = dim_size <= map_size;
                        bool is_init_edge = dim_size < map_size;
                        auto init_state = std::make_shared<DFA::IterationState>(directive_var, IterationPosition::Init, 1, is_init_unroll, is_init_edge);
                        iter_state_list->push_back(init_state);

                        // 2. Steady case
                        /* LF: TODO I dont think this is correct... see comments on AnalyzeInputMappingSizes()
                         *          Check all the conditions... (dim_size == map_size), (dim_size < map_size) --> Init case???
                         */
                        int num_tp_steady_iters = (dim_size - map_size) / map_ofs;
                        bool has_tp_steady_state = map_size < dim_size;
                        bool has_tp_edge_state = num_tp_steady_iters * map_ofs + map_size < dim_size;

#ifdef DEBUG_ITERATION_ANALYSIS
                        std::cout << "Num tp_steady states = " << dim_size << " - " << map_size << " / " << map_ofs << std::endl;
#endif

                        if(has_tp_steady_state && num_tp_steady_iters > 0) {
                            auto steady_state = std::make_shared<DFA::IterationState>(directive_var, IterationPosition::Steady, num_tp_steady_iters, false, false);
                            iter_state_list->push_back(steady_state);
                        }

                        // 3. Edge case
                        if(!is_init_edge && has_tp_edge_state) {
                            auto edge_state = std::make_shared<DFA::IterationState>(directive_var, IterationPosition::Edge, 1, false, false);
                            iter_state_list->push_back(edge_state);
                        }
                    }  // End of if (directive_class == TemporalMap)
                    else if(directive_class == DFA::directive::DirectiveClass::SpatialMap) {
                        int num_sub_clusters = cluster_->GetNumClusters(false);

                        // 1. Init case
                        int spatial_coverage = map_size + map_ofs * (num_sub_clusters -1);
                        bool is_init_unroll = dim_size <= spatial_coverage;
                        bool is_init_edge = dim_size < spatial_coverage;
                        bool has_init_sp_edge_edge = false;

                        if (dim_size < map_size){has_init_sp_edge_edge = true;}
                        else {
                            int num_init_full_clusters = ((dim_size - map_size) / map_ofs + 1);
                            has_init_sp_edge_edge = (dim_size > num_init_full_clusters * map_ofs);
                        }
                        has_init_sp_edge_edge = has_init_sp_edge_edge && is_init_edge;

                        auto init_state = std::make_shared<DFA::IterationState>(directive_var, IterationPosition::Init, 1, is_init_unroll, is_init_edge, has_init_sp_edge_edge);
                        iter_state_list->push_back(init_state);

                        // 2. Steady case
                        int num_sp_steady_iters = 0;
                        if(dim_size > spatial_coverage) {num_sp_steady_iters = ((dim_size - map_size) / map_ofs + 1) / num_sub_clusters -1;}
                        else {num_sp_steady_iters = 0;}
                        bool has_sp_steady_state = (num_sp_steady_iters > 0);

                        if(has_sp_steady_state) {
                            auto steady_state = std::make_shared<DFA::IterationState>(directive_var, IterationPosition::Steady, num_sp_steady_iters, false, false);
                            iter_state_list->push_back(steady_state);
                        }

                        //3 . Edge case
                        int remaining_items = 0;
                        bool has_sp_edge_edge = false;
                        bool has_edge_state = false;

                        if(dim_size > spatial_coverage) {

                            int tot_coverage = ((num_sp_steady_iters +1)*num_sub_clusters -1)*map_ofs + map_size;
                            if (dim_size == tot_coverage) {remaining_items = 0;}
                            else {remaining_items = dim_size - ((num_sp_steady_iters + 1) * num_sub_clusters)* map_ofs;}

                            if (remaining_items == 0) {
                                has_sp_edge_edge = false;
                                has_edge_state = false;
                            }
                            else {
                                has_edge_state = true;
                                if(remaining_items < map_size) {has_sp_edge_edge = true;}
                                else {
                                    int num_init_full_clusters = ((dim_size - map_size) / map_ofs + 1);
                                    has_sp_edge_edge = (dim_size > num_init_full_clusters*map_ofs);
                                }
                            }
                        }

                        if(has_edge_state) {
                            auto edge_state = std::make_shared<DFA::IterationState>(directive_var, IterationPosition::Edge, 1, false, false, has_sp_edge_edge);
                            iter_state_list->push_back(edge_state);
                        }
                    } // End of else if (directive_class == SpatialMap)

                    valid_iteration_states_->push_back(iter_state_list);
                } // End of for_each (directive in dataflow)
            } // End of void AnalyzeIterationStates


           void ConstructIterationStatusTable() {
               int num_dimensions = valid_iteration_states_->size();

               // Calculate the total number of combinations
               int num_total_cases = 1;
               for (auto& dim_iter_states : *valid_iteration_states_) {
                   num_total_cases *= dim_iter_states->size();
               }

               // Create a vector to hold the current indices for each dimension
               std::vector<int> current_indices(num_dimensions, 0);

               for (int case_id = 0; case_id < num_total_cases; case_id++) {
                   std::shared_ptr<IterationStatus> iter_status_this_case = std::make_shared<IterationStatus>();
                   int num_occurrence = 1;

                   // Add the current combination of states to iter_status_this_case
                   for (int dim = 0; dim < num_dimensions; dim++) {
                       auto& dim_iter_states = valid_iteration_states_->at(dim);
                       auto this_state = dim_iter_states->at(current_indices[dim]);
                       iter_status_this_case->AddIterState(this_state);
                       num_occurrence *= this_state->GetNumOccurrence();
                   }

                   iter_status_this_case->SetNumOccurrences(num_occurrence);
                   iteration_status_table_->push_back(iter_status_this_case);

                   // Increment the indices for the next combination
                   for (int dim = num_dimensions - 1; dim >= 0; dim--) {
                       current_indices[dim]++;
                       if (current_indices[dim] < valid_iteration_states_->at(dim)->size()) {
                           break;
                       }
                       current_indices[dim] = 0; // Reset and carry over to the next dimension
                   }
               }
           }

        }; // End of class IterationAnalysis
    }
}

#endif
