/*
 * base_packed_component.h
 *
 *  Created on: Feb 5, 2013
 *      Author: mthurley
 */

#ifndef SHARP_SAT_BASE_PACKED_COMPONENT_H_
#define SHARP_SAT_BASE_PACKED_COMPONENT_H_

#include <sharpSAT/primitive_types.h>

#include <assert.h>
#include <cstddef>
#include <gmpxx.h>

namespace sharpSAT {

template <class T>
 class BitStuffer {
 public:
  BitStuffer(T *data):data_start_(data),p(data){
    *p = 0;
  }

  void stuff(const unsigned val, const unsigned num_bits_val){
      assert(num_bits_val > 0);
      assert((val >> num_bits_val) == 0);
      if(end_of_bits_ == 0)
        *p = 0;
      assert((*p >> end_of_bits_) == 0);
      *p |= val << end_of_bits_;
      end_of_bits_ += num_bits_val;
      if (end_of_bits_ > _bits_per_block){
        //assert(*p);
        end_of_bits_ -= _bits_per_block;
        *(++p) = val >> (num_bits_val - end_of_bits_);
        assert(!(end_of_bits_ == 0) | (*p == 0));
      }
      else if (end_of_bits_ == _bits_per_block){
        end_of_bits_ -= _bits_per_block;
        p++;
      }
  }

  void assert_size(unsigned size){
    if(end_of_bits_ == 0)
       p--;
    assert(p - data_start_ == size - 1);
  }

 private:
  T *data_start_ = nullptr;
  T *p = nullptr;
  // in the current block
  // the bit postion just after the last bit written
  unsigned end_of_bits_ = 0;

  static const unsigned _bits_per_block = (sizeof(T) << 3);

};


class BasePackedComponent {
public:
  static unsigned bits_per_variable() {
    return _bits_per_variable;
  }
  static unsigned variable_mask() {
      return _variable_mask;
  }
  static unsigned bits_per_clause() {
    return _bits_per_clause;
  }

  static unsigned bits_per_block(){
	  return _bits_per_block;
  }

  static unsigned bits_of_data_size(){
    return _bits_of_data_size;
  }

  static void adjustPackSize(VariableIndex maxVarId, ClauseIndex maxClId);

  BasePackedComponent() {}
  BasePackedComponent(unsigned creation_time): creation_time_(creation_time) {}

  ~BasePackedComponent() {
    if (data_)
      delete[] data_;
  }
  static void outbit(unsigned v);


  static unsigned log2(unsigned v){
         // taken from
         // http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogLookup
         static const char LogTable256[256] =
         {
         #define SHARP_SAT_LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
             -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
             SHARP_SAT_LT(4), SHARP_SAT_LT(5), SHARP_SAT_LT(5),
             SHARP_SAT_LT(6), SHARP_SAT_LT(6), SHARP_SAT_LT(6),
             SHARP_SAT_LT(6), SHARP_SAT_LT(7), SHARP_SAT_LT(7),
             SHARP_SAT_LT(7), SHARP_SAT_LT(7), SHARP_SAT_LT(7),
             SHARP_SAT_LT(7), SHARP_SAT_LT(7), SHARP_SAT_LT(7)
         };

         unsigned r;     // r will be lg(v)
         unsigned int t, tt; // temporaries

         if ((tt = (v >> 16)))
         {
           r = (t = (tt >> 8)) ? 24 + LogTable256[t] : 16 + LogTable256[tt];
         }
         else
         {
           r = (t = (v >> 8)) ? 8 + LogTable256[t] : LogTable256[v];
         }
         return r;
  }

  unsigned creation_time() {
    return creation_time_;
  }

  const mpz_class &model_count() const {
    return model_count_;
  }

  unsigned alloc_of_model_count() const{
        return sizeof(mpz_class)
               + model_count_.get_mpz_t()->_mp_alloc * sizeof(mp_limb_t);
  }

  void set_creation_time(unsigned time) {
    creation_time_ = time;
  }

  void set_model_count(const mpz_class &rn, unsigned time) {
    model_count_ = rn;
    length_solution_period_and_flags_ = (time - creation_time_) | (length_solution_period_and_flags_ & 1);
  }

  unsigned hashkey() const  {
    return hashkey_;
  }

  bool modelCountFound(){
    return (length_solution_period_and_flags_ >> 1);
  }

 // inline bool equals(const BasePackedComponent &comp) const;

  // a cache entry is deletable
  // only if it is not connected to an active
  // component in the component stack
  bool isDeletable() const {
    return length_solution_period_and_flags_ & 1;
  }
  void set_deletable() {
    length_solution_period_and_flags_ |= 1;
  }

  void clear() {
    // before deleting the contents of this component,
    // we should make sure that this component is not present in the component stack anymore!
    assert(isDeletable());
    if (data_)
      delete[] data_;
    data_ = nullptr;
  }

  static unsigned _debug_static_val;

protected:
  // data_ contains in packed form the variable indices
  // and clause indices of the component ordered
  // structure is
  // var var ... clause clause ...
  // clauses begin at clauses_ofs_
  unsigned* data_ = nullptr;

  unsigned hashkey_ = 0;

  mpz_class model_count_;

  unsigned creation_time_ = 1;


  // this is:  length_solution_period = length_solution_period_and_flags_ >> 1
  // length_solution_period == 0 means unsolved
  // and the first bit is "delete_permitted"
  unsigned length_solution_period_and_flags_ = 0;

  // deletion is permitted only after
  // the copy of this component in the stack
  // does not exist anymore


protected:
  static unsigned _bits_per_clause, _bits_per_variable; // bitsperentry
  static unsigned _bits_of_data_size; // number of bits needed to store the data size
  static unsigned _data_size_mask;
  static unsigned _variable_mask, _clause_mask;
  static const unsigned _bits_per_block= (sizeof(unsigned) << 3);

};
} // sharpSAT namespace
#endif /* BASE_PACKED_COMPONENT_H_ */
