#pragma once

#include "XDataSource.hpp"
#include "Dnd.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"

#include <xcb/xcb.h>
#include <xcb/res.h>
#include <xcb/xfixes.h>
#include <xcb/composite.h>
#include <xcb/xcb_errors.h>
#include <hyprutils/os/FileDescriptor.hpp>
#include <cinttypes> // for PRIxPTR
#include <cstdint>

struct wl_event_source;
class CXWaylandSurfaceResource;
struct SXSelection;

struct SXTransfer {
    ~SXTransfer();

    SXSelection&                   selection;
    bool                           out = true;

    bool                           incremental   = false;
    bool                           flushOnDelete = false;
    bool                           propertySet   = false;

    Hyprutils::OS::CFileDescriptor wlFD;
    wl_event_source*               eventSource = nullptr;

    std::vector<uint8_t>           data;

    xcb_selection_request_event_t  request;

    int                            propertyStart;
    xcb_get_property_reply_t*      propertyReply;
    xcb_window_t                   incomingWindow;

    bool                           getIncomingSelectionProp(bool erase);
};

struct SXSelection {
    xcb_window_t     window    = 0;
    xcb_window_t     owner     = 0;
    xcb_timestamp_t  timestamp = 0;
    SP<CXDataSource> dataSource;
    bool             notifyOnFocus = false;

    void             onSelection();
    void             onKeyboardFocus();
    bool             sendData(xcb_selection_request_event_t* e, std::string mime);
    int              onRead(int fd, uint32_t mask);
    int              onWrite();

    struct {
        CHyprSignalListener setSelection;
        CHyprSignalListener keyboardFocusChange;
    } listeners;

    std::vector<UP<SXTransfer>> transfers;
};

class CXCBConnection {
  public:
    CXCBConnection(int fd) : m_connection{xcb_connect_to_fd(fd, nullptr)} {
        ;
    }

    ~CXCBConnection() {
        if (m_connection) {
            Debug::log(LOG, "Disconnecting XCB connection {:x}", (uintptr_t)m_connection);
            xcb_disconnect(m_connection);
            m_connection = nullptr;
        } else
            Debug::log(ERR, "Double xcb_disconnect attempt");
    }

    bool hasError() const {
        return xcb_connection_has_error(m_connection);
    }

    operator xcb_connection_t*() const {
        return m_connection;
    }

  private:
    xcb_connection_t* m_connection = nullptr;
};

class CXCBErrorContext {
  public:
    explicit CXCBErrorContext(xcb_connection_t* connection) {
        if (xcb_errors_context_new(connection, &m_errors) != 0)
            m_errors = nullptr;
    }

    ~CXCBErrorContext() {
        if (m_errors)
            xcb_errors_context_free(m_errors);
    }

    bool isValid() const {
        return m_errors != nullptr;
    }

  private:
    xcb_errors_context_t* m_errors = nullptr;
};

class CXWM {
  public:
    CXWM();
    ~CXWM();

    int                onEvent(int fd, uint32_t mask);
    SP<CX11DataDevice> getDataDevice();
    SP<IDataOffer>     createX11DataOffer(SP<CWLSurfaceResource> surf, SP<IDataSource> source);

  private:
    void                 setCursor(unsigned char* pixData, uint32_t stride, const Vector2D& size, const Vector2D& hotspot);

    void                 gatherResources();
    void                 getVisual();
    void                 getRenderFormat();
    void                 createWMWindow();
    void                 initSelection();

    void                 onNewSurface(SP<CWLSurfaceResource> surf);
    void                 onNewResource(SP<CXWaylandSurfaceResource> resource);

    void                 setActiveWindow(xcb_window_t window);
    void                 sendState(SP<CXWaylandSurface> surf);
    void                 focusWindow(SP<CXWaylandSurface> surf);
    void                 activateSurface(SP<CXWaylandSurface> surf, bool activate);
    bool                 isWMWindow(xcb_window_t w);
    void                 updateOverrideRedirect(SP<CXWaylandSurface> surf, bool overrideRedirect);

    void                 sendWMMessage(SP<CXWaylandSurface> surf, xcb_client_message_data_t* data, uint32_t mask);

    SP<CXWaylandSurface> windowForXID(xcb_window_t wid);
    SP<CXWaylandSurface> windowForWayland(SP<CWLSurfaceResource> surf);

    void                 readWindowData(SP<CXWaylandSurface> surf);
    void                 associate(SP<CXWaylandSurface> surf, SP<CWLSurfaceResource> wlSurf);
    void                 dissociate(SP<CXWaylandSurface> surf);

    void                 updateClientList();

    // event handlers
    void         handleCreate(xcb_create_notify_event_t* e);
    void         handleDestroy(xcb_destroy_notify_event_t* e);
    void         handleConfigureRequest(xcb_configure_request_event_t* e);
    void         handleConfigureNotify(xcb_configure_notify_event_t* e);
    void         handleMapRequest(xcb_map_request_event_t* e);
    void         handleMapNotify(xcb_map_notify_event_t* e);
    void         handleUnmapNotify(xcb_unmap_notify_event_t* e);
    void         handlePropertyNotify(xcb_property_notify_event_t* e);
    void         handleClientMessage(xcb_client_message_event_t* e);
    void         handleFocusIn(xcb_focus_in_event_t* e);
    void         handleFocusOut(xcb_focus_out_event_t* e);
    void         handleError(xcb_value_error_t* e);

    bool         handleSelectionEvent(xcb_generic_event_t* e);
    void         handleSelectionNotify(xcb_selection_notify_event_t* e);
    bool         handleSelectionPropertyNotify(xcb_property_notify_event_t* e);
    void         handleSelectionRequest(xcb_selection_request_event_t* e);
    bool         handleSelectionXFixesNotify(xcb_xfixes_selection_notify_event_t* e);

    void         selectionSendNotify(xcb_selection_request_event_t* e, bool success);
    xcb_atom_t   mimeToAtom(const std::string& mime);
    std::string  mimeFromAtom(xcb_atom_t atom);
    void         setClipboardToWayland(SXSelection& sel);
    void         getTransferData(SXSelection& sel);
    std::string  getAtomName(uint32_t atom);
    void         readProp(SP<CXWaylandSurface> XSURF, uint32_t atom, xcb_get_property_reply_t* reply);

    SXSelection* getSelection(xcb_atom_t atom);

    //
    UP<CXCBConnection>                        m_connection;
    xcb_errors_context_t*                     m_errors = nullptr;
    xcb_screen_t*                             m_screen = nullptr;

    xcb_window_t                              m_wmWindow;

    wl_event_source*                          m_eventSource = nullptr;

    const xcb_query_extension_reply_t*        m_xfixes      = nullptr;
    const xcb_query_extension_reply_t*        m_xres        = nullptr;
    int                                       m_xfixesMajor = 0;

    xcb_visualid_t                            m_visualID;
    xcb_colormap_t                            m_colormap;
    uint32_t                                  m_cursorXID = 0;

    xcb_render_pictformat_t                   m_renderFormatID;

    std::vector<WP<CXWaylandSurfaceResource>> m_shellResources;
    std::vector<SP<CXWaylandSurface>>         m_surfaces;
    std::vector<WP<CXWaylandSurface>>         m_mappedSurfaces;         // ordered by map time
    std::vector<WP<CXWaylandSurface>>         m_mappedSurfacesStacking; // ordered by stacking

    WP<CXWaylandSurface>                      m_focusedSurface;
    uint64_t                                  m_lastFocusSeq = 0;

    SXSelection                               m_clipboard;
    SXSelection                               m_primarySelection;
    SXSelection                               m_dndSelection;
    SP<CX11DataDevice>                        m_dndDataDevice = makeShared<CX11DataDevice>();
    std::vector<SP<CX11DataOffer>>            m_dndDataOffers;

    inline xcb_connection_t*                  getConnection() {
        return m_connection ? (xcb_connection_t*)*m_connection : nullptr;
    }
    struct {
        CHyprSignalListener newWLSurface;
        CHyprSignalListener newXShellSurface;
    } m_listeners;

    friend class CXWaylandSurface;
    friend class CXWayland;
    friend class CXDataSource;
    friend class CX11DataDevice;
    friend class CX11DataSource;
    friend class CX11DataOffer;
    friend class CWLDataDeviceProtocol;
    friend struct SXSelection;
    friend struct SXTransfer;
};
