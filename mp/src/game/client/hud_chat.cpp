//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"

#include "hud_basechat.h"
#include "hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include <vgui/ILocalize.h>

#include "tier0/memdbgon.h"

#define MOM_LOBBY_DATA_MEMBER_TYPING_YES "y"
#define MOM_LOBBY_DATA_MEMBER_TYPING_NO "n"

DECLARE_HUDELEMENT(CHudChat);

DECLARE_HUD_MESSAGE(CHudChat, SayText);
DECLARE_HUD_MESSAGE(CHudChat, SayText2);
DECLARE_HUD_MESSAGE(CHudChat, TextMsg);

//=====================
// CHudChat
//=====================

CHudChat::CHudChat(const char *pElementName) : BaseClass(pElementName)
{
    m_vTypingMembers = CUtlVector<CSteamID>();
    m_hfInfoTextFont = 0;
    m_uiLobbyId = 0;
    m_bVisible = m_bTyping = false;
}

void CHudChat::Init(void)
{
    BaseClass::Init();

    m_hfInfoTextFont =
        vgui::scheme()->GetIScheme(vgui::scheme()->GetScheme("ClientScheme"))->GetFont("CloseCaption_Normal");

    HOOK_HUD_MESSAGE(CHudChat, SayText);
    HOOK_HUD_MESSAGE(CHudChat, SayText2);
    HOOK_HUD_MESSAGE(CHudChat, TextMsg);
}

void CHudChat::OnLobbyEnter(LobbyEnter_t *pParam)
{
    if (pParam->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess)
    {
        m_uiLobbyId = pParam->m_ulSteamIDLobby;
    }
}

void CHudChat::OnLobbyMessage(LobbyChatMsg_t *pParam)
{
    CSteamID msgSender = CSteamID(pParam->m_ulSteamIDUser);
    if (pParam->m_ulSteamIDUser == steamapicontext->SteamUser()->GetSteamID().ConvertToUint64())
    {
        DevLog("Got our own message! Just ignoring it...\n");
        return;
    }

    char personName[MAX_PLAYER_NAME_LENGTH];
    Q_strncpy(personName, steamapicontext->SteamFriends()->GetFriendPersonaName(msgSender), MAX_PLAYER_NAME_LENGTH);

    char *message = new char[4096];
    // MOM_TODO: This won't be just text in the future, if we captialize on being able to send binary data. Wrap this is
    // something and parse it
    int written = steamapicontext->SteamMatchmaking()->GetLobbyChatEntry(
        CSteamID(pParam->m_ulSteamIDLobby), pParam->m_iChatID, nullptr, message, 4096, nullptr);
    DevLog("CLIENT: written: %i\n", written);
    Printf(CHAT_FILTER_NONE, "%s: %s", personName, message);
    delete[] message;
}

void CHudChat::OnLobbyChatUpdate(LobbyChatUpdate_t *pParam)
{
    uint32 state = pParam->m_rgfChatMemberStateChange;
    CSteamID changedPerson = CSteamID(pParam->m_ulSteamIDUserChanged);
    const char *pName = steamapicontext->SteamFriends()->GetFriendPersonaName(changedPerson);
    if (state & k_EChatMemberStateChangeEntered)
    {
        // Somebody joined us! Huzzah!
        // GetLobbyMemberSteamData(changedPerson); MOM_TODO: Does this happen asynchronously?

        Printf(CHAT_FILTER_JOINLEAVE | CHAT_FILTER_SERVERMSG, "%s has joined the lobby.", pName);
    }
    if (state & k_EChatMemberStateChangeLeft || state & k_EChatMemberStateChangeDisconnected)
    {
        if (changedPerson == steamapicontext->SteamUser()->GetSteamID())
        {
            // We're the one who's leaving.
            m_uiLobbyId = 0;
        }
        Printf(CHAT_FILTER_JOINLEAVE | CHAT_FILTER_SERVERMSG, "%s has left the lobby.", pName);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a player's Chat text from the server
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_SayText2(bf_read &msg)
{
    int client = msg.ReadByte();
    bool bWantsToChat = msg.ReadByte();

    wchar_t szBuf[6][256];
    char untranslated_msg_text[256];
    wchar_t *msg_text = ReadLocalizedString(msg, szBuf[0], sizeof(szBuf[0]), false, untranslated_msg_text,
        sizeof(untranslated_msg_text));

    // keep reading strings and using C format strings for subsituting the strings into the localised text string
    ReadChatTextString(msg, szBuf[1], sizeof(szBuf[1])); // player name
    ReadChatTextString(msg, szBuf[2], sizeof(szBuf[2])); // chat text
    ReadLocalizedString(msg, szBuf[3], sizeof(szBuf[3]), true);
    ReadLocalizedString(msg, szBuf[4], sizeof(szBuf[4]), true);

    g_pVGuiLocalize->ConstructString(szBuf[5], sizeof(szBuf[5]), msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4]);

    char ansiString[512];
    g_pVGuiLocalize->ConvertUnicodeToANSI(ConvertCRtoNL(szBuf[5]), ansiString, sizeof(ansiString));

    if (bWantsToChat)
    {
        // print raw chat text
        ChatPrintf(client, CHAT_FILTER_NONE, "%s", ansiString);

        Msg("%s\n", RemoveColorMarkup(ansiString));
    }
    else
    {
        // print raw chat text
        ChatPrintf(client, CHAT_FILTER_NONE, "%s", ansiString);
    }
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pszName -
//			iSize -
//			*pbuf -
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_SayText(bf_read &msg)
{
    char szString[256];

    msg.ReadByte(); // client ID
    msg.ReadString(szString, sizeof(szString));
    Printf(CHAT_FILTER_NONE, "%s", szString);
}

// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in
// titles.txt
// the next (optional) one to four strings are parameters for that string (which can also be message names if they begin
// with '#')
void CHudChat::MsgFunc_TextMsg(bf_read &msg)
{
    char szString[2048];
    int msg_dest = msg.ReadByte();
    static char szBuf[6][256];

    msg.ReadString(szString, sizeof(szString));
    char *msg_text = hudtextmessage->LookupString(szString, &msg_dest);
    Q_strncpy(szBuf[0], msg_text, sizeof(szBuf[0]));
    msg_text = szBuf[0];

    // keep reading strings and using C format strings for subsituting the strings into the localised text string
    msg.ReadString(szString, sizeof(szString));
    char *sstr1 = hudtextmessage->LookupString(szString);
    Q_strncpy(szBuf[1], sstr1, sizeof(szBuf[1]));
    sstr1 = szBuf[1];

    StripEndNewlineFromString(
        sstr1); // these strings are meant for subsitution into the main strings, so cull the automatic end newlines
    msg.ReadString(szString, sizeof(szString));
    char *sstr2 = hudtextmessage->LookupString(szString);
    Q_strncpy(szBuf[2], sstr2, sizeof(szBuf[2]));
    sstr2 = szBuf[2];

    StripEndNewlineFromString(sstr2);
    msg.ReadString(szString, sizeof(szString));
    char *sstr3 = hudtextmessage->LookupString(szString);
    Q_strncpy(szBuf[3], sstr3, sizeof(szBuf[3]));
    sstr3 = szBuf[3];

    StripEndNewlineFromString(sstr3);
    msg.ReadString(szString, sizeof(szString));
    char *sstr4 = hudtextmessage->LookupString(szString);
    Q_strncpy(szBuf[4], sstr4, sizeof(szBuf[4]));
    sstr4 = szBuf[4];

    StripEndNewlineFromString(sstr4);
    char *psz = szBuf[5];

    if (!cl_showtextmsg.GetInt())
        return;

    switch (msg_dest)
    {
        case HUD_PRINTCENTER:
            Q_snprintf(psz, sizeof(szBuf[5]), msg_text, sstr1, sstr2, sstr3, sstr4);
            internalCenterPrint->Print(ConvertCRtoNL(psz));
            break;

        case HUD_PRINTNOTIFY:
            psz[0] = 1; // mark this message to go into the notify buffer
            Q_snprintf(psz + 1, sizeof(szBuf[5]) - 1, msg_text, sstr1, sstr2, sstr3, sstr4);
            Msg("%s", ConvertCRtoNL(psz));
            break;

        case HUD_PRINTTALK:
            Q_snprintf(psz, sizeof(szBuf[5]), msg_text, sstr1, sstr2, sstr3, sstr4);
            Printf(CHAT_FILTER_NONE, "%s", ConvertCRtoNL(psz));
            break;

        case HUD_PRINTCONSOLE:
            Q_snprintf(psz, sizeof(szBuf[5]), msg_text, sstr1, sstr2, sstr3, sstr4);
            Msg("%s", ConvertCRtoNL(psz));
            break;
    }
}

void CHudChat::StartMessageMode(int)
{
    m_bVisible = true;
}


void CHudChat::StopMessageMode()
{
    BaseClass::StopMessageMode();
    if (m_uiLobbyId != 0) // Only if already on lobby
    {
        steamapicontext->SteamMatchmaking()->SetLobbyMemberData(m_uiLobbyId, "isTyping",
            MOM_LOBBY_DATA_MEMBER_TYPING_NO);
    }

    // Can't be typing if we close the chat
    m_bVisible = m_bTyping = false;

}

void CHudChat::OnThink()
{
    if (m_uiLobbyId != 0 && GetMessageMode() != 0 && GetInputPanel())
    {
        const int isSomethingTyped = GetInputPanel()->GetTextLength() > 0;
        if (isSomethingTyped && !m_bTyping)
        {
            steamapicontext->SteamMatchmaking()->SetLobbyMemberData(m_uiLobbyId, "isTyping", MOM_LOBBY_DATA_MEMBER_TYPING_YES);
            m_bTyping = true;
        }
        else if (!isSomethingTyped && m_bTyping)
        {
            steamapicontext->SteamMatchmaking()->SetLobbyMemberData(m_uiLobbyId, "isTyping", MOM_LOBBY_DATA_MEMBER_TYPING_NO);
            m_bTyping = true;
        }
    }
}

void CHudChat::OnLobbyDataUpdate(LobbyDataUpdate_t *pParam)
{
    // If something other than the lobby...
    if (pParam->m_bSuccess && pParam->m_ulSteamIDLobby != pParam->m_ulSteamIDMember)
    {
        const char *typingStatus =
            steamapicontext->SteamMatchmaking()->GetLobbyMemberData(m_uiLobbyId, pParam->m_ulSteamIDMember, "isTyping");
        if (typingStatus)
        {
            const int typingIndex = m_vTypingMembers.Find(pParam->m_ulSteamIDMember);
            const bool isValidIndex = m_vTypingMembers.IsValidIndex(typingIndex);
            if (Q_strcmp(typingStatus, MOM_LOBBY_DATA_MEMBER_TYPING_YES) == 0 && !isValidIndex)
            {
                m_vTypingMembers.AddToTail(pParam->m_ulSteamIDMember);
            }
            else if (Q_strcmp(typingStatus, MOM_LOBBY_DATA_MEMBER_TYPING_NO) == 0 && isValidIndex)
            {
                m_vTypingMembers.FastRemove(typingIndex);
            }
        }
    }
}

void CHudChat::Paint()
{
    BaseClass::Paint();
    if (m_vTypingMembers.Count() > 0 && m_bVisible)
    {
        char typingText[BUFSIZ];
        // This line is a shameful reminder of my lack of control over C-strings
        typingText[0] = '\0';
        wchar_t wcTypingText[BUFSIZ];
        if (m_vTypingMembers.Count() <= 3)
        {
            char nameChunk[MAX_PLAYER_NAME_LENGTH + 3];
            for (int i = 0; i < m_vTypingMembers.Count(); i++)
            {
                Q_strncpy(nameChunk,
                    steamapicontext->SteamFriends()->GetFriendPersonaName(CSteamID(m_vTypingMembers[i])),
                    MAX_PLAYER_NAME_LENGTH);
                Q_strcat(nameChunk, i < m_vTypingMembers.Count() - 1 ? ", " : " ", MAX_PLAYER_NAME_LENGTH + 2);
                Q_strcat(typingText, nameChunk, BUFSIZ);
            }
            Q_strcat(typingText, "typing...", BUFSIZ);
        }
        else
        {
            Q_snprintf(typingText, BUFSIZ, "%d people typing...", m_vTypingMembers.Count());
        }
        const int count = g_pVGuiLocalize->ConvertANSIToUnicode(typingText, wcTypingText, BUFSIZ);
        vgui::surface()->DrawSetTextFont(m_hfInfoTextFont);
        vgui::surface()->DrawSetTextPos(0, 0);
        vgui::surface()->DrawSetTextColor(255, 255, 255, 255);
        vgui::surface()->DrawPrintText(wcTypingText, count);
    }
}

void CHudChat::OnLobbyKicked(LobbyKicked_t* pParam)
{
    m_uiLobbyId = 0;
}
