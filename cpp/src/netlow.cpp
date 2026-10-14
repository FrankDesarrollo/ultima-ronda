// Red de bajo nivel con ENet (UDP): un anfitrión y un invitado.
// Va aparte del resto porque las cabeceras de Windows chocan con las de raylib.
// Canal 0: estado frecuente (no confiable, se descarta si llega tarde). Canal 1: eventos (confiable y en orden).
#include "netlow.h"
#include <enet/enet.h>
#include <windows.h>
#include <wininet.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>

static ENetHost *host = nullptr;
static ENetPeer *peer = nullptr;
static bool inited = false;

static bool Init() {
    if (!inited) inited = enet_initialize() == 0;
    return inited;
}

bool NL_Host(int port) {
    NL_Close();
    if (!Init()) return false;
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = (enet_uint16)port;
    host = enet_host_create(&addr, 1, 2, 0, 0);
    return host != nullptr;
}

bool NL_Connect(const char *ip, int port) {
    NL_Close();
    if (!Init()) return false;
    host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!host) return false;
    ENetAddress addr;
    if (enet_address_set_host(&addr, ip) != 0) { NL_Close(); return false; }
    addr.port = (enet_uint16)port;
    peer = enet_host_connect(host, &addr, 2, 0);
    if (!peer) { NL_Close(); return false; }
    enet_peer_timeout(peer, 32, 4000, 8000);   // da por perdida la conexión tras ~8 s sin respuesta
    return true;
}

void NL_Close() {
    if (peer) { enet_peer_disconnect_now(peer, 0); peer = nullptr; }
    if (host) { enet_host_flush(host); enet_host_destroy(host); host = nullptr; }
}

bool NL_Connected() { return peer && peer->state == ENET_PEER_STATE_CONNECTED; }
int NL_Ping() { return peer ? (int)peer->roundTripTime : 0; }

int NL_Poll(NLEvent &out) {
    if (!host) return NL_NONE;
    ENetEvent e;
    while (enet_host_service(host, &e, 0) > 0) {
        switch (e.type) {
        case ENET_EVENT_TYPE_CONNECT:
            if (peer && peer != e.peer) { enet_peer_disconnect_now(e.peer, 0); continue; }   // solo un invitado
            peer = e.peer;
            enet_peer_timeout(peer, 32, 4000, 8000);
            return NL_CONNECT;
        case ENET_EVENT_TYPE_DISCONNECT:
            if (e.peer != peer) continue;
            peer = nullptr;
            return NL_DISCONNECT;
        case ENET_EVENT_TYPE_RECEIVE:
            out.data.assign(e.packet->data, e.packet->data + e.packet->dataLength);
            out.channel = e.channelID;
            enet_packet_destroy(e.packet);
            return NL_DATA;
        default: break;
        }
    }
    return NL_NONE;
}

void NL_Send(const void *data, size_t len, bool reliable) {
    if (!NL_Connected()) return;
    ENetPacket *p = enet_packet_create(data, len, reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(peer, reliable ? 1 : 0, p);
}

void NL_Flush() { if (host) enet_host_flush(host); }

// ---------- IP pública (se pregunta a ipify en segundo plano) ----------
static std::mutex ipMutex;
static std::string publicIP;
static std::atomic<bool> ipBusy{ false };
void NL_FetchPublicIP() {
    if (ipBusy.exchange(true)) return;
    std::thread([]() {
        std::string got;
        HINTERNET net = InternetOpenA("UltimaRonda", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (net) {
            HINTERNET url = InternetOpenUrlA(net, "https://api.ipify.org", nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
            if (url) {
                char buf[64] = { 0 }; DWORD n = 0;
                if (InternetReadFile(url, buf, sizeof(buf) - 1, &n)) got.assign(buf, n);
                InternetCloseHandle(url);
            }
            InternetCloseHandle(net);
        }
        std::lock_guard<std::mutex> lk(ipMutex);
        publicIP = got.empty() ? "no se pudo averiguar" : got;
        ipBusy = false;
    }).detach();
}
std::string NL_PublicIP() { std::lock_guard<std::mutex> lk(ipMutex); return publicIP; }

// IP de esta PC en la red de casa (para el reenvío de puertos del router)
std::string NL_LocalIP() {
    char name[256];
    if (gethostname(name, sizeof(name)) != 0) return "";
    hostent *h = gethostbyname(name);
    if (!h) return "";
    for (int i = 0; h->h_addr_list[i]; i++) {
        unsigned char *a = (unsigned char *)h->h_addr_list[i];
        if (a[0] == 192 || a[0] == 10 || (a[0] == 172 && a[1] >= 16 && a[1] < 32)) {
            char b[32]; snprintf(b, 32, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]); return b;
        }
    }
    return "";
}
