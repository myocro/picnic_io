#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include "picnic_io.hpp"

int main(int argc, char *argv[]) {
  std::string address;
  int interval_sec = 3;

  if (argc >= 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    std::cout << "Usage: " << argv[0] << " [ADDRESS:PORT] [INTERVAL]\n"
              << "\n"
              << "  ADDRESS:PORT  Target address (default: 192.168.0.200:10020)\n"
              << "  INTERVAL      Polling interval in seconds (default: 3, 0=once)\n"
              << "\n"
              << "  If no arguments are given, prompts interactively.\n";
    return 0;
  }

  if (argc >= 2) {
    address = argv[1];
    if (argc >= 3) {
      interval_sec = std::atoi(argv[2]);
      if (interval_sec < 0) interval_sec = 3;
    }
  } else {
    std::cout << "Address:Port [192.168.0.200:10020]: ";
    std::getline(std::cin, address);
    if (address.empty()) address = "192.168.0.200:10020";

    std::string interval_str;
    std::cout << "Interval sec [3]: ";
    std::getline(std::cin, interval_str);
    if (!interval_str.empty()) {
      interval_sec = std::atoi(interval_str.c_str());
      if (interval_sec < 0) interval_sec = 3;
    }
  }

  std::cout << "Target: " << address << std::endl;
  std::cout << "Interval: " << interval_sec << " sec" << std::endl;

  PICNICIO picnic;
  if (!picnic.open(address)) {
    std::cerr << "Open failed: " << picnic.getDebugMsg() << std::endl;
    return 1;
  }
  std::cout << picnic.getDebugMsg() << std::endl;
  std::cout << "Ctrl+C to quit." << std::endl;

  while (true) {
    std::vector<uint16_t> values;
    if (picnic.getValues(values)) {
      printf("\n--- %s ---\n", address.c_str());
      for (size_t i = 0; i < values.size(); i++) {
        printf("  ADC Ch%zu: %u\n", i, values[i]);
      }
    } else {
      std::cerr << "Error: " << picnic.getDebugMsg() << std::endl;
    }
    if (interval_sec > 0) {
      std::this_thread::sleep_for(std::chrono::seconds(interval_sec));
    } else {
      break;
    }
  }

  return 0;
}
