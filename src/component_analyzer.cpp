/*
 * component_analyzer.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: mthurley
 */

#include <sharpSAT/component_analyzer.h>

using namespace std;

namespace sharpSAT {

static_assert(((sizeof(CAClauseHeader) - 2 * sizeof(LiteralID)) / sizeof(LiteralID)) * sizeof(LiteralID) + 2 * sizeof(LiteralID) == sizeof(CAClauseHeader),
                "Modified size of CAClauseHeader and LiteralID must be evenly divisible");

void STDComponentAnalyzer::initialize(LiteralIndexedVector<Literal> & literals,
    vector<LiteralID> &lit_pool) {

  unsigned max_variable_id = static_cast<unsigned>(literals.end_lit().var()) - 1;
  max_variable_id_ = VariableIndex(max_variable_id);

  search_stack_.reserve(max_variable_id + 1);
  var_frequency_scores_.resize(max_variable_id + 1, 0);
  variable_occurrence_lists_pool_.clear();
  variable_link_list_offsets_.resize(max_variable_id + 1, 0);

  literal_pool_.reserve(lit_pool.size());


  map_clause_id_to_ofs_.clear();
  map_clause_id_to_ofs_.push_back(ClauseOfs(0));

  VariableIndexedVector<vector<ClauseOfs>> occs_(max_variable_id + 1);
  ClauseOfs current_clause_ofs(0);
  max_clause_id_ = ClauseIndex(0);
  unsigned curr_clause_length = 0;
  for (auto it_lit = lit_pool.begin(); it_lit < lit_pool.end(); it_lit++) {
    if (*it_lit == SENTINEL_LIT) {

      if (it_lit + 1 == lit_pool.end()) {
        literal_pool_.push_back(SENTINEL_LIT);
        break;
      }

      ++max_clause_id_;
      literal_pool_.push_back(SENTINEL_LIT);
      for (unsigned i = 0; i < CAClauseHeader::overheadInLits(); i++)
        literal_pool_.push_back(LiteralID());
      current_clause_ofs = ClauseOfs(literal_pool_.size());
      getHeaderOf(current_clause_ofs).clause_id = max_clause_id_;
      it_lit += ClauseHeader::overheadInLits();
      curr_clause_length = 0;

      assert(ClauseIndex(map_clause_id_to_ofs_.size()) == max_clause_id_);
      map_clause_id_to_ofs_.push_back(current_clause_ofs);

    } else {
      assert(it_lit->var() <= max_variable_id_);
      literal_pool_.push_back(*it_lit);
      curr_clause_length++;
      occs_[it_lit->var()].push_back(current_clause_ofs);
    }
  }

  ComponentArchetype::initArrays(max_variable_id_, max_clause_id_);
  // the unified link list
  unified_variable_links_lists_pool_.clear();
  unified_variable_links_lists_pool_.push_back(0u);
  unified_variable_links_lists_pool_.push_back(0u);

  for (VariableIndex v(1); v < VariableIndex(occs_.size()); ++v) {
    variable_link_list_offsets_[v] = unified_variable_links_lists_pool_.size();

    for (auto l : literals[LiteralID(v, false)].binary_links_)
      if (l != SENTINEL_LIT) {
        unified_variable_links_lists_pool_.push_back(l.var());
      }
    for (auto l : literals[LiteralID(v, true)].binary_links_)
      if (l != SENTINEL_LIT) {
        unified_variable_links_lists_pool_.push_back(l.var());
      }
    unified_variable_links_lists_pool_.push_back(varsSENTINEL);

    unified_variable_links_lists_pool_.insert(
        unified_variable_links_lists_pool_.end(),
        occs_[v].begin(),
        occs_[v].end());

    unified_variable_links_lists_pool_.push_back(SENTINEL_CL);
  }
}






void STDComponentAnalyzer::recordComponentOf(const VariableIndex var) {

  search_stack_.clear();
  search_stack_.push_back(var);

  archetype_.setVar_seen(var);

  for (auto vt = search_stack_.begin();
      vt != search_stack_.end(); vt++) {
    // the for-loop is applicable here because componentSearchStack.capacity() == countAllVars()
    //BEGIN traverse binary clauses
    assert(isActive(*vt));
    auto pvar = beginOfLinkList(*vt);
    for (; pvar->get<VariableIndex>() != varsSENTINEL; pvar++) {
      if(isUnseenAndActive(pvar->get<VariableIndex>())) {
        setSeenAndStoreInSearchStack(pvar->get<VariableIndex>());
        var_frequency_scores_[pvar->get<VariableIndex>()]++;
        var_frequency_scores_[*vt]++;
      }
    }
    //END traverse binary clauses

    // start traversing links to long clauses
    // not that that list starts right after the 0 termination of the prvious list
    // hence  pcl_ofs = pvar + 1
    for (auto pcl_ofs = pvar + 1; pcl_ofs->get<ClauseOfs>() != SENTINEL_CL; pcl_ofs++) {
      // Type-safety problem. Is pcl_ofs a ClauseOfs or ClauseIndex?
      ClauseIndex clID = getClauseID(pcl_ofs->get<ClauseOfs>());
      if(archetype_.clause_unseen_in_sup_comp(clID)){
        auto itVEnd = search_stack_.end();
        bool all_lits_active = true;
        for (auto itL = beginOfClause(pcl_ofs->get<ClauseOfs>()); *itL != SENTINEL_LIT; itL++) {
          assert(itL->var() <= max_variable_id_);
          if(archetype_.var_nil(itL->var())){
            assert(!isActive(*itL));
            all_lits_active = false;
            if (isResolved(*itL))
              continue;
            //BEGIN accidentally entered a satisfied clause: undo the search process
            while (search_stack_.end() != itVEnd) {
              assert(search_stack_.back() <= max_variable_id_);
              archetype_.setVar_in_sup_comp_unseen(search_stack_.back());
              search_stack_.pop_back();
            }
            archetype_.setClause_nil(clID);
            for (auto itX = beginOfClause(pcl_ofs->get<ClauseOfs>()); itX != itL; itX++) {
              if (var_frequency_scores_[itX->var()] > 0)
                var_frequency_scores_[itX->var()]--;
            }
            //END accidentally entered a satisfied clause: undo the search process
            break;
          } else {
            assert(isActive(*itL));
            var_frequency_scores_[itL->var()]++;
            if(isUnseenAndActive(itL->var()))
              setSeenAndStoreInSearchStack(itL->var());
          }
        }

        if (archetype_.clause_nil(clID))
          continue;
        archetype_.setClause_seen(clID);
        if(all_lits_active)
          archetype_.setClause_all_lits_active(clID);

#ifndef NDEBUG
        test_checkArchetypeRepForClause(pcl_ofs);
#endif
      }
    }
  }
}

} // sharpSAT namespace
