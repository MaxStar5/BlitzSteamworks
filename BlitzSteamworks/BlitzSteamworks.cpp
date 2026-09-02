#include "BlitzSteamworks.h"

CallbackHandler* CallbackHandler::instance = nullptr;

BS_API(int) Init() {
	if (SteamAPI_Init()) {
		CallbackHandler::instance = new CallbackHandler();
		return 0;
	}
	return 1;
}

BS_API(int) RestartAppIfNecessary(int appId) {
	return SteamAPI_RestartAppIfNecessary(appId);
}

BS_API(void) Update() {
	SteamAPI_RunCallbacks();
}

BS_API(void) Shutdown() {
	delete CallbackHandler::instance;

	SteamAPI_Shutdown();
}

int b = 1;

BS_API(int) Achieve(const char* apiName) {
	if (b != 0) {
		if (SteamUserStats()->SetAchievement(apiName)) {
			if (SteamUserStats()->StoreStats()) {
				return 0;
			}
			return 2;
		}
		return 1;
	}
	return b == 1 ? b : 3;
}

BS_API(int) UnAchieve(const char* apiName) {
	if (b != 0) {
		if (SteamUserStats()->ClearAchievement(apiName)) {
			if (SteamUserStats()->StoreStats()) {
				return 0;
			}
			return 2;
		}
		return 1;
	}
	return b == 1 ? b : 3;
}

BS_API(const char*) GetGameLanguage() {
	return SteamApps()->GetCurrentGameLanguage();
}


int overlayState = 0;
int overlayStatePrev = 0;

BS_API(int) GetOverlayState() {
	return overlayState;
}

BS_API(int) GetOverlayUpdated() {
	if (overlayStatePrev == overlayState) {
		return -1;
	}
	overlayStatePrev = overlayState;
	return overlayState;
}

BS_API(void) SetOverlayNotificationInset(int x, int y) {
	SteamUtils()->SetOverlayNotificationInset(x, y);
}

BS_API(void) SetOverlayNotificationPosition(int pos) {
	SteamUtils()->SetOverlayNotificationPosition(static_cast<ENotificationPosition>(pos));
}


BS_API(int) SetRichPresence(const char* key, const char* value) {
	return SteamFriends()->SetRichPresence(key, value);
}


int idUpper(uint64 cid) {
	return cid >> 32;
}

int idLower(uint64 cid) {
	return cid & 0xffffffff;
}

uint64 idMerge(int upper, int lower) {
	return ((uint64) upper << 32) | lower;
}

BS_API(int) SteamID64To32(const char* cid) {
	CSteamID id((uint64)atoll(cid));
	return id.GetAccountID();
}

BS_API(int) StringToIDUpper(const char* cid) {
	return idUpper(atoll(cid));
}

BS_API(int) StringToIDLower(const char* cid) {
	return idLower(atoll(cid));
}

BS_API(const char*) IDToString(int upperID, int lowerID) {
	return std::to_string(idMerge(upperID, lowerID)).c_str();
}

BS_API(int) GetPlayerIDUpper() {
	return idUpper(SteamUser()->GetSteamID().ConvertToUint64());
}

BS_API(int) GetPlayerIDLower() {
	return idLower(SteamUser()->GetSteamID().ConvertToUint64());
}

BS_API(const char*) GetPlayerName() {
	return SteamFriends()->GetPersonaName();
}

BS_API(const char*) GetOtherPlayerName(int upperID, int lowerID) {
	return SteamFriends()->GetFriendPersonaName(idMerge(upperID, lowerID));
}


std::vector<uint8_t> p2poutput(0);

template <typename T>
void Push(T t) {
	uint8_t* ptr = (uint8_t*) &t;
	for (int i = 0; i < sizeof(T); i++) {
		p2poutput.push_back(*ptr);
		ptr++;
	}
}

BS_API(void) PushByte(int i) {
	Push((uint8_t) i);
}

BS_API(void) PushShort(int i) {
	Push((short) i);
}

BS_API(void) PushInt(int i) {
	Push(i);
}

BS_API(void) PushFloat(float f) {
	Push(f);
}

BS_API(void) PushString(const char* c) {
	char ch;
	do {
		ch = *c++;
		p2poutput.push_back(ch);
	} while (ch);
}

void* p2pinputstart = nullptr; // Maintained for freeing the memory block later
uint8_t* p2pinput = nullptr; // Current reading position pointer

int senderIDUpper = 0;
int senderIDLower = 0;

template <typename T>
T Pull() {
	T* t = reinterpret_cast<T*>(p2pinput);
	p2pinput += sizeof(T);
	return *t;
}

BS_API(int) PullByte() {
	return Pull<uint8_t>();
}

BS_API(int) PullShort() {
	return Pull<short>();
}

BS_API(int) PullInt() {
	return Pull<int>();
}

BS_API(float) PullFloat() {
	return Pull<float>();
}

BS_API(const char*) PullString() {
	const char* c = (const char*) p2pinput;
	while (*p2pinput++); // Move our pointer to the end of the string (after the null termination byte)
	return c;
}

BS_API(int) GetSenderIDUpper() {
	return senderIDUpper;
}

BS_API(int) GetSenderIDLower() {
	return senderIDLower;
}

BS_API(int) LoadPacket() {
	uint32 msgSize = 0;
	if (SteamNetworking()->IsP2PPacketAvailable(&msgSize)) {
		if (p2pinputstart != nullptr) {
			free(p2pinputstart);
			p2pinputstart = nullptr;
		}
		p2pinputstart = malloc(msgSize);
		CSteamID steamIDRemote;
		uint32 bytesRead = 0;
		if (SteamNetworking()->ReadP2PPacket(p2pinputstart, msgSize, &bytesRead, &steamIDRemote)) {
			p2pinput = (uint8_t*) p2pinputstart;
			uint64 id = steamIDRemote.ConvertToUint64();
			senderIDUpper = idUpper(id);
			senderIDLower = idLower(id);
			return 1;
		} else {
			return -1;
		}
	} else {
		return 0;
	}
}

BS_API(int) SendPacketToUser(int upperID, int lowerID, int reliable) {
    EP2PSend sendFlags = reliable ? k_EP2PSendReliable : k_EP2PSendUnreliable;
    bool b = SteamNetworking()->SendP2PPacket(idMerge(upperID, lowerID), p2poutput.data(), p2poutput.size(), sendFlags);
    p2poutput.clear();
    return b ? 1 : 0;
}

BS_API(int) CloseConnection(int upperID, int lowerID) {
	return SteamNetworking()->CloseP2PSessionWithUser(idMerge(upperID, lowerID));
}

static int lobbyState = 0;
static std::uint64_t lobbyId;
BS_API(int) CreateLobby(int lobbyType, int maxMembers) {
	if (lobbyState > 0) { return 0; }
	CallbackHandler::instance->lobbyCreatedCallback.Set(SteamMatchmaking()->CreateLobby(static_cast<ELobbyType>(lobbyType), maxMembers), CallbackHandler::instance, &CallbackHandler::handleLobbyCreated);
	lobbyState = 100;
	return 1;
}

BS_API(int) JoinLobby(int upperLobbyID, int lowerLobbyID) {
	if (lobbyState > 0) { return 0; }
	CallbackHandler::instance->lobbyEnteredCallback.Set(SteamMatchmaking()->JoinLobby(idMerge(upperLobbyID, lowerLobbyID)), CallbackHandler::instance, &CallbackHandler::handleLobbyEntered);
	lobbyState = 101;
	return 1;
}

BS_API(void) LeaveLobby() {
	SteamMatchmaking()->LeaveLobby(lobbyId);
	lobbyState = 0;
}

// 0 = no lobby
// 100 = waiting to create lobby
// 101 = waiting to join lobby
// 1 = in lobby
// 2 = in lobby, owner
// -x = failed to create/join lobby with error code x
BS_API(int) GetLobbyState() {
	return lobbyState;
}

BS_API(int) GetLobbyIDUpper() {
	return idUpper(lobbyId);
}

BS_API(int) GetLobbyIDLower() {
	return idLower(lobbyId);
}

BS_API(int) GetNumLobbyMembers() {
	return SteamMatchmaking()->GetNumLobbyMembers(lobbyId);
}

BS_API(int) GetLobbyMemberIDUpper(int index) {
	return idUpper(SteamMatchmaking()->GetLobbyMemberByIndex(lobbyId, index).ConvertToUint64());
}

BS_API(int) GetLobbyMemberIDLower(int index) {
	return idLower(SteamMatchmaking()->GetLobbyMemberByIndex(lobbyId, index).ConvertToUint64());
}

BS_API(int) GetLobbyOwnerIDUpper() {
	return idUpper(SteamMatchmaking()->GetLobbyOwner(lobbyId).ConvertToUint64());
}

BS_API(int) GetLobbyOwnerIDLower() {
	return idLower(SteamMatchmaking()->GetLobbyOwner(lobbyId).ConvertToUint64());
}

BS_API(int) GetLobbyMemberCount(int upperLobbyID, int lowerLobbyID) {
	return SteamMatchmaking()->GetNumLobbyMembers(idMerge(upperLobbyID, lowerLobbyID));
}

BS_API(int) GetLobbyMaxMembers(int upperLobbyID, int lowerLobbyID) {
	return SteamMatchmaking()->GetLobbyMemberLimit(idMerge(upperLobbyID, lowerLobbyID));
}

BS_API(void) ActivateOverlayInviteDialog() {
	SteamFriends()->ActivateGameOverlayInviteDialog(lobbyId);
}

static bool acceptLobbyInvites = true;
BS_API(void) SetAcceptLobbyInvites(bool accept) {
	acceptLobbyInvites = accept;
}


BS_API(void) OpenOnScreenKeyboard(int mode, int x, int y, int width, int height) {
	SteamUtils()->ShowFloatingGamepadTextInput(static_cast<EFloatingGamepadTextInputMode>(mode), x, y, width, height);
}

BS_API(void) CloseOnScreenKeyboard() {
	SteamUtils()->DismissFloatingGamepadTextInput();
}

BS_API(const char*) EE(const char* cid) {
	P2PSessionState_t p2pSessionState;
	if (SteamNetworking()->GetP2PSessionState((uint64) std::atoll(cid), &p2pSessionState)) {
		std::string s = (std::to_string(p2pSessionState.m_bConnecting));
		s.append("  ");
		s.append(std::to_string(p2pSessionState.m_bConnectionActive));
		s.append("  ");
		s.append(std::to_string(p2pSessionState.m_bUsingRelay));
		s.append("  ");
		s.append(std::to_string(p2pSessionState.m_eP2PSessionError));
		s.append("  ");
		s.append(std::to_string(p2pSessionState.m_nBytesQueuedForSend));
		s.append("  ");
		s.append(std::to_string(p2pSessionState.m_nPacketsQueuedForSend));
		s.append("  ");
		s.append(std::to_string(p2pSessionState.m_nRemoteIP));
		s.append("  ");
		s.append(std::to_string(p2pSessionState.m_nRemotePort));
		return s.c_str();
	}
	return "";
}

// Auth session
EAuthSessionResponse authResponse;
uint64_t authSteamID;

BS_API(int) BeginAuthSession(void** ticket, int ticketSize, int upperID, int lowerID) {
	const uint8_t* authTicket = static_cast<const uint8_t*>(*ticket);
	return SteamUser()->BeginAuthSession(authTicket, ticketSize, idMerge(upperID, lowerID));
}

BS_API(void) EndAuthSession(int upperID, int lowerID) {
	SteamUser()->EndAuthSession(idMerge(upperID, lowerID));
}

BS_API(int) GetAuthSessionTicket(void** ticket, int ticketSize) {
	uint8_t* authTicket = static_cast<uint8_t*>(*ticket);
	uint32_t pcbTicket = 0;

	HAuthTicket handle = SteamUser()->GetAuthSessionTicket(authTicket, ticketSize, &pcbTicket, nullptr);
	return handle;
}

BS_API(int) _GetAuthSessionResponse() {
	return authResponse;
}

BS_API(int) GetAuthSessionReponseIDLower() {
	return idLower(authSteamID);
}

BS_API(int) GetAuthSessionReponseIDUpper() {
	return idUpper(authSteamID);
}

BS_API(void) CancelAuthTicket(HAuthTicket handle) {
	SteamUser()->CancelAuthTicket(handle);
}

BS_API(int) GetFriendCount() {
	return SteamFriends()->GetFriendCount(k_EFriendFlagImmediate);
}

BS_API(int) GetFriendIDUpper(int friendIndex) {
	CSteamID id = SteamFriends()->GetFriendByIndex(friendIndex, k_EFriendFlagImmediate);
	return idUpper(id.ConvertToUint64());
}

BS_API(int) GetFriendIDLower(int friendIndex) {
	CSteamID id = SteamFriends()->GetFriendByIndex(friendIndex, k_EFriendFlagImmediate);
	return idLower(id.ConvertToUint64());
}

BS_API(int) GetFriendLobbyIDUpper(int friendIndex) {
	CSteamID friendID = SteamFriends()->GetFriendByIndex(friendIndex, k_EFriendFlagImmediate);
	FriendGameInfo_t gameInfo{};

	if (!SteamFriends()->GetFriendGamePlayed(friendID, &gameInfo)) return 0;

	return idUpper(gameInfo.m_steamIDLobby.ConvertToUint64());
}

BS_API(int) GetFriendLobbyIDLower(int friendIndex) {
	CSteamID friendID = SteamFriends()->GetFriendByIndex(friendIndex, k_EFriendFlagImmediate);
	FriendGameInfo_t gameInfo{};

	if (!SteamFriends()->GetFriendGamePlayed(friendID, &gameInfo)) return 0;

	return idLower(gameInfo.m_steamIDLobby.ConvertToUint64());
}

BS_API(const char*) GetFriendName(int friendIndex) {
	CSteamID friendID = SteamFriends()->GetFriendByIndex(friendIndex, k_EFriendFlagImmediate);

	return SteamFriends()->GetFriendPersonaName(friendID);
}

void CallbackHandler::handleUserStatsReceived(UserStatsReceived_t* callback) {
	b = -static_cast<int>(callback->m_eResult);
}

void CallbackHandler::handleGameOverlayActivated(GameOverlayActivated_t* callback) {
	overlayState = callback->m_bActive;
}

void CallbackHandler::handleP2PSessionRequest(P2PSessionRequest_t* callback) {
	// TODO accept everything for now!
	SteamNetworking()->AcceptP2PSessionWithUser(callback->m_steamIDRemote);
}

int c = -1;
void CallbackHandler::handleP2PSessionConnectFail(P2PSessionConnectFail_t* callback) {
	c = callback->m_eP2PSessionError;
}


void CallbackHandler::handleLobbyCreated(LobbyCreated_t* callback, bool bIOFailure) {
	if (callback->m_eResult == k_EResultOK) {
		lobbyState = 2;
		lobbyId = callback->m_ulSteamIDLobby;
	} else {
		lobbyState = -static_cast<int>(callback->m_eResult);
	}
}

void CallbackHandler::handleLobbyEntered(LobbyEnter_t* callback, bool bIOFailure) {
	if (callback->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess) {
		if (lobbyState != 2) {
			lobbyState = 1;
			lobbyId = callback->m_ulSteamIDLobby;
		}
	} else {
		lobbyState = -static_cast<int>(callback->m_EChatRoomEnterResponse);
	}
}

void CallbackHandler::handleGameLobbyJoinRequested(GameLobbyJoinRequested_t* callback) {
	if (!acceptLobbyInvites) { return; }
	if (lobbyState > 0) { SteamMatchmaking()->LeaveLobby(lobbyId); }
	lobbyState = 101;
	instance->lobbyEnteredCallback.Set(SteamMatchmaking()->JoinLobby(callback->m_steamIDLobby), instance, &CallbackHandler::handleLobbyEntered);
}

void CallbackHandler::handleAuthTicketResponse(ValidateAuthTicketResponse_t* callback) {
	authResponse = callback->m_eAuthSessionResponse;
	authSteamID = callback->m_SteamID.ConvertToUint64();
}
