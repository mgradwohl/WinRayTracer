#include "pch.h"

#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#undef GetCurrentTime

#include <string>
#include <sstream>
#include <algorithm>

#include <WinUser.h>
#include <Shobjidl.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include "microsoft.ui.xaml.window.h"
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Microsoft.Graphics.Canvas.Text.h>
#include <winrt/Microsoft.Graphics.Canvas.UI.Xaml.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>

#include <wil/cppwinrt.h>
#include <wil/cppwinrt_helpers.h>

#include "Log.h"
#include "fpscounter.h"

using namespace winrt;
namespace winrt::WinRayTracer::implementation
{
    void MainWindow::InitializeComponent()
    {
        Util::Log::Init();
        ML_INFO("Log Initialized");
        ML_METHOD;

        //https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
        MainWindowT::InitializeComponent();

        PropertyChanged({ this, &MainWindow::OnPropertyChanged });

        SetMyTitleBar();
        OnFirstRun();
        StartGameLoop();
    }

    void MainWindow::OnFirstRun()
    {
        ML_METHOD;

        //initializes _dpi
        _dpi = gsl::narrow_cast<float>(GetDpiForWindow(GetWindowHandle()));
        const float dpi2 = canvasBoard().Dpi();

        if (dpi2 < _dpi)
        {
            canvasBoard().DpiScale(_dpi / dpi2);
        }
        const float dpi3 = canvasBoard().Dpi();
        if (_dpi != dpi3)
        {
            __debugbreak();
		}

        // initialize _canvasDevice
        _canvasDevice = Microsoft::Graphics::Canvas::CanvasDevice::GetSharedDevice();

    }

    void MainWindow::Pause()
    {
        SetStatus("Paused. Press Play to start. Left mouse button to draw. Right right mouse button to erase.");
        GoButton().Icon(Microsoft::UI::Xaml::Controls::SymbolIcon(Microsoft::UI::Xaml::Controls::Symbol::Play));
        GoButton().Label(L"Play");
    }

    void MainWindow::Play()
    {
        SetStatus("Running... Left mouse button to draw. Right right mouse button to erase.");
        GoButton().Icon(Microsoft::UI::Xaml::Controls::SymbolIcon(Microsoft::UI::Xaml::Controls::Symbol::Pause));
        GoButton().Label(L"Pause");
    }

    void MainWindow::StartGameLoop()
    {
        ML_METHOD;

        // prep the play button
        Pause();

        // start the FPSCounter
        fps.Start();

        // draw the initial population
        InvalidateIfNeeded();
    }

    void MainWindow::PumpProperties()
    {
        _propertyChanged(*this, PropertyChangedEventArgs{ L"FPSAverage" });
        _propertyChanged(*this, PropertyChangedEventArgs{ L"GenerationCount" });
        _propertyChanged(*this, PropertyChangedEventArgs{ L"LiveCount" });
    }

    void MainWindow::InvalidateIfNeeded()
    {
        canvasBoard().Invalidate();
        PumpProperties();
    }

    fire_and_forget MainWindow::ShowMessageBox(const hstring& title, const hstring& message)
    {
        winrt::Microsoft::UI::Xaml::Controls::ContentDialog dialog;

        // XamlRoot must be set in the case of a ContentDialog running in a Desktop app
        dialog.XamlRoot(this->Content().XamlRoot());
        dialog.Style( Microsoft::UI::Xaml::Application::Current().Resources().TryLookup(winrt::box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>() );
        dialog.Title(winrt::box_value(title));
        dialog.Content(winrt::box_value(message));
        dialog.PrimaryButtonText(L"OK");
        dialog.DefaultButton(winrt::Microsoft::UI::Xaml::Controls::ContentDialogButton::Primary);

        auto result = co_await dialog.ShowAsync();
        result;
    }

    void MainWindow::OnPointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
    {
        if (sender != canvasBoard())
        {
            return;
        }

        if (e.GetCurrentPoint(canvasBoard().as<Microsoft::UI::Xaml::UIElement>()).Properties().IsLeftButtonPressed())
        {
            _PointerMode = PointerMode::Left;
        }
        else if (e.GetCurrentPoint(canvasBoard().as<Microsoft::UI::Xaml::UIElement>()).Properties().IsRightButtonPressed())
        {
			_PointerMode = PointerMode::Right;
		}
        else
        {
			_PointerMode = PointerMode::None;
		}
    }

    void MainWindow::OnPointerMoved([[maybe_unused]] winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
    {
        if (_PointerMode == PointerMode::None || _PointerMode == PointerMode::Middle)
        {
			return;
		}

        bool on = (_PointerMode == PointerMode::Left);

        for (const Microsoft::UI::Input::PointerPoint& point : e.GetIntermediatePoints(canvasBoard().as<Microsoft::UI::Xaml::UIElement>()))
        {

            //ML_TRACE("Point {},{} Cell grid {},{}", point.Position().X, point.Position().Y, g.x, g.y);
            //SetStatus("Drawing. Left mouse button to draw. Right right mouse button to erase.");

        }
        InvalidateIfNeeded();
    }

    void MainWindow::OnPointerReleased([[maybe_unused]] winrt::Windows::Foundation::IInspectable const& sender, [[maybe_unused]] winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e) noexcept
    {
        //SetStatus("Drawing mode completed.");
        _PointerMode = PointerMode::None;
    }
    
    void MainWindow::OnPointerExited([[maybe_unused]] winrt::Windows::Foundation::IInspectable const& sender, [[maybe_unused]] winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e) noexcept
    {
        //SetStatus("Drawing mode completed.");
        _PointerMode = PointerMode::None;
    }

    void MainWindow::SetBestCanvasandWindowSizes()
    {
        ML_METHOD;
        if (_dpi == 0.0f)
        {
            return;
        }

        const Microsoft::UI::WindowId idWnd = Microsoft::UI::GetWindowIdFromWindow(GetWindowHandle());

        // get the window size
        Microsoft::UI::Windowing::DisplayArea displayAreaFallback(nullptr);
        Microsoft::UI::Windowing::DisplayArea displayArea = Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(idWnd, Microsoft::UI::Windowing::DisplayAreaFallback::Nearest);
        const Windows::Graphics::RectInt32 rez = displayArea.OuterBounds();

        // have the renderer figure out the best canvas size, which initializes CanvasSize
        // TODO on WindowResize should call the below
        //_renderer.FindBestCanvasSize(rez.Height);

        // setup offsets for sensible default window size
        constexpr int border = 20; // from XAML TODO can we call 'measure' and just retrieve the border width?
        constexpr int stackpanelwidth = 200; // from XAML TODO can we call 'measure' and just retrieve the stackpanel width?
        constexpr int statusheight = 28;

        // ResizeClient wants pixels, not DIPs
        const int wndWidth = 800;
        const int wndHeight = 600;

        // resize the window
        if (auto appWnd = Microsoft::UI::Windowing::AppWindow::GetFromWindowId(idWnd); appWnd)
        {
            appWnd.ResizeClient(Windows::Graphics::SizeInt32{ wndWidth, wndHeight });
        }
    }

    void MainWindow::CanvasBoard_Draw([[maybe_unused]] Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl  const& sender, Microsoft::Graphics::Canvas::UI::Xaml::CanvasDrawEventArgs const& args)
    {
        fps.AddFrame();
        PumpProperties();
    }

    void MainWindow::SetStatus(const std::string& message)
    {
        _statusMain = message;
        _propertyChanged(*this, PropertyChangedEventArgs{ L"StatusMain" });
    }

    // property & event handlers
    void MainWindow::GoButton_Click([[maybe_unused]] IInspectable const& sender, [[maybe_unused]] winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        Play();
    }

    void MainWindow::OnRandomizeBoard()
    {
        SetStatus("Board reset");

        RandomizeBoard();
        InvalidateIfNeeded();

        StartGameLoop();
    }

    void MainWindow::OnCanvasDeviceChanged()
    {
        ML_METHOD;
        SetStatus("Canvas device changed");

        _canvasDevice = Microsoft::Graphics::Canvas::CanvasDevice::GetSharedDevice();
        OnDPIChanged();
        SetBestCanvasandWindowSizes();
        InvalidateIfNeeded();
    }

    void MainWindow::OnDPIChanged()
    {
        ML_METHOD;
        SetStatus("DPI changed");

        const auto dpi = gsl::narrow_cast<float>(GetDpiForWindow(GetWindowHandle()));
        if (_dpi != dpi)
        {
            ML_TRACE("MainWindow DPI changed from {} to {}", dpi, _dpi);
            _dpi = dpi;
            // TODO if dpi changes there's a lot of work to do in the renderer
        }
        else
        {
            ML_TRACE("MainWindow DPI Not Changed {}", _dpi);
        }
    }

    void MainWindow::OnBoardResized()
    {
        ML_METHOD;
        SetStatus("Board resized");
        // create the board, lock it in the case that OnTick is updating it
        // we lock it because changing board parameters will call StartGameLoop()
        Pause();

        InvalidateIfNeeded();

        StartGameLoop();
    }

    // boilerplate and standard Windows stuff below
    void MainWindow::OnPropertyChanged([[maybe_unused]] IInspectable const& sender, PropertyChangedEventArgs const& args)
    {
        if (args.PropertyName() == L"DPIChanged")
        {
            OnDPIChanged();
        }

        if (args.PropertyName() == L"MaxAge")
        {
            OnMaxAgeChanged();
        }

        if (args.PropertyName() == L"BoardWidth")
        {
            OnBoardResized();
        }

        if (args.PropertyName() == L"ShowLegend")
        {
            InvalidateIfNeeded();
        }

        if (args.PropertyName() == L"NewCanvasDevice")
        {
            OnCanvasDeviceChanged();
        }

        if (args.PropertyName() == L"RandomPercent")
        {
            OnRandomizeBoard();
        }

        if (args.PropertyName() == L"FirstTime")
        {
            //SetStatus("First run event.");
            //OnFirstRun();
        }
    }

    void MainWindow::CanvasBoard_CreateResources([[maybe_unused]] Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender, [[maybe_unused]] Microsoft::Graphics::Canvas::UI::CanvasCreateResourcesEventArgs const& args)
    {
        ML_METHOD;

        // TODO might want to do the code in the if-block in all cases (for all args.Reason()s
        if (args.Reason() == Microsoft::Graphics::Canvas::UI::CanvasCreateResourcesReason::DpiChanged)
        {
            _propertyChanged(*this, PropertyChangedEventArgs{ L"DPIChanged" });
        }

        if (args.Reason() == Microsoft::Graphics::Canvas::UI::CanvasCreateResourcesReason::NewDevice)
        {
            _propertyChanged(*this, PropertyChangedEventArgs{ L"NewCanvasDevice" });
        }

        if (args.Reason() == Microsoft::Graphics::Canvas::UI::CanvasCreateResourcesReason::FirstTime)
        {
            _propertyChanged(*this, PropertyChangedEventArgs{ L"FirstTime" });
        }
    }

    [[nodiscard]] hstring MainWindow::StatusMain() const
    {
        return winrt::to_hstring(_statusMain);
    }

    [[nodiscard]] hstring MainWindow::LiveCount() const
    {
        return winrt::to_hstring("hello");
    }

    [[nodiscard]] hstring MainWindow::GenerationCount() const
    {
        return winrt::to_hstring("world");
    }

    [[nodiscard]] hstring MainWindow::FPSAverage() const
    {
        std::string f = std::format("{:.2f}", fps.FPS());
        return winrt::to_hstring(f);
    }

    void MainWindow::CanvasBoard_SizeChanged([[maybe_unused]] IInspectable const& sender, [[maybe_unused]] winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e)
    {
        //_renderer.Size(BoardWidth(), BoardHeight());
    }

    void MainWindow::SetMyTitleBar()
    {
        // Set window title
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());

        const Microsoft::UI::WindowId idWnd = Microsoft::UI::GetWindowIdFromWindow(GetWindowHandle());
        if (auto appWnd = Microsoft::UI::Windowing::AppWindow::GetFromWindowId(idWnd); appWnd)
        {
            appWnd.Title(L"ModernLife");

#ifdef _DEBUG
            AppTitlePreview().Text(L"PREVIEW DEBUG");
#endif

            // position near the top of the screen only on launch
            Windows::Graphics::PointInt32 pos{ appWnd.Position() };
            pos.Y = 20;
            appWnd.Move(pos);
        }
    }

    [[nodiscard]] HWND MainWindow::GetWindowHandle() const
    {
        // get window handle, window ID
        auto windowNative{ this->try_as<::IWindowNative>() };
        HWND hWnd{ nullptr };
        winrt::check_hresult(windowNative->get_WindowHandle(&hWnd));
                
        return hWnd;
    }

    void MainWindow::OnWindowActivate([[maybe_unused]] IInspectable const& sender, [[maybe_unused]] WindowActivatedEventArgs const& args)
    {
        if (args.WindowActivationState() == Microsoft::UI::Xaml::WindowActivationState::Deactivated)
        {
            Microsoft::UI::Xaml::Media::SolidColorBrush brush = Microsoft::UI::Xaml::ResourceDictionary().Lookup(winrt::box_value(L"WindowCaptionForegroundDisabled")).as<Microsoft::UI::Xaml::Media::SolidColorBrush>();
            AppTitleTextBlock().Foreground(brush);
            AppTitlePreview().Foreground(brush);
        }
        else
        {
            Microsoft::UI::Xaml::Media::SolidColorBrush brush = Microsoft::UI::Xaml::ResourceDictionary().Lookup(winrt::box_value(L"WindowCaptionForeground")).as<Microsoft::UI::Xaml::Media::SolidColorBrush>();
            AppTitleTextBlock().Foreground(brush);
            AppTitlePreview().Foreground(brush);
        }
    }

    void MainWindow::OnWindowClosed([[maybe_unused]] IInspectable const& sender, [[maybe_unused]] winrt::Microsoft::UI::Xaml::WindowEventArgs const& args) noexcept
    {
        ML_METHOD;
        //timer.Revoke();
        Util::Log::Shutdown();
        //PropertyChangedRevoker();
    }

    void MainWindow::OnWindowResized([[maybe_unused]] Windows::Foundation::IInspectable const& sender, [[maybe_unused]] Microsoft::UI::Xaml::WindowSizeChangedEventArgs const& args) noexcept
    {
        ML_METHOD;

        if (_dpi == 0)
        {
            ML_TRACE("Ignoring resize, DPI == 0");
            return;
        }

        SetBestCanvasandWindowSizes();

        // TODO lots to do here if we let the user resize the window and that resizes the canvas
        // right now the canvas size is fixed
    }
}