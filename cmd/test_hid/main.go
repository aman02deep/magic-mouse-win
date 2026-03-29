package main

import (
	"fmt"

	"github.com/karalabe/hid"
)

func main() {
	devices := hid.Enumerate(0, 0)
	if len(devices) == 0 {
		fmt.Println("No HID devices found at all.")
		return
	}
	fmt.Printf("Found %d total HID devices\n", len(devices))
	for _, dev := range devices {
		if dev.VendorID != 0 {
			fmt.Printf("VID: 0x%04x PID: 0x%04x UsagePage: 0x%04x Usage: 0x%04x Product: %s\n",
				dev.VendorID, dev.ProductID, dev.UsagePage, dev.Usage, dev.Product)
		}
	}
}
