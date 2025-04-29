#include "PSD2.hpp"

#include <CAEN_FELib.h>

#include <bitset>
#include <fstream>
#include <iostream>

PSD2::PSD2() {}
PSD2::~PSD2()
{
  SendCommand("/cmd/Reset");
  Close();
}

bool PSD2::SendSWTrigger()
{
  auto err = CAEN_FELib_SendCommand(fHandle, "/cmd/SendSwTrigger");
  CheckError(err);

  return err == CAEN_FELib_Success;
}

void PSD2::LoadConfig(std::string path)
{
  std::cout << "Load config: " << path << std::endl;
  std::ifstream configFile(path);
  if (!configFile.is_open()) {
    std::cerr << "Failed to open config file" << std::endl;
    exit(1);
  }

  std::string line;
  while (std::getline(configFile, line)) {
    if (line[0] == '#' || line.size() == 0) {
      continue;
    }
    // split by white space
    auto pos = line.find(" ");
    if (pos == std::string::npos) {
      std::cerr << "Invalid config file \n" << line << std::endl;
      exit(1);
    }
    auto key = line.substr(0, pos);
    auto value = line.substr(pos + 1);
    if (key == "URL") {
      fURL = value;
    } else if (key == "Debug") {
      std::transform(value.begin(), value.end(), value.begin(), ::tolower);
      if (value == "true" || value == "1" || value == "yes") {
        fDebugFlag = true;
      } else {
        fDebugFlag = false;
      }
    } else if (key == "Threads") {
      fNThreads = std::stoi(value);
      if (fNThreads < 1) {
        fNThreads = 1;
      }
    } else {
      fConfig.push_back({key, value});
    }
  }
}

bool PSD2::Initialize()
{
  auto status = true;
  if (fURL != "") {
    status &= Open(fURL);
  } else {
    std::cerr << "URL is not set" << std::endl;
    status = false;
  }
  return status;
}

bool PSD2::Open(std::string URL)
{
  std::cout << "Open URL: " << URL << std::endl;
  auto err = CAEN_FELib_Open(URL.c_str(), &fHandle);
  CheckError(err);

  return err == CAEN_FELib_Success;
}

bool PSD2::Close()
{
  std::cout << "Close digitizer" << std::endl;
  auto err = CAEN_FELib_Close(fHandle);
  CheckError(err);

  return err == CAEN_FELib_Success;
}

bool PSD2::Configure()
{
  SendCommand("/cmd/Reset");

  auto status = true;
  for (auto &config : fConfig) {
    status &= SetParameter(config[0], config[1]);
  }

  std::string buf;
  GetParameter("/ch/0/par/ChRecordLengthS", buf);
  auto rl = std::stoi(buf);
  if (rl < 0) {
    std::cerr << "Record length is not set" << std::endl;
    return false;
  }
  fRecordLength = rl;
  std::cout << "Record length: " << fRecordLength << std::endl;

  status &= EndpointConfigure();

  GetParameter("/par/MaxRawDataSize", buf);
  fMaxRawDataSize = std::stoi(buf);
  std::cout << "Max raw data size: " << fMaxRawDataSize << std::endl;

  status &= SendCommand("/cmd/ArmAcquisition");

  return status;
}

bool PSD2::StartAcquisition()
{
  std::cout << "Start acquisition" << std::endl;

  fRawToPSD2 = std::make_unique<RawToPSD2>(fNThreads);
  std::string buf;
  auto sampleRate = 0;
  GetParameter("/par/ADC_SamplRate", buf);
  sampleRate = std::stoi(buf);
  auto timeStep = 1000 / sampleRate;
  fRawToPSD2->SetTimeStep(timeStep);

  fDataTakingFlag = true;
  for (uint32_t i = 0; i < fNThreads; i++) {
    fReadDataThreads.emplace_back(&PSD2::ReadDataThread, this);
  }

  fRawToPSD2->SetDumpFlag(fDebugFlag);

  auto status = true;
  status &= SendCommand("/cmd/SwStartAcquisition");
  return status;
}

bool PSD2::StopAcquisition()
{
  std::cout << "Stop acquisition" << std::endl;

  auto status = SendCommand("/cmd/SwStopAcquisition");
  status &= SendCommand("/cmd/DisarmAcquisition");

  while (true) {
    if (CAEN_FELib_HasData(fReadDataHandle, 100) == CAEN_FELib_Timeout) {
      break;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  fDataTakingFlag = false;

  for (auto &thread : fReadDataThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  return status;
}

bool PSD2::EndpointConfigure()
{
  // Configure endpoint
  uint64_t epHandle;
  uint64_t epFolderHandle;
  bool status = true;
  auto err = CAEN_FELib_GetHandle(fHandle, "/endpoint/RAW", &epHandle);
  status &= CheckError(err);
  err = CAEN_FELib_GetParentHandle(epHandle, nullptr, &epFolderHandle);
  status &= CheckError(err);
  err = CAEN_FELib_SetValue(epFolderHandle, "/par/activeendpoint", "RAW");
  status &= CheckError(err);

  // Set data format
  nlohmann::json readDataJSON = GetReadDataFormatRAW();
  std::string readData = readDataJSON.dump();
  err = CAEN_FELib_GetHandle(fHandle, "/endpoint/RAW", &fReadDataHandle);
  status &= CheckError(err);
  err = CAEN_FELib_SetReadDataFormat(fReadDataHandle, readData.c_str());
  status &= CheckError(err);

  return status;
}

int PSD2::ReadDataWithLock(std::unique_ptr<RawData_t> &rawData, int timeOut)
{
  int retCode = CAEN_FELib_Timeout;

  if (fReadDataMutex.try_lock()) {
    if (CAEN_FELib_HasData(fReadDataHandle, timeOut) == CAEN_FELib_Success) {
      retCode =
          CAEN_FELib_ReadData(fReadDataHandle, timeOut, rawData->data.data(),
                              &(rawData->size), &(rawData->nEvents));
    }
    fReadDataMutex.unlock();
  }
  return retCode;
}

void PSD2::ReadDataThread()
{
  auto rawData = std::make_unique<RawData_t>(fMaxRawDataSize);
  while (fDataTakingFlag) {
    constexpr auto timeOut = 10;
    auto err = ReadDataWithLock(rawData, timeOut);

    if (err == CAEN_FELib_Success) {
      fRawToPSD2->AddData(std::move(rawData));
      rawData = std::make_unique<RawData_t>(fMaxRawDataSize);
    } else if (err == CAEN_FELib_Timeout) {
      // std::string buf;
      // GetParameter("/ch/16/par/ChRealtimeMonitor", buf);
      // std::cout << "Realtime monitor: " << buf << std::endl;
      // GetParameter("/ch/16/par/ChDeadtimeMonitor", buf);
      // std::cout << "Deadtime monitor: " << buf << std::endl;
      // GetParameter("/ch/16/par/ChTriggerCnt", buf);
      // std::cout << "Trigger count: " << buf << std::endl;
      // GetParameter("/ch/16/par/ChSavedEventCnt", buf);
      // std::cout << "Saved event count: " << buf << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

std::unique_ptr<std::vector<std::unique_ptr<PSD2Data>>> PSD2::GetData()
{
  return fRawToPSD2->GetData();
}

nlohmann::json PSD2::GetReadDataFormatRAW()
{
  nlohmann::json readDataJSON;
  nlohmann::json dataJSON;
  dataJSON["name"] = "DATA";
  dataJSON["type"] = "U8";
  dataJSON["dim"] = 1;
  readDataJSON.push_back(dataJSON);
  nlohmann::json sizeJSON;
  sizeJSON["name"] = "SIZE";
  sizeJSON["type"] = "SIZE_T";
  sizeJSON["dim"] = 0;
  readDataJSON.push_back(sizeJSON);
  nlohmann::json nEventsJSON;
  nEventsJSON["name"] = "N_EVENTS";
  nEventsJSON["type"] = "U32";
  nEventsJSON["dim"] = 0;
  readDataJSON.push_back(nEventsJSON);

  return readDataJSON;
}

bool PSD2::CheckError(int err)
{
  auto errCode = static_cast<CAEN_FELib_ErrorCode>(err);
  if (errCode != CAEN_FELib_Success) {
    std::cout << "\x1b[31m";

    auto errName = std::string(32, '\0');
    CAEN_FELib_GetErrorName(errCode, errName.data());
    std::cerr << "Error code: " << errName << std::endl;

    auto errDesc = std::string(256, '\0');
    CAEN_FELib_GetErrorDescription(errCode, errDesc.data());
    std::cerr << "Error description: " << errDesc << std::endl;

    auto details = std::string(1024, '\0');
    CAEN_FELib_GetLastError(details.data());
    std::cerr << "Details: " << details << std::endl;

    std::cout << "\x1b[0m" << std::endl;
  }

  return errCode == CAEN_FELib_Success;
}

bool PSD2::SendCommand(std::string path)
{
  auto err = CAEN_FELib_SendCommand(fHandle, path.c_str());
  CheckError(err);

  return err == CAEN_FELib_Success;
}

bool PSD2::GetParameter(std::string path, std::string &value)
{
  char buf[256];
  auto err = CAEN_FELib_GetValue(fHandle, path.c_str(), buf);
  CheckError(err);
  value = std::string(buf);

  return err == CAEN_FELib_Success;
}

bool PSD2::SetParameter(std::string path, std::string value)
{
  auto err = CAEN_FELib_SetValue(fHandle, path.c_str(), value.c_str());
  CheckError(err);

  return err == CAEN_FELib_Success;
}