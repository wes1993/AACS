// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "ModeSwitcher.h"
#include "Configuration.h"
#include "FfsFunction.h"
#include "Function.h"
#include "Gadget.h"
#include "Library.h"
#include "MassStorageFunction.h"
#include "ModeSwitcher.h"
#include "Udc.h"
#include "descriptors.h"
#include "utils.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <fcntl.h>
#include <iostream>
#include <linux/usb/functionfs.h>
#include <unistd.h>

ssize_t ModeSwitcher::handleSwitchMessage(int fd, const void *buf,
                                          size_t nbytes) {
  const usb_functionfs_event *event = (const usb_functionfs_event *)buf;
  for (size_t n = nbytes / sizeof *event; n; --n, ++event) {
    if (event->type == FUNCTIONFS_SETUP) {
      const struct usb_ctrlrequest &setup = event->u.setup;
      if (setup.bRequest == 51) {
        auto ret = write(fd, "\002\000", 2);
        std::cout << "Got 51, write=" << ret << std::endl;
      } else if (setup.bRequest == 52) {
        std::cout << "Got some info: " << setup.wIndex << "=";
        if (setup.wLength < nbytes) {
          std::cout << std::endl;
        }
      } else if (setup.bRequest == 53) {
        std::cout << "Got 53, exit" << std::endl;
        return 0;
      }
    } else {
      std::cout << std::string((char *)buf, nbytes) << std::endl;
    }
  }
  return nbytes;
}

void ModeSwitcher::handleSwitchToAccessoryMode(const Library &lib) {
  std::cout << "=== Starting handleSwitchToAccessoryMode ===" << std::endl;
  
  // Clean up any existing gadget with the same name first
  std::string gadget_name = rr("initial_state");
  std::cout << "Gadget name: " << gadget_name << std::endl;
  
  usbg_gadget *existing_gadget = usbg_get_gadget(lib.getState(), gadget_name.c_str());
  if (existing_gadget != NULL) {
    std::cout << "Removing existing gadget: " << gadget_name << std::endl;
    int ret = usbg_rm_gadget(existing_gadget, USBG_RM_RECURSE);
    if (ret != USBG_SUCCESS) {
      std::cout << "Warning: failed to remove existing gadget: " << usbg_strerror((usbg_error)ret) << std::endl;
    }
  }
  
  std::cout << "Creating initial gadget (VID: 0x12d1, PID: 0x107e)..." << std::endl;
  Gadget initialGadget(lib, 0x12d1, 0x107e, gadget_name);
  
  std::string serial = sr("TAGAAS");
  std::cout << "Setting strings (Manufacturer: TAG, Product: AAServer, Serial: " << serial << ")..." << std::endl;
  initialGadget.setStrings("TAG", "AAServer", serial);
  
  // Create virtual disk file
  auto lun0path = (boost::filesystem::temp_directory_path() / rr("lun0"));
  std::cout << "Creating virtual disk at: " << lun0path << std::endl;
  std::ofstream ofs(lun0path.c_str(), std::ios::binary | std::ios::out);
  if (!ofs.is_open()) {
    std::cerr << "ERROR: Failed to create virtual disk file!" << std::endl;
    return;
  }
  ofs.seekp((4 << 20) - 1);
  ofs.write("", 1);
  ofs.flush();
  ofs.close();
  std::cout << "Virtual disk created (4MB)" << std::endl;
  
  std::cout << "Creating mass storage function..." << std::endl;
  MassStorageFunction ms_function(initialGadget, rr("massstorage_initial"),
                                  lun0path.c_str());
  
  auto tmpMountpoint = boost::filesystem::temp_directory_path() /
                       rr("AAServer_mp_loopback_initial");
  std::cout << "Creating FunctionFS mountpoint: " << tmpMountpoint << std::endl;
  create_directory(tmpMountpoint);
  
  std::cout << "Creating FunctionFS function..." << std::endl;
  FfsFunction ffs_function(initialGadget, rr("ffs_initial"),
                           tmpMountpoint.c_str());
  
  std::cout << "Creating configuration..." << std::endl;
  Configuration configuration(initialGadget, "c0");
  configuration.addFunction(ms_function, "massstorage_initial");
  configuration.addFunction(ffs_function, "loopback_initial");
  
  auto ep0_path = tmpMountpoint / "ep0";
  std::cout << "Opening FunctionFS ep0: " << ep0_path << std::endl;
  auto fd = open(ep0_path.c_str(), O_RDWR);
  if (fd < 0) {
    std::cerr << "ERROR: Failed to open ep0! fd=" << fd << ", errno=" << errno 
              << " (" << strerror(errno) << ")" << std::endl;
    return;
  }
  std::cout << "ep0 opened successfully, fd=" << fd << std::endl;
  
  std::cout << "Writing USB descriptors..." << std::endl;
  write_descriptors_default(fd);
  std::cout << "Descriptors written" << std::endl;
  
  std::cout << "Getting UDC..." << std::endl;
  auto udc = Udc::getUdcById(lib, 0);
  std::cout << "Enabling gadget on UDC..." << std::endl;
  initialGadget.enable(udc);
  std::cout << "Gadget enabled and active!" << std::endl;
  
  auto eSize = sizeof(struct usb_functionfs_event);
  auto bufSize = 4 * eSize;
  std::cout << "Event size: " << eSize << " bytes, buffer size: " << bufSize << " bytes" << std::endl;
  uint8_t buffer[bufSize];
  
  int iteration = 0;
  for (;;) {
    iteration++;
    std::cout << "\n=== Iteration " << iteration << " ===" << std::endl;
    std::cout << "Waiting for USB events on fd=" << fd << "..." << std::endl;
    
    // Check if fd is still valid
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
      std::cerr << "ERROR: fd is invalid! errno=" << errno 
                << " (" << strerror(errno) << ")" << std::endl;
      break;
    }
    std::cout << "fd is valid, flags=0x" << std::hex << flags << std::dec << std::endl;
    
    errno = 0;  // Clear errno before read
    auto length = read(fd, buffer, bufSize);
    int read_errno = errno;  // Save errno immediately
    
    std::cout << "read() returned: " << length << ", errno=" << read_errno;
    if (read_errno != 0) {
      std::cout << " (" << strerror(read_errno) << ")";
    }
    std::cout << std::endl;
    
    // Now check the error with your wrapper
    try {
      length = checkError(length, {EINTR, EAGAIN});
      std::cout << "checkError passed, length=" << length << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "ERROR: checkError threw exception: " << e.what() << std::endl;
      std::cerr << "read_errno was: " << read_errno << " (" << strerror(read_errno) << ")" << std::endl;
      throw;  // Re-throw to get full stack trace
    }
    
    if (length == 0) {
      std::cout << "No data read, continuing..." << std::endl;
      continue;
    }
    if (length == -1) {
      std::cout << "Error reading, breaking loop" << std::endl;
      break;
    }
    
    std::cout << "Read " << length << " bytes, processing events..." << std::endl;
    
    // Dump buffer contents for debugging
    std::cout << "Buffer contents (hex): ";
    for (int i = 0; i < std::min((ssize_t)length, (ssize_t)32); i++) {
      printf("%02x ", buffer[i]);
    }
    std::cout << std::endl;
    
    std::cout << "Calling handleSwitchMessage..." << std::endl;
    auto ret = handleSwitchMessage(fd, buffer, length);
    std::cout << "handleSwitchMessage returned: " << ret << std::endl;
    
    try {
      checkError(ret, {EINTR, EAGAIN});
    } catch (const std::exception& e) {
      std::cerr << "ERROR: handleSwitchMessage checkError threw: " << e.what() << std::endl;
      throw;
    }
    
    if (ret == 0) {
      std::cout << "handleSwitchMessage returned 0, exiting loop" << std::endl;
      break;
    }
  }
  
  std::cout << "\n=== Cleaning up ===" << std::endl;
  std::cout << "Closing fd=" << fd << std::endl;
  close(fd);
  std::cout << "fd closed successfully" << std::endl;
  std::cout << "=== handleSwitchToAccessoryMode completed ===" << std::endl;
} 
 
 
 
 
 
 
 
