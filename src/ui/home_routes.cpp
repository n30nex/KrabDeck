// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "home_routes.h"

#include <cstring>

namespace sigurdos::ui {
namespace {

constexpr HomeRoute ROUTES[] = {
    {"CHATS",   true,  Screen::Chat,     {0, false}},
    {"MAP",     false, Screen::Map,      {0, false}},
    {"NETWORK", false, Screen::Network,  {0, false}},
    {"MORE",    false, Screen::Settings, {0, false}},
};
static_assert(sizeof(ROUTES) / sizeof(ROUTES[0]) == HOME_ROUTE_COUNT);

} // namespace

std::size_t homeRouteCount() { return HOME_ROUTE_COUNT; }

const HomeRoute* homeRouteAt(std::size_t index)
{
    return index < homeRouteCount() ? &ROUTES[index] : nullptr;
}

const HomeRoute* findHomeRoute(const char* label)
{
    if (!label) return nullptr;
    for (const auto& route : ROUTES) {
        if (std::strcmp(route.label, label) == 0) return &route;
    }
    return nullptr;
}

HomeTileFilters home_tile_filters(int tile_index)
{
    const HomeRoute* route = tile_index >= 0
        ? homeRouteAt(static_cast<std::size_t>(tile_index)) : nullptr;
    return route ? route->filters : HomeTileFilters{0, false};
}

} // namespace sigurdos::ui
