#include "mpris.hpp"
#include <mutex>
#include <queue>
#include <atomic>
#include <thread>
#include <chrono>

#ifdef ENABLE_MPRIS
#include <dbus/dbus.h>

namespace {
    std::queue<MprisAction> g_action_queue;
    std::mutex g_action_mutex;
    std::atomic<bool> g_mpris_running{false};
    std::thread g_mpris_thread;

    void push_action(MprisAction action) {
        std::lock_guard<std::mutex> lock(g_action_mutex);
        g_action_queue.push(action);
    }
}

void start_mpris_server() {
    if (g_mpris_running.load()) return;
    g_mpris_running = true;

    g_mpris_thread = std::thread([]() {
        DBusError err;
        dbus_error_init(&err);
        DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
            dbus_error_free(&err);
        }
        if (!conn) {
            g_mpris_running = false;
            return;
        }

        dbus_bus_request_name(conn, "org.mpris.MediaPlayer2.vibe_fi", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
        if (dbus_error_is_set(&err)) {
            dbus_error_free(&err);
        }

        while (g_mpris_running.load()) {
            dbus_connection_read_write(conn, 50);
            DBusMessage* msg = dbus_connection_pop_message(conn);
            if (msg) {
                if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "PlayPause")) {
                    push_action(MprisAction::PLAY_PAUSE);
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Play")) {
                    push_action(MprisAction::PLAY_PAUSE);
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Pause")) {
                    push_action(MprisAction::PLAY_PAUSE);
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Next")) {
                    push_action(MprisAction::NEXT);
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Previous")) {
                    push_action(MprisAction::PREVIOUS);
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Stop")) {
                    push_action(MprisAction::STOP);
                }

                // Send reply if method call to prevent client timeout
                if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL) {
                    DBusMessage* reply = dbus_message_new_method_return(msg);
                    if (reply) {
                        dbus_connection_send(conn, reply, nullptr);
                        dbus_message_unref(reply);
                    }
                }
                dbus_message_unref(msg);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        dbus_connection_unref(conn);
    });
}

void stop_mpris_server() {
    if (g_mpris_running.load()) {
        g_mpris_running = false;
        if (g_mpris_thread.joinable()) {
            g_mpris_thread.join();
        }
    }
}

bool poll_mpris_action(MprisAction& action) {
    std::lock_guard<std::mutex> lock(g_action_mutex);
    if (!g_action_queue.empty()) {
        action = g_action_queue.front();
        g_action_queue.pop();
        return true;
    }
    return false;
}

#else

// Stubs when MPRIS is not compiled or supported
void start_mpris_server() {}
void stop_mpris_server() {}
bool poll_mpris_action(MprisAction& action) {
    (void)action;
    return false;
}

#endif
