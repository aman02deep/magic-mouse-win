package bluetooth

// battery_hid.go — Battery reading strategies that work without admin access

import (
	"fmt"
	"log"
	"strconv"
	"strings"

	"magicmouse-util/internal/device"
)

// ReadBattery tries strategies to get battery level for the given InstanceId.
// ReadBattery attempts multiple non-admin strategies to read Magic Mouse battery.
func ReadBattery(instanceID string) (level int, chargingState device.ChargingState) {
	log.Printf("[Battery] Starting lookup for base InstanceID=%s", instanceID)

	// Strategy 1: DEVPKEY_Device_BatteryLevel lookup on HID children
	log.Println("[Battery] Attempting Strategy 1 (HID DEVPKEY)...")
	level, err := batteryViaHIDNode(instanceID)
	if err == nil && level >= 0 {
		log.Printf("[Battery] Strategy 1 successful: %d%%", level)
		return level, device.StateDischarging
	}
	log.Printf("[Battery] Strategy 1 failed: %v", err)

	// Strategy 2: Property scan on parent node
	log.Println("[Battery] Attempting Strategy 2 (PnP Property Scan)...")
	level, err = batteryViaPnpProperty(instanceID)
	if err == nil && level >= 0 {
		log.Printf("[Battery] Strategy 2 successful: %d%%", level)
		return level, device.StateDischarging
	}
	log.Printf("[Battery] Strategy 2 failed: %v", err)

	log.Println("[Battery] All strategies failed - returning Unavailable.")
	return -1, device.StateUnknown
}

// batteryViaHIDNode reads battery from HID collection nodes matching Apple VID
func batteryViaHIDNode(instanceID string) (int, error) {
	script := `
$hidDevices = Get-PnpDevice | Where-Object {
    ($_.InstanceId -like 'HID\VID_05AC&PID_0269*') -or
    ($_.InstanceId -like 'HID\VID_05AC&PID_030D*') -or
    ($_.InstanceId -like 'HID\VID_05AC&PID_0374*')
}
foreach ($d in $hidDevices) {
    try {
        $prop = Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName '{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2' -ErrorAction Stop
        if ($prop.Type -ne 'Empty' -and $null -ne $prop.Data -and $prop.Data -ge 0 -and $prop.Data -le 100) {
            Write-Output "HID:$($prop.Data)"; exit
        }
    } catch {}

    # Registry fallback
    $regPath = "HKLM:\SYSTEM\CurrentControlSet\Enum\" + $d.InstanceId + "\Device Parameters"
    try {
        $val = Get-ItemPropertyValue -Path $regPath -Name "BatteryLevel" -ErrorAction Stop
        if ($val -ge 0 -and $val -le 100) { Write-Output "REG:$val"; exit }
    } catch {}
}
Write-Output "-1"
`
	out, err := runPowerShell(script)
	if err != nil {
		return -1, fmt.Errorf("powershell failed: %w, %s", err, out)
	}

	lines := strings.Split(strings.TrimSpace(out), "\n")
	for _, raw := range lines {
		line := strings.TrimSpace(raw)
		if line == "" {
			continue
		}
		log.Printf("[Battery-PnP] Parsed line: '%s'", line)
		parts := strings.SplitN(line, ":", 2)
		if len(parts) == 2 {
			valStr := strings.TrimSpace(parts[1])
			val, parseErr := strconv.Atoi(valStr)
			if parseErr == nil {
				log.Printf("[Battery-PnP] Successfully parsed %d%% from %s", val, parts[0])
				return val, nil
			}
		}
	}

	return -1, fmt.Errorf("no valid battery property returned via PnP")
}

// batteryViaPnpProperty scans DEVPKEY on the device and its Apple HID siblings
func batteryViaPnpProperty(instanceID string) (int, error) {
	script := fmt.Sprintf(`
$ids = @('%s')
$hidNodes = Get-PnpDevice | Where-Object { $_.InstanceId -like 'HID\VID_05AC*' } | Select-Object -ExpandProperty InstanceId
$ids += $hidNodes
foreach ($id in $ids) {
    try {
        $prop = Get-PnpDeviceProperty -InstanceId $id -KeyName '{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2' -ErrorAction Stop
        if ($prop.Type -ne 'Empty' -and $null -ne $prop.Data -and $prop.Data -ge 0 -and $prop.Data -le 100) {
            Write-Output $prop.Data; exit
        }
    } catch {}
}
Write-Output "-1"
`, instanceID)

	out, err := runPowerShell(script)
	if err != nil {
		return -1, fmt.Errorf("powershell failed: %w, %s", err, out)
	}
	level, err := strconv.Atoi(firstLine(out))
	if err != nil || level < 0 || level > 100 {
		return -1, fmt.Errorf("invalid battery percentage: %s", out)
	}
	return level, nil
}

func firstLine(s string) string {
	s = strings.TrimSpace(s)
	if idx := strings.Index(s, "\n"); idx != -1 {
		return strings.TrimSpace(s[:idx])
	}
	return s
}
