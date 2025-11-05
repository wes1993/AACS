// Distributed under GPLv3 only as specified in repository's root LICENSE file

#include "MassStorageFunction.h"
#include "Gadget.h"
#include "utils.h"
#include <ServerUtils.h>
#include <usbg/function/ms.h>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

using namespace std;

MassStorageFunction::MassStorageFunction(const Gadget &gadget,
                                         const string &function_name,
                                         const string &lun) {
  usbg_function *function;
  
  // Store the LUN file path to keep it valid
  lun_file = lun;
  
  // Create the function without attributes first
  checkUsbgError(usbg_create_function(
      gadget.getGadget(), usbg_function_type::USBG_F_MASS_STORAGE,
      function_name.c_str(), NULL, &function));
  
  this->function = function;
  
  // Convert to mass storage function type
  usbg_f_ms *ms_func = usbg_to_ms_function(function);
  if (!ms_func) {
    throw runtime_error("Failed to convert to mass storage function");
  }
  
  // Remove existing LUN if it exists (from previous run)
  int ret = usbg_f_ms_rm_lun(ms_func, 0);
  // Ignore errors if LUN doesn't exist
  
  // Initialize LUN attributes
  struct usbg_f_ms_lun_attrs lun_attrs;
  lun_attrs.id = 0;
  lun_attrs.cdrom = 1;
  lun_attrs.ro = 0;
  lun_attrs.nofua = 0;
  lun_attrs.removable = 1;
  lun_attrs.file = lun_file.c_str();
  lun_attrs.inquiry_string = NULL;
  
  // Create the LUN - handle "Already exist" error gracefully
  ret = usbg_f_ms_create_lun(ms_func, 0, &lun_attrs);
  if (ret != USBG_SUCCESS && ret != USBG_ERROR_EXIST) {
    checkUsbgError(ret);
  }
}
