#include "mpris.hpp"
#include <mutex>
#include <queue>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#ifdef ENABLE_MPRIS
#include <dbus/dbus.h>

namespace {
    std::queue<MprisAction> g_action_queue;
    std::mutex g_action_mutex;
    std::atomic<bool> g_mpris_running{false};
    std::thread g_mpris_thread;

    MprisMetadata g_current_meta;
    std::mutex g_meta_mutex;

    void push_action(MprisAction action) {
        std::lock_guard<std::mutex> lock(g_action_mutex);
        g_action_queue.push(action);
    }

    void append_dict_variant_string(DBusMessageIter* dict, const char* key, const char* val) {
        DBusMessageIter entry, var;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &val);
        dbus_message_iter_close_container(&entry, &var);
        dbus_message_iter_close_container(dict, &entry);
    }

    void append_dict_variant_int64(DBusMessageIter* dict, const char* key, dbus_int64_t val) {
        DBusMessageIter entry, var;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "x", &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_INT64, &val);
        dbus_message_iter_close_container(&entry, &var);
        dbus_message_iter_close_container(dict, &entry);
    }

    void append_dict_variant_double(DBusMessageIter* dict, const char* key, double val) {
        DBusMessageIter entry, var;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "d", &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_DOUBLE, &val);
        dbus_message_iter_close_container(&entry, &var);
        dbus_message_iter_close_container(dict, &entry);
    }

    void append_dict_variant_bool(DBusMessageIter* dict, const char* key, dbus_bool_t val) {
        DBusMessageIter entry, var;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &val);
        dbus_message_iter_close_container(&entry, &var);
        dbus_message_iter_close_container(dict, &entry);
    }

    void append_dict_variant_string_list(DBusMessageIter* dict, const char* key, const std::vector<std::string>& vals) {
        DBusMessageIter entry, var, arr;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "as", &var);
        dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "s", &arr);
        for (const auto& v : vals) {
            const char* c_str = v.c_str();
            dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &c_str);
        }
        dbus_message_iter_close_container(&var, &arr);
        dbus_message_iter_close_container(&entry, &var);
        dbus_message_iter_close_container(dict, &entry);
    }

    void append_metadata_to_dict(DBusMessageIter* dict, const MprisMetadata& meta) {
        const char* track_id = "/org/mpris/MediaPlayer2/Track/0";
        DBusMessageIter entry, var;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        const char* key_id = "mpris:trackid";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_id);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "o", &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_OBJECT_PATH, &track_id);
        dbus_message_iter_close_container(&entry, &var);
        dbus_message_iter_close_container(dict, &entry);

        if (!meta.title.empty()) {
            append_dict_variant_string(dict, "xesam:title", meta.title.c_str());
        }
        if (!meta.artist.empty()) {
            append_dict_variant_string_list(dict, "xesam:artist", {meta.artist});
        }
        if (!meta.album.empty()) {
            append_dict_variant_string(dict, "xesam:album", meta.album.c_str());
        }
        if (!meta.url.empty()) {
            append_dict_variant_string(dict, "xesam:url", meta.url.c_str());
        }
        if (meta.length_us > 0) {
            append_dict_variant_int64(dict, "mpris:length", meta.length_us);
        }
    }
}

void update_mpris_metadata(const MprisMetadata& meta) {
    std::lock_guard<std::mutex> lock(g_meta_mutex);
    g_current_meta = meta;
}

void update_mpris_playback_status(const std::string& status) {
    std::lock_guard<std::mutex> lock(g_meta_mutex);
    g_current_meta.playback_status = status;
}

void update_mpris_position(int64_t position_us) {
    std::lock_guard<std::mutex> lock(g_meta_mutex);
    g_current_meta.position_us = position_us;
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

        // Register multiple aliases so playerctl -p vibe, -p vibe_fi, -p vibefi all resolve
        dbus_bus_request_name(conn, "org.mpris.MediaPlayer2.vibe", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
        if (dbus_error_is_set(&err)) dbus_error_free(&err);

        dbus_bus_request_name(conn, "org.mpris.MediaPlayer2.vibe_fi", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
        if (dbus_error_is_set(&err)) dbus_error_free(&err);

        dbus_bus_request_name(conn, "org.mpris.MediaPlayer2.vibefi", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
        if (dbus_error_is_set(&err)) dbus_error_free(&err);

        while (g_mpris_running.load()) {
            dbus_connection_read_write(conn, 50);
            DBusMessage* msg = dbus_connection_pop_message(conn);
            if (msg) {
                // 1. Playback control methods
                bool handled_action = false;
                if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "PlayPause")) {
                    push_action(MprisAction::PLAY_PAUSE);
                    handled_action = true;
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Play")) {
                    push_action(MprisAction::PLAY);
                    handled_action = true;
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Pause")) {
                    push_action(MprisAction::PAUSE);
                    handled_action = true;
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Next")) {
                    push_action(MprisAction::NEXT);
                    handled_action = true;
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Previous")) {
                    push_action(MprisAction::PREVIOUS);
                    handled_action = true;
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2.Player", "Stop")) {
                    push_action(MprisAction::STOP);
                    handled_action = true;
                } else if (dbus_message_is_method_call(msg, "org.mpris.MediaPlayer2", "Quit")) {
                    push_action(MprisAction::STOP);
                    handled_action = true;
                }

                if (handled_action) {
                    DBusMessage* reply = dbus_message_new_method_return(msg);
                    if (reply) {
                        dbus_connection_send(conn, reply, nullptr);
                        dbus_message_unref(reply);
                    }
                } else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get")) {
                    // 2. Property Get method
                    const char *iface = nullptr, *prop = nullptr;
                    if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID)) {
                        DBusMessage* reply = dbus_message_new_method_return(msg);
                        DBusMessageIter iter, var;
                        dbus_message_iter_init_append(reply, &iter);

                        MprisMetadata meta;
                        {
                            std::lock_guard<std::mutex> lock(g_meta_mutex);
                            meta = g_current_meta;
                        }

                        std::string p(prop);
                        if (p == "Metadata") {
                            DBusMessageIter dict;
                            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "a{sv}", &var);
                            dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "{sv}", &dict);
                            append_metadata_to_dict(&dict, meta);
                            dbus_message_iter_close_container(&var, &dict);
                            dbus_message_iter_close_container(&iter, &var);
                        } else if (p == "PlaybackStatus") {
                            const char* st = meta.playback_status.c_str();
                            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                            dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &st);
                            dbus_message_iter_close_container(&iter, &var);
                        } else if (p == "Identity") {
                            const char* id = "Vibe-Fi";
                            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &var);
                            dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &id);
                            dbus_message_iter_close_container(&iter, &var);
                        } else if (p == "Volume") {
                            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "d", &var);
                            dbus_message_iter_append_basic(&var, DBUS_TYPE_DOUBLE, &meta.volume);
                            dbus_message_iter_close_container(&iter, &var);
                        } else if (p == "Position") {
                            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "x", &var);
                            dbus_message_iter_append_basic(&var, DBUS_TYPE_INT64, &meta.position_us);
                            dbus_message_iter_close_container(&iter, &var);
                        } else if (p == "CanControl" || p == "CanPlay" || p == "CanPause" || p == "CanSeek" || p == "CanGoNext" || p == "CanGoPrevious") {
                            dbus_bool_t b = TRUE;
                            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                            dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &b);
                            dbus_message_iter_close_container(&iter, &var);
                        } else if (p == "CanQuit") {
                            dbus_bool_t b = TRUE;
                            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                            dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &b);
                            dbus_message_iter_close_container(&iter, &var);
                        } else if (p == "CanRaise" || p == "HasTrackList") {
                            dbus_bool_t b = FALSE;
                            dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &var);
                            dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &b);
                            dbus_message_iter_close_container(&iter, &var);
                        }

                        dbus_connection_send(conn, reply, nullptr);
                        dbus_message_unref(reply);
                    }
                } else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "GetAll")) {
                    // 3. Property GetAll method
                    const char* iface = nullptr;
                    if (dbus_message_get_args(msg, &err, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID)) {
                        DBusMessage* reply = dbus_message_new_method_return(msg);
                        DBusMessageIter iter, dict;
                        dbus_message_iter_init_append(reply, &iter);
                        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);

                        MprisMetadata meta;
                        {
                            std::lock_guard<std::mutex> lock(g_meta_mutex);
                            meta = g_current_meta;
                        }

                        if (std::string(iface) == "org.mpris.MediaPlayer2.Player") {
                            append_dict_variant_string(&dict, "PlaybackStatus", meta.playback_status.c_str());
                            append_dict_variant_bool(&dict, "CanControl", TRUE);
                            append_dict_variant_bool(&dict, "CanPlay", TRUE);
                            append_dict_variant_bool(&dict, "CanPause", TRUE);
                            append_dict_variant_bool(&dict, "CanSeek", TRUE);
                            append_dict_variant_bool(&dict, "CanGoNext", TRUE);
                            append_dict_variant_bool(&dict, "CanGoPrevious", TRUE);
                            append_dict_variant_double(&dict, "Volume", meta.volume);
                            append_dict_variant_int64(&dict, "Position", meta.position_us);

                            DBusMessageIter entry, var, mdict;
                            const char* mkey = "Metadata";
                            dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
                            dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &mkey);
                            dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a{sv}", &var);
                            dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "{sv}", &mdict);
                            append_metadata_to_dict(&mdict, meta);
                            dbus_message_iter_close_container(&var, &mdict);
                            dbus_message_iter_close_container(&entry, &var);
                            dbus_message_iter_close_container(&dict, &entry);
                        } else if (std::string(iface) == "org.mpris.MediaPlayer2") {
                            append_dict_variant_string(&dict, "Identity", "Vibe-Fi");
                            append_dict_variant_bool(&dict, "CanQuit", TRUE);
                            append_dict_variant_bool(&dict, "CanRaise", FALSE);
                            append_dict_variant_bool(&dict, "HasTrackList", FALSE);
                            append_dict_variant_string_list(&dict, "SupportedUriSchemes", {"file", "http", "https"});
                            append_dict_variant_string_list(&dict, "SupportedMimeTypes", {"audio/mpeg", "audio/ogg", "audio/flac", "audio/wav", "audio/webm"});
                        }

                        dbus_message_iter_close_container(&iter, &dict);
                        dbus_connection_send(conn, reply, nullptr);
                        dbus_message_unref(reply);
                    }
                } else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
                    // 4. Introspect method
                    const char* xml =
                        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
                        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
                        "<node>\n"
                        "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                        "    <method name=\"Introspect\">\n"
                        "      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
                        "    </method>\n"
                        "  </interface>\n"
                        "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
                        "    <method name=\"Get\">\n"
                        "      <arg name=\"interface\" direction=\"in\" type=\"s\"/>\n"
                        "      <arg name=\"propname\" direction=\"in\" type=\"s\"/>\n"
                        "      <arg name=\"value\" direction=\"out\" type=\"v\"/>\n"
                        "    </method>\n"
                        "    <method name=\"GetAll\">\n"
                        "      <arg name=\"interface\" direction=\"in\" type=\"s\"/>\n"
                        "      <arg name=\"props\" direction=\"out\" type=\"a{sv}\"/>\n"
                        "    </method>\n"
                        "  </interface>\n"
                        "  <interface name=\"org.mpris.MediaPlayer2\">\n"
                        "    <method name=\"Raise\"/>\n"
                        "    <method name=\"Quit\"/>\n"
                        "    <property name=\"CanQuit\" type=\"b\" access=\"read\"/>\n"
                        "    <property name=\"CanRaise\" type=\"b\" access=\"read\"/>\n"
                        "    <property name=\"HasTrackList\" type=\"b\" access=\"read\"/>\n"
                        "    <property name=\"Identity\" type=\"s\" access=\"read\"/>\n"
                        "  </interface>\n"
                        "  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
                        "    <method name=\"Next\"/>\n"
                        "    <method name=\"Previous\"/>\n"
                        "    <method name=\"Pause\"/>\n"
                        "    <method name=\"PlayPause\"/>\n"
                        "    <method name=\"Stop\"/>\n"
                        "    <method name=\"Play\"/>\n"
                        "    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>\n"
                        "    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>\n"
                        "    <property name=\"Volume\" type=\"d\" access=\"readwrite\"/>\n"
                        "    <property name=\"Position\" type=\"x\" access=\"read\"/>\n"
                        "    <property name=\"CanControl\" type=\"b\" access=\"read\"/>\n"
                        "    <property name=\"CanPlay\" type=\"b\" access=\"read\"/>\n"
                        "    <property name=\"CanPause\" type=\"b\" access=\"read\"/>\n"
                        "    <property name=\"CanSeek\" type=\"b\" access=\"read\"/>\n"
                        "    <property name=\"CanGoNext\" type=\"b\" access=\"read\"/>\n"
                        "    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>\n"
                        "  </interface>\n"
                        "</node>\n";
                    DBusMessage* reply = dbus_message_new_method_return(msg);
                    dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
                    dbus_connection_send(conn, reply, nullptr);
                    dbus_message_unref(reply);
                } else if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL) {
                    // Send default empty reply for other method calls to prevent client timeout
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
void update_mpris_metadata(const MprisMetadata& meta) { (void)meta; }
void update_mpris_playback_status(const std::string& status) { (void)status; }
void update_mpris_position(int64_t position_us) { (void)position_us; }

#endif
