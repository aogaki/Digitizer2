#ifndef PSD2DATA_HPP
#define PSD2DATA_HPP 1

#include <cstdint>
#include <vector>

class PSD2Data
{
 public:
  PSD2Data(size_t size = 0)
      : timeStamp(0),
        timeStampNs(0),
        waveformSize(0),
        eventSize(0),
        aggregateCounter(0),
        fineTimeStamp(0),
        energy(0),
        energyShort(0),
        flagsLowPriority(0),
        flagsHighPriority(0),
        triggerThr(0),
        channel(0),
        timeResolution(0),
        analogProbe1Type(0),
        analogProbe2Type(0),
        digitalProbe1Type(0),
        digitalProbe2Type(0),
        digitalProbe3Type(0),
        digitalProbe4Type(0),
        downSampleFactor(0),
        boardFail(false),
        flush(false)
  {
    if (size > 0) Resize(size);
  };
  ~PSD2Data() {};

  // Copy constructor
  PSD2Data(const PSD2Data &data)
  {
    timeStamp = data.timeStamp;
    timeStampNs = data.timeStampNs;
    waveformSize = data.waveformSize;
    eventSize = data.eventSize;
    analogProbe1 = data.analogProbe1;
    analogProbe2 = data.analogProbe2;
    digitalProbe1 = data.digitalProbe1;
    digitalProbe2 = data.digitalProbe2;
    digitalProbe3 = data.digitalProbe3;
    digitalProbe4 = data.digitalProbe4;
    aggregateCounter = data.aggregateCounter;
    fineTimeStamp = data.fineTimeStamp;
    energy = data.energy;
    energyShort = data.energyShort;
    flagsLowPriority = data.flagsLowPriority;
    flagsHighPriority = data.flagsHighPriority;
    triggerThr = data.triggerThr;
    channel = data.channel;
    timeResolution = data.timeResolution;
    analogProbe1Type = data.analogProbe1Type;
    analogProbe2Type = data.analogProbe2Type;
    digitalProbe1Type = data.digitalProbe1Type;
    digitalProbe2Type = data.digitalProbe2Type;
    digitalProbe3Type = data.digitalProbe3Type;
    digitalProbe4Type = data.digitalProbe4Type;
    boardFail = data.boardFail;
    flush = data.flush;
  };

  void Resize(size_t size)
  {
    waveformSize = size;
    analogProbe1.resize(size);
    analogProbe2.resize(size);
    digitalProbe1.resize(size);
    digitalProbe2.resize(size);
    digitalProbe3.resize(size);
    digitalProbe4.resize(size);
  };

  uint64_t timeStamp;
  double timeStampNs;
  size_t waveformSize;
  size_t eventSize;
  std::vector<int32_t> analogProbe1;
  std::vector<int32_t> analogProbe2;
  std::vector<uint8_t> digitalProbe1;
  std::vector<uint8_t> digitalProbe2;
  std::vector<uint8_t> digitalProbe3;
  std::vector<uint8_t> digitalProbe4;
  uint32_t aggregateCounter;
  uint16_t fineTimeStamp;
  uint16_t energy;
  uint16_t energyShort;
  uint16_t flagsLowPriority;
  uint16_t flagsHighPriority;
  uint16_t triggerThr;
  uint8_t channel;
  uint8_t timeResolution;
  uint8_t analogProbe1Type;
  uint8_t analogProbe2Type;
  uint8_t digitalProbe1Type;
  uint8_t digitalProbe2Type;
  uint8_t digitalProbe3Type;
  uint8_t digitalProbe4Type;
  uint8_t downSampleFactor;
  bool boardFail;
  bool flush;
};

typedef PSD2Data PSD2Data_t;

#endif  // PSD2DATA_HPP