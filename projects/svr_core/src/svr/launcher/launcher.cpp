#include <svr/launcher.hpp>
#include <svr/os.hpp>

static const auto COMPLETION_EVENT_NAME = "crashfort-svr-completion-event";
static const auto COM_EVENT_NAME = "crashfort-svr-com-event";
static const auto COM_PIPE_NAME = "crashfort-svr-com-pipe";

namespace svr
{
    os_handle* launcher_create_completion_event()
    {
        return os_create_event(COMPLETION_EVENT_NAME);
    }

    void launcher_signal_completion_event()
    {
        auto event = os_open_event(COMPLETION_EVENT_NAME);
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
