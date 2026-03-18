using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace MagicMouse.Settings.Pages;

public partial class ScrollPage : UserControl
{
    private static readonly string ConfigPath = Path.Combine(
        AppContext.BaseDirectory, "..", "..", "..", "..", "..", "configs", "default.json");

    private JsonObject _config = new();
    private bool _isLoaded = false;

    public ScrollPage()
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

            var scroll = _config["scroll"]?.AsObject() ?? new JsonObject();

            NaturalScrollToggle.IsChecked = scroll["natural"]?.GetValue<bool>() ?? true;
            SpeedSlider.Value = scroll["speed"]?.GetValue<int>() ?? 5;
            InertiaToggle.IsChecked = scroll["inertia"]?.GetValue<bool>() ?? true;
            FrictionSlider.Value = scroll["inertia_friction"]?.GetValue<int>() ?? 4;
            CurveCombo.SelectedIndex = scroll["curve"]?.GetValue<int>() ?? 1;

            _isLoaded = true;
            UpdateUI();
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
            var scroll = _config["scroll"]?.AsObject() ?? new JsonObject();
            scroll["natural"] = NaturalScrollToggle.IsChecked == true;
            scroll["speed"] = (int)SpeedSlider.Value;
            scroll["inertia"] = InertiaToggle.IsChecked == true;
            scroll["inertia_friction"] = (int)FrictionSlider.Value;
            scroll["curve"] = CurveCombo.SelectedIndex;

            _config["scroll"] = scroll;

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

    private void UpdateUI()
    {
        if (!_isLoaded) return;
        InertiaPanel.Visibility = InertiaToggle.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        StatusBar.Visibility = Visibility.Collapsed;
    }

    private void NaturalScroll_Toggled(object sender, RoutedEventArgs e) => UpdateUI();
    private void Speed_Changed(object sender, RoutedPropertyChangedEventArgs<double> e) => UpdateUI();
    private void Inertia_Toggled(object sender, RoutedEventArgs e) => UpdateUI();
    private void Friction_Changed(object sender, RoutedPropertyChangedEventArgs<double> e) => UpdateUI();
    private void Curve_Changed(object sender, SelectionChangedEventArgs e) => UpdateUI();

    private void SaveButton_Click(object sender, RoutedEventArgs e)
    {
        SaveConfig();
    }
}
