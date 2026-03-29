package tray

// tray.go — Pure Go system tray using Shell_NotifyIconW (no CGo, no external deps)

import (
	"fmt"
	"log"
	"os/exec"
	"runtime"
	"sync"
	"unsafe"

	"magicmouse-util/internal/bluetooth"

	"golang.org/x/sys/windows"
)

const (
	wmApp          = 0x8000 // WM_APP
	wmTrayCallback = wmApp + 1

	nimAdd    = 0x00000000
	nimModify = 0x00000001
	nimDelete = 0x00000002

	nifIcon    = 0x00000002
	nifTip     = 0x00000004
	nifMessage = 0x00000001

	wmLButtonUp  = 0x0202
	wmRButtonUp  = 0x0205
	wmCommand    = 0x0111
	wmDestroy    = 0x0002
	wmCreate     = 0x0001

	mfString    = 0x00000000
	mfSeparator = 0x00000800
	mfGrayed    = 0x00000001

	tpmBottomAlign = 0x0020
	tpmLeftAlign   = 0x0000

	idOpen = 1001
	idQuit = 1002
)

var (
	shell32          = windows.NewLazySystemDLL("shell32.dll")
	user32           = windows.NewLazySystemDLL("user32.dll")
	kernel32         = windows.NewLazySystemDLL("kernel32.dll")

	pShellNotifyIcon  = shell32.NewProc("Shell_NotifyIconW")
	pLoadImage        = user32.NewProc("LoadImageW")
	pRegisterClassExW = user32.NewProc("RegisterClassExW")
	pCreateWindowExW  = user32.NewProc("CreateWindowExW")
	pDefWindowProcW   = user32.NewProc("DefWindowProcW")
	pGetMessageW      = user32.NewProc("GetMessageW")
	pTranslateMessage = user32.NewProc("TranslateMessage")
	pDispatchMessageW = user32.NewProc("DispatchMessageW")
	pPostQuitMessage  = user32.NewProc("PostQuitMessage")
	pCreatePopupMenu  = user32.NewProc("CreatePopupMenu")
	pAppendMenuW      = user32.NewProc("AppendMenuW")
	pTrackPopupMenu   = user32.NewProc("TrackPopupMenu")
	pDestroyMenu      = user32.NewProc("DestroyMenu")
	pSetForegroundWindow = user32.NewProc("SetForegroundWindow")
	pGetCursorPos     = user32.NewProc("GetCursorPos")
	pGetModuleHandleW = kernel32.NewProc("GetModuleHandleW")
)

// NOTIFYICONDATAW is the Shell_NotifyIconW struct
type notifyIconData struct {
	CbSize           uint32
	HWnd             uintptr
	UID              uint32
	UFlags           uint32
	UCallbackMessage uint32
	HIcon            uintptr
	SzTip            [128]uint16
}

type wndClassEx struct {
	CbSize        uint32
	Style         uint32
	LpfnWndProc   uintptr
	CbClsExtra    int32
	CbWndExtra    int32
	HInstance     uintptr
	HIcon         uintptr
	HCursor       uintptr
	HbrBackground uintptr
	LpszMenuName  *uint16
	LpszClassName *uint16
	HIconSm       uintptr
}

type msg struct {
	HWnd    uintptr
	Message uint32
	WParam  uintptr
	LParam  uintptr
	Time    uint32
	PtX     int32
	PtY     int32
}

type point struct {
	X, Y int32
}

// App manages the system tray icon
type App struct {
	monitor *bluetooth.Monitor
	hwnd    uintptr
	nid     notifyIconData
	mu      sync.Mutex
}

// New creates a new tray app
func New(monitor *bluetooth.Monitor) *App {
	return &App{monitor: monitor}
}

// Start initializes and runs the system tray (blocks)
func (a *App) Start() {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	hInst, _, _ := pGetModuleHandleW.Call(0)

	className, _ := windows.UTF16PtrFromString("MagicMouseTray")
	winName, _ := windows.UTF16PtrFromString("MagicMouseTrayWin")

	wc := wndClassEx{
		Style:         0x0003, // CS_HREDRAW | CS_VREDRAW
		LpfnWndProc:   windows.NewCallback(a.wndProc),
		HInstance:     hInst,
		LpszClassName: className,
	}
	wc.CbSize = uint32(unsafe.Sizeof(wc))

	atom, _, err := pRegisterClassExW.Call(uintptr(unsafe.Pointer(&wc)))
	if atom == 0 {
		log.Printf("Tray: RegisterClassExW failed: %v", err)
		return
	}

	hwnd, _, err := pCreateWindowExW.Call(
		0,
		uintptr(unsafe.Pointer(className)),
		uintptr(unsafe.Pointer(winName)),
		0, // WS_POPUP not needed for message-only
		0, 0, 0, 0,
		^uintptr(2), // HWND_MESSAGE
		0, hInst, 0,
	)
	if hwnd == 0 {
		log.Printf("Tray: CreateWindowExW failed: %v", err)
		return
	}
	a.hwnd = hwnd

	// Load default application icon
	icon := loadDefaultIcon()

	// Create notification icon
	a.nid = notifyIconData{
		HWnd:             hwnd,
		UID:              1,
		UFlags:           nifIcon | nifTip | nifMessage,
		UCallbackMessage: wmTrayCallback,
		HIcon:            icon,
	}
	a.nid.CbSize = uint32(unsafe.Sizeof(a.nid))
	setTipText(&a.nid, "Magic Mouse Utility")

	pShellNotifyIcon.Call(nimAdd, uintptr(unsafe.Pointer(&a.nid)))

	log.Println("Tray: icon added")

	// Start background updater
	go a.updateLoop()

	// Message pump
	var m msg
	for {
		r, _, _ := pGetMessageW.Call(uintptr(unsafe.Pointer(&m)), 0, 0, 0)
		if r == 0 || r == ^uintptr(0) {
			break
		}
		pTranslateMessage.Call(uintptr(unsafe.Pointer(&m)))
		pDispatchMessageW.Call(uintptr(unsafe.Pointer(&m)))
	}

	// Cleanup
	pShellNotifyIcon.Call(nimDelete, uintptr(unsafe.Pointer(&a.nid)))
	log.Println("Tray: exited")
}

func (a *App) wndProc(hwnd, umsg, wParam, lParam uintptr) uintptr {
	switch umsg {
	case wmTrayCallback:
		switch lParam {
		case wmRButtonUp, wmLButtonUp:
			a.showMenu()
		}
		return 0
	case wmCommand:
		id := int(wParam & 0xFFFF)
		switch id {
		case idOpen:
			openBrowser("http://localhost:7878")
		case idQuit:
			pShellNotifyIcon.Call(nimDelete, uintptr(unsafe.Pointer(&a.nid)))
			pPostQuitMessage.Call(0)
		}
		return 0
	case wmDestroy:
		pPostQuitMessage.Call(0)
		return 0
	}
	r, _, _ := pDefWindowProcW.Call(hwnd, umsg, wParam, lParam)
	return r
}

func (a *App) showMenu() {
	hMenu, _, _ := pCreatePopupMenu.Call()
	if hMenu == 0 {
		return
	}

	mouse := a.monitor.GetMouse()

	// Status line (disabled/grayed)
	var statusText string
	if mouse != nil && mouse.Connected {
		statusText = fmt.Sprintf("🖱 %s", mouse.Name)
	} else {
		statusText = "⚠ No mouse connected"
	}
	statusPtr, _ := windows.UTF16PtrFromString(statusText)
	pAppendMenuW.Call(hMenu, mfString|mfGrayed, 0, uintptr(unsafe.Pointer(statusPtr)))

	// Battery line (disabled)
	var battText string
	if mouse != nil && mouse.BatteryAvailable {
		battText = fmt.Sprintf("🔋 Battery: %d%%", mouse.BatteryLevel)
	} else {
		battText = "🔋 Battery: unavailable"
	}
	battPtr, _ := windows.UTF16PtrFromString(battText)
	pAppendMenuW.Call(hMenu, mfString|mfGrayed, 0, uintptr(unsafe.Pointer(battPtr)))

	// Separator
	pAppendMenuW.Call(hMenu, mfSeparator, 0, 0)

	// Open Dashboard
	openPtr, _ := windows.UTF16PtrFromString("Open Dashboard")
	pAppendMenuW.Call(hMenu, mfString, uintptr(idOpen), uintptr(unsafe.Pointer(openPtr)))

	// Separator
	pAppendMenuW.Call(hMenu, mfSeparator, 0, 0)

	// Quit
	quitPtr, _ := windows.UTF16PtrFromString("Quit")
	pAppendMenuW.Call(hMenu, mfString, uintptr(idQuit), uintptr(unsafe.Pointer(quitPtr)))

	// Show at cursor
	var pt point
	pGetCursorPos.Call(uintptr(unsafe.Pointer(&pt)))
	pSetForegroundWindow.Call(a.hwnd)
	pTrackPopupMenu.Call(hMenu, tpmBottomAlign|tpmLeftAlign, uintptr(pt.X), uintptr(pt.Y), 0, a.hwnd, 0)
	pDestroyMenu.Call(hMenu)
}

func (a *App) updateLoop() {
	// Update tooltip periodically based on mouse state
	for {
		mouse := a.monitor.GetMouse()
		var tip string
		if mouse != nil && mouse.Connected {
			if mouse.BatteryAvailable {
				tip = fmt.Sprintf("Magic Mouse — %d%%", mouse.BatteryLevel)
			} else {
				tip = fmt.Sprintf("Magic Mouse — %s", mouse.Name)
			}
		} else {
			tip = "Magic Mouse — Disconnected"
		}

		a.mu.Lock()
		setTipText(&a.nid, tip)
		pShellNotifyIcon.Call(nimModify, uintptr(unsafe.Pointer(&a.nid)))
		a.mu.Unlock()

		// Sleep 10s using a channel to avoid importing time in tight loop
		select {
		case <-make(chan struct{}):
		default:
		}
		// Use a goroutine-safe sleep
		sleepMs(10000)
	}
}

func setTipText(nid *notifyIconData, text string) {
	tip, _ := windows.UTF16FromString(text)
	copy(nid.SzTip[:], tip)
}

func loadDefaultIcon() uintptr {
	// Load the standard Windows application icon (IDI_APPLICATION = 32512)
	icon, _, _ := pLoadImage.Call(
		0,                    // NULL hInstance = load system icon
		uintptr(32512),       // IDI_APPLICATION
		1,                    // IMAGE_ICON
		0, 0,                 // default size
		0x00008000,           // LR_SHARED
	)
	return icon
}

func openBrowser(url string) {
	var cmd *exec.Cmd
	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("rundll32", "url.dll,FileProtocolHandler", url)
	default:
		cmd = exec.Command("xdg-open", url)
	}
	if err := cmd.Start(); err != nil {
		log.Printf("Failed to open browser: %v", err)
	}
}

// sleepMs sleeps for the given milliseconds using Windows API
func sleepMs(ms uint32) {
	k32Sleep := kernel32.NewProc("Sleep")
	k32Sleep.Call(uintptr(ms))
}
