/*
 * alt_component_analyzer.h
 *
 *  Created on: Mar 5, 2013
 *      Author: mthurley
 */

#ifndef SHARP_SAT_ALT_COMPONENT_ANALYZER_H_
#define SHARP_SAT_ALT_COMPONENT_ANALYZER_H_



#include <sharpSAT/containers.h>
#include <sharpSAT/unions.h>
#include <sharpSAT/stack.h>
#include <sharpSAT/component_types/component.h>
#include <sharpSAT/component_types/base_packed_component.h>
#include <sharpSAT/component_types/component_archetype.h>



#include <vector>
#include <cmath>
#include <cstddef>
#include <gmpxx.h>

namespace sharpSAT {

class AltComponentAnalyzer {
public:
	AltComponentAnalyzer(LiteralIndexedVector<TriValue> & lit_values) :
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


  bool isUnseenAndActive(VariableIndex v) {
    assert(v <= max_variable_id_);
    return archetype_.var_unseen_in_sup_comp(v);
  }

  // manages the literal whenever it occurs in component analysis
  // returns true iff the underlying variable was unseen before
  //
  bool manageSearchOccurrenceOf(LiteralID lit){
      if(archetype_.var_unseen_in_sup_comp(lit.var())){
        search_stack_.push_back(lit.var());
        archetype_.setVar_seen(lit.var());
        return true;
      }
      return false;
    }
  bool manageSearchOccurrenceAndScoreOf(LiteralID lit){
    var_frequency_scores_[lit.var()] += isActive(lit);
    return manageSearchOccurrenceOf(lit);
  }

  void setSeenAndStoreInSearchStack(VariableIndex v){
    assert(isActive(v));
    search_stack_.push_back(v);
        archetype_.setVar_seen(v);
  }


  void setupAnalysisContext(StackLevel &top, Component & super_comp){
     archetype_.reInitialize(top,super_comp);

     for (auto vt = super_comp.varsBegin(); vt->get<VariableIndex>() != varsSENTINEL; vt++)
       if (isActive(vt->get<VariableIndex>())) {
         archetype_.setVar_in_sup_comp_unseen(vt->get<VariableIndex>());
         var_frequency_scores_[vt->get<VariableIndex>()] = 0;
       }

     for (auto itCl = super_comp.clsBegin(); itCl->get<ClauseIndex>() != clsSENTINEL; itCl++)
         archetype_.setClause_in_sup_comp_unseen(itCl->get<ClauseIndex>());
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

private:
  // the id of the last clause
  // note that clause ID is the clause number,
  // different from the offset of the clause in the literal pool
  ClauseIndex max_clause_id_ = ClauseIndex(0);
  VariableIndex max_variable_id_ = VariableIndex(0);

  // this contains clause offsets of the clauses
  // where each variable occurs in;
  std::vector<ClauseOfs> variable_occurrence_lists_pool_;

  // this is a new idea,
  // for every variable we have a list
  // 0 binarylinks 0 occurrences 0
  // this should give better cache behaviour,
  // because all links of one variable (binary and nonbinray) are found
  // in one contiguous chunk of memory
  std::vector<Variant<ClauseIndex,LiteralID,VariableIndex,unsigned>> unified_variable_links_lists_pool_;


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

  bool isActive(const VariableIndex v) {
    return literal_values_[LiteralID(v, true)] == TriValue::X_TRI;
  }

  typename std::vector<Variant<ClauseIndex,LiteralID,VariableIndex,unsigned>>::iterator beginOfLinkList(VariableIndex v) {
    return unified_variable_links_lists_pool_.begin() + variable_link_list_offsets_[v];
  }

  // stores all information about the component of var
  // in variables_seen_, clauses_seen_ and
  // component_search_stack
  // we have an isolated variable iff
  // after execution component_search_stack.size()==1
  void recordComponentOf(const VariableIndex var);


  void getClause(std::vector<LiteralID> &tmp,
   		       std::vector<LiteralID>::iterator & it_start_of_cl,
   		       VariableIndex omitVar) {
  	  tmp.clear();
  	  for (auto it_lit = it_start_of_cl;*it_lit != SENTINEL_LIT; it_lit++) {
   		  if(it_lit->var() != omitVar)
   			 tmp.push_back(*it_lit);
   	  }
     }


  void searchClause(VariableIndex vt, ClauseIndex clID,
      std::vector<Variant<ClauseIndex,LiteralID,VariableIndex,unsigned>>::iterator pstart_cls) {

    auto original_size = search_stack_.size();
    bool all_lits_active = true;
    for (auto itL = pstart_cls; static_cast<unsigned>(*itL) != 0; itL++) {

      assert(itL->get<LiteralID>().var() <= max_variable_id_);
      if(!archetype_.var_nil(itL->get<LiteralID>().var())) {
        manageSearchOccurrenceAndScoreOf(itL->get<LiteralID>());
      } else {
        assert(!isActive(itL->get<LiteralID>()));
        all_lits_active = false;
        if (isResolved(itL->get<LiteralID>()))
          continue;
        //BEGIN accidentally entered a satisfied clause: undo the search process
        while (search_stack_.size() != original_size) {
          assert(search_stack_.back() <= max_variable_id_);
          archetype_.setVar_in_sup_comp_unseen(search_stack_.back());
          search_stack_.pop_back();
        }
        archetype_.setClause_nil(clID);
        while(static_cast<unsigned>(*itL) != 0) {
          --itL;
          // Type-safety violation!!!
          // Sometimes *itL is not LiteralID.
          // Yet we still force-cast it to LiteralID.
          LiteralID lit(static_cast<unsigned>(*itL));
          if(isActive(lit))
            var_frequency_scores_[lit.var()]--;
        }
        //END accidentally entered a satisfied clause: undo the search process
        break;
      }
    }

    if (!archetype_.clause_nil(clID)){
      var_frequency_scores_[vt]++;
      archetype_.setClause_seen(clID,all_lits_active);
    }
  }


//  void searchThreeClause(VariableIndex vt, ClauseIndex clID, LiteralID * pstart_cls){
//      auto itVEnd = search_stack_.end();
//      bool all_lits_active = true;
//      // LiteralID * pstart_cls = reinterpret_cast<LiteralID *>(p + 1 + *(p+1));
//      for (auto itL = pstart_cls; itL != pstart_cls+2; itL++) {
//        assert(itL->get<VariableIndex>() <= max_variable_id_);
//        if(archetype_.var_nil(itL->get<VariableIndex>())){
//          assert(!isActive(*itL));
//          all_lits_active = false;
//          if (isResolved(*itL))
//            continue;
//          //BEGIN accidentally entered a satisfied clause: undo the search process
//          while (search_stack_.end() != itVEnd) {
//            assert(search_stack_.back() <= max_variable_id_);
//            archetype_.setVar_in_sup_comp_unseen(search_stack_.back());
//            search_stack_.pop_back();
//          }
//          archetype_.setClause_nil(clID);
//          while(itL != pstart_cls - 1)
//            if(isActive(*(--itL)))
//              var_frequency_scores_[itL->get<VariableIndex>()]--;
//          //END accidentally entered a satisfied clause: undo the search process
//          break;
//        } else {
//          assert(isActive(*itL));
//          var_frequency_scores_[itL->get<VariableIndex>()]++;
//          if(isUnseenAndActive(itL->get<VariableIndex>()))
//            setSeenAndStoreInSearchStack(itL->get<VariableIndex>());
//        }
//      }
//
//      if (!archetype_.clause_nil(clID)){
//        var_frequency_scores_[vt]++;
//        archetype_.setClause_seen(clID,all_lits_active);
//      }
//    }
};
} // sharpSAT namespace
#endif /* ALT_COMPONENT_ANALYZER_H_ */
