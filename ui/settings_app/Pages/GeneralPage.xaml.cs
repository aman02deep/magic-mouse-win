using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace MagicMouse.Settings.Pages;

public partial class GeneralPage : UserControl
{
    private static readonly string ConfigPath = Path.Combine(
        AppContext.BaseDirectory, "..", "..", "..", "..", "..", "configs", "default.json");

    private JsonObject _config = new();
    private bool _isLoaded = false;

    public GeneralPage()
    {
        InitializeComponent();
        LoadConfig();
        VersionText.Text = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version?.ToString(3) ?? "0.1.4";
    }

    private void LoadConfig()
    {
        try
        {
            if (File.Exists(ConfigPath))
            {
                var text = File.ReadAllText(ConfigPath);
                _config = JsonNode.Parse(text)?.AsObject() ?? new JsonObject();
            }

            EnableToggle.IsChecked = _config["enabled"]?.GetValue<bool>() ?? true;
            StartupToggle.IsChecked = _config["run_at_startup"]?.GetValue<bool>() ?? true;
            _isLoaded = true;
        }
        catch (Exception ex)
        {
            ShowStatus(false, $"Could not load config: {ex.Message}");
        }
    }

    private void SaveConfig()
    {
        if (!_isLoaded) return;
        try
        {
            _config["enabled"] = EnableToggle.IsChecked == true;
            _config["run_at_startup"] = StartupToggle.IsChecked == true;

            var dir = Path.GetDirectoryName(ConfigPath)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(ConfigPath,
                JsonSerializer.Serialize(_config, new JsonSerializerOptions { WriteIndented = true }));

            ShowStatus(true, "Settings saved.");
        }
        catch (Exception ex)
        {
            ShowStatus(false, $"Could not save config: {ex.Message}");
        }
    }

    private void ShowStatus(bool success, string message)
    {
        StatusBar.Foreground = new SolidColorBrush(success ? Colors.LightGreen : Colors.Salmon);
        StatusBar.Text = message;
        StatusBar.Visibility = Visibility.Visible;
    }

    private static void SetStartup(bool enable)
    {
        const string keyPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
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

    private void Toggle_Changed(object sender, RoutedEventArgs e)
    {
        SaveConfig();
    }

    private void StartupToggle_Changed(object sender, RoutedEventArgs e)
    {
        SetStartup(StartupToggle.IsChecked == true);
        SaveConfig();
    }
}
