#include "InputEvent.pb.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <chrono>
#include <ctime>

using namespace std;

string get_timestamp() {
  auto now = chrono::system_clock::now();
  auto now_c = chrono::system_clock::to_time_t(now);
  auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
  
  char buf[100];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now_c));
  return string(buf) + "." + to_string(ms.count());
}

uint8_t openChannel(int aaServerFd) {
  uint8_t buffer[3];
  buffer[0] = 0; // packet type == GetChannelNumberByChannelType
  buffer[1] = 1; // channel type == Input
  buffer[2] = 0; // specific?
  if (write(aaServerFd, buffer, sizeof buffer) != sizeof buffer)
    throw runtime_error("failed to write open channel");
  uint8_t channelNumber;
  if (read(aaServerFd, &channelNumber, sizeof(channelNumber)) !=
      sizeof(channelNumber))
    throw runtime_error("failed to read video channel number");
  cout << "channelNumber: " << to_string(channelNumber) << endl;
  buffer[0] = 1; // packet type == Raw
  buffer[1] = channelNumber;
  buffer[2] = 0; // specific? don't care
  if (write(aaServerFd, buffer, sizeof buffer) != sizeof buffer)
    throw runtime_error("failed to write open channel");
  return channelNumber;
}

int getSocketFd(std::string socketName) {
  int fd;
  if ((fd = socket(PF_UNIX, SOCK_SEQPACKET, 0)) < 0) {
    throw runtime_error("socket failed");
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, socketName.c_str());
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    throw runtime_error("connect failed");
  }
  return fd;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    cout << "Needs 1 argument: socket name" << endl;
    exit(1);
  }
  auto fd = getSocketFd(argv[1]);
  openChannel(fd);

  cout << "Starting input event logger..." << endl;
  cout << "Logging all button presses (no events will be forwarded)" << endl;
  cout << "========================================" << endl;

  const auto maxBufSize = 1024;
  uint8_t buffer[maxBufSize];
  while (true) {
    auto realSize = read(fd, buffer, maxBufSize);
    if (realSize <= 0)
      throw runtime_error("failed to read input event");
    
    class tag::aas::InputEvent ie;
    ie.ParseFromArray(buffer + 4, realSize - 4);
    
    if (!ie.has_touch_event())
      continue;
    
    auto& touch_event = ie.touch_event();
    string timestamp = get_timestamp();
    
    // Log touch locations
    for (auto tl : touch_event.touch_location()) {
      cout << "[" << timestamp << "] Touch location - x: " << tl.x() 
           << ", y: " << tl.y() << ", pid: " << tl.pid() << endl;
    }
    
    // Log button press events
    if (touch_event.touch_action() == tag::aas::TouchAction::Press ||
        touch_event.touch_action() == tag::aas::TouchAction::Down) {
      cout << "[" << timestamp << "] BUTTON PRESS" << endl;
    }
    
    // Log button release events
    if (touch_event.touch_action() == tag::aas::TouchAction::Release ||
        touch_event.touch_action() == tag::aas::TouchAction::Up) {
      cout << "[" << timestamp << "] BUTTON RELEASE" << endl;
    }
    
    cout << "---" << endl;
  }
  return 0;
}