#include <svr/launcher.hpp>
#include <svr/os.hpp>

static const auto SUCCESS_EVENT_NAME = "crashfort-svr-success-event";
static const auto FAIL_EVENT_NAME = "crashfort-svr-fail-event";
static const auto COM_EVENT_NAME = "crashfort-svr-com-event";
static const auto COM_PIPE_NAME = "crashfort-svr-com-pipe";

namespace svr
{
    os_handle* launcher_create_success_event()
    {
        return os_create_event(SUCCESS_EVENT_NAME);
    }

    os_handle* launcher_create_fail_event()
    {
        return os_create_event(FAIL_EVENT_NAME);
    }

    void launcher_signal_success_event()
    {
        auto event = os_open_event(SUCCESS_EVENT_NAME);
        os_set_event(event);
        os_close_handle(event);
    }

    void launcher_signal_fail_event()
    {
        auto event = os_open_event(FAIL_EVENT_NAME);
        os_set_event(event);
        os_close_handle(event);
    }

    os_handle* launcher_create_com_pipe()
    {
        return os_create_pipe_read(COM_PIPE_NAME);
    }

    os_handle* launcher_open_com_pipe()
    {
        return os_open_pipe_write(COM_PIPE_NAME);
    }

    os_handle* launcher_create_com_link_event()
    {
        return os_create_event(COM_EVENT_NAME);
    }

    void launcher_signal_com_link()
    {
        auto event = os_open_event(COM_EVENT_NAME);
        os_set_event(event);
        os_close_handle(event);
    }
}
