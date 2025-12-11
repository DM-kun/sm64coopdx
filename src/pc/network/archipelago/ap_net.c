
#include <stdio.h>
#include "pc/configfile.h"
#include "pc/debuglog.h"
#include "pc/djui/djui.h"
#include "ap_net.h"
#ifdef ARCHIPELAGO
#include "pc/archipelago/sm64ap.h"

static bool ns_archipelago_initialize(enum NetworkType networkType, UNUSED bool reconnecting) {
    LOG_INFO("initialized");

    // success
    return true;
}

static s64 ns_archipelago_get_id(UNUSED u8 localId) {
    return 0;
}

static char* ns_archipelago_get_id_str(UNUSED u8 localId) {
    return "0";
}

static void ns_archipelago_save_id(u8 localId, UNUSED s64 networkId) {
    LOG_INFO("saved addr for id %d", localId);
}

static void ns_archipelago_clear_id(u8 localId) {
    if (localId == 0) { return; }
    LOG_INFO("cleared addr for id %d", localId);
}

static void* ns_archipelago_dup_addr(UNUSED u8 localIndex) {
    return 0;
}

static bool ns_archipelago_match_addr(UNUSED void* addr1, UNUSED void* addr2) {
    return true;
}

static void ns_archipelago_update(void) {
    if (gNetworkType == NT_NONE) { return; }
    do {
        SM64AP_PrintNext();
    } while (true);
}

static int ns_archipelago_send(UNUSED u8 localIndex, UNUSED void* address, UNUSED u8* data, UNUSED u16 dataLength) {
    return 0;
}

static void ns_archipelago_get_lobby_id(char* destination, u32 destLength) {
    snprintf(destination, destLength, "%s", ""); // TODO: we can probably hook this up
}

static void ns_archipelago_get_lobby_secret(char* destination, u32 destLength) {
    snprintf(destination, destLength, "%s", ""); // TODO: we can probably hook this up
}

static void ns_archipelago_shutdown(UNUSED bool reconnecting) {
    SM64AP_Shutdown();
    LOG_INFO("shutdown");
}

struct NetworkSystem gNetworkSystemArchipelago = {
    .initialize       = ns_archipelago_initialize,
    .get_id           = ns_archipelago_get_id,
    .get_id_str       = ns_archipelago_get_id_str,
    .save_id          = ns_archipelago_save_id,
    .clear_id         = ns_archipelago_clear_id,
    .dup_addr         = ns_archipelago_dup_addr,
    .match_addr       = ns_archipelago_match_addr,
    .update           = ns_archipelago_update,
    .send             = ns_archipelago_send,
    .get_lobby_id     = ns_archipelago_get_lobby_id,
    .get_lobby_secret = ns_archipelago_get_lobby_secret,
    .shutdown         = ns_archipelago_shutdown,
    .requireServerBroadcast = false,
    .name             = "Archipelago",
};

#endif
