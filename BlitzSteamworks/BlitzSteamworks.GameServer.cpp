#include "BlitzSteamworks.h"

int steamServersConnected;
extern uint64 idMerge(int upper, int lower);

BS_API(int) GS_Init(int IP, int gamePort, int queryPort, EServerMode mode, const char* version) {
	if (SteamGameServer_Init(IP, gamePort, queryPort, mode, version)) {
		CallbackHandler::instance = new CallbackHandler();
		return 1;
	}
	return 0;
}

BS_API(void) GS_LogOn(const char* appID, const char* desc) {
	SteamGameServer()->SetProduct(appID);
	SteamGameServer()->SetGameDescription(desc);

	SteamGameServer()->LogOnAnonymous();
}

BS_API(void) GS_LogOff() {
	SteamGameServer()->LogOff();
}

BS_API(int) GS_IsLoggedOn() {
	return SteamGameServer()->BLoggedOn();
}

BS_API(void) GS_Update() {
	SteamGameServer_RunCallbacks();
}

BS_API(void) GS_EndAuthSession(int upperID, int lowerID) {
	SteamGameServer()->EndAuthSession(idMerge(upperID, lowerID));
}

BS_API(int) GS_BeginAuthSession(void** ticket, int ticketSize, int upperID, int lowerID) {
	const uint8_t* authTicket = static_cast<const uint8_t*>(*ticket);
	return SteamGameServer()->BeginAuthSession(authTicket, ticketSize, idMerge(upperID, lowerID));
}

BS_API(int) GS_GetSteamServersConnected() { return steamServersConnected; }

void CallbackHandler::handleSteamServersConnected(SteamServersConnected_t* pCallback) {
	steamServersConnected = 1;
}

void CallbackHandler::handleSteamServersConnectFailure(SteamServerConnectFailure_t* pCallBack) {
	steamServersConnected = static_cast<int>(pCallBack->m_eResult);
}
