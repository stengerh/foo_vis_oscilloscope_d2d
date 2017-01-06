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
{
    set_configuration(p_data);
}

void oscilloscope_ui_element_instance::initialize_window(HWND p_parent) {
    this->Create(p_parent);
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
            break;
        case 2:
            m_config.m_downmix_enabled = !m_config.m_downmix_enabled;
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
    }
}

static service_factory_single_t< ui_element_impl_visualisation< oscilloscope_ui_element_instance> > g_ui_element_factory;
