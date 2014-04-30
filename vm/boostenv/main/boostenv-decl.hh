// Copyright © 2011, Université catholique de Louvain
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// *  Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// *  Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZART_BOOSTENV_DECL_H
#define MOZART_BOOSTENV_DECL_H

#include <mozart.hh>

#include <ctime>
#include <cstdio>
#include <cerrno>
#include <forward_list>
#include <mutex>

#include <boost/thread.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind.hpp>

namespace mozart { namespace boostenv {

class BoostEnvironment;

class BoostVM : VirtualMachine {
public:
  BoostVM(BoostEnvironment& environment,
          nativeint identifier,
          VirtualMachineOptions options,
          const std::string& app, bool isURL);

  static BoostVM& forVM(VM vm) {
    return *static_cast<BoostVM*>(vm);
  }

// Run and preemption
public:
  void run();
private:
  void start(std::string app, bool isURL);
  void onPreemptionTimerExpire(const boost::system::error_code& error);

// UUID generation
public:
  UUID genUUID();
private:
  inline
  static std::uint64_t bytes2uint64(const std::uint8_t* bytes);

// VM Port
public:
  bool streamAsked();

  bool portClosed();

  void getStream(UnstableNode &stream);

  void closeStream();

  void receiveOnVMPort(UnstableNode value);

  void receiveOnVMPort(std::vector<unsigned char>* buffer);

  void addMonitor(BoostVM& monitor);

// Termination
public:
  bool isRunning();

  void requestTermination();

private:
  void tellMonitors();

  void terminate();

// Management of nodes used by asynchronous operations for feedback
public:
  inline
  ProtectedNode allocAsyncIONode(StableNode* node);

  inline
  void releaseAsyncIONode(const ProtectedNode& node);

  inline
  ProtectedNode createAsyncIOFeedbackNode(UnstableNode& readOnly);

  template <class LT, class... Args>
  inline
  void bindAndReleaseAsyncIOFeedbackNode(const ProtectedNode& ref,
                                         LT&& label, Args&&... args);

  template <class LT, class... Args>
  inline
  void raiseAndReleaseAsyncIOFeedbackNode(const ProtectedNode& ref,
                                          LT&& label, Args&&... args);

// Notification from asynchronous work
public:
  inline
  void postVMEvent(std::function<void()> callback);

// GC
public:
  void gCollect(GC gc) {
    gc->copyStableRef(_headOfStream, _headOfStream);
    gc->copyStableRef(_stream, _stream);
  }

public:
  VM vm;
  BoostEnvironment& env;
  nativeint identifier;

// Random number and UUID generation
public:
  typedef boost::random::mt19937 random_generator_t;
  random_generator_t random_generator;
private:
  boost::uuids::random_generator uuidGenerator;

// VM stream
private:
  StableNode* _headOfStream;
  StableNode* _stream;

// Number of asynchronous IO nodes - used for termination detection
private:
  size_t _asyncIONodeCount;

// Synchronization condition variable telling there is work to do in the VM
private:
  boost::condition_variable _conditionWorkToDoInVM;
  boost::mutex _conditionWorkToDoInVMMutex;

// Preemption and alarms
private:
  boost::asio::deadline_timer preemptionTimer;
  boost::asio::deadline_timer alarmTimer;

// IO-driven events that must work with the VM store
private:
  std::queue<std::function<void()> > _vmEventsCallbacks;

// Monitors
private:
  std::vector<VM> _monitors;
  std::mutex _monitorsMutex;

// Running thread management
private:
  boost::asio::io_service::work* _work;
  boost::thread _thread;
  std::atomic_bool _terminated;
};

//////////////////////
// BoostEnvironment //
//////////////////////

class BoostEnvironment: public VirtualMachineEnvironment {
private:
  using BootLoader = std::function<bool(VM vm, const std::string& url,
                                        UnstableNode& result)>;
  using VMStarter = std::function<bool(VM vm, const std::string& app, bool isURL)>;

public:
  static BoostEnvironment& forVM(VM vm) {
    return static_cast<BoostEnvironment&>(vm->getEnvironment());
  }

public:
  BoostEnvironment(const VMStarter& vmStarter, VirtualMachineOptions options);

  BoostVM& addVM(const std::string& app, bool isURL);

  BoostVM& getVM(VM vm, nativeint identifier);

  UnstableNode listVMs(VM vm);

  void killVM(VM vm, nativeint exitCode);

// Configuration

public:
  const BootLoader& getBootLoader() {
    return _bootLoader;
  }

  void setBootLoader(const BootLoader& loader) {
    _bootLoader = loader;
  }

// Run and preemption

public:
  void runIO();

// Time

  static std::int64_t getReferenceTime() {
    return ptimeToReferenceTime(
      boost::posix_time::microsec_clock::universal_time());
  }

  static boost::posix_time::ptime referenceTimeToPTime(std::int64_t time) {
    return epoch() + boost::posix_time::millisec(time);
  }

  static std::int64_t ptimeToReferenceTime(boost::posix_time::ptime time) {
    return (time - epoch()).total_milliseconds();
  }

  static boost::posix_time::ptime epoch() {
    using namespace boost::gregorian;
    return boost::posix_time::ptime(date(1970, Jan, 1));
  }

// UUID generation

public:
  UUID genUUID(VM vm) {
    return BoostVM::forVM(vm).genUUID();
  }

// BigInt

public:
  inline
  std::shared_ptr<BigIntImplem> newBigIntImplem(VM vm, nativeint value);

  inline
  std::shared_ptr<BigIntImplem> newBigIntImplem(VM vm, double value);

  inline
  std::shared_ptr<BigIntImplem> newBigIntImplem(VM vm, const std::string& value);

// VM Port
public:
  void sendToVMPort(VM from, VM to, RichNode value);

// GC
public:
  void gCollect(GC gc) {
    BoostVM::forVM(gc->vm).gCollect(gc);
  }

// VMs
private:
  std::forward_list<BoostVM> vms;
  std::mutex _vmsMutex;
  nativeint _nextVMIdentifier;
  std::atomic_int _aliveVMs;

  VirtualMachineOptions _options;

// Bootstrap
private:
  BootLoader _bootLoader;

public:
  VMStarter vmStarter;

// ASIO service
public:
  boost::asio::io_service io_service;
};

///////////////
// Utilities //
///////////////

template <typename T>
inline
void MOZART_NORETURN raiseOSError(VM vm, const char* function,
                                  nativeint errnum, T&& message);

inline
void MOZART_NORETURN raiseOSError(VM vm, const char* function, int errnum);

inline
void MOZART_NORETURN raiseLastOSError(VM vm, const char* function);

inline
void MOZART_NORETURN raiseOSError(VM vm, const char* function,
                                  boost::system::error_code& ec);

inline
void MOZART_NORETURN raiseOSError(VM vm, const char* function,
                                  const boost::system::system_error& error);

} }

#endif // MOZART_BOOSTENV_DECL_H
