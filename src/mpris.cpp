#include "mpris.hpp"
#include <dbus/dbus.h>
#include <iostream>
#include <thread>
#include <chrono>

void start_mpris_server(
    std::function<void()> on_playpause,
    std::function<void()> on_next,
    std::function<void()> on_prev
) {
    std::thread([=]() {
        DBusError err;
        dbus_error_init(&err);
        DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
            // std::cerr << "DBus Error: " << err.message << "\n";
            dbus_error_free(&err);
        }
        if (!conn) return;

        dbus_bus_request_name(conn, "org.mpris.MediaPlayer2.vibe_fi", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
        if (dbus_error_is_set(&err)) {
            dbus_error_free(&err);
        }

        while (true) {
            dbus_connection_read_write(conn, 100);
            DBusMessage* msg = dbus_connection_pop_message(conn);
            if (msg) {
                if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "PlayPause") ||
                    dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Play") ||
                    dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Pause")) {
                    if (on_playpause) on_playpause();
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Next")) {
                    if (on_next) on_next();
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Previous")) {
                    if (on_prev) on_prev();
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Stop")) {
                    if (on_playpause) on_playpause();
                }
                
                // Reply is needed to avoid caller timeout
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
    }).detach();
}
