#ifndef MPRIS_HPP
#define MPRIS_HPP

#include <functional>

void start_mpris_server(
    std::function<void()> on_playpause,
    std::function<void()> on_next,
    std::function<void()> on_prev
);

#endif // MPRIS_HPP
