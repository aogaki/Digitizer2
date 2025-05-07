#ifndef PSD2_HPP
#define PSD2_HPP 1

#include <deque>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "PSD2Data.hpp"
#include "RawData.hpp"
#include "RawToPSD2.hpp"

class PSD2
{
 public:
  PSD2();
  ~PSD2();

  bool Initialize();
  bool Configure();
  bool StartAcquisition();
  bool StopAcquisition();

  bool SendSWTrigger();

  bool CheckStatus();

  void LoadConfig(std::string path);

  uint64_t GetHandle() { return fHandle; }

  std::unique_ptr<std::vector<std::unique_ptr<PSD2Data_t>>> GetData();

 private:
  uint64_t fHandle;
  size_t fMaxRawDataSize;

  std::string fURL = "";
  bool fDebugFlag = false;
  uint32_t fNThreads = 1;
  std::vector<std::array<std::string, 2>> fConfig;

  bool CheckError(int err);
  bool SendCommand(std::string path);
  bool GetParameter(std::string path, std::string &value);
  bool SetParameter(std::string path, std::string value);
  nlohmann::json GetReadDataFormatRAW();

  bool Open(std::string URL);
  bool Close();

  std::mutex fDataMutex;
  bool fDataTakingFlag;
  void ReadDataThread();
  int ReadDataWithLock(std::unique_ptr<RawData_t> &dummy, int timeOut);
  std::vector<std::thread> fReadDataThreads;
  std::mutex fReadDataMutex;

  // RawToPSD2 converter
  std::unique_ptr<RawToPSD2> fRawToPSD2;

  // Configure and read data structure
  uint64_t fReadDataHandle;
  uint64_t fRecordLength;
  bool EndpointConfigure();
};

#endif  // PSD2_HPP