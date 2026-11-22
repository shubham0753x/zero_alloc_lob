#pragma once
#include <vector>
#include <cstdint>

class Flat_heirarchical_bit_mask{
  std::vector<uint64_t> hmask;
  std::vector<int> level_offsets;
  public:
  Flat_heirarchical_bit_mask() = default;
  Flat_heirarchical_bit_mask(int n_elements){
    int curr_offset = 0;
    while(true){
       level_offsets.push_back(curr_offset);
      int curr_size = (n_elements + 63) >> 6;
      curr_offset += curr_size;
      n_elements = curr_size;
      if(curr_size == 1) break;
    }
    hmask.resize(curr_offset,0);
    std::fill(hmask.begin(), hmask.end(), 0);
  }

  void activate(int indx){
    int curr_indx = indx;
    for(int offset: level_offsets){
        int in_level_indx = curr_indx >> 6;
        int pos = curr_indx&63;
        bool already_non_zero = hmask[offset + in_level_indx] != 0;
        hmask[offset + in_level_indx] |= 1ULL << pos;
        if(already_non_zero) return;
        curr_indx = in_level_indx;
    }
  }

  void deactivate(int indx){
    int curr_indx = indx;
    for(int offset: level_offsets){
        int in_level_indx = curr_indx >> 6;
        int pos = curr_indx&63;
        hmask[offset + in_level_indx] &= ~(uint64_t{1}<<pos);
        if(hmask[offset + in_level_indx]) return;
        curr_indx = in_level_indx;
    }
  }

  int get_most_left_active() const {
    if(hmask.back() == 0) return -1;
    int curr_indx = 0;
    for(int level = level_offsets.size() - 1; level>=0;level --){
        curr_indx = (curr_indx << 6) + __builtin_ctzll(hmask[curr_indx + level_offsets[level]]);
    }
    return curr_indx;
  }

  int get_most_right_active() const {
    if(hmask.back() == 0) return -1;
    int curr_indx = 0;
    for(int level = level_offsets.size() - 1; level>=0;level --){
        curr_indx = (curr_indx << 6) + 63 - __builtin_clzll(hmask[curr_indx + level_offsets[level]]);
    }
    return curr_indx;
  }

};

