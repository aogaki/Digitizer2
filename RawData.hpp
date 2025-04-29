#ifndef RAWDATA_HPP
#define RAWDATA_HPP 1

#include <cstdint>
#include <vector>

class RawData
{
 public:
  RawData(size_t dataSize = 0)
  {
    if (dataSize > 0) Resize(dataSize);
  };
  ~RawData() {};

  std::vector<uint8_t> data;
  size_t size;
  uint32_t nEvents;

 private:
  void Resize(size_t size) { data.resize(size); };
};

typedef RawData RawData_t;

#endif  // RAWDATA_HPP