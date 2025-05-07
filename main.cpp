#include <CAEN_FELib.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "PSD2.hpp"

enum class AppState { Quit, Reload, Continue };

AppState InputCheck()
{
  struct termios oldt, newt;
  char ch = -1;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if (ch == 'q' || ch == 'Q') {
    return AppState::Quit;
  } else if (ch == 'r' || ch == 'R') {
    return AppState::Reload;
  }

  return AppState::Continue;
}

int main()
{
  TApplication app("app", 0, nullptr);

  std::vector<int32_t> waveform(1024);
  for (int i = 0; i < 1024; i++) {
    waveform[i] = i * i;
  }

  auto digitizer = std::make_unique<PSD2>();
  digitizer->LoadConfig("PSD2.conf");

  if (!digitizer->Initialize()) {
    std::cerr << "Failed to initialize digitizer" << std::endl;
    exit(1);
  }

  if (!digitizer->Configure()) {
    std::cerr << "Failed to configure digitizer" << std::endl;
    exit(1);
  }

  if (!digitizer->StartAcquisition()) {
    std::cerr << "Failed to start acquisition" << std::endl;
    exit(1);
  }

  double_t eveCounter = 0;
  auto startTime = std::chrono::system_clock::now();
  while (true) {
    auto state = InputCheck();
    if (state == AppState::Quit) {
      break;
    }

    auto data = digitizer->GetData();
    if (data->size() > 0) {
      eveCounter += data->size();
    }
  }
  auto endTime = std::chrono::system_clock::now();

  if (!digitizer->StopAcquisition()) {
    std::cerr << "Failed to stop acquisition" << std::endl;
  }

  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime)
          .count();
  auto eveRate = static_cast<double>(eveCounter) / duration * 1000;
  std::cout << "Event rate: " << eveRate << " Hz" << std::endl;

  std::cout << "Exiting..." << std::endl;

  return 0;
}
