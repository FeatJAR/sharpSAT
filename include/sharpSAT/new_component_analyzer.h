/*
 * new_component_analyzer.h
 *
 *  Created on: Mar 1, 2013
 *      Author: mthurley
 */

#ifndef SHARP_SAT_NEW_COMPONENT_ANALYZER_H_
#define SHARP_SAT_NEW_COMPONENT_ANALYZER_H_



#include <sharpSAT/containers.h>
#include <sharpSAT/stack.h>
#include <sharpSAT/component_types/component.h>
#include <sharpSAT/component_types/base_packed_component.h>
#include <sharpSAT/component_types/component_archetype.h>


#include <vector>
#include <cmath>
#include <cstddef>
#include <gmpxx.h>

namespace sharpSAT {

struct CAClauseHeader {
  ClauseIndex clause_id = ClauseIndex(0);
  LiteralID lit_A;
  LiteralID lit_B;

  static unsigned overheadInLits() {
    return (sizeof(CAClauseHeader) - 2 * sizeof(LiteralID)) / sizeof(LiteralID);
  }
};


class NewComponentAnalyzer;


class NewComponentAnalyzer {
public:
	NewComponentAnalyzer(LiteralIndexedVector<TriValue> & lit_values) :
        literal_values_(lit_values) {
  }

  unsigned scoreOf(VariableIndex v) {
    return var_frequency_scores_[v];
  }

  ComponentArchetype &current_archetype(){
    return archetype_;
  }

  void initialize(LiteralIndexedVector<Literal> & literals,
      std::vector<LiteralID> &lit_pool);


  bool isUnseenAndActive(VariableIndex v){
    assert(v <= max_variable_id_);
    return archetype_.var_unseen_in_sup_comp(v);
  }

  void setSeenAndStoreInSearchStack(VariableIndex v){
    assert(isActive(v));
    search_stack_.push_back(v);
    archetype_.setVar_seen(v);
  }


  void setupAnalysisContext(StackLevel &top, Component & super_comp){
     archetype_.reInitialize(top,super_comp);

     for (auto vt = super_comp.varsBegin(); VariableIndex(*vt) != varsSENTINEL; vt++)
       if (isActive(VariableIndex(*vt))) {
         archetype_.setVar_in_sup_comp_unseen(VariableIndex(*vt));
         var_frequency_scores_[VariableIndex(*vt)] = 0;
       }

     for (auto itCl = super_comp.clsBegin(); ClauseIndex(*itCl) != clsSENTINEL; itCl++)
         archetype_.setClause_in_sup_comp_unseen(ClauseIndex(*itCl));
  }

  // returns true, iff the component found is non-trivial
  bool exploreRemainingCompOf(VariableIndex v) {
    assert(archetype_.var_unseen_in_sup_comp(v));
    recordComponentOf(v);

    if (search_stack_.size() == 1) {
      archetype_.stack_level().includeSolution(2);
      archetype_.setVar_in_other_comp(v);
      return false;
    }
    return true;
  }


  inline Component *makeComponentFromArcheType(){
    return archetype_.makeComponentFromState(search_stack_.size());
  }

  ClauseIndex max_clause_id(){
     return max_clause_id_;
  }

  VariableIndex max_variable_id(){
    return max_variable_id_;
  }

  ComponentArchetype &getArchetype(){
    return archetype_;
  }

  //begin DEBUG
  void test_checkArchetypeRepForClause(unsigned *pcl_ofs){
      ClauseIndex clID = getClauseID(ClauseOfs(*pcl_ofs));
      bool all_a = true;
      for (auto itL = beginOfClause(ClauseOfs(*pcl_ofs)); *itL != SENTINEL_LIT; itL++) {
        if(!isActive(*itL))
          all_a = false;
      }
      assert(all_a == archetype_.clause_all_lits_active(clID));
  }
  //end DEBUG

private:
  // the id of the last clause
  // note that clause ID is the clause number,
  // different from the offset of the clause in the literal pool
  ClauseIndex max_clause_id_ = ClauseIndex(0);
  VariableIndex max_variable_id_ = VariableIndex(0);

  // pool of clauses as lists of LiteralIDs
  // Note that with a clause begin position p we have
  // the clause ID at position p-1
  std::vector<LiteralID> literal_pool_;

  // this contains clause offsets of the clauses
  // where each variable occurs in;
  std::vector<ClauseOfs> variable_occurrence_lists_pool_;

  // this is a new idea,
  // for every variable we have a list
  // 0 binarylinks 0 occurrences 0
  // this should give better cache behaviour,
  // because all links of one variable (binary and nonbinray) are found
  // in one contiguous chunk of memory
  std::vector<unsigned> unified_variable_links_lists_pool_;

  std::vector<ClauseOfs> map_clause_id_to_ofs_;
  VariableIndexedVector<unsigned> variable_link_list_offsets_;
  LiteralIndexedVector<TriValue> & literal_values_;

  VariableIndexedVector<unsigned> var_frequency_scores_;

  ComponentArchetype  archetype_;

  std::vector<VariableIndex> search_stack_;

  bool isResolved(const LiteralID lit) {
    return literal_values_[lit] == TriValue::F_TRI;
  }

  bool isSatisfied(const LiteralID lit) {
    return literal_values_[lit] == TriValue::T_TRI;
  }
  bool isActive(const LiteralID lit) {
      return literal_values_[lit] == TriValue::X_TRI;
  }

  bool isSatisfiedByFirstTwoLits(ClauseOfs cl_ofs) {
      return isSatisfied(getHeaderOf(cl_ofs).lit_A) || isSatisfied(getHeaderOf(cl_ofs).lit_B);
    }

  bool isActive(const VariableIndex v) {
    return literal_values_[LiteralID(v, true)] == TriValue::X_TRI;
  }

  ClauseIndex getClauseID(ClauseOfs cl_ofs) {
    return ClauseIndex(
     reinterpret_cast<CAClauseHeader *>(&literal_pool_[
       static_cast<unsigned>(cl_ofs) - CAClauseHeader::overheadInLits()
      ])->clause_id
    );
  }

  CAClauseHeader &getHeaderOf(ClauseOfs cl_ofs) {
    return *reinterpret_cast<CAClauseHeader *>(&literal_pool_[
      static_cast<unsigned>(cl_ofs) - CAClauseHeader::overheadInLits()]);
  }

  unsigned *beginOfLinkList(VariableIndex v) {
    return &unified_variable_links_lists_pool_[variable_link_list_offsets_[v]];
  }

  std::vector<LiteralID>::iterator beginOfClause(ClauseOfs cl_ofs) {
    return literal_pool_.begin() + static_cast<unsigned>(cl_ofs);
  }

  // stores all information about the component of var
  // in variables_seen_, clauses_seen_ and
  // component_search_stack
  // we have an isolated variable iff
  // after execution component_search_stack.size()==1
  void recordComponentOf(const VariableIndex var);

  void pushLitsInto(std::vector<unsigned> &target,
		       const std::vector<LiteralID> &lit_pool,
		       unsigned start_of_cl,
		       LiteralID & omitLit){
	  for (auto it_lit = lit_pool.begin() + start_of_cl;
			  (*it_lit != SENTINEL_LIT); it_lit++) {
		  if(it_lit->var() != omitLit.var())
			  target.push_back(it_lit->raw());
	  }
	  target.push_back(SENTINEL_LIT.raw());
  }

};
} // sharpSAT namespace
#endif /* NEW_COMPONENT_ANALYZER_H_ */
