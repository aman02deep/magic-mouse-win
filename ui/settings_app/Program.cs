using System;
using Microsoft.UI.Xaml;
using Microsoft.Windows.ApplicationModel.DynamicDependency;

namespace MagicMouse.Settings;

class Program
{
    [STAThread]
    static void Main(string[] args)
    {
        // Explicitly initialize the Windows App Runtime bootstrap.
        // This is required for unpackaged WinUI 3 apps (WindowsPackageType=None)
        // to properly find and load the Windows App SDK runtime.
        Bootstrap.Initialize(0x00010005); // Windows App SDK 1.5

        try
        {
            WinRT.ComWrappersSupport.InitializeComWrappers();
            Application.Start(p => new App());
        }
        finally
        {
            Bootstrap.Uninitialize();
        }
    }
}
