using Microsoft.UI.Xaml;

namespace MagicMouse.Settings;

public partial class App : Application
{
    private MainWindow? _window;

    public App()
    {
        this.InitializeComponent();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        _window = new MainWindow();
        _window.Activate();
    }
}
