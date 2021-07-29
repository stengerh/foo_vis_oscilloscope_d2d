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
    , m_refresh_interval(10)
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

    UpdateChannelMode();
    UpdateRefreshRateLimit();
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
        UpdateChannelMode();
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
        m_pRenderTarget->SetAntialiasMode(m_config.m_low_quality_enabled ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

        t_ui_color colorBackground = m_callback->query_std_color(ui_color_background);

        m_pRenderTarget->Clear(D2D1::ColorF(GetRValue(colorBackground) / 255.0f, GetGValue(colorBackground) / 255.0f, GetBValue(colorBackground) / 255.0f));

        if (m_vis_stream.is_valid()) {
            double time;
            if (m_vis_stream->get_absolute_time(time)) {
                double window_duration = m_config.get_window_duration();
                audio_chunk_impl chunk;
                if (m_vis_stream->get_chunk_absolute(chunk, time - window_duration / 2, window_duration * (m_config.m_trigger_enabled ? 2 : 1))) {
                    RenderChunk(chunk);
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

HRESULT oscilloscope_ui_element_instance::RenderChunk(const audio_chunk &chunk) {
    HRESULT hr = S_OK;

    D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();

    CComPtr<ID2D1PathGeometry> pPath;

    hr = m_pDirect2dFactory->CreatePathGeometry(&pPath);

    if (SUCCEEDED(hr)) {
        CComPtr<ID2D1GeometrySink> pSink;
            
        hr = pPath->Open(&pSink);

        audio_chunk_impl chunk2;
        chunk2.copy(chunk);

        if (m_config.m_resample_enabled) {
            unsigned display_sample_rate = (unsigned) (rtSize.width / m_config.get_window_duration());
            unsigned target_sample_rate = chunk.get_sample_rate();
            while (target_sample_rate >= 2 && target_sample_rate > display_sample_rate) {
                target_sample_rate /= 2;
            }
            if (target_sample_rate != chunk.get_sample_rate()) {
                dsp::ptr resampler;
                metadb_handle::ptr track;
                if (static_api_ptr_t<playback_control>()->get_now_playing(track) && resampler_entry::g_create(resampler, chunk.get_sample_rate(), target_sample_rate, 1.0f)) {
                    dsp_chunk_list_impl chunk_list;
                    chunk_list.add_chunk(&chunk);
                    resampler->run(&chunk_list, track, dsp::FLUSH);
                    resampler->flush();

                    bool consistent_format = true;
                    unsigned total_sample_count = 0;
                    for (t_size chunk_index = 0; chunk_index < chunk_list.get_count(); ++chunk_index) {
                        if ((chunk_list.get_item(chunk_index)->get_sample_rate() == chunk_list.get_item(0)->get_sample_rate())
                            && (chunk_list.get_item(chunk_index)->get_channel_count() == chunk_list.get_item(0)->get_channel_count())) {
                                total_sample_count += chunk_list.get_item(chunk_index)->get_sample_count();
                        } else {
                            consistent_format = false;
                            break;
                        }
                    }
                    if (consistent_format && chunk_list.get_count() > 0) {
                        unsigned channel_count = chunk_list.get_item(0)->get_channels();
                        unsigned sample_rate = chunk_list.get_item(0)->get_sample_rate();

                        pfc::array_t<audio_sample> buffer;
                        buffer.prealloc(channel_count * total_sample_count);
                        for (t_size chunk_index = 0; chunk_index < chunk_list.get_count(); ++chunk_index) {
                            audio_chunk * c = chunk_list.get_item(chunk_index);
                            buffer.append_fromptr(c->get_data(), c->get_channel_count() * c->get_sample_count());
                        }

                        chunk2.set_data(buffer.get_ptr(), total_sample_count, channel_count, sample_rate);
                    }
                }
            }
        }

        t_uint32 channel_count = chunk2.get_channel_count();
        t_uint32 sample_count_total = chunk2.get_sample_count();
        t_uint32 sample_count = m_config.m_trigger_enabled ? sample_count_total / 2 : sample_count_total;
        const audio_sample *samples = chunk2.get_data();

        if (m_config.m_trigger_enabled) {
            t_uint32 cross_min = sample_count;
            t_uint32 cross_max = 0;

            for (t_uint32 channel_index = 0; channel_index < channel_count; ++channel_index) {
                audio_sample sample0 = samples[channel_index];
                audio_sample sample1 = samples[1 * channel_count + channel_index];
                audio_sample sample2;
                for (t_uint32 sample_index = 2; sample_index < sample_count; ++sample_index) {
                    sample2 = samples[sample_index * channel_count + channel_index];
                    if ((sample0 < 0.0) && (sample1 >= 0.0) && (sample2 >= 0.0)) {
                        if (cross_min > sample_index - 1)
                            cross_min = sample_index - 1;
                        if (cross_max < sample_index - 1)
                            cross_max = sample_index - 1;
                    }
                    sample0 = sample1;
                    sample1 = sample2;
                }
            }

            samples += cross_min * channel_count;
        }

        for (t_uint32 channel_index = 0; channel_index < channel_count; ++channel_index) {
            float zoom = (float) m_config.get_zoom_factor();
            float channel_baseline = (float) (channel_index + 0.5) / (float) channel_count * rtSize.height;
            for (t_uint32 sample_index = 0; sample_index < sample_count; ++sample_index) {
                audio_sample sample = samples[sample_index * channel_count + channel_index];
                float x = (float) sample_index / (float) (sample_count - 1) * rtSize.width;
                float y = channel_baseline - sample * zoom * rtSize.height / 2 / channel_count + 0.5f;
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

            m_pRenderTarget->DrawGeometry(pPath, m_pStrokeBrush, (FLOAT)m_config.get_line_stroke_width(), pStrokeStyle);
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
        menu.AppendMenu(MF_STRING, IDM_TOGGLE_FULLSCREEN, TEXT("Toggle Full-Screen Mode"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING | (m_config.m_downmix_enabled ? MF_CHECKED : 0), IDM_DOWNMIX_ENABLED, TEXT("Downmix Channels"));
        menu.AppendMenu(MF_STRING | (m_config.m_low_quality_enabled ? MF_CHECKED : 0), IDM_LOW_QUALITY_ENABLED, TEXT("Low Quality Mode"));
        menu.AppendMenu(MF_STRING | (m_config.m_trigger_enabled ? MF_CHECKED : 0), IDM_TRIGGER_ENABLED, TEXT("Trigger on Zero Crossing"));

        CMenu durationMenu;
        durationMenu.CreatePopupMenu();
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 5) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_5, TEXT("5 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 10) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_10, TEXT("10 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 20) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_20, TEXT("20 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 50) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_50, TEXT("50 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 100) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_100, TEXT("100 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 200) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_200, TEXT("200 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 300) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_300, TEXT("300 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 400) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_400, TEXT("400 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 500) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_500, TEXT("500 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 600) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_600, TEXT("600 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 700) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_700, TEXT("700 ms"));
        durationMenu.AppendMenu(MF_STRING | ((m_config.m_window_duration_millis == 800) ? MF_CHECKED : 0), IDM_WINDOW_DURATION_800, TEXT("800 ms"));

        menu.AppendMenu(MF_STRING, durationMenu, TEXT("Window Duration"));

        CMenu zoomMenu;
        zoomMenu.CreatePopupMenu();
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 50) ? MF_CHECKED : 0), IDM_ZOOM_50, TEXT("50 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 75) ? MF_CHECKED : 0), IDM_ZOOM_75, TEXT("75 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 100) ? MF_CHECKED : 0), IDM_ZOOM_100, TEXT("100 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 150) ? MF_CHECKED : 0), IDM_ZOOM_150, TEXT("150 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 200) ? MF_CHECKED : 0), IDM_ZOOM_200, TEXT("200 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 300) ? MF_CHECKED : 0), IDM_ZOOM_300, TEXT("300 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 400) ? MF_CHECKED : 0), IDM_ZOOM_400, TEXT("400 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 600) ? MF_CHECKED : 0), IDM_ZOOM_600, TEXT("600 %"));
        zoomMenu.AppendMenu(MF_STRING | ((m_config.m_zoom_percent == 800) ? MF_CHECKED : 0), IDM_ZOOM_800, TEXT("800 %"));

        menu.AppendMenu(MF_STRING, zoomMenu, TEXT("Zoom"));

        CMenu refreshRateLimitMenu;
        refreshRateLimitMenu.CreatePopupMenu();
        refreshRateLimitMenu.AppendMenu(MF_STRING | ((m_config.m_refresh_rate_limit_hz == 20) ? MF_CHECKED : 0), IDM_REFRESH_RATE_LIMIT_20, TEXT("20 Hz"));
        refreshRateLimitMenu.AppendMenu(MF_STRING | ((m_config.m_refresh_rate_limit_hz == 60) ? MF_CHECKED : 0), IDM_REFRESH_RATE_LIMIT_60, TEXT("60 Hz"));
        refreshRateLimitMenu.AppendMenu(MF_STRING | ((m_config.m_refresh_rate_limit_hz == 100) ? MF_CHECKED : 0), IDM_REFRESH_RATE_LIMIT_100, TEXT("100 Hz"));
        refreshRateLimitMenu.AppendMenu(MF_STRING | ((m_config.m_refresh_rate_limit_hz == 200) ? MF_CHECKED : 0), IDM_REFRESH_RATE_LIMIT_200, TEXT("200 Hz"));

        menu.AppendMenu(MF_STRING, refreshRateLimitMenu, TEXT("Refresh Rate Limit"));

        CMenu lineStrokeWidthMenu;
        lineStrokeWidthMenu.CreatePopupMenu();
        lineStrokeWidthMenu.AppendMenu(MF_STRING | ((m_config.m_line_stroke_width == 5) ? MF_CHECKED : 0), IDM_LINE_STROKE_WIDTH_5, TEXT("0.5 px"));
        lineStrokeWidthMenu.AppendMenu(MF_STRING | ((m_config.m_line_stroke_width == 10) ? MF_CHECKED : 0), IDM_LINE_STROKE_WIDTH_10, TEXT("1.0 px"));
        lineStrokeWidthMenu.AppendMenu(MF_STRING | ((m_config.m_line_stroke_width == 15) ? MF_CHECKED : 0), IDM_LINE_STROKE_WIDTH_15, TEXT("1.5 px"));
        lineStrokeWidthMenu.AppendMenu(MF_STRING | ((m_config.m_line_stroke_width == 20) ? MF_CHECKED : 0), IDM_LINE_STROKE_WIDTH_20, TEXT("2.0 px"));
        lineStrokeWidthMenu.AppendMenu(MF_STRING | ((m_config.m_line_stroke_width == 25) ? MF_CHECKED : 0), IDM_LINE_STROKE_WIDTH_25, TEXT("2.5 px"));
        lineStrokeWidthMenu.AppendMenu(MF_STRING | ((m_config.m_line_stroke_width == 30) ? MF_CHECKED : 0), IDM_LINE_STROKE_WIDTH_30, TEXT("3.0 px"));

        menu.AppendMenu(MF_STRING, lineStrokeWidthMenu, TEXT("Line Stroke Width"));

        menu.AppendMenu(MF_SEPARATOR);

        menu.AppendMenu(MF_STRING | (m_config.m_resample_enabled ? MF_CHECKED : 0), IDM_RESAMPLE_ENABLED, TEXT("Resample For Display"));
        menu.AppendMenu(MF_STRING | (m_config.m_hw_rendering_enabled ? MF_CHECKED : 0), IDM_HW_RENDERING_ENABLED, TEXT("Allow Hardware Rendering"));

        menu.SetMenuDefaultItem(IDM_TOGGLE_FULLSCREEN);

        int cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, point.x, point.y, *this);

        switch (cmd) {
        case IDM_TOGGLE_FULLSCREEN:
            ToggleFullScreen();
            break;
        case IDM_HW_RENDERING_ENABLED:
            m_config.m_hw_rendering_enabled = !m_config.m_hw_rendering_enabled;
            DiscardDeviceResources();
            break;
        case IDM_DOWNMIX_ENABLED:
            m_config.m_downmix_enabled = !m_config.m_downmix_enabled;
            UpdateChannelMode();
            break;
        case IDM_LOW_QUALITY_ENABLED:
            m_config.m_low_quality_enabled = !m_config.m_low_quality_enabled;
            break;
        case IDM_TRIGGER_ENABLED:
            m_config.m_trigger_enabled = !m_config.m_trigger_enabled;
            break;
        case IDM_RESAMPLE_ENABLED:
            m_config.m_resample_enabled = !m_config.m_resample_enabled;
            break;
        case IDM_WINDOW_DURATION_50:
            m_config.m_window_duration_millis = 50;
            break;
        case IDM_WINDOW_DURATION_100:
            m_config.m_window_duration_millis = 100;
            break;
        case IDM_WINDOW_DURATION_200:
            m_config.m_window_duration_millis = 200;
            break;
        case IDM_WINDOW_DURATION_300:
            m_config.m_window_duration_millis = 300;
            break;
        case IDM_WINDOW_DURATION_400:
            m_config.m_window_duration_millis = 400;
            break;
        case IDM_WINDOW_DURATION_500:
            m_config.m_window_duration_millis = 500;
            break;
        case IDM_WINDOW_DURATION_600:
            m_config.m_window_duration_millis = 600;
            break;
        case IDM_WINDOW_DURATION_700:
            m_config.m_window_duration_millis = 700;
            break;
        case IDM_WINDOW_DURATION_800:
            m_config.m_window_duration_millis = 800;
            break;
        case IDM_ZOOM_50:
            m_config.m_zoom_percent = 50;
            break;
        case IDM_ZOOM_75:
            m_config.m_zoom_percent = 75;
            break;
        case IDM_ZOOM_100:
            m_config.m_zoom_percent = 100;
            break;
        case IDM_ZOOM_150:
            m_config.m_zoom_percent = 150;
            break;
        case IDM_ZOOM_200:
            m_config.m_zoom_percent = 200;
            break;
        case IDM_ZOOM_300:
            m_config.m_zoom_percent = 300;
            break;
        case IDM_ZOOM_400:
            m_config.m_zoom_percent = 400;
            break;
        case IDM_ZOOM_600:
            m_config.m_zoom_percent = 600;
            break;
        case IDM_ZOOM_800:
            m_config.m_zoom_percent = 800;
            break;
        case IDM_REFRESH_RATE_LIMIT_20:
            m_config.m_refresh_rate_limit_hz = 20;
            UpdateRefreshRateLimit();
            break;
        case IDM_REFRESH_RATE_LIMIT_60:
            m_config.m_refresh_rate_limit_hz = 60;
            UpdateRefreshRateLimit();
            break;
        case IDM_REFRESH_RATE_LIMIT_100:
            m_config.m_refresh_rate_limit_hz = 100;
            UpdateRefreshRateLimit();
            break;
        case IDM_REFRESH_RATE_LIMIT_200:
            m_config.m_refresh_rate_limit_hz = 200;
            UpdateRefreshRateLimit();
            break;
        case IDM_LINE_STROKE_WIDTH_5:
            m_config.m_line_stroke_width = 5;
            break;
        case IDM_LINE_STROKE_WIDTH_10:
            m_config.m_line_stroke_width = 10;
            break;
        case IDM_LINE_STROKE_WIDTH_15:
            m_config.m_line_stroke_width = 15;
            break;
        case IDM_LINE_STROKE_WIDTH_20:
            m_config.m_line_stroke_width = 20;
            break;
        case IDM_LINE_STROKE_WIDTH_25:
            m_config.m_line_stroke_width = 25;
            break;
        case IDM_LINE_STROKE_WIDTH_30:
            m_config.m_line_stroke_width = 30;
            break;
        }

        Invalidate();
    }
}

void oscilloscope_ui_element_instance::OnLButtonDblClk(UINT nFlags, CPoint point) {
    ToggleFullScreen();
}

void oscilloscope_ui_element_instance::ToggleFullScreen() {
    static_api_ptr_t<ui_element_common_methods_v2>()->toggle_fullscreen(g_get_guid(), core_api::get_main_window());
}

void oscilloscope_ui_element_instance::UpdateChannelMode() {
    if (m_vis_stream.is_valid()) {
        m_vis_stream->set_channel_mode(m_config.m_downmix_enabled ? visualisation_stream_v2::channel_mode_mono : visualisation_stream_v2::channel_mode_default);
    }
}

void oscilloscope_ui_element_instance::UpdateRefreshRateLimit() {
    m_refresh_interval = pfc::clip_t<DWORD>(1000 / m_config.m_refresh_rate_limit_hz, 5, 1000);
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
