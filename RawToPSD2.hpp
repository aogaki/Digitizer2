#ifndef RAWTOPSD2_HPP
#define RAWTOPSD2_HPP 1

#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "PSD2Data.hpp"
#include "RawData.hpp"

enum class DataType {
  Start,
  Stop,
  Event,
  Unknown,
};

class RawToPSD2
{
 public:
  RawToPSD2(uint32_t nThreads = 1);
  ~RawToPSD2();

  void SetTimeStep(uint32_t timeStep) { fTimeStep = timeStep; }

  // Check start, stop, or event
  DataType AddData(std::unique_ptr<RawData_t> rawData);

  std::unique_ptr<std::vector<std::unique_ptr<PSD2Data_t>>> GetData();

  void SetDumpFlag(bool dumpFlag) { fDumpFlag = dumpFlag; }

 private:
  std::deque<std::unique_ptr<RawData_t>> fRawDataQueue;
  std::mutex fRawDataMutex;
  bool fDumpFlag = false;

  DataType CheckDataType(std::unique_ptr<RawData_t> &rawData);
  bool CheckStart(std::unique_ptr<RawData_t> &rawData);
  bool CheckStop(std::unique_ptr<RawData_t> &rawData);

  std::unique_ptr<std::vector<std::unique_ptr<PSD2Data_t>>> fPSD2DataVec;
  std::mutex fPSD2DataMutex;
  uint32_t fTimeStep = 1;
  bool fDecodeFlag = false;
  void DecodeThread();
  void DecodeData(std::unique_ptr<RawData_t> rawData);
  std::vector<std::thread> fDecodeThreads;
  uint64_t fLastCounter = 0;
};

#endif  // RAWTOPSD2_HPP