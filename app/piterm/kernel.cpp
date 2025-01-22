//
// kernel.cpp
//
#include "kernel.h"

#include <circle/util.h>
#include <sdcard/emmc.h>
#include <fatfs/ff.h>
#include "firmware.h"

static const u8 ipaddress[] = {172, 16, 103, 103} ;
static const u8 netmask[]   = {255, 255, 255, 0} ;
static const u8 gateway[]   = {172, 16, 103, 254} ;
static const u8 dns[]       = {172, 16, 103, 254} ;

extern volatile bool interrupted ;

extern queue_t keyboard_queue ;
CSerialDevice *pSerial ;
CKernel *pthis ;
volatile bool fwupd = false ;

CKernel::CKernel (void)
:	screen(options.GetWidth(), options.GetHeight()),
	timer(&interrupt),
	serial(&interrupt),
	logger(options.GetLogLevel (), &timer),
	cpuThrottle(CPUSpeedMaximum),
    usbhci(&interrupt, &timer, true),
	net(ipaddress, netmask, gateway, dns, "pip11-term"),
	console(&actLED, &deviceNameService, &interrupt, &timer),
	screenBrightness(100)
{
	pSerial = &serial ;
	pthis = this ;

	unsigned sbr = 0 ;
	CKernelOptions* kops = CKernelOptions::Get() ;
	if (kops) {
		sbr = kops->GetBacklight() ;
		if (sbr > 0) {
			screenBrightness = sbr ;
		}
	}
}

CKernel::~CKernel (void) {
	pthis = 0 ;
}

boolean CKernel::Initialize (void) {
	boolean bOK = TRUE ;

	if (bOK) {
		bOK = screen.Initialize() ;
	}

	if (bOK) {
		bOK = serial.Initialize(115200) ;
		serial.SetOptions(0) ;
	}

	if (bOK) {
		cpuThrottle.SetSpeed(CPUSpeedMaximum, false) ;
	}

	if (bOK) {
		CDevice *pTarget = deviceNameService.GetDevice(options.GetLogDevice(), FALSE) ;
		if (pTarget == 0) {
			pTarget = &screen;
		}

		bOK = logger.Initialize(pTarget) ;
	}

	if (bOK) {
		bOK = interrupt.Initialize();
	}

	if (bOK) {
		bOK = timer.Initialize();
	}

	if (bOK) {
		bOK = usbhci.Initialize ();

		if (bOK) {
			logger.Write("kernel", LogNotice, "waiting for keyboard") ;

			while (!console.hasKeyboard()) {
			    bool updated = usbhci.UpdatePlugAndPlay() ;
				if (updated) {
					CUSBKeyboardDevice *keyboard = (CUSBKeyboardDevice *) deviceNameService.GetDevice("ukbd1", FALSE) ;
					console.attachKeyboard(keyboard) ;
				}
			}

			logger.Write("kernel", LogNotice, "keyboard attached") ;
		}
	}

	if (bOK) {
		bOK = net.Initialize() ;

		if (bOK) {
			CString ips ;
			net.GetConfig()->GetIPAddress()->Format(&ips);
			logger.Write("CKernel::Initialize", LogError, ips) ;
		} else {
			logger.Write("CKernel::Initialize", LogError, "net init") ;
		}
	}

	if (bOK) {
		this->console.init(&this->screen) ;
	}

	return bOK ;
}

bool CKernel::hotkeyHandler(const unsigned char modifiers, const unsigned char hid_key, void *context) {
    Console *console = (Console *)context ;
    if (
        modifiers & (KEYBOARD_MODIFIER_LEFTCTRL) &&
        modifiers & (KEYBOARD_MODIFIER_LEFTALT) &&
        hid_key == HID_KEY_DELETE
    ) {
		console->shutdownMode = ShutdownReboot ;
		return true ;
    }

    if (
        modifiers & (KEYBOARD_MODIFIER_RIGHTCTRL) &&
        modifiers & (KEYBOARD_MODIFIER_RIGHTALT) &&
        hid_key == HID_KEY_DELETE
    ) {
        u8 c = 0201 ;
		pSerial->Write(&c, 1) ;
		return true ;
    }

	if (
        modifiers & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL) &&
        modifiers & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT) &&
        hid_key == HID_KEY_BACKSPACE
    ) {
        u8 c = 0202 ;
		pSerial->Write(&c, 1) ;
        return true ;
    }
	
	if (
        modifiers & (KEYBOARD_MODIFIER_RIGHTCTRL) &&
        modifiers & (KEYBOARD_MODIFIER_RIGHTALT) &&
        hid_key == HID_KEY_F12
    ) {
        u8 c = 0203 ;
		pSerial->Write(&c, 1) ;
        return true ;
    }

	if (!modifiers && hid_key == HID_KEY_F12) {
        console->vt52_mode = !console->vt52_mode ;
        console->showStatus() ;
        return true ;
    }

    if (!modifiers && hid_key == HID_KEY_F11) {
        fwupd = true ;
        return true ;
    }

    if (modifiers & KEYBOARD_MODIFIER_LEFTGUI && hid_key == HID_KEY_SPACE) {
        console->koi7n1 = !console->koi7n1 ;
        console->showRusLat() ;
        return true ;
    }

	if (!modifiers && hid_key == HID_KEY_F9) {
		if (pthis->screenBrightness > 50) {
			pthis->screenBrightness -= 10 ;
			pthis->screen.GetFrameBuffer()->SetBacklightBrightness(pthis->screenBrightness) ;
			iprintf("Brightness %d", pthis->screenBrightness) ;
		}
		return true ;
	}

	if (!modifiers && hid_key == HID_KEY_F10) {
		if (pthis->screenBrightness < 180) {
			pthis->screenBrightness += 10 ;
			pthis->screen.GetFrameBuffer()->SetBacklightBrightness(pthis->screenBrightness) ;
			iprintf("Brightness %d", pthis->screenBrightness) ;
		}
		return true ;
	}

    return false ;
}

TShutdownMode CKernel::updateFirmware() {
	bool bOK = true ;
	screen.ClearScreen() ;

	CEMMCDevice emmc(&interrupt, &timer, &actLED) ;
	if (bOK) {
		bOK = emmc.Initialize() ;
		if (!bOK) {
			logger.Write("CKernel::updateFirmware", LogError, " emmc Initialize error") ;
		}
	}

	FATFS fileSystem ;

	if (bOK) {
		if (FR_OK != f_mount(&fileSystem, "SD:", 1)) {
			logger.Write("Kernel", LogError, "SD Card not inserted or SD Card error!") ;
		} else {
			new Firmware(&net, &fileSystem) ;
		}
	}

	while (true) {
		TShutdownMode mode = this->console.loop() ;

		if (mode != ShutdownNone) {
			return mode ;
		}

		scheduler.Yield() ;
	}
	

	return ShutdownNone ;
}

TShutdownMode CKernel::Run (void) {
	unsigned char c ;

	console.vtCls() ;
	console.setHotkeyHandler(hotkeyHandler, &console) ;
	console.showStatus() ;
	console.showRusLat() ;
	console.showThrottle(false) ;
	CString txt ;
	txt.Format("PiTerm " __DATE__ " " __TIME__ " %dx%d (%dx%d) (%dx%d)", screen.GetWidth(), screen.GetHeight(), screen.GetColumns(), screen.GetRows(), screen.getCharWidth(), screen.getCharHeight()) ;
	this->console.write(txt, 0, 23, BRIGHT_BLACK_COLOR) ;

	bool clean = false ;

	while (true) {
		TShutdownMode mode = this->console.loop() ;

		if (mode != ShutdownNone) {
			return mode ;
		}

		if (fwupd) {
			mode = updateFirmware() ;
			if (mode != ShutdownNone) {
				return mode ;
			}
		}

		if (queue_try_remove(&keyboard_queue, &c)) {
			if (!clean) {
				clean = true ;
                console.vtFillRegion(0, 23, TEXTMODE_COLS, 23, ' ', WHITE_COLOR, CONS_BACKGROUND_COLOR) ;
			}

			if (serial.Write(&c, 1) != 1) {
				gprintf("serial write error") ;
			}
		}

		if (serial.Read(&c, 1) == 1) {
			console.putCharVT(c) ;
		}

		cpuThrottle.Update() ;
		scheduler.Yield() ;
	}
}
