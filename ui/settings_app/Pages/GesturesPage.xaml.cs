using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace MagicMouse.Settings.Pages;

public partial class GesturesPage : UserControl
{
    private static readonly string ConfigPath = Path.Combine(
        AppContext.BaseDirectory, "..", "..", "..", "..", "..", "configs", "default.json");

    private JsonObject _config = new();
    private bool _isLoaded = false;

    public GesturesPage()
    {
        InitializeComponent();
        LoadConfig();
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

            var gestures = _config["gestures"]?.AsObject() ?? new JsonObject();

            SwipeEnabled.IsChecked = gestures["swipe_enabled"]?.GetValue<bool>() ?? true;
            SwipeSensitivity.Value = gestures["swipe_sensitivity"]?.GetValue<int>() ?? 5;

            ThreeFingerEnabled.IsChecked = gestures["three_finger_enabled"]?.GetValue<bool>() ?? true;
            ThreeFingerSensitivity.Value = gestures["three_finger_sensitivity"]?.GetValue<int>() ?? 5;

            TapClickEnabled.IsChecked = gestures["tap_click_enabled"]?.GetValue<bool>() ?? false;
            SmartZoomEnabled.IsChecked = gestures["smart_zoom_enabled"]?.GetValue<bool>() ?? true;

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
            var gestures = _config["gestures"]?.AsObject() ?? new JsonObject();

            gestures["swipe_enabled"] = SwipeEnabled.IsChecked == true;
            gestures["swipe_sensitivity"] = (int)SwipeSensitivity.Value;

            gestures["three_finger_enabled"] = ThreeFingerEnabled.IsChecked == true;
            gestures["three_finger_sensitivity"] = (int)ThreeFingerSensitivity.Value;

            gestures["tap_click_enabled"] = TapClickEnabled.IsChecked == true;
            gestures["smart_zoom_enabled"] = SmartZoomEnabled.IsChecked == true;

            _config["gestures"] = gestures;

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

    private void Gesture_Toggled(object sender, RoutedEventArgs e)
    {
        if (_isLoaded) StatusBar.Visibility = Visibility.Collapsed;
    }

    private void Sensitivity_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_isLoaded) StatusBar.Visibility = Visibility.Collapsed;
    }

    private void SaveButton_Click(object sender, RoutedEventArgs e)
    {
        SaveConfig();
    }
}
