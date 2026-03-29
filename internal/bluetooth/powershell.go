package bluetooth

// powershell.go — PowerShell helpers for device detection (fallback)

import (
	"encoding/json"
	"log"
	"os/exec"
	"strings"
	"syscall"
)

// pnpDevice is used to parse PowerShell output
type pnpDevice struct {
	FriendlyName string `json:"FriendlyName"`
	InstanceID   string `json:"InstanceId"`
	Status       string `json:"Status"`
	Present      bool   `json:"Present"`
}

// queryPnpDevice uses PowerShell to find Magic Mouse in PnP devices
func queryPnpDevice() *pnpDevice {
	log.Println("[PowerShell] Starting queryPnpDevice scan...")
	script := `
$devices = Get-PnpDevice | Where-Object {
    $_.FriendlyName -like '*Magic Mouse*' -or
    $_.FriendlyName -like "*Apple*Mouse*" -or
    ($_.InstanceId -like '*BTHENUM\DEV_*' -and $_.FriendlyName -like '*Mouse*') -or
    ($_.InstanceId -like '*VID&0001004C*PID&0269*') -or
    ($_.InstanceId -like '*VID&0001004C*PID&030D*') -or
    ($_.InstanceId -like '*VID&0001004C*PID&0374*')
}

if ($devices) {
    # Prefer the actual Bluetooth device over the generic HID interface
    $bthDevice = $devices | Where-Object { $_.InstanceId -like '*BTHENUM\DEV_*' -or $_.Class -eq 'Bluetooth' } | Select-Object -First 1
    if (-not $bthDevice) {
        $bthDevice = $devices | Select-Object -First 1
    }

    $obj = @{
        FriendlyName = $bthDevice.FriendlyName
        InstanceId   = $bthDevice.InstanceId
        Status       = $bthDevice.Status
        Present      = ($bthDevice.Status -eq 'OK')
    }
    ConvertTo-Json $obj
}
`
	out, err := runPowerShell(script)
	if err != nil {
		log.Printf("[PowerShell] queryPnpDevice script error: %v, Output: %s", err, string(out))
		return nil
	}

	strOut := strings.TrimSpace(out)
	if strOut == "" {
		log.Println("[PowerShell] No PnP devices matched criteria.")
		return nil
	}

	log.Printf("[PowerShell] Received JSON: %s", strOut)

	var dev pnpDevice
	if err := json.Unmarshal([]byte(strOut), &dev); err != nil {
		log.Printf("[PowerShell] JSON Unmarshal error: %v (raw string: %s)", err, strOut)
		return nil
	}

	log.Printf("[PowerShell] Successfully parsed PnP device: %s (Status: %s)", dev.FriendlyName, dev.Status)
	return &dev
}

// queryAllBluetoothDevices returns all BT/HID devices for diagnostics
func QueryAllBluetoothDevices() (string, error) {
	script := `
Get-PnpDevice | Where-Object {
    $_.Class -eq 'Bluetooth' -or $_.Class -eq 'Mouse' -or
    $_.Class -eq 'HIDClass' -or $_.InstanceId -like '*BTHENUM*'
} | Select-Object FriendlyName, InstanceId, Status, Class | ConvertTo-Json
`
	return runPowerShell(script)
}

// runPowerShell executes a PowerShell script hidden (no popup window) and returns stdout
func runPowerShell(script string) (string, error) {
	cmd := exec.Command("powershell",
		"-NoProfile",
		"-NonInteractive",
		"-WindowStyle", "Hidden",
		"-Command", script,
	)
	cmd.SysProcAttr = &syscall.SysProcAttr{
		HideWindow:    true,
		CreationFlags: 0x08000000, // CREATE_NO_WINDOW
	}
	out, err := cmd.Output()
	if err != nil {
		log.Printf("PowerShell error: %v", err)
		return "", err
	}
	return string(out), nil
}
