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
}

ui_element_config::ptr oscilloscope_ui_element_instance::get_configuration() {
    return g_get_default_configuration();
}

static service_factory_single_t< ui_element_impl_visualisation< oscilloscope_ui_element_instance> > g_ui_element_factory;
