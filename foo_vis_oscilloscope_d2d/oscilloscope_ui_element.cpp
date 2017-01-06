#include "stdafx.h"

#include "oscilloscope_ui_element.h"

void oscilloscope_ui_element_instance::g_get_name(pfc::string_base & p_out) {
    p_out = "Oscilloscope (Direct2D)";
}

const char * oscilloscope_ui_element_instance::g_get_description() {
    return "Oscilloscope visualization using Direct2D.";
}

GUID oscilloscope_ui_element_instance::g_get_guid() {
    // {3DC976A0-F5DB-4B07-B9ED-01BDE2360249}
    static const GUID guid = 
    { 0x3dc976a0, 0xf5db, 0x4b07, { 0xb9, 0xed, 0x1, 0xbd, 0xe2, 0x36, 0x2, 0x49 } };

    return guid;
}

GUID oscilloscope_ui_element_instance::g_get_subclass() {
    return ui_element_subclass_playback_visualisation;
}

ui_element_config::ptr oscilloscope_ui_element_instance::g_get_default_configuration() {
    ui_element_config_builder builder;
    oscilloscope_config config;
    config.build(builder);
    return builder.finish(g_get_guid());
}

oscilloscope_ui_element_instance::oscilloscope_ui_element_instance(ui_element_config::ptr p_data, ui_element_instance_callback::ptr p_callback)
    : m_callback(p_callback)
    , m_last_refresh(0)
    , m_refresh_interval(50)
{
    set_configuration(p_data);
}

void oscilloscope_ui_element_instance::initialize_window(HWND p_parent) {
    this->Create(p_parent, nullptr, nullptr, 0, WS_EX_STATICEDGE);
}

void oscilloscope_ui_element_instance::set_configuration(ui_element_config::ptr p_data) {
    ui_element_config_parser parser(p_data);
    oscilloscope_config config;
    config.parse(parser);
    m_config = config;
}

ui_element_config::ptr oscilloscope_ui_element_instance::get_configuration() {
    ui_element_config_builder builder;
    m_config.build(builder);
    return builder.finish(g_get_guid());
}

void oscilloscope_ui_element_instance::notify(const GUID & p_what, t_size p_param1, const void * p_param2, t_size p_param2size) {
    if (p_what == ui_element_notify_colors_changed) {
        m_pStrokeBrush.Release();
        Invalidate();
    }
}

CWndClassInfo& oscilloscope_ui_element_instance::GetWndClassInfo()
{
	static ATL::CWndClassInfo wc =
	{
        { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, StartWindowProc,
		  0, 0, NULL, NULL, NULL, (HBRUSH) NULL, NULL, TEXT("OscilloscopeD2D"), NULL },
		NULL, NULL, IDC_ARROW, TRUE, 0, _T("")
	};
	return wc;
}

LRESULT oscilloscope_ui_element_instance::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    HRESULT hr = S_OK;
    
    hr = CreateDeviceIndependentResources();

    if (FAILED(hr)) {
        console::formatter() << core_api::get_my_file_name() << ": could not create Direct2D factory";
    }

    try {
        static_api_ptr_t<visualisation_manager> vis_manager;

        vis_manager->create_stream(m_vis_stream, 0);

        m_vis_stream->request_backlog(0.8);
        m_vis_stream->set_channel_mode(m_config.m_downmix_enabled ? visualisation_stream_v2::channel_mode_mono : visualisation_stream_v2::channel_mode_default);
    } catch (std::exception & exc) {
        console::formatter() << core_api::get_my_file_name() << ": exception while creating visualisation stream: " << exc;
    }

    return 0;
}

void oscilloscope_ui_element_instance::OnDestroy() {
    m_vis_stream.release();

    m_pDirect2dFactory.Release();
    m_pRenderTarget.Release();
    m_pStrokeBrush.Release();
}

void oscilloscope_ui_element_instance::OnTimer(UINT_PTR nIDEvent) {
    KillTimer(ID_REFRESH_TIMER);
    Invalidate();
}

void oscilloscope_ui_element_instance::OnPaint(CDCHandle dc) {
    Render();
    ValidateRect(nullptr);

    DWORD now = GetTickCount();
    if (m_vis_stream.is_valid()) {
        DWORD next_refresh = m_last_refresh + m_refresh_interval;
        // (next_refresh < now) would break when GetTickCount() overflows
        if ((long) (next_refresh - now) < 0) {
            next_refresh = now;
        }
        SetTimer(ID_REFRESH_TIMER, next_refresh - now);
    }
    m_last_refresh = now;
}

void oscilloscope_ui_element_instance::OnSize(UINT nType, CSize size) {
    if (m_pRenderTarget) {
        m_pRenderTarget->Resize(D2D1::SizeU(size.cx, size.cy));
    }
}

HRESULT oscilloscope_ui_element_instance::Render() {
    HRESULT hr = S_OK;

    hr = CreateDeviceResources();

    if (SUCCEEDED(hr)) {
        m_pRenderTarget->BeginDraw();

        m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

        t_ui_color colorBackground = m_callback->query_std_color(ui_color_background);

        m_pRenderTarget->Clear(D2D1::ColorF(GetRValue(colorBackground) / 255.0f, GetGValue(colorBackground) / 255.0f, GetBValue(colorBackground) / 255.0f));

        if (m_vis_stream.is_valid()) {
            double time;
            if (m_vis_stream->get_absolute_time(time)) {
                double window_duration = m_config.get_window_duration();
                audio_chunk_impl chunk;
                if (m_vis_stream->get_chunk_absolute(chunk, time - window_duration / 2, window_duration)) {
                    D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();

                    CComPtr<ID2D1PathGeometry> pPath;

                    hr = m_pDirect2dFactory->CreatePathGeometry(&pPath);

                    if (SUCCEEDED(hr)) {
                        CComPtr<ID2D1GeometrySink> pSink;
            
                        hr = pPath->Open(&pSink);

                        t_uint32 channel_count = chunk.get_channel_count();
                        t_uint32 sample_count = chunk.get_sample_count();
                        const audio_sample *samples = chunk.get_data();

                        for (t_uint32 channel_index = 0; channel_index < channel_count; ++channel_index) {
                            float zoom = (float) m_config.get_zoom_factor();
                            float channel_baseline = (float) (channel_index + 0.5) / (float) channel_count * rtSize.height;
                            for (t_uint32 sample_index = 0; sample_index < sample_count; ++sample_index) {
                                audio_sample sample = samples[sample_index * channel_count + channel_index];
                                float x = (float) sample_index / (float) (sample_count - 1) * rtSize.width;
                                float y = channel_baseline - sample * zoom * rtSize.height / 2 / channel_count;
                                if (sample_index == 0) {
                                    pSink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_HOLLOW);
                                } else {
                                    pSink->AddLine(D2D1::Point2F(x, y));
                                }
                            }
                            if (channel_count > 0 && sample_count > 0) {
                                pSink->EndFigure(D2D1_FIGURE_END_OPEN);
                            }
                        }

                        if (SUCCEEDED(hr)) {
                            hr = pSink->Close();
                        }

                        if (SUCCEEDED(hr)) {
                            D2D1_STROKE_STYLE_PROPERTIES strokeStyleProperties = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_BEVEL);

                            CComPtr<ID2D1StrokeStyle> pStrokeStyle;
                            m_pDirect2dFactory->CreateStrokeStyle(strokeStyleProperties, nullptr, 0, &pStrokeStyle);

                            m_pRenderTarget->DrawGeometry(pPath, m_pStrokeBrush, 0.75f, pStrokeStyle);
                        }
                    }
                }
            }
        }

        hr = m_pRenderTarget->EndDraw();

        if (hr == D2DERR_RECREATE_TARGET)
        {
            hr = S_OK;
            DiscardDeviceResources();
        }
    }

    return hr;
}

void oscilloscope_ui_element_instance::OnContextMenu(CWindow wnd, CPoint point) {
    if (m_callback->is_edit_mode_enabled()) {
        SetMsgHandled(FALSE);
    } else {
        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING | (m_config.m_hw_rendering_enabled ? MF_CHECKED : 0), 1, TEXT("Allow hardware rendering"));
        menu.AppendMenu(MF_STRING | (m_config.m_downmix_enabled ? MF_CHECKED : 0), 2, TEXT("Downmix channels"));

        CMenu durationMenu;
        durationMenu.CreatePopupMenu();
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 100) ? MF_CHECKED : 0), 3, TEXT("100 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 200) ? MF_CHECKED : 0), 4, TEXT("200 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 300) ? MF_CHECKED : 0), 5, TEXT("300 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 400) ? MF_CHECKED : 0), 6, TEXT("400 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 500) ? MF_CHECKED : 0), 7, TEXT("500 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 600) ? MF_CHECKED : 0), 8, TEXT("600 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 700) ? MF_CHECKED : 0), 9, TEXT("700 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 800) ? MF_CHECKED : 0), 10, TEXT("800 ms"));

        menu.AppendMenu(MF_STRING, durationMenu, TEXT("Window duration"));

        CMenu zoomMenu;
        zoomMenu.CreatePopupMenu();
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 50) ? MF_CHECKED : 0), 11, TEXT("50 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 75) ? MF_CHECKED : 0), 12, TEXT("75 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 100) ? MF_CHECKED : 0), 13, TEXT("100 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 150) ? MF_CHECKED : 0), 14, TEXT("150 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 200) ? MF_CHECKED : 0), 15, TEXT("200 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 300) ? MF_CHECKED : 0), 16, TEXT("300 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 400) ? MF_CHECKED : 0), 17, TEXT("400 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 600) ? MF_CHECKED : 0), 18, TEXT("600 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 800) ? MF_CHECKED : 0), 19, TEXT("800 %"));

        menu.AppendMenu(MF_STRING, zoomMenu, TEXT("Zoom"));

        int cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, point.x, point.y, *this);

        switch (cmd) {
        case 1:
            m_config.m_hw_rendering_enabled = !m_config.m_hw_rendering_enabled;
            DiscardDeviceResources();
            break;
        case 2:
            m_config.m_downmix_enabled = !m_config.m_downmix_enabled;
            if (m_vis_stream.is_valid()) {
                m_vis_stream->set_channel_mode(m_config.m_downmix_enabled ? visualisation_stream_v2::channel_mode_mono : visualisation_stream_v2::channel_mode_default);
            }
            break;
        case 3:
            m_config.m_window_duration_millis = 100;
            break;
        case 4:
            m_config.m_window_duration_millis = 200;
            break;
        case 5:
            m_config.m_window_duration_millis = 300;
            break;
        case 6:
            m_config.m_window_duration_millis = 400;
            break;
        case 7:
            m_config.m_window_duration_millis = 500;
            break;
        case 8:
            m_config.m_window_duration_millis = 600;
            break;
        case 9:
            m_config.m_window_duration_millis = 700;
            break;
        case 10:
            m_config.m_window_duration_millis = 800;
            break;
        case 11:
            m_config.m_zoom_percent = 50;
            break;
        case 12:
            m_config.m_zoom_percent = 75;
            break;
        case 13:
            m_config.m_zoom_percent = 100;
            break;
        case 14:
            m_config.m_zoom_percent = 150;
            break;
        case 15:
            m_config.m_zoom_percent = 200;
            break;
        case 16:
            m_config.m_zoom_percent = 300;
            break;
        case 17:
            m_config.m_zoom_percent = 400;
            break;
        case 18:
            m_config.m_zoom_percent = 600;
            break;
        case 19:
            m_config.m_zoom_percent = 800;
            break;
        }

        Invalidate();
    }
}

HRESULT oscilloscope_ui_element_instance::CreateDeviceIndependentResources() {
    HRESULT hr = S_OK;

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pDirect2dFactory);

    return hr;
}

HRESULT oscilloscope_ui_element_instance::CreateDeviceResources() {
    HRESULT hr = S_OK;

    if (m_pDirect2dFactory) {
        if (!m_pRenderTarget) {
            CRect rcClient;
            GetClientRect(rcClient);

            D2D1_SIZE_U size = D2D1::SizeU(rcClient.Width(), rcClient.Height());

            D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties(m_config.m_hw_rendering_enabled ? D2D1_RENDER_TARGET_TYPE_DEFAULT : D2D1_RENDER_TARGET_TYPE_SOFTWARE);

            D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRenderTargetProperties = D2D1::HwndRenderTargetProperties(m_hWnd, size);

            hr = m_pDirect2dFactory->CreateHwndRenderTarget(renderTargetProperties, hwndRenderTargetProperties, &m_pRenderTarget);
        }

        if (SUCCEEDED(hr) && !m_pStrokeBrush) {
            t_ui_color colorText = m_callback->query_std_color(ui_color_text);

            hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(GetRValue(colorText) / 255.0f, GetGValue(colorText) / 255.0f, GetBValue(colorText) / 255.0f), &m_pStrokeBrush);
        }
    } else {
        hr = S_FALSE;
    }

    return hr;
}

void oscilloscope_ui_element_instance::DiscardDeviceResources() {
    m_pRenderTarget.Release();
    m_pStrokeBrush.Release();
}

static service_factory_single_t< ui_element_impl_visualisation< oscilloscope_ui_element_instance> > g_ui_element_factory;
