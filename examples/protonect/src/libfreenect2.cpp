/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2014 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <libusb.h>

#include <libfreenect2/libfreenect2.hpp>

#include <libfreenect2/usb/event_loop.h>
#include <libfreenect2/usb/transfer_pool.h>
#include <libfreenect2/rgb_packet_stream_parser.h>
#include <libfreenect2/rgb_packet_processor.h>
#include <libfreenect2/depth_packet_stream_parser.h>
#include <libfreenect2/protocol/usb_control.h>
#include <libfreenect2/protocol/command.h>
#include <libfreenect2/protocol/command_transaction.h>

namespace libfreenect2
{
using namespace libfreenect2;
using namespace libfreenect2::usb;
using namespace libfreenect2::protocol;

class Freenect2DeviceImpl : public Freenect2Device
{
private:
  enum State
  {
    Created,
    Open,
    Streaming,
    Closed
  };

  State state_;
  bool has_usb_interfaces_;

  Freenect2Impl *context_;
  libusb_device *usb_device_;
  libusb_device_handle *usb_device_handle_;

  BulkTransferPool rgb_transfer_pool_;
  IsoTransferPool ir_transfer_pool_;

  UsbControl usb_control_;
  CommandTransaction command_tx_;
  int command_seq_;

  TurboJpegRgbPacketProcessor rgb_packet_processor_;
  CpuDepthPacketProcessor depth_packet_processor_;

  RgbPacketStreamParser rgb_packet_parser_;
  DepthPacketStreamParser depth_packet_parser_;
public:
  Freenect2DeviceImpl(Freenect2Impl *context, libusb_device *usb_device, libusb_device_handle *usb_device_handle);
  virtual ~Freenect2DeviceImpl();

  bool isSameUsbDevice(libusb_device* other);

  virtual std::string getSerialNumber();

  int nextCommandSeq();

  bool open();

  virtual void setColorFrameListener(libfreenect2::FrameListener* rgb_frame_listener);
  virtual void setIrAndDepthFrameListener(libfreenect2::FrameListener* ir_frame_listener);
  virtual void start();
  virtual void stop();
  virtual void close();
};

class Freenect2Impl
{
private:
  bool managed_usb_context_;
  libusb_context *usb_context_;
  EventLoop usb_event_loop_;
public:
  typedef std::vector<libusb_device *> UsbDeviceVector;
  typedef std::vector<Freenect2DeviceImpl *> DeviceVector;

  std::string protonect_path_;

  bool has_device_enumeration_;
  UsbDeviceVector enumerated_devices_;
  DeviceVector devices_;

  Freenect2Impl(const std::string& protonect_path, void *usb_context) :
    managed_usb_context_(usb_context == 0),
    usb_context_(reinterpret_cast<libusb_context *>(usb_context)),
    protonect_path_(protonect_path),
    has_device_enumeration_(false)
  {
    if(managed_usb_context_)
    {
      int r = libusb_init(&usb_context_);
      // TODO: error handling
      if(r != 0)
      {
        std::cout << "[Freenect2Impl] failed to create usb context!" << std::endl;
      }
    }

    usb_event_loop_.start(usb_context_);
  }

  ~Freenect2Impl()
  {
    clearDevices();
    clearDeviceEnumeration();

    usb_event_loop_.stop();

    if(managed_usb_context_ && usb_context_ != 0)
    {
      libusb_exit(usb_context_);
      usb_context_ = 0;
    }
  }

  void addDevice(Freenect2DeviceImpl *device)
  {
    devices_.push_back(device);
  }

  void removeDevice(Freenect2DeviceImpl *device)
  {
    DeviceVector::iterator it = std::find(devices_.begin(), devices_.end(), device);

    if(it != devices_.end())
    {
      devices_.erase(it);
    }
    else
    {
      std::cout << "[Freenect2Impl] tried to remove device, which is not in the internal device list!" << std::endl;
    }
  }

  bool tryGetDevice(libusb_device *usb_device, Freenect2DeviceImpl **device)
  {
    for(DeviceVector::iterator it = devices_.begin(); it != devices_.end(); ++it)
    {
      if((*it)->isSameUsbDevice(usb_device))
      {
        *device = *it;
        return true;
      }
    }

    return false;
  }

  void clearDevices()
  {
    DeviceVector devices(devices_.begin(), devices_.end());

    for(DeviceVector::iterator it = devices.begin(); it != devices.end(); ++it)
    {
      delete (*it);
    }

    if(!devices_.empty())
    {
      std::cout << "[Freenect2Impl] after deleting all devices the internal device list should be empty!" << std::endl;
    }
  }

  void clearDeviceEnumeration()
  {
    // free enumerated device pointers, this should not affect opened devices
    for(UsbDeviceVector::iterator it = enumerated_devices_.begin(); it != enumerated_devices_.end(); ++it)
    {
      libusb_unref_device(*it);
    }

    enumerated_devices_.clear();
    has_device_enumeration_ = false;
  }

  void enumerateDevices()
  {
    std::cout << "[Freenect2Impl] enumerating devices..." << std::endl;
    libusb_device **device_list;
    int num_devices = libusb_get_device_list(usb_context_, &device_list);

    std::cout << "[Freenect2Impl] " << num_devices << " usb devices connected" << std::endl;

    if(num_devices > 0)
    {
      for(int idx = 0; idx < num_devices; ++idx)
      {
        libusb_device *dev = device_list[idx];
        libusb_device_descriptor dev_desc;

        int r = libusb_get_device_descriptor(dev, &dev_desc);
        // TODO: error handling

        if(dev_desc.idVendor == Freenect2Device::VendorId && dev_desc.idProduct == Freenect2Device::ProductId)
        {
          // valid Kinect v2
          enumerated_devices_.push_back(dev);
        }
        else
        {
          libusb_unref_device(dev);
        }
      }
    }

    libusb_free_device_list(device_list, 0);
    has_device_enumeration_ = true;

    std::cout << "[Freenect2Impl] found " << enumerated_devices_.size() << " devices" << std::endl;
  }

  int getNumDevices()
  {
    if(!has_device_enumeration_)
    {
      enumerateDevices();
    }
    return enumerated_devices_.size();
  }
};


Freenect2Device::~Freenect2Device()
{
}

Freenect2DeviceImpl::Freenect2DeviceImpl(Freenect2Impl *context, libusb_device *usb_device, libusb_device_handle *usb_device_handle) :
  state_(Created),
  has_usb_interfaces_(false),
  context_(context),
  usb_device_(usb_device),
  usb_device_handle_(usb_device_handle),
  rgb_transfer_pool_(usb_device_handle, 0x83),
  ir_transfer_pool_(usb_device_handle, 0x84),
  usb_control_(usb_device_handle_),
  command_tx_(usb_device_handle_, 0x81, 0x02),
  command_seq_(0),
  rgb_packet_processor_(),
  depth_packet_processor_(),
  rgb_packet_parser_(&rgb_packet_processor_),
  depth_packet_parser_(&depth_packet_processor_)
{
  rgb_transfer_pool_.setCallback(&rgb_packet_parser_);
  ir_transfer_pool_.setCallback(&depth_packet_parser_);

  depth_packet_processor_.load11To16LutFromFile((context_->protonect_path_ + "/11to16.bin").c_str());
  depth_packet_processor_.loadXTableFromFile((context_->protonect_path_ + "/xTable.bin").c_str());
  depth_packet_processor_.loadZTableFromFile((context_->protonect_path_ + "/zTable.bin").c_str());
}

Freenect2DeviceImpl::~Freenect2DeviceImpl()
{
  close();
  context_->removeDevice(this);
}

int Freenect2DeviceImpl::nextCommandSeq()
{
  return command_seq_++;
}

bool Freenect2DeviceImpl::isSameUsbDevice(libusb_device* other)
{
  bool result = false;

  if(state_ != Closed && usb_device_ != 0)
  {
    unsigned char bus = libusb_get_bus_number(usb_device_);
    unsigned char port = libusb_get_port_number(usb_device_);

    unsigned char other_bus = libusb_get_bus_number(other);
    unsigned char other_port = libusb_get_port_number(other);

    result = (bus == other_bus) && (port == other_port);
  }

  return result;
}

std::string Freenect2DeviceImpl::getSerialNumber()
{
  throw std::exception();
}

void Freenect2DeviceImpl::setColorFrameListener(libfreenect2::FrameListener* rgb_frame_listener)
{
  // TODO: should only be possible, if not started
  rgb_packet_processor_.setFrameListener(rgb_frame_listener);
}

void Freenect2DeviceImpl::setIrAndDepthFrameListener(libfreenect2::FrameListener* ir_frame_listener)
{
  // TODO: should only be possible, if not started
  depth_packet_processor_.setFrameListener(ir_frame_listener);
}

bool Freenect2DeviceImpl::open()
{
  std::cout << "[Freenect2DeviceImpl] opening..." << std::endl;

  if(state_ != Created) return false;

  if(usb_control_.setConfiguration() != UsbControl::Success) return false;
  if(!has_usb_interfaces_ && usb_control_.claimInterfaces() != UsbControl::Success) return false;
  has_usb_interfaces_ = true;

  if(usb_control_.setIsochronousDelay() != UsbControl::Success) return false;
  // TODO: always fails right now with error 6 - TRANSFER_OVERFLOW!
  //if(usb_control_.setPowerStateLatencies() != UsbControl::Success) return false;
  if(usb_control_.setIrInterfaceState(UsbControl::Disabled) != UsbControl::Success) return false;
  if(usb_control_.enablePowerStates() != UsbControl::Success) return false;
  if(usb_control_.setVideoTransferFunctionState(UsbControl::Disabled) != UsbControl::Success) return false;

  size_t max_iso_packet_size = libusb_get_max_iso_packet_size(usb_device_, 0x84);

  if(max_iso_packet_size < 0x8400)
  {
    std::cout << "[Freenect2DeviceImpl] max iso packet size for endpoint 0x84 too small! (expected: " << 0x8400 << " got: " << max_iso_packet_size << ")" << std::endl;
    return false;
  }

  rgb_transfer_pool_.allocate(50, 0x4000);
  ir_transfer_pool_.allocate(80, 8, max_iso_packet_size);

  state_ = Open;

  std::cout << "[Freenect2DeviceImpl] opened" << std::endl;

  return true;
}

void Freenect2DeviceImpl::start()
{
  std::cout << "[Freenect2DeviceImpl] starting..." << std::endl;
  if(state_ != Open) return;

  CommandTransaction::Result result;

  usb_control_.setVideoTransferFunctionState(UsbControl::Enabled);

  command_tx_.execute(ReadFirmwareVersionsCommand(nextCommandSeq()), result);
  //hexdump(result.data, result.length, "ReadFirmwareVersions");

  command_tx_.execute(ReadData0x14Command(nextCommandSeq()), result);
  //hexdump(result.data, result.length, "ReadData0x14");

  command_tx_.execute(ReadSerialNumberCommand(nextCommandSeq()), result);
  //hexdump(result.data, result.length, "ReadSerialNumber");

  command_tx_.execute(ReadDepthCameraParametersCommand(nextCommandSeq()), result);

  command_tx_.execute(ReadP0TablesCommand(nextCommandSeq()), result);
  depth_packet_processor_.loadP0TablesFromCommandResponse(result.data, result.length);

  command_tx_.execute(ReadRgbCameraParametersCommand(nextCommandSeq()), result);

  command_tx_.execute(ReadStatus0x090000Command(nextCommandSeq()), result);
  //hexdump(result.data, result.length, "Status");

  command_tx_.execute(InitStreamsCommand(nextCommandSeq()), result);

  usb_control_.setIrInterfaceState(UsbControl::Enabled);

  command_tx_.execute(ReadStatus0x090000Command(nextCommandSeq()), result);
  //hexdump(result.data, result.length, "Status");

  command_tx_.execute(SetStreamEnabledCommand(nextCommandSeq()), result);

  //command_tx_.execute(SetModeEnabledCommand(nextCommandSeq()), result);
  //command_tx_.execute(SetModeDisabledCommand(nextCommandSeq()), result);
  //command_tx_.execute(SetModeEnabledWith0x00640064Command(nextCommandSeq()), result);
  //command_tx_.execute(ReadData0x26Command(nextCommandSeq(), result);
  //command_tx_.execute(ReadStatus0x100007Command(nextCommandSeq()), result);

  std::cout << "[Freenect2DeviceImpl] enabling usb transfer submission..." << std::endl;
  rgb_transfer_pool_.enableSubmission();
  ir_transfer_pool_.enableSubmission();

  std::cout << "[Freenect2DeviceImpl] submitting usb transfers..." << std::endl;
  rgb_transfer_pool_.submit(20);
  ir_transfer_pool_.submit(60);

  state_ = Streaming;
  std::cout << "[Freenect2DeviceImpl] started" << std::endl;
}

void Freenect2DeviceImpl::stop()
{
  std::cout << "[Freenect2DeviceImpl] stopping..." << std::endl;

  if(state_ != Streaming)
  {
    std::cout << "[Freenect2DeviceImpl] already stopped, doing nothing" << std::endl;
    return;
  }

  std::cout << "[Freenect2DeviceImpl] disabling usb transfer submission..." << std::endl;
  rgb_transfer_pool_.disableSubmission();
  ir_transfer_pool_.disableSubmission();

  std::cout << "[Freenect2DeviceImpl] canceling usb transfers..." << std::endl;
  rgb_transfer_pool_.cancel();
  ir_transfer_pool_.cancel();

  // wait for completion of transfer cancelation
  // TODO: better implementation
  libfreenect2::this_thread::sleep_for(libfreenect2::chrono::seconds(2));

  usb_control_.setIrInterfaceState(UsbControl::Disabled);

  CommandTransaction::Result result;
  command_tx_.execute(Unknown0x0ACommand(nextCommandSeq()), result);
  command_tx_.execute(SetStreamDisabledCommand(nextCommandSeq()), result);

  usb_control_.setVideoTransferFunctionState(UsbControl::Disabled);

  state_ = Open;
  std::cout << "[Freenect2DeviceImpl] stopped" << std::endl;
}

void Freenect2DeviceImpl::close()
{
  std::cout << "[Freenect2DeviceImpl] closing..." << std::endl;

  if(state_ == Closed)
  {
    std::cout << "[Freenect2DeviceImpl] already closed, doing nothing" << std::endl;
    return;
  }

  if(state_ == Streaming)
  {
    stop();
  }

  rgb_packet_processor_.setFrameListener(0);
  depth_packet_processor_.setFrameListener(0);

  if(has_usb_interfaces_)
  {
    std::cout << "[Freenect2DeviceImpl] releasing usb interfaces..." << std::endl;

    usb_control_.releaseInterfaces();
    has_usb_interfaces_ = false;
  }

  std::cout << "[Freenect2DeviceImpl] deallocating usb transfer pools..." << std::endl;
  rgb_transfer_pool_.deallocate();
  ir_transfer_pool_.deallocate();

  std::cout << "[Freenect2DeviceImpl] closing usb device..." << std::endl;

  libusb_close(usb_device_handle_);
  usb_device_handle_ = 0;
  usb_device_ = 0;

  state_ = Closed;
  std::cout << "[Freenect2DeviceImpl] closed" << std::endl;
}

Freenect2::Freenect2(const std::string& protonect_path, void *usb_context) :
    impl_(new Freenect2Impl(protonect_path, usb_context))
{
}

Freenect2::~Freenect2()
{
  delete impl_;
}

int Freenect2::enumerateDevices()
{
  impl_->clearDeviceEnumeration();
  return impl_->getNumDevices();
}

std::string Freenect2::getDeviceSerialNumber(int idx)
{
  throw std::exception();
}

std::string Freenect2::getDefaultDeviceSerialNumber()
{
  return getDeviceSerialNumber(0);
}

Freenect2Device *Freenect2::openDevice(int idx)
{
  int num_devices = impl_->getNumDevices();

  if(idx < num_devices)
  {
    libusb_device *dev = impl_->enumerated_devices_[idx];
    libusb_device_handle *dev_handle;

    Freenect2DeviceImpl *device;

    if(impl_->tryGetDevice(dev, &device))
    {
      return device;
    }
    else
    {
      int r = libusb_open(dev, &dev_handle);
      // TODO: error handling

      device = new Freenect2DeviceImpl(impl_, dev, dev_handle);
      impl_->addDevice(device);

      if(device->open())
      {
        return device;
      }
      else
      {
        delete device;

        // TODO: error handling
        return 0;
      }
    }
  }
  else
  {
    // TODO: error handling
    return 0;
  }
}

Freenect2Device *Freenect2::openDevice(const std::string &serial)
{
  throw std::exception();
}

Freenect2Device *Freenect2::openDefaultDevice()
{
  return openDevice(0);
}

} /* namespace libfreenect2 */
