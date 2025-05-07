#include "RawToPSD2.hpp"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <iomanip>
#include <iostream>

RawToPSD2::RawToPSD2(uint32_t nThreads)
{
  if (nThreads < 1) {
    nThreads = 1;
  }
  fPSD2DataVec = std::make_unique<std::vector<std::unique_ptr<PSD2Data_t>>>();
  fDecodeFlag = true;
  for (uint32_t i = 0; i < nThreads; i++) {
    fDecodeThreads.emplace_back(&RawToPSD2::DecodeThread, this);
  }
}

RawToPSD2::~RawToPSD2()
{
  fDecodeFlag = false;
  for (auto &thread : fDecodeThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

std::unique_ptr<std::vector<std::unique_ptr<PSD2Data_t>>> RawToPSD2::GetData()
{
  auto data = std::make_unique<std::vector<std::unique_ptr<PSD2Data_t>>>();
  {
    std::lock_guard<std::mutex> lock(fPSD2DataMutex);
    data->swap(*fPSD2DataVec);
    fPSD2DataVec->clear();
  }
  return data;
}

void RawToPSD2::DecodeThread()
{
  while (fDecodeFlag) {
    std::unique_ptr<RawData_t> rawData = nullptr;
    {
      std::lock_guard<std::mutex> lock(fRawDataMutex);
      if (fRawDataQueue.empty()) {
        continue;
      }
      rawData = std::move(fRawDataQueue.front());
      fRawDataQueue.pop_front();
    }

    if (rawData) {
      DecodeData(std::move(rawData));
    }

    if (fDecodeThreads.size() > 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void RawToPSD2::DecodeData(std::unique_ptr<RawData_t> rawData)
{
  constexpr size_t oneWordSize = 8;
  uint64_t buf = 0;
  if (fDumpFlag) {
    std::cout << "Data size: " << rawData->size << std::endl;
    for (size_t i = 0; i < rawData->size; i += oneWordSize) {
      std::memcpy(&buf, &(*(rawData->data.begin() + i)), oneWordSize);
      std::cout << std::bitset<64>(buf) << std::endl;
    }
  }

  // Check header
  // bit[60:63] = 0x2
  std::memcpy(&buf, &(*(rawData->data.begin())), sizeof(uint64_t));
  auto check = ((buf >> 60) & 0xF) == 0x2;
  if (!check) {
    std::cerr << "Data is not valid" << std::endl;
  }

  // bit 56 = fail check
  auto failCheck = ((buf >> 56) & 0b1) == 0x1;
  if (failCheck) {
    std::cerr << "Board fail" << std::endl;
  }

  // bit[32:55] = aggregate counter
  // MT case, checking make no sense
  auto aggregateCounter = (buf >> 32) & 0xFFFF;
  if (fDecodeThreads.size() == 1) {
    if ((aggregateCounter != 0) && (aggregateCounter != fLastCounter + 1)) {
      std::cerr << "Aggregate counter is not continuous: " << fLastCounter
                << " -> " << aggregateCounter << std::endl;
    }
    fLastCounter = aggregateCounter;
  }

  // bit[0:31] = tota size
  auto totalSize = static_cast<uint32_t>(buf & 0xFFFFFFFF);
  if (totalSize * oneWordSize != rawData->size) {
    std::cerr << "Total size is not equal to data size" << std::endl;
  }

  auto dataStart = rawData->data.begin();
  std::vector<std::unique_ptr<PSD2Data_t>> psd2DataVec;
  psd2DataVec.reserve(totalSize / 2);  // For waveform case, this is too big
  PSD2Data_t psd2Data;
  for (size_t i = 1; i < totalSize; i++) {
    uint64_t firstWord = 0;
    std::memcpy(&firstWord, &(*(dataStart + i * oneWordSize)),
                sizeof(uint64_t));
    i++;  // Go to the next word
    uint64_t secondWord = 0;
    std::memcpy(&secondWord, &(*(dataStart + i * oneWordSize)),
                sizeof(uint64_t));

    // First word
    // bit 63 = 0x0
    auto firstWordCheck = ((firstWord >> 63) & 0b1) == 0x0;
    if (fDumpFlag) {
      std::cout << "First word check: " << firstWordCheck << std::endl;
    }

    // bit[56:62] = channel
    psd2Data.channel = ((firstWord >> 56) & 0x7F);
    if (fDumpFlag) {
      std::cout << "Channel: " << psd2Data.channel << std::endl;
    }
    // bit[0:47] = time stamp
    psd2Data.timeStamp = static_cast<uint64_t>(firstWord & 0xFFFFFFFFFFFF);
    psd2Data.timeStamp = psd2Data.timeStamp * fTimeStep;
    if (fDumpFlag) {
      std::cout << "Time stamp: " << psd2Data.timeStamp << std::endl;
    }

    // Second word
    // bit 63 = last word
    auto lastWord = ((secondWord >> 63) & 0b1) == 0x1;
    if (lastWord) {
      // Check it is really the last word
    }

    // bit 62 = including waveform
    auto withWaveformFlag = ((secondWord >> 62) & 0b1) == 0x1;

    // bit[50:61] = low priority flags
    psd2Data.flagsLowPriority =
        static_cast<uint16_t>((secondWord >> 50) & 0x7FF);
    if (fDumpFlag) {
      std::cout << "Low priority flags: " << psd2Data.flagsLowPriority
                << std::endl;
    }
    // bit[42:49] = high priority flags
    psd2Data.flagsHighPriority =
        static_cast<uint16_t>((secondWord >> 42) & 0xFF);
    if (fDumpFlag) {
      std::cout << "High priority flags: " << psd2Data.flagsHighPriority
                << std::endl;
    }
    // bit[26:41] = short gate
    psd2Data.energyShort = ((secondWord >> 26) & 0xFFFF);
    if (fDumpFlag) {
      std::cout << "Short gate: " << psd2Data.energyShort << std::endl;
    }
    // bit [16:25] = fine time stamp
    auto fineTSbuf = ((secondWord >> 16) & 0x3FF);
    psd2Data.timeStampNs =
        psd2Data.timeStamp + (fineTSbuf / 1024.0 * fTimeStep);
    if (fDumpFlag) {
      // 20 digits
      std::cout << std::fixed << std::setprecision(20);
      std::cout << "Fine time stamp: " << psd2Data.timeStampNs << std::endl;
      std::cout << std::defaultfloat;
    }
    // bit[0:15] = energy
    psd2Data.energy = static_cast<uint16_t>(secondWord & 0xFFFF);
    if (fDumpFlag) {
      std::cout << "Energy: " << psd2Data.energy << std::endl;
    }

    if (withWaveformFlag) {
      i++;  // Go to the next word
      uint64_t waveformHeader = 0;
      std::memcpy(&waveformHeader, &(*(dataStart + i * oneWordSize)),
                  sizeof(uint64_t));
      // bit 63 = 0x1
      auto waveHeaderCheck1 = ((waveformHeader >> 63) & 0b1) == 0x1;
      // bit [60:62] = 0x0
      auto waveHeaderCheck2 = ((waveformHeader >> 60) & 0x7) == 0x0;
      if (!(waveHeaderCheck1 && waveHeaderCheck2)) {
        std::cerr << "Waveform header check failed" << std::endl;
      }
      // bit [44:45] = time resolution
      auto timeResolution = ((waveformHeader >> 44) & 0x3);
      if (timeResolution == 0x0) {
        psd2Data.downSampleFactor = 1;
      } else if (timeResolution == 0x1) {
        psd2Data.downSampleFactor = 2;
      } else if (timeResolution == 0x2) {
        psd2Data.downSampleFactor = 4;
      } else if (timeResolution == 0x3) {
        psd2Data.downSampleFactor = 8;
      }

      // bit [28:43] = trigger threshold
      psd2Data.triggerThr =
          static_cast<uint16_t>((waveformHeader >> 28) & 0xFFFF);

      // bit [24:27] = digital probe 4 information
      psd2Data.digitalProbe4Type =
          static_cast<uint8_t>((waveformHeader >> 24) & 0xF);

      // bit [20:23] = digital probe 3 information
      psd2Data.digitalProbe3Type =
          static_cast<uint8_t>((waveformHeader >> 20) & 0xF);

      // bit [16:19] = digital probe 2 information
      psd2Data.digitalProbe2Type =
          static_cast<uint8_t>((waveformHeader >> 16) & 0xF);

      // bit [12:15] = digital probe 1 information
      psd2Data.digitalProbe1Type =
          static_cast<uint8_t>((waveformHeader >> 12) & 0xF);

      // bit [6:8] = analog probe 2 information
      psd2Data.analogProbe2Type =
          static_cast<uint8_t>((waveformHeader >> 6) & 0x7);
      // bit 9 = isSigned
      bool ap2IsSigned = ((waveformHeader >> 9) & 0b1) == 0x1;
      // bit [10:11] = multiplication factor
      auto ap2MulFactor = ((waveformHeader >> 10) & 0x3);
      if (ap2MulFactor == 0x0) {
        ap2MulFactor = 1;
      } else if (ap2MulFactor == 0x1) {
        ap2MulFactor = 4;
      } else if (ap2MulFactor == 0x2) {
        ap2MulFactor = 8;
      } else if (ap2MulFactor == 0x3) {
        ap2MulFactor = 16;
      }

      // bit [0:2] = analog probe 0 information
      psd2Data.analogProbe1Type =
          static_cast<uint8_t>((waveformHeader >> 0) & 0x7);
      // bit 3 = isSigned
      bool ap1IsSigned = ((waveformHeader >> 3) & 0b1) == 0x1;
      // bit [4:5] = multiplication factor
      auto ap1MulFactor = ((waveformHeader >> 4) & 0x3);
      if (ap1MulFactor == 0x0) {
        ap1MulFactor = 1;
      } else if (ap1MulFactor == 0x1) {
        ap1MulFactor = 4;
      } else if (ap1MulFactor == 0x2) {
        ap1MulFactor = 8;
      } else if (ap1MulFactor == 0x3) {
        ap1MulFactor = 16;
      }

      i++;  // Go to the next word
            // bit [0:11] = number of words
      uint64_t nWordsWaveform = 0;
      std::memcpy(&nWordsWaveform, &(*(dataStart + i * oneWordSize)),
                  sizeof(uint64_t));
      nWordsWaveform = nWordsWaveform & 0xFFF;
      psd2Data.Resize(nWordsWaveform * 2);  // 1 word has 2 data points

      std::vector<uint32_t> pointData;
      pointData.resize(2);
      for (size_t j = 0; j < nWordsWaveform; j++) {
        i++;  // Go to the next word
        uint64_t buf = 0;
        std::memcpy(&buf, &(*(dataStart + i * oneWordSize)), sizeof(uint64_t));
        pointData[0] = static_cast<uint32_t>(buf & 0xFFFFFFFF);
        pointData[1] = static_cast<uint32_t>((buf >> 32) & 0xFFFFFFFF);

        // analog probe #1 = bit [0:13]
        // digital probe #1 = bit 14
        // digital probe #2 = bit 15
        // analog probe #2 = bit [16:29]
        // digital probe #3 = bit 30
        // digital probe #4 = bit 31
        for (auto iPoint = 0; iPoint < 2; iPoint++) {
          auto nData = j * 2 + iPoint;
          auto point = pointData[iPoint];
          // analog probe #1
          if (ap1IsSigned) {
            psd2Data.analogProbe1[nData] =
                static_cast<int32_t>((point >> 0) & 0x3FFF) * ap1MulFactor;
          } else {
            psd2Data.analogProbe1[nData] =
                static_cast<uint32_t>((point >> 0) & 0x3FFF) * ap1MulFactor;
          }
          // analog probe #2
          if (ap2IsSigned) {
            psd2Data.analogProbe2[nData] =
                static_cast<int32_t>((point >> 16) & 0x3FFF) * ap2MulFactor;
          } else {
            psd2Data.analogProbe2[nData] =
                static_cast<uint32_t>((point >> 16) & 0x3FFF) * ap2MulFactor;
          }

          // digital probe #1
          psd2Data.digitalProbe1[nData] =
              static_cast<uint8_t>((point >> 14) & 0b1);
          // digital probe #2
          psd2Data.digitalProbe2[nData] =
              static_cast<uint8_t>((point >> 15) & 0b1);
          // digital probe #3
          psd2Data.digitalProbe3[nData] =
              static_cast<uint8_t>((point >> 30) & 0b1);
          // digital probe #4
          psd2Data.digitalProbe4[nData] =
              static_cast<uint8_t>((point >> 31) & 0b1);
        }
      }
    } else {
      // No waveform
      psd2Data.Resize(0);
    }

    psd2Data.timeResolution = fTimeStep;
    psd2DataVec.emplace_back(std::make_unique<PSD2Data_t>(psd2Data));
  }

  {
    std::lock_guard<std::mutex> lock(fPSD2DataMutex);
    fPSD2DataVec->insert(fPSD2DataVec->end(),
                         std::make_move_iterator(psd2DataVec.begin()),
                         std::make_move_iterator(psd2DataVec.end()));
  }
}

DataType RawToPSD2::AddData(std::unique_ptr<RawData_t> rawData)
{
  constexpr uint32_t oneWordSize = 8;
  if (rawData->size % oneWordSize != 0) {
    std::cerr << "Data size is not a multiple of " << oneWordSize << " Bytes"
              << std::endl;
    return DataType::Unknown;
  }

  // change big endian to little endian
  for (size_t i = 0; i < rawData->size; i += oneWordSize) {
    std::reverse(rawData->data.begin() + i,
                 rawData->data.begin() + i + oneWordSize);
  }

  auto dataType = CheckDataType(rawData);
  if (dataType == DataType::Event) {
    std::lock_guard<std::mutex> lock(fRawDataMutex);
    fRawDataQueue.push_back(std::move(rawData));
  } else if (dataType == DataType::Unknown) {
    std::cout << "Unknown data type" << std::endl;
    exit(1);
  }

  return dataType;
}

DataType RawToPSD2::CheckDataType(std::unique_ptr<RawData_t> &rawData)
{
  constexpr size_t oneWordSize = 8;
  if (rawData->size < 3 * oneWordSize) {
    return DataType::Unknown;
  } else if (rawData->size == 3 * oneWordSize) {
    if (CheckStop(rawData)) {
      return DataType::Stop;
    }
  } else if (rawData->size == 4 * oneWordSize) {
    if (CheckStart(rawData)) {
      return DataType::Start;
    }
  }

  return DataType::Event;
}

bool RawToPSD2::CheckStop(std::unique_ptr<RawData_t> &rawData)
{
  uint64_t buf = 0;
  // The first word bit[60:63] = 0x3
  // The first word bit[56:59] = 0x2
  std::memcpy(&buf, &(*(rawData->data.begin())), sizeof(uint64_t));
  auto firstCondition =
      ((buf >> 60) & 0xF) == 0x3 && ((buf >> 56) & 0xF) == 0x2;

  // The second word bit[56:63] = 0x0
  std::memcpy(&buf, &(*(rawData->data.begin() + 8)), sizeof(uint64_t));
  auto secondCondition = ((buf >> 56) & 0xF) == 0x0;

  // The third word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 16)), sizeof(uint64_t));
  auto thirdCondition = ((buf >> 56) & 0xF) == 0x1;

  if (firstCondition && secondCondition && thirdCondition) {
    // The third word bit[0:31] = dead time
    // auto deadTime = static_cast<uint32_t>(buf & 0xFFFFFFFF);
    // std::cout << "Dead time: " << deadTime * 8 << " ns" << std::endl;
    return true;
  }

  return false;
}

bool RawToPSD2::CheckStart(std::unique_ptr<RawData_t> &rawData)
{
  uint64_t buf = 0;
  // The first word bit[60:63] = 0x3
  // The first word bit[56:59] = 0x0
  std::memcpy(&buf, &(*(rawData->data.begin())), sizeof(uint64_t));
  auto firstCondition =
      ((buf >> 60) & 0xF) == 0x3 && ((buf >> 56) & 0xF) == 0x0;

  // The second word bit[56:63] = 0x2
  std::memcpy(&buf, &(*(rawData->data.begin() + 8)), sizeof(uint64_t));
  auto secondCondition = ((buf >> 56) & 0xF) == 0x2;

  // The third word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 16)), sizeof(uint64_t));
  auto thirdCondition = ((buf >> 56) & 0xF) == 0x1;

  // The fourth word bit[56:63] = 0x1
  std::memcpy(&buf, &(*(rawData->data.begin() + 24)), sizeof(uint64_t));
  auto fourthCondition = ((buf >> 56) & 0xF) == 0x1;

  if (firstCondition && secondCondition && thirdCondition && fourthCondition) {
    return true;
  }

  return false;
}
