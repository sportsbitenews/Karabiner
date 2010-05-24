#include "CommonData.hpp"
#include "Config.hpp"
#include "Core.hpp"
#include "EventWatcher.hpp"
#include "FlagStatus.hpp"
#include "IOLockWrapper.hpp"
#include "ListHookedConsumer.hpp"

namespace org_pqrs_KeyRemap4MacBook {
  namespace {
    ListHookedConsumer listHookedConsumer;
  }

  ListHookedConsumer&
  ListHookedConsumer::instance(void)
  {
    return listHookedConsumer;
  }

  // ----------------------------------------------------------------------
  namespace {
    void
    hook_KeyboardSpecialEventCallback(OSObject* target,
                                      unsigned int eventType,
                                      unsigned int flags,
                                      unsigned int key,
                                      unsigned int flavor,
                                      UInt64 guid,
                                      bool repeat,
                                      AbsoluteTime ts,
                                      OSObject* sender,
                                      void* refcon)
    {
      if (! CommonData::eventLock) return;
      IOLockWrapper::ScopedLock lk(CommonData::eventLock);

      IOHIKeyboard* kbd = OSDynamicCast(IOHIKeyboard, sender);
      if (! kbd) return;

      HookedConsumer* hc = ListHookedConsumer::instance().get(kbd);
      if (! hc) return;

      // ------------------------------------------------------------
      CommonData::setcurrent_ts(ts);
      CommonData::setcurrent_vendorIDproductID(hc->getVendorID(), hc->getProductID());

      // ------------------------------------------------------------
      // Because we handle the key repeat ourself, drop the key repeat by hardware.
      if (repeat) return;

      // ------------------------------------------------------------
      if (EventType::DOWN == eventType) {
        CommonData::setcurrent_workspacedata();
      }

      // ------------------------------------------------------------
      // clear temporary_count_
      FlagStatus::set();

      // ------------------------------------------------------------
      Params_KeyboardSpecialEventCallback::auto_ptr ptr(Params_KeyboardSpecialEventCallback::alloc(EventType(eventType), Flags(flags), ConsumerKeyCode(key),
                                                                                                   flavor, guid, repeat));
      if (! ptr) return;
      Params_KeyboardSpecialEventCallback& params = *ptr;

      if (params.eventType == EventType::DOWN) {
        EventWatcher::on();
      }

      Core::remap_KeyboardSpecialEventCallback(params);
    }
  }

  bool
  HookedConsumer::initialize(IOHIDevice* d)
  {
    if (! d) return false;

    const char* name = d->getName();
    if (! name) return false;

    if (! HookedDevice::isConsumer(name)) return false;

    device_ = d;
    IOLog("KeyRemap4MacBook HookedConsumer::initialize name = %s, device = %p\n", name, device_);

    return refresh();
  }

  bool
  HookedConsumer::refresh(void)
  {
    if (! config.initialized) {
      goto restore;
    }
    // Logitech USB Headset
    if (isEqualVendorIDProductID(DeviceVendorID(0x046d), DeviceProductID(0x0a0b))) {
      goto restore;
    }
    // Logitech Cordless Presenter
    if (config.general_dont_remap_logitech_cordless_presenter &&
        isEqualVendorIDProductID(DeviceVendorID(0x046d), DeviceProductID(0xc515))) {
      goto restore;
    }
#if 0
    // Apple Internal Keyboard
    if (isEqualVendorIDProductID(DeviceVendorID(0x05ac), DeviceProductID(0x21a))) {
      goto restore;
    }
#endif
#if 0
    // Apple External Keyboard
    if (isEqualVendorIDProductID(DeviceProductID(0x05ac), DeviceProductID(0x0222))) {
      goto restore;
    }
#endif

    return replaceEventAction();

  restore:
    return restoreEventAction();
  }

  bool
  HookedConsumer::terminate(void)
  {
    bool result = restoreEventAction();

    device_ = NULL;
    orig_keyboardSpecialEventAction_ = NULL;
    orig_keyboardSpecialEventTarget_ = NULL;

    return result;
  }

  bool
  HookedConsumer::replaceEventAction(void)
  {
    if (! device_) return false;

    IOHIKeyboard* kbd = OSDynamicCast(IOHIKeyboard, device_);
    if (! kbd) return false;

    KeyboardSpecialEventCallback callback = reinterpret_cast<KeyboardSpecialEventCallback>(kbd->_keyboardSpecialEventAction);
    if (callback == hook_KeyboardSpecialEventCallback) return false;

    // ------------------------------------------------------------
    IOLog("KeyRemap4MacBook HookedConsumer::replaceEventAction (device_ = %p)\n", device_);

    orig_keyboardSpecialEventAction_ = callback;
    orig_keyboardSpecialEventTarget_ = kbd->_keyboardSpecialEventTarget;

    kbd->_keyboardSpecialEventAction = reinterpret_cast<KeyboardSpecialEventAction>(hook_KeyboardSpecialEventCallback);

    return true;
  }

  bool
  HookedConsumer::restoreEventAction(void)
  {
    if (! device_) return false;

    IOHIKeyboard* kbd = OSDynamicCast(IOHIKeyboard, device_);
    if (! kbd) return false;

    KeyboardSpecialEventCallback callback = reinterpret_cast<KeyboardSpecialEventCallback>(kbd->_keyboardSpecialEventAction);
    if (callback != hook_KeyboardSpecialEventCallback) return false;

    // ----------------------------------------
    IOLog("KeyRemap4MacBook HookedConsumer::restoreEventAction (device_ = %p)\n", device_);

    kbd->_keyboardSpecialEventAction = reinterpret_cast<KeyboardSpecialEventAction>(orig_keyboardSpecialEventAction_);

    orig_keyboardSpecialEventAction_ = NULL;
    orig_keyboardSpecialEventTarget_ = NULL;

    return true;
  }
}
