using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace MagicMouse.Settings.Pages;

public sealed partial class GeneralPage : Page
{
    // Path to the shared config consumed by the service/engines.
    private static readonly string ConfigPath = Path.Combine(
        AppContext.BaseDirectory, "..", "..", "..", "..", "..", "configs", "default.json");

    private JsonObject _config = new();

    public GeneralPage()
    {
        this.InitializeComponent();
        LoadConfig();
    }

    // ── Config I/O ────────────────────────────────────────────────────────────

    private void LoadConfig()
    {
        try
        {
            if (File.Exists(ConfigPath))
            {
                var text = File.ReadAllText(ConfigPath);
                _config = JsonNode.Parse(text)?.AsObject() ?? new JsonObject();
            }

            EnableToggle.IsOn  = _config["enabled"]?.GetValue<bool>()   ?? true;
            StartupToggle.IsOn = _config["run_at_startup"]?.GetValue<bool>() ?? true;
        }
        catch (Exception ex)
        {
            ShowStatus(InfoBarSeverity.Error, $"Could not load config: {ex.Message}");
        }
    }

    private void SaveConfig()
    {
        try
        {
            _config["enabled"]       = EnableToggle.IsOn;
            _config["run_at_startup"] = StartupToggle.IsOn;

            var dir = Path.GetDirectoryName(ConfigPath)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(ConfigPath,
                JsonSerializer.Serialize(_config, new JsonSerializerOptions { WriteIndented = true }));

            ShowStatus(InfoBarSeverity.Success, "Settings saved.");
        }
        catch (Exception ex)
        {
            ShowStatus(InfoBarSeverity.Error, $"Could not save config: {ex.Message}");
        }
    }

    private void ShowStatus(InfoBarSeverity severity, string message)
    {
        StatusBar.Severity = severity;
        StatusBar.Message  = message;
        StatusBar.IsOpen   = true;
    }

    // ── Startup registry helper ───────────────────────────────────────────────

    private static void SetStartup(bool enable)
    {
        const string keyPath  = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
        const string valueName = "MagicMouseTrayHost";

        using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(keyPath, writable: true);
        if (key is null) return;

        if (enable)
        {
            var exe = Path.Combine(AppContext.BaseDirectory, "tray_host.exe");
            key.SetValue(valueName, $"\"{exe}\"");
        }
        else
        {
            key.DeleteValue(valueName, throwOnMissingValue: false);
        }
    }

    // ── Event handlers ────────────────────────────────────────────────────────

    private void EnableToggle_Toggled(object sender, RoutedEventArgs e)
    {
        SaveConfig();
        // TODO: send enable/disable IPC command to the service.
    }

    private void StartupToggle_Toggled(object sender, RoutedEventArgs e)
    {
        SetStartup(StartupToggle.IsOn);
        SaveConfig();
    }
}
