/*
 * solver.h
 *
 *  Created on: Aug 23, 2012
 *      Author: marc
 */

#ifndef SHARP_SAT_SOLVER_H_
#define SHARP_SAT_SOLVER_H_


#include <sharpSAT/statistics.h>
#include <sharpSAT/instance.h>
#include <sharpSAT/component_management.h>
#include <sharpSAT/solver_config.h>
#include <sharpSAT/stopwatch.h>

namespace sharpSAT {

enum class retStateT {
	EXIT, RESOLVED, PROCESS_COMPONENT, BACKTRACK
};

class Solver: public Instance {
public:
	Solver():
        comp_manager_(config_, statistics_, literal_values_) {
		stopwatch_.setTimeBound(config_.time_bound_seconds);
	}

	/**
	 * Attempts to solve the #SAT instance.
	 *
	 * The instance must be loaded via the \ref Instance public API
	 * (\ref Instance::initialize, \ref Instance::add_clause and
	 * \ref Instance::finalize).
	 *
	 * Time-limit in seconds can be set by `setTimeBound()`.
	 *
	 * Calculation result can be found in `statistics().exit_state_`
	 * and `statistics().final_solution_count()`.
	 */
	void solve();

	void load_and_solve(const std::string & file_name);

	SolverConfiguration &config() {
		return config_;
	}

	DataAndStatistics &statistics() {
	        return statistics_;
	}
	void setTimeBound(long int i) {
		config().time_bound_seconds = i;
		stopwatch_.setTimeBound(i);
	}

private:
	SolverConfiguration config_;

	DecisionStack stack_; // decision stack

	//! When a literal is assigned, it is pushed to this vector.
	std::vector<LiteralID> literal_stack_;

	StopWatch stopwatch_;

	ComponentManager comp_manager_;

	// the last time conflict clauses have been deleted
	unsigned long last_ccl_deletion_time_ = 0;
	// the last time the conflict clause storage has been compacted
	unsigned long last_ccl_cleanup_time_ = 0;

	/*!
	 * Simple preprocessing should be called before backtracking.
	 *
	 * Propagates all unit clauses (repeatedly)
	 * and calls compactification methods.
	 */
	bool simplePreProcess();

	bool prepFailedLiteralTest();
	// we assert that the formula is consistent
	// and has not been found UNSAT yet
	// hard wires all assertions in the literal stack into the formula
	// removes all set variables and essentially reinitiallizes all
	// further data
	void HardWireAndCompact();

	SOLVER_StateT countSAT();

	void decideLiteral();
	bool bcp();


	 void decayActivitiesOf(Component & comp) {
	   for (auto it = comp.varsBegin(); it->get<VariableIndex>() != varsSENTINEL; it++) {
	          literal(LiteralID(it->get<VariableIndex>(), true)).activity_score_ *=0.5;
	          literal(LiteralID(it->get<VariableIndex>(), false)).activity_score_ *=0.5;
	       }
	}
	///  this method performs Failed literal tests online
	bool implicitBCP();

	/*!
	 * Boolean constraint propagation algorithm.
	 *
	 * Starts propagating all literals in \ref literal_stack_,
	 * begining at offset `start_at_stack_ofs`.
	 */
	bool BCP(size_t start_at_stack_ofs);

	retStateT backtrack();

	// if on the current decision level
	// a second branch can be visited, RESOLVED is returned
	// otherwise returns BACKTRACK
	retStateT resolveConflict();

	/////////////////////////////////////////////
	//  BEGIN small helper functions
	/////////////////////////////////////////////

	float scoreOf(VariableIndex v) {
		float score = comp_manager_.scoreOf(v);
		score += 10.0 * literal(LiteralID(v, true)).activity_score_;
		score += 10.0 * literal(LiteralID(v, false)).activity_score_;
//		score += (10*stack_.get_decision_level()) * literal(LiteralID(v, true)).activity_score_;
//		score += (10*stack_.get_decision_level()) * literal(LiteralID(v, false)).activity_score_;

		return score;
	}

	/*!
	 * Assign the literal to be `true` (if not assigned already).
	 *
	 * Side-effects:
	 * - Variable data structure is updated.
	 * - Both \ref literal_stack_ and \ref literal_values_ are updated.
	 * - If the antecendant is a clause, its score is increased.
	 *
	 * The "undo" method is \ref unSet.
	 *
	 * \param[in] lit literal that will be assigned `true`
	 * \param[in] ant cause of the assignment
	 *
	 * \returns `true` if the literal has not been assigned already
	 */
	bool setLiteralIfFree(LiteralID lit,
			Antecedent ant = Antecedent()) {
		if (!isActive(lit))
			return false;
		var(lit).decision_level = stack_.get_decision_level();
		var(lit).ante = ant;
		literal_stack_.push_back(lit);
		if (ant.isAClause() && ant.asCl() != NOT_A_CLAUSE)
			getHeaderOf(ant.asCl()).increaseScore();
		literal_values_[lit] = TriValue::T_TRI;
		literal_values_[lit.neg()] = TriValue::F_TRI;
		return true;
	}

	void printOnlineStats();

	void print(std::vector<LiteralID> &vec);
	void print(std::vector<unsigned> &vec);


	void setConflictState(LiteralID litA, LiteralID litB) {
		violated_clause.clear();
		violated_clause.push_back(litA);
		violated_clause.push_back(litB);
	}
	void setConflictState(ClauseOfs cl_ofs) {
		getHeaderOf(cl_ofs).increaseScore();
		violated_clause.clear();
		for (auto it = beginOf(cl_ofs); *it != SENTINEL_LIT; it++)
			violated_clause.push_back(*it);
	}

	std::vector<LiteralID>::const_iterator TOSLiteralsBegin() {
		return literal_stack_.begin() + stack_.top().literal_stack_ofs();
	}

	void initStack(unsigned int num_variables) {
		stack_.clear();
		stack_.reserve(num_variables);
		literal_stack_.clear();
		literal_stack_.reserve(num_variables);
		// initialize the stack to contain at least level zero
		stack_.push_back(StackLevel(1, 0, 2));
		stack_.back().changeBranch();
	}

	const LiteralID &TOS_decLit() {
		assert(stack_.top().literal_stack_ofs() < literal_stack_.size());
		return literal_stack_[stack_.top().literal_stack_ofs()];
	}

	void reactivateTOS() {
		for (auto it = TOSLiteralsBegin(); it != literal_stack_.end(); it++)
			unSet(*it);
		comp_manager_.cleanRemainingComponentsOf(stack_.top());
		literal_stack_.resize(stack_.top().literal_stack_ofs());
		stack_.top().resetRemainingComps();
	}

	bool fail_test(LiteralID lit) {
		size_t sz = literal_stack_.size();
		// we increase the decLev artificially
		// s.t. after the tentative BCP call, we can learn a conflict clause
		// relative to the assignment of *jt
		stack_.startFailedLitTest();
		setLiteralIfFree(lit);

		assert(!hasAntecedent(lit));

		bool bSucceeded = BCP(sz);
		if (!bSucceeded)
			recordAllUIPCauses();

		stack_.stopFailedLitTest();

		while (literal_stack_.size() > sz) {
			unSet(literal_stack_.back());
			literal_stack_.pop_back();
		}
		return bSucceeded;
	}
	/////////////////////////////////////////////
	//  BEGIN conflict analysis
	/////////////////////////////////////////////

	// if the state name is CONFLICT,
	// then violated_clause contains the clause determining the conflict;
	std::vector<LiteralID> violated_clause;
	// this is an array of all the clauses found
	// during the most recent conflict analysis
	// it might contain more than 2 clauses
	// but always will have:
	//      uip_clauses_.front() the 1UIP clause found
	//      uip_clauses_.back() the lastUIP clause found
	//  possible clauses in between will be other UIP clauses
	std::vector<std::vector<LiteralID> > uip_clauses_;

	// the assertion level of uip_clauses_.back()
	// or (if the decision variable did not have an antecedent
	// before) then assertionLevel_ == DL;
	int assertion_level_ = 0;

	// build conflict clauses from most recent conflict
	// as stored in state_.violated_clause
	// solver state must be CONFLICT to work;
	// this first method record only the last UIP clause
	// so as to create clause that asserts the current decision
	// literal
	void recordLastUIPCauses();
	void recordAllUIPCauses();

	void minimizeAndStoreUIPClause(LiteralID uipLit,
			std::vector<LiteralID> & tmp_clause,
			const VariableIndexedVector<bool>& seen);
	void storeUIPClause(LiteralID uipLit, std::vector<LiteralID> & tmp_clause);
	int getAssertionLevel() const {
		return assertion_level_;
	}

	/////////////////////////////////////////////
	//  END conflict analysis
	/////////////////////////////////////////////
};
} // sharpSAT namespace
#endif /* SOLVER_H_ */
