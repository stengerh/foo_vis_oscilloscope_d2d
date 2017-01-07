#pragma once

#include "oscilloscope_config.h"

typedef oscilloscope_config_v2 oscilloscope_config;

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
    virtual void notify(const GUID & p_what, t_size p_param1, const void * p_param2, t_size p_param2size);

    static CWndClassInfo& GetWndClassInfo();

    LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnTimer(UINT_PTR nIDEvent);
    void OnPaint(CDCHandle dc);
    void OnSize(UINT nType, CSize size);
    void OnContextMenu(CWindow wnd, CPoint point);
    void OnLButtonDblClk(UINT nFlags, CPoint point);

    void ToggleFullScreen();

    HRESULT Render();
    HRESULT RenderChunk(const audio_chunk &chunk);
    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();

    BEGIN_MSG_MAP_EX(oscilloscope_ui_element_instance)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_TIMER(OnTimer)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_SIZE(OnSize)
        MSG_WM_CONTEXTMENU(OnContextMenu)
        MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
    END_MSG_MAP()

protected:
    ui_element_instance_callback::ptr m_callback;

private:
    enum {
        ID_REFRESH_TIMER = 1
    };

    enum {
        IDM_TOGGLE_FULLSCREEN = 1,
        IDM_HW_RENDERING_ENABLED,
        IDM_DOWNMIX_ENABLED,
        IDM_TRIGGER_ENABLED,
        IDM_WINDOW_DURATION_100,
        IDM_WINDOW_DURATION_200,
        IDM_WINDOW_DURATION_300,
        IDM_WINDOW_DURATION_400,
        IDM_WINDOW_DURATION_500,
        IDM_WINDOW_DURATION_600,
        IDM_WINDOW_DURATION_700,
        IDM_WINDOW_DURATION_800,
        IDM_ZOOM_50,
        IDM_ZOOM_75,
        IDM_ZOOM_100,
        IDM_ZOOM_150,
        IDM_ZOOM_200,
        IDM_ZOOM_300,
        IDM_ZOOM_400,
        IDM_ZOOM_600,
        IDM_ZOOM_800,
    };

    oscilloscope_config m_config;
    DWORD m_last_refresh;
    DWORD m_refresh_interval;

    visualisation_stream_v2::ptr m_vis_stream;

    CComPtr<ID2D1Factory> m_pDirect2dFactory;
    CComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
    CComPtr<ID2D1SolidColorBrush> m_pStrokeBrush;
};
