// Red de bajo nivel (netlow.cpp): sin raylib ni windows.h en esta cabecera.
#pragma once
#include <vector>
#include <string>
#include <cstddef>

enum { NL_NONE, NL_CONNECT, NL_DISCONNECT, NL_DATA };
struct NLEvent { std::vector<unsigned char> data; int channel = 0; };

bool NL_Host(int port);
bool NL_Connect(const char *ip, int port);
void NL_Close();
bool NL_Connected();
int NL_Ping();                    // ida y vuelta en ms
int NL_Poll(NLEvent &out);        // NL_NONE cuando no queda nada
void NL_Send(const void *data, size_t len, bool reliable);
void NL_Flush();
void NL_FetchPublicIP();          // en segundo plano
std::string NL_PublicIP();        // vacío mientras se averigua
std::string NL_LocalIP();
