/*
 * base_packed_component.cpp
 *
 *  Created on: Feb 4, 2013
 *      Author: mthurley
 */
#include <sharpSAT/component_types/base_packed_component.h>
#include <math.h>
#include <iostream>

namespace sharpSAT {

unsigned BasePackedComponent::_bits_per_clause = 0;
unsigned BasePackedComponent::_bits_per_variable = 0; // bitsperentry
unsigned BasePackedComponent::_variable_mask = 0;
unsigned BasePackedComponent::_clause_mask = 0; // bitsperentry
unsigned BasePackedComponent::_debug_static_val=0;
unsigned BasePackedComponent::_bits_of_data_size=0;
unsigned BasePackedComponent::_data_size_mask = 0;


void BasePackedComponent::adjustPackSize(VariableIndex maxVarId,
    ClauseIndex maxClId) {

  _bits_per_variable = log2(static_cast<unsigned>(maxVarId)) + 1;
  _bits_per_clause   = log2(static_cast<unsigned>(maxClId)) + 1;

  _bits_of_data_size = log2(static_cast<unsigned>(maxVarId)
                          + static_cast<unsigned>(maxClId) ) + 1;

  _variable_mask = _clause_mask = _data_size_mask = 0;
  for (unsigned int i = 0; i < _bits_per_variable; i++)
    _variable_mask = (_variable_mask << 1) + 1;
  for (unsigned int i = 0; i < _bits_per_clause; i++)
    _clause_mask = (_clause_mask << 1) + 1;
  for (unsigned int i = 0; i < _bits_of_data_size; i++)
    _data_size_mask = (_data_size_mask << 1) + 1;
}

void BasePackedComponent::outbit(unsigned v){
    for(auto i = 0; i < 32; ++i) {
        std::cout << ((v&2147483648)? "1" : "0");
        v &= 2147483648-1;
        v <<= 1;
    }
}


} // sharpSAT namespace