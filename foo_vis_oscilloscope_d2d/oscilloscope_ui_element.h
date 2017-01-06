#pragma once

class oscilloscope_ui_element_instance : public ui_element_instance, public CWindowImpl<oscilloscope_ui_element_instance> {
public:
    static void g_get_name(pfc::string_base & p_out);
    static const char * g_get_description();
    static GUID g_get_guid();
    static GUID g_get_subclass();
    static ui_element_config::ptr g_get_default_configuration();

    oscilloscope_ui_element_instance(ui_element_config::ptr p_data, ui_element_instance_callback::ptr p_callback);

    void initialize_window(HWND p_parent);
	virtual void set_configuration(ui_element_config::ptr p_data);
	virtual ui_element_config::ptr get_configuration();

    BEGIN_MSG_MAP_EX(oscilloscope_ui_element_instance)
    END_MSG_MAP()

protected:
    ui_element_instance_callback::ptr m_callback;
};
