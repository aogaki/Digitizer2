#ifndef PSD2STATS_HPP
#define PSD2STATS_HPP 1

#include <CAEN_FELib.h>

#include <iostream>
#include <string>
#include <vector>

class PSD2StatsData
{
 public:
  PSD2StatsData() {};
  ~PSD2StatsData() {};

  std::vector<int32_t> triggerCount;
  std::vector<int32_t> savedEventCount;
};

#endif