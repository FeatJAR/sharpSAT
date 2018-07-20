/*
 * component_management.h
 *
 *  Created on: Aug 23, 2012
 *      Author: Marc Thurley
 */

#ifndef SHARP_SAT_COMPONENT_MANAGEMENT_H_
#define SHARP_SAT_COMPONENT_MANAGEMENT_H_



#include <sharpSAT/alt_component_analyzer.h>
#include <sharpSAT/component_cache.h>
#include <sharpSAT/containers.h>
#include <sharpSAT/stack.h>
#include <sharpSAT/solver_config.h>
#include <sharpSAT/component_types/component.h>

#include <vector>
#include <cstddef>
#include <gmpxx.h>

namespace sharpSAT {

typedef AltComponentAnalyzer ComponentAnalyzer;

class ComponentManager {
public:
  ComponentManager(SolverConfiguration &config, DataAndStatistics &statistics,
        LiteralIndexedVector<TriValue> & lit_values) :
        config_(config), cache_(statistics),
        ana_(lit_values) {
  }

  void initialize(LiteralIndexedVector<Literal> & literals,
        std::vector<LiteralID> &lit_pool);

  unsigned scoreOf(VariableIndex v) {
      return ana_.scoreOf(v);
  }

  void cacheModelCountOf(unsigned stack_comp_id, const mpz_class &value) {
    if (config_.perform_component_caching)
      cache_.storeValueOf(component_stack_[stack_comp_id]->id(), value);
  }

  Component & superComponentOf(StackLevel &lev) {
    assert(component_stack_.size() > lev.super_component());
    return *component_stack_[lev.super_component()];
  }

  unsigned component_stack_size() {
    return component_stack_.size();
  }

  void cleanRemainingComponentsOf(StackLevel &top) {
    while (component_stack_.size() > top.remaining_components_ofs()) {
      if (cache_.hasEntry(component_stack_.back()->id()))
        cache_.entry(component_stack_.back()->id()).set_deletable();
      delete component_stack_.back();
      component_stack_.pop_back();
    }
    assert(top.remaining_components_ofs() <= component_stack_.size());
  }

  Component & currentRemainingComponentOf(StackLevel &top) {
    assert(component_stack_.size() > top.currentRemainingComponent());
    return *component_stack_[top.currentRemainingComponent()];
  }

  // checks for the next yet to explore remaining component of top
  // returns true if a non-trivial non-cached component
  // has been found and is now stack_.TOS_NextComp()
  // returns false if all components have been processed;
  inline bool findNextRemainingComponentOf(StackLevel &top);

  inline void recordRemainingCompsFor(StackLevel &top);

  inline void sortComponentStackRange(size_t start, size_t end);

  void gatherStatistics(){
//     statistics_.cache_bytes_memory_usage_ =
//	     cache_.recompute_bytes_memory_usage();
    cache_.compute_byte_size_infrasture();
  }

  void removeAllCachePollutionsOf(StackLevel &top);

private:

  SolverConfiguration &config_;

  std::vector<Component *> component_stack_;
  ComponentCache cache_;
  ComponentAnalyzer ana_;
};


void ComponentManager::sortComponentStackRange(size_t start, size_t end){
    assert(start <= end);
    // sort the remaining components for processing
    for (size_t i = start; i < end; i++)
      for (size_t j = i + 1; j < end; j++) {
        if (component_stack_[i]->num_variables()
            < component_stack_[j]->num_variables())
          std::swap(component_stack_[i], component_stack_[j]);
      }
  }

bool ComponentManager::findNextRemainingComponentOf(StackLevel &top) {
    // record Remaining Components if there are none!
    if (component_stack_.size() <= top.remaining_components_ofs())
      recordRemainingCompsFor(top);
    assert(!top.branch_found_unsat());
    if (top.hasUnprocessedComponents())
      return true;
    // if no component remains
    // make sure, at least that the current branch is considered SAT
    top.includeSolution(1);
    return false;
  }


void ComponentManager::recordRemainingCompsFor(StackLevel &top) {
   Component & super_comp = superComponentOf(top);
   size_t new_comps_start_ofs = component_stack_.size();

   ana_.setupAnalysisContext(top, super_comp);

   for (auto vt = super_comp.varsBegin(); VariableIndex(*vt) != varsSENTINEL; vt++) 
     if (ana_.isUnseenAndActive(VariableIndex(*vt)) &&
         ana_.exploreRemainingCompOf(VariableIndex(*vt))){

       Component *p_new_comp = ana_.makeComponentFromArcheType();
       CacheableComponent *packed_comp = new CacheableComponent(ana_.getArchetype().current_comp_for_caching_);
         if (!cache_.manageNewComponent(top, *packed_comp)){
            component_stack_.push_back(p_new_comp);
            p_new_comp->set_id(cache_.storeAsEntry(*packed_comp, super_comp.id()));
         }
         else {
           delete packed_comp;
           delete p_new_comp;
         }
     }

   top.set_unprocessed_components_end(component_stack_.size());
   sortComponentStackRange(new_comps_start_ofs, component_stack_.size());
}
} // sharpSAT namespace
#endif /* COMPONENT_MANAGEMENT_H_ */
