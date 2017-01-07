#include "stdafx.h"

#include "oscilloscope_config.h"

t_uint32 oscilloscope_config::g_get_version() {
    return 5;
}

oscilloscope_config::oscilloscope_config() {
    reset();
}

void oscilloscope_config::reset() {
    m_hw_rendering_enabled = true;
    m_downmix_enabled = false;
    m_trigger_enabled = false;
    m_resample_enabled = false;
    m_low_quality_enabled = false;
    m_window_duration_millis = 100;
    m_zoom_percent = 100;
    m_refresh_rate_limit_hz = 20;
}

void oscilloscope_config::parse(ui_element_config_parser & parser) {
    reset();

    try {
        t_uint32 version;
        parser >> version;
        switch (version) {
        case 5:
            parser >> m_low_quality_enabled;
            // fall through
        case 4:
            parser >> m_resample_enabled;
            // fall through
        case 3:
            parser >> m_refresh_rate_limit_hz;
            m_refresh_rate_limit_hz = pfc::clip_t<t_uint32>(m_refresh_rate_limit_hz, 20, 200);
            // fall through
        case 2:
            parser >> m_trigger_enabled;
            // fall through
        case 1:
            parser >> m_hw_rendering_enabled;
            parser >> m_downmix_enabled;
            parser >> m_window_duration_millis;
            m_window_duration_millis = pfc::clip_t<t_uint32>(m_window_duration_millis, 50, 800);
            parser >> m_zoom_percent;
            m_zoom_percent = pfc::clip_t<t_uint32>(m_zoom_percent, 50, 800);
            break;
        default:
            console::formatter() << core_api::get_my_file_name() << ": unknown configuration format version: " << version;
        }
    } catch (exception_io &exc) {
        console::formatter() << core_api::get_my_file_name() << ": exception while reading configuration data: " << exc;
    }
}

void oscilloscope_config::build(ui_element_config_builder & builder) {
    builder << g_get_version();
    builder << m_low_quality_enabled;
    builder << m_resample_enabled;
    builder << m_refresh_rate_limit_hz;
    builder << m_trigger_enabled;
    builder << m_hw_rendering_enabled;
    builder << m_downmix_enabled;
    builder << m_window_duration_millis;
    builder << m_zoom_percent;
}
