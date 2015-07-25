/*
 * command.c
 *
 * Copyright (C) 2012 - 2015 James Booth <boothj5@gmail.com>
 *
 * This file is part of Profanity.
 *
 * Profanity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Profanity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Profanity.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link the code of portions of this program with the OpenSSL library under
 * certain conditions as described in each individual source file, and
 * distribute linked combinations including the two.
 *
 * You must obey the GNU General Public License in all respects for all of the
 * code used other than OpenSSL. If you modify file(s) with this exception, you
 * may extend this exception to your version of the file(s), but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version. If you delete this exception statement from all
 * source files in the program, then also delete it here.
 *
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "config.h"

#include "chat_session.h"
#include "command/command.h"
#include "command/commands.h"
#include "common.h"
#include "config/accounts.h"
#include "config/preferences.h"
#include "config/theme.h"
#include "contact.h"
#include "roster_list.h"
#include "jid.h"
#include "xmpp/form.h"
#include "log.h"
#include "muc.h"
#ifdef HAVE_LIBOTR
#include "otr/otr.h"
#endif
#include "profanity.h"
#include "tools/autocomplete.h"
#include "tools/parser.h"
#include "tools/tinyurl.h"
#include "xmpp/xmpp.h"
#include "xmpp/bookmark.h"
#include "ui/ui.h"
#include "window_list.h"

static gboolean _cmd_execute(ProfWin *window, const char * const command, const char * const inp);

static char * _cmd_complete_parameters(ProfWin *window, const char * const input);

static char * _sub_autocomplete(ProfWin *window, const char * const input);
static char * _notify_autocomplete(ProfWin *window, const char * const input);
static char * _theme_autocomplete(ProfWin *window, const char * const input);
static char * _autoaway_autocomplete(ProfWin *window, const char * const input);
static char * _autoconnect_autocomplete(ProfWin *window, const char * const input);
static char * _account_autocomplete(ProfWin *window, const char * const input);
static char * _who_autocomplete(ProfWin *window, const char * const input);
static char * _roster_autocomplete(ProfWin *window, const char * const input);
static char * _group_autocomplete(ProfWin *window, const char * const input);
static char * _bookmark_autocomplete(ProfWin *window, const char * const input);
static char * _otr_autocomplete(ProfWin *window, const char * const input);
static char * _pgp_autocomplete(ProfWin *window, const char * const input);
static char * _connect_autocomplete(ProfWin *window, const char * const input);
static char * _statuses_autocomplete(ProfWin *window, const char * const input);
static char * _alias_autocomplete(ProfWin *window, const char * const input);
static char * _join_autocomplete(ProfWin *window, const char * const input);
static char * _log_autocomplete(ProfWin *window, const char * const input);
static char * _form_autocomplete(ProfWin *window, const char * const input);
static char * _form_field_autocomplete(ProfWin *window, const char * const input);
static char * _occupants_autocomplete(ProfWin *window, const char * const input);
static char * _kick_autocomplete(ProfWin *window, const char * const input);
static char * _ban_autocomplete(ProfWin *window, const char * const input);
static char * _affiliation_autocomplete(ProfWin *window, const char * const input);
static char * _role_autocomplete(ProfWin *window, const char * const input);
static char * _resource_autocomplete(ProfWin *window, const char * const input);
static char * _titlebar_autocomplete(ProfWin *window, const char * const input);
static char * _inpblock_autocomplete(ProfWin *window, const char * const input);
static char * _time_autocomplete(ProfWin *window, const char * const input);
static char * _receipts_autocomplete(ProfWin *window, const char * const input);

GHashTable *commands = NULL;

#define END_ARGS { NULL, NULL }



/*
 * Command list
 */
static struct cmd_t command_defs[] =
{
    // NEW STYLE
    { "/help",
        cmd_help, parse_args, 0, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/help [<area>|<command>]",
            NULL
        },
            "Help on using Profanity. Passing no arguments list help areas.",
        {
            { "<area>",    "Summary help for commands in a certain area of functionality." },
            { "<command>", "Full help for a specific command, for example '/help connect'." },
            END_ARGS },
        {
            "/help commands",
            "/help presence",
            "/help who",
            NULL } }
        },

    { "/about",
        cmd_about, parse_args, 0, 0, NULL,
        { NULL, NULL, { NULL },
        {
            "/about",
            NULL
        },
            "Show version and license information.",
        {
            END_ARGS },
        {
            NULL } }
        },

    { "/connect",
        cmd_connect, parse_args, 0, 5, NULL,
        { NULL, NULL, { NULL },
        {
            "/connect [<account>]",
            "/connect <account> [server <server>] [port <port>]",
            NULL
        },
            "Login to a chat service. "
            "If no account is specified, the default is used if one is configured. "
            "A local account is created with the JID as it's name if it doesn't already exist.",
        {
            { "<account>",         "The local account you wish to connect with, or a JID if connecting for the first time." },
            { "server <server>",   "Supply a server if it is different to the domain part of your JID." },
            { "port <port>",       "The port to use if different to the default (5222, or 5223 for SSL)." },
            END_ARGS },
        {
            "/connect",
            "/connect myuser@gmail.com",
            "/connect myuser@mycompany.com server talk.google.com",
            "/connect bob@someplace port 5678",
            "/connect me@chatty server chatty.com port 5443",
            NULL } }
        },

    { "/disconnect",
        cmd_disconnect, parse_args, 0, 0, NULL,
        { NULL, NULL, { NULL },
        {
            "/disconnect",
            NULL
        },
            "Disconnect from the current chat service.",
        {
            END_ARGS },
        {
            NULL } }
        },

    { "/msg",
        cmd_msg, parse_args_with_freetext, 1, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/msg <contact> [<message>]",
            "/msg <nick> [<message>]",
            NULL
        },
            "Send a one to one chat message, or a private message to a chat room occupant. "
            "If the message is omitted, a new chat window will be opened without sending a message. "
            "Use quotes if the nickname includes spaces.",
        {
            { "<contact>",             "Open chat window with contact, by JID or nickname." },
            { "<contact> [<message>]", "Send message to contact, by JID or nickname." },
            { "<nick>",                "Open private chat window with chat room occupant." },
            { "<nick> [<message>]",    "Send a private message to a chat room occupant." },
            END_ARGS },
        {
            "/msg myfriend@server.com Hey, here's a message!",
            "/msg otherfriend@server.com",
            "/msg Bob Here is a private message",
            "/msg \"My Friend\" Hi, how are you?",
            NULL } }
        },

    { "/roster",
        cmd_roster, parse_args_with_freetext, 0, 3, NULL,
        { NULL, NULL, { NULL },
        {
            "/roster",
            "/roster online",
            "/roster show [offline|resource|empty]",
            "/roster hide [offline|resource|empty]",
            "/roster by group|presence|none",
            "/roster size <percent>",
            "/roster add <jid> [<nick>]",
            "/roster remove <jid>",
            "/roster remove_all contacts",
            "/roster nick <jid> <nick>",
            "/roster clearnick <jid>",
            NULL
        },
          "Manage your roster, and roster display settings. "
          "Passing no arguments lists all contacts in your roster.",
        {
            { "online",              "Show all online contacts in your roster." },
            { "show",                "Show the roster panel." },
            { "show offline",        "Show offline contacts in the roster panel." },
            { "show resource",       "Show contact's connected resources in the roster panel." },
            { "show empty",          "When grouping by presence, show empty presence groups." },
            { "show empty",          "When grouping by presence, show empty presence groups." },
            { "hide",                "Hide the roster panel." },
            { "hide offline",        "Hide offline contacts in the roster panel." },
            { "hide resource",       "Hide contact's connected resources in the roster panel." },
            { "hide empty",          "When grouping by presence, hide empty presence groups." },
            { "by group",            "Group contacts in the roster panel by roster group." },
            { "by presence",         "Group contacts in the roster panel by presence." },
            { "by none",             "No grouping in the roster panel." },
            { "size <precent>",      "Percentage of the screen taken up by the roster (1-99)." },
            { "add <jid> [<nick>]",  "Add a new item to the roster." },
            { "remove <jid>",        "Removes an item from the roster." },
            { "remove_all contacts", "Remove all items from roster." },
            { "nick <jid> <nick>",   "Change a contacts nickname." },
            { "clearnick <jid>",     "Removes the current nickname." },
            END_ARGS },
        {
            "/roster",
            "/roster add someone@contacts.org",
            "/roster add someone@contacts.org Buddy",
            "/roster remove someone@contacts.org",
            "/roster nick myfriend@chat.org My Friend",
            "/roster clearnick kai@server.com",
            "/roster size 15",
            NULL } }
        },

    { "/group",
        cmd_group, parse_args_with_freetext, 0, 3, NULL,
        { NULL, NULL, { NULL },
        {
            "/group",
            "/group show <group>",
            "/group remove <group> <contact>",
            NULL
        },
          "View, add to, and remove from roster groups. "
          "Passing no argument will list all roster groups.",
        {
            { "show <group>",             "List all roster items a group." },
            { "add <group> <contact>",    "Add a contact to a group." },
            { "remove <group> <contact>", "Remove a contact from a group." },
            END_ARGS },
        {
            "/group",
            "/group show friends",
            "/group add friends newfriend@server.org",
            "/group add family Brother",
            "/group remove colleagues boss@work.com",
            NULL } }
        },

    { "/info",
        cmd_info, parse_args, 0, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/info",
            "/info <contact>|<nick>",
            NULL
        },
          "Show information about a contact, room, or room member. "
          "Passing no argument in a chat window will use the current recipient. "
          "Passing no argument in a chat room will display information about the room.",
        {
            { "<contact>", "The contact you wish to view information about." },
            { "<nick>",    "When in a chat room, the occupant you wish to view information about." },
            END_ARGS },
        {
            "/info mybuddy@chat.server.org",
            "/info kai",
            NULL } }
        },

    { "/caps",
        cmd_caps, parse_args, 0, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/caps",
            "/caps <fulljid>|<nick>",
            NULL
        },
            "Find out a contacts, or room members client software capabilities. "
            "If in private chat initiated from a chat room, no parameter is required.",
        {
            { "<fulljid>", "If in the console or a chat window, the full JID for which you wish to see capabilities." },
            { "<nick>",    "If in a chat room, nickname for which you wish to see capabilities." },
            END_ARGS },
        {
            "/caps mybuddy@chat.server.org/laptop",
            "/caps mybuddy@chat.server.org/phone",
            "/caps bruce",
            NULL } }
        },

    { "/software",
        cmd_software, parse_args, 0, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/software",
            "/software <fulljid>|<nick>",
            NULL
        },
          "Find out a contact, or room members software version information. "
          "If in private chat initiated from a chat room, no parameter is required. "
          "If the contact's software does not support software version requests, nothing will be displayed.",
        {
            { "<fulljid>", "If in the console or a chat window, the full JID for which you wish to see software information." },
            { "<nick>",    "If in a chat room, nickname for which you wish to see software information." },
            END_ARGS },
        {
            "/software mybuddy@chat.server.org/laptop",
            "/software mybuddy@chat.server.org/phone",
            "/software bruce",
            NULL } }
        },

    { "/status",
        cmd_status, parse_args, 0, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/status",
            "/status <contact>|<nick>",
            NULL
        },
          "Find out a contact, or room members presence information. "
          "If in a chat window the parameter is not required, the current recipient will be used.",
        {
            { "<contact>", "The contact who's presence you which to see." },
            { "<nick>",    "If in a chat room, the occupant who's presence you wish to see." },
            END_ARGS },
        {
            "/status buddy@server.com",
            "/status jon",
            NULL } }
        },

    { "/resource",
        cmd_resource, parse_args, 1, 2, &cons_resource_setting,
        { NULL, NULL, { NULL },
        {
            "/resource set <resource>",
            "/resource off",
            "/resource title on|off",
            "/resource message on|off",
            NULL
        },
            "Override chat session resource, and manage resource display settings.",
        {
            { "set <resource>", "Set the resource to which messages will be sent." },
            { "off",            "Let the server choose which resource to route messages to." },
            { "title on|off",   "Show or hide the current resource in the titlebar." },
            { "message on|off", "Show or hide the resource when showing an incoming message." },
            END_ARGS },
        {
            NULL } }
        },

    { "/join",
        cmd_join, parse_args, 0, 5, NULL,
        { NULL, NULL, { NULL },
        {
            "/join",
            "/join <room> [nick <nick>] [password <password>]",
            NULL
        },
            "Join a chat room at the conference server. "
            "If no room is supplied, a generated name will be used with the format private-chat-[UUID]. "
            "If the domain part is not included in the room name, the account preference 'muc.service' will be used. "
            "If no nickname is specified the account preference 'muc.nick' will be used which by default is the localpart of your JID. "
            "If the room doesn't exist, and the server allows it, a new one will be created.",
        {
            { "<room>",                "The chat room to join." },
            { "nick <nick>",         "Nickname to use in the room." },
            { "password <password>", "Password if the room requires one." },
            END_ARGS },
        {
            "/join",
            "/join jdev@conference.jabber.org",
            "/join jdev@conference.jabber.org nick mynick",
            "/join private@conference.jabber.org nick mynick password mypassword",
            "/join jdev",
            NULL } }
        },

    { "/leave",
        cmd_leave, parse_args, 0, 0, NULL,
        { NULL, NULL, { NULL },
        {
            "/leave",
            NULL
        },
            "Leave the current chat room.",
        {
            END_ARGS },
        {
            NULL } }
        },

    { "/invite",
        cmd_invite, parse_args_with_freetext, 1, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/invite <contact> [<message>]",
            NULL
        },
            "Send an invite to a contact for the current chat room.",
        {
            { "<contact>", "The contact you wish to invite." },
            { "<message>", "An optional message to send with the invite." },
            END_ARGS },
        {
            NULL } }
        },

    { "/invites",
        cmd_invites, parse_args_with_freetext, 0, 0, NULL,
        { NULL, NULL, { NULL },
        {
            "/invites",
            NULL
        },
            "Show all rooms that you have been invited to, and not accepted or declined.",
        {
            END_ARGS },
        {
            NULL } }
        },

    { "/decline",
        cmd_decline, parse_args_with_freetext, 1, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/decline <room>",
            NULL
        },
            "Decline a chat room invitation.",
        {
            { "<room>", "The room for the invite you wish to decline." },
            END_ARGS },
        {
            NULL } }
        },

    { "/room",
        cmd_room, parse_args, 1, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/room accept|destroy|config",
            NULL
        },
            "Chat room configuration.",
        {
            { "accept",  "Accept default room configuration." },
            { "destroy", "Reject default room configuration, and destroy the room." },
            { "config",  "Edit room configuration." },
            END_ARGS },
        {
            NULL } }
        },

    { "/kick",
        cmd_kick, parse_args_with_freetext, 1, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/kick <nick> [<reason>]",
            NULL
        },
            "Kick occupant from chat room.",
        {
            { "<nick>",   "Nickname of the occupant to kick from the room." },
            { "<reason>", "Optional reason for kicking the occupant." },
            END_ARGS },
        {
            NULL } }
        },

    { "/ban",
        cmd_ban, parse_args_with_freetext, 1, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/ban <jid> [<reason>]",
            NULL
        },
            "Ban user from chat room.",
        {
            { "<jid>",    "Bare JID of the user to ban from the room." },
            { "<reason>", "Optional reason for banning the user." },
            END_ARGS },
        {
            NULL } }
        },

    { "/subject",
        cmd_subject, parse_args_with_freetext, 0, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/subject set <subject>",
            "/subject clear",
            NULL
        },
            "Set or clear room subject.",
        {
            { "set <subject>", "Set the room subject." },
            { "clear",         "Clear the room subject." },
            END_ARGS },
        {
            NULL } }
        },

    { "/affiliation",
        cmd_affiliation, parse_args_with_freetext, 1, 4, NULL,
        { NULL, NULL, { NULL },
        {
            "/affiliation set <affiliation> <jid> [<reason>]",
            "/list [<affiliation>]",
            NULL
        },
            "Manage room affiliations. "
            "Affiliation may be one of owner, admin, member, outcast or none.",
        {
            { "set <affiliation> <jid> [<reason>]", "Set the affiliation of user with jid, with an optional reason." },
            { "list [<affiliation>]",               "List all users with the specified affiliation, or all if none specified." },
            END_ARGS },
        {
            NULL } }
        },

    { "/role",
        cmd_role, parse_args_with_freetext, 1, 4, NULL,
        { NULL, NULL, { NULL },
        {
            "/role set <role> <nick> [<reason>]",
            "/list [<role>]",
            NULL
        },
            "Manage room roles. "
            "Role may be one of moderator, participant, visitor or none.",
        {
            { "set <role> <nick> [<reason>]", "Set the role of occupant with nick, with an optional reason." },
            { "list [<role>]",                "List all occupants with the specified role, or all if none specified." },
            END_ARGS },
        {
            NULL } }
        },

    { "/occupants",
        cmd_occupants, parse_args, 1, 3, cons_occupants_setting,
        { NULL, NULL, { NULL },
        {
            "/occupants show|hide [jid]",
            "/occupants default show|hide [jid]",
            "/occupants size [<percent>]",
            NULL
        },
            "Show or hide room occupants, and occupants panel display settings.",
        {
            { "show",                  "Show the occupants panel in current room." },
            { "hide",                  "Hide the occupants panel in current room." },
            { "show jid",              "Show jid in the occupants panel in current room." },
            { "hide jid",              "Hide jid in the occupants panel in current room." },
            { "default show|hide",     "Whether occupants are shown by default in new rooms." },
            { "default show|hide jid", "Whether occupants jids are shown by default in new rooms." },
            { "size <percent>",        "Percentage of the screen taken by the occupants list in rooms (1-99)." },
            END_ARGS },
        {
            NULL } }
        },

    { "/form",
        cmd_form, parse_args, 1, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/form show",
            "/form submit",
            "/form cancel",
            "/form help [<tag>]",
            NULL
        },
            "Form configuration.",
        {
            { "show",         "Show the current form." },
            { "submit",       "Submit the current form." },
            { "cancel",       "Cancel changes to the current form." },
            { "help [<tag>]", "Display help for form, or a specific field." },
            END_ARGS },
        {
            NULL } }
        },

    { "/rooms",
        cmd_rooms, parse_args, 0, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/rooms [<service>]",
            NULL
        },
            "List the chat rooms available at the specified conference service. "
            "If no argument is supplied, the account preference 'muc.service' is used, 'conference.<domain-part>' by default.",
        {
            { "<service>", "The conference service to query." },
            END_ARGS },
        {
            "/rooms conference.jabber.org",
            NULL } }
        },

    { "/bookmark",
        cmd_bookmark, parse_args, 0, 8, NULL,
        { NULL, NULL, { NULL },
        {
            "/bookmark",
            "/bookmark list",
            "/bookmark add <room> [nick <nick>] [password <password>] [autojoin on|off]",
            "/bookmark update <room> [nick <nick>] [password <password>] [autojoin on|off]",
            "/bookmark remove <room>",
            "/bookmark join <room>",
            NULL
        },
            "Manage bookmarks and join bookmarked rooms. "
            "In a chat room, no arguments will bookmark the current room, setting autojoin to \"on\".",
        {
            { "list", "List all bookmarks." },
            { "add <room>", "Add a bookmark." },
            { "remove <room>", "Remove a bookmark." },
            { "update <room>", "Update the properties associated with a bookmark." },
            { "nick <nick>", "Nickname used in the chat room." },
            { "password <password>", "Password if required, may be stored in plaintext on your server." },
            { "autojoin on|off", "Whether to join the room automatically on login." },
            { "join <room>", "Join room using the properties associated with the bookmark." },
            END_ARGS },
        {
            NULL } }
        },

    { "/disco",
        cmd_disco, parse_args, 1, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/disco info [<jid>]",
            "/disco items [<jid>]",
            NULL
        },
            "Find out information about an entities supported services. "
            "Calling with no arguments will query the server you are currently connected to.",
        {
            { "info [<jid>]", "List protocols and features supported by an entity." },
            { "items [<jid>]", "List items associated with an entity." },
            END_ARGS },
        {
            "/disco info",
            "/disco items myserver.org",
            "/disco items conference.jabber.org",
            "/disco info myfriend@server.com/laptop",
            NULL } }
        },

    { "/nick",
        cmd_nick, parse_args_with_freetext, 1, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/nick <nickname>",
            NULL
        },
            "Change your nickname in the current chat room.",
        {
            { "<nickname>", "Your new nickname." },
            END_ARGS },
        {
            NULL } }
        },

    { "/win",
        cmd_win, parse_args, 1, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/win <num>",
            NULL
        },
            "Move to the specified window.",
        {
            { "<num>", "Window number to display." },
            END_ARGS },
        {
            NULL } }
        },

    { "/wins",
        cmd_wins, parse_args, 0, 3, NULL,
        { NULL, NULL, { NULL },
        {
            "/wins tidy",
            "/wins prune",
            "/wins swap <source> <target>",
            NULL
        },
            "Manage windows. "
            "Passing no argument will list all currently active windows and information about their usage.",
        {
            { "tidy", "Move windows so there are no gaps." },
            { "prune", "Close all windows with no unread messages, and then tidy so there are no gaps." },
            { "swap <source> <target>", "Swap windows, target may be an empty position." },
            END_ARGS },
        {
            NULL } }
        },

    { "/sub",
        cmd_sub, parse_args, 1, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/sub request [<jid>]",
            "/sub allow [<jid>]",
            "/sub deny [<jid>]",
            "/sub show [<jid>]",
            "/sub sent",
            "/sub received",
            NULL
        },
            "Manage subscriptions to contact presence. "
            "If jid is omitted, the contact of the current window is used.",
        {
            { "request [<jid>]", "Send a subscription request to the user." },
            { "allow [<jid>]",   "Approve a contact's subscription request." },
            { "deny [<jid>]",    "Remove subscription for a contact, or deny a request." },
            { "show [<jid>]",    "Show subscription status for a contact." },
            { "sent",            "Show all sent subscription requests pending a response." },
            { "received",        "Show all received subscription requests awaiting your response." },
            END_ARGS },
        {
            "/sub request myfriend@jabber.org",
            "/sub allow myfriend@jabber.org",
            "/sub request",
            "/sub sent",
            NULL } }
        },

    { "/tiny",
        cmd_tiny, parse_args, 1, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/tiny <url>",
            NULL
        },
            "Send url as tinyurl in current chat.",
        {
            { "<url>", "The url to make tiny." },
            END_ARGS },
        {
            "Example: /tiny http://www.profanity.im",
            NULL } }
        },

    { "/who",
        cmd_who, parse_args, 0, 2, NULL,
        { NULL, NULL, { NULL },
        {
            "/who",
            "/who online|offline|away|dnd|xa|chat|available|unavailable|any [<group>]",
            "/who moderator|participant|visitor",
            "/who owner|admin|member",
            NULL
        },
            "Show contacts or room occupants with chosen status, role or affiliation",
        {
            { "offline|away|dnd|xa|chat", "Show contacts or room occupants with specified presence." },
            { "online", "Contacts that are online, chat, away, xa, dnd." },
            { "available", "Contacts that are available for chat - online, chat." },
            { "unavailable", "Contacts that are not available for chat - offline, away, xa, dnd." },
            { "any", "Contacts with any status (same as calling with no argument)." },
            { "<group>", "Filter the results by the specified roster group, not applicable in chat rooms." },
            { "moderator|participant|visitor", "Room occupants with the specified role." },
            { "owner|admin|member", "Room occupants with the specified affiliation." },
            END_ARGS },
        {
            "/who",
            "/who xa",
            "/who online friends",
            "/who any family",
            "/who participant",
            "/who admin",
            NULL } }
        },

    { "/close",
        cmd_close, parse_args, 0, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/close [<num>]",
            "/close all|read",
            NULL
        },
            "Close windows. "
            "Passing no argument closes the current window.",
        {
            { "<num>", "Close the specified window." },
            { "all", "Close all windows." },
            { "read", "Close all windows that have no unread messages." },
            END_ARGS },
        {
            NULL } }
        },

    { "/clear",
        cmd_clear, parse_args, 0, 0, NULL,
        { NULL, NULL, { NULL },
        {
            "/clear",
            NULL
        },
            "Clear the current window.",
        {
            END_ARGS },
        {
            NULL } }
        },

    { "/quit",
        cmd_quit, parse_args, 0, 0, NULL,
        { NULL, NULL, { NULL },
        {
            "/quit",
            NULL
        },
            "Logout of any current session, and quit Profanity.",
        {
            END_ARGS },
        {
            NULL } }
        },

    { "/privileges",
        cmd_privileges, parse_args, 1, 1, &cons_privileges_setting,
        { NULL, NULL, { NULL },
        {
            "/privileges on|off",
            NULL
        },
            "Group occupants panel by role, and show role information in chat rooms.",
        {
            { "on|off", "Enable or disable privilege information." },
            END_ARGS },
        {
            NULL } }
        },

    { "/beep",
        cmd_beep, parse_args, 1, 1, &cons_beep_setting,
        { NULL, NULL, { NULL },
        {
            "/beep on|off",
            NULL
        },
            "Switch the terminal bell on or off. "
            "The bell will sound when incoming messages are received. "
            "If the terminal does not support sounds, it may attempt to flash the screen instead.",
        {
            { "on|off", "Enable or disable terminal bell." },
            END_ARGS },
        {
            NULL } }
        },

    { "/encwarn",
        cmd_encwarn, parse_args, 1, 1, &cons_encwarn_setting,
        { NULL, NULL, { NULL },
        {
            "/encwarn on|off",
            NULL
        },
            "Titlebar encryption warning.",
        {
            { "on|off", "Enabled or disable the unencrypted warning message in the titlebar." },
            END_ARGS },
        {
            NULL } }
        },

    { "/presence",
        cmd_presence, parse_args, 1, 1, &cons_presence_setting,
        { NULL, NULL, { NULL },
        {
            "/presence on|off",
            NULL
        },
            "Show the contacts presence in the titlebar.",
        {
            { "on|off", "Switch display of the contacts presence in the titlebar on or off." },
            END_ARGS },
        {
            NULL } }
        },

    { "/wrap",
        cmd_wrap, parse_args, 1, 1, &cons_wrap_setting,
        { NULL, NULL, { NULL },
        {
            "/wrap on|off",
            NULL
        },
            "Word wrapping.",
        {
            { "on|off", "Enable or disable word wrapping in the main window." },
            END_ARGS },
        {
            NULL } }
        },

    { "/winstidy",
        cmd_winstidy, parse_args, 1, 1, &cons_winstidy_setting,
        { NULL, NULL, { NULL },
        {
            "/winstidy on|off",
            NULL
        },
            "Auto tidy windows, when a window is closed, windows will be moved to fill the gap.",
        {
            { "on|off", "Enable or disable auto window tidy." },
            END_ARGS },
        {
            NULL } }
        },

    { "/time",
        cmd_time, parse_args, 1, 3, &cons_time_setting,
        { NULL, NULL, { NULL },
        {
            "/time main set <format>",
            "/time main off",
            "/time statusbar set <format>",
            "/time statusbar off",
            NULL
        },
            "Configure time display preferences. "
            "Time formats are strings supported by g_date_time_format. "
            "See https://developer.gnome.org/glib/stable/glib-GDateTime.html#g-date-time-format for more details. "
            "Setting the format to an unsupported string, will display the string. "
            "If the format contains spaces, it must be surrounded with double quotes.",
        {
            { "main set <format>", "Change time format in main window." },
            { "main off", "Do not show time in main window." },
            { "statusbar set <format>", "Change time format in statusbar." },
            { "statusbar off", "Change time format in status bar." },
            END_ARGS },
        {
            "/time main set \"%d-%m-%y %H:%M\"",
            "/time main off",
            "/time statusbar set %H:%M",
            NULL } }
        },

    { "/inpblock",
        cmd_inpblock, parse_args, 2, 2, &cons_inpblock_setting,
        { NULL, NULL, { NULL },
        {
            "/inpblock timeout <millis>",
            "/inpblock dynamic on|off",
            NULL
        },
            "How long to wait for keyboard input before checking for new messages or checking for state changes such as 'idle'.",
        {
            { "timeout <millis>", "Time to wait (1-1000) in milliseconds before reading input from the terminal buffer, default: 1000." },
            { "dynamic on|off", "Start with 0 millis and dynamically increase up to timeout when no activity, default: on." },
            END_ARGS },
        {
            NULL } }
        },

    { "/notify",
        cmd_notify, parse_args, 2, 3, &cons_notify_setting,
        { NULL, NULL, { NULL },
        {
            "/notify message on|off",
            "/notify message current on|off",
            "/notify message text on|off",
            "/notify room on|off|mention",
            "/notify room current on|off",
            "/notify room text on|off",
            "/notify remind <seconds>",
            "/notify typing on|off",
            "/notify typing current on|off",
            "/notify invite on|off",
            "/notify sub on|off",
            NULL
        },
            "Settings for various kinds of desktop notifications.",
        {
            { "message on|off", "Notifications for regular chat messages." },
            { "message current on|off", "Whether messages in the current window trigger notifications." },
            { "message text on|off", "Show message text in regular message notifications." },
            { "room on|off|mention", "Notifications for chat room messages, mention triggers notifications only when your nick is mentioned." },
            { "room current on|off", "Whether chat room messages in the current window trigger notifications." },
            { "room text on|off", "Show message text in chat room message notifications." },
            { "remind <seconds>", "Notification reminder period for unread messages, use 0 to disable." },
            { "typing on|off", "Notifications when contacts are typing." },
            { "typing current on|off", "Whether typing notifications are triggered for the current window." },
            { "invite on|off", "Notifications for chat room invites." },
            { "sub on|off", "Notifications for subscription requests." },
            END_ARGS },
        {
            "/notify message on",
            "/notify message text on",
            "/notify room mention",
            "/notify room current off",
            "/notify room text off",
            "/notify remind 10",
            "/notify typing on",
            "/notify invite on",
            NULL } }
        },

    { "/flash",
        cmd_flash, parse_args, 1, 1, &cons_flash_setting,
        { NULL, NULL, { NULL },
        {
            "/flash on|off",
            NULL
        },
            "Make the terminal flash when incoming messages are received in another window. "
            "If the terminal doesn't support flashing, it may attempt to beep.",
        {
            { "on|off", "Enable or disable terminal flash." },
            END_ARGS },
        {
            NULL } }
        },

    { "/intype",
        cmd_intype, parse_args, 1, 1, &cons_intype_setting,
        { NULL, NULL, { NULL },
        {
            "/intype on|off",
            NULL
        },
            "Show when a contact is typing in the console, and in active message window.",
        {
            { "on|off", "Enable or disable contact typing messages." },
            END_ARGS },
        {
            NULL } }
        },


    { "/splash",
        cmd_splash, parse_args, 1, 1, &cons_splash_setting,
        { NULL, NULL, { NULL },
        {
            "/splash on|off",
            NULL
        },
            "Switch on or off the ascii logo on start up and when the /about command is called.",
        {
            { "on|off", "Enable or disable splash logo." },
            END_ARGS },
        {
            NULL } }
        },

    { "/autoconnect",
        cmd_autoconnect, parse_args, 1, 2, &cons_autoconnect_setting,
        { NULL, NULL, { NULL },
        {
            "/autoconnect set <account>",
            "/autoconnect off",
            NULL
        },
            "Enable or disable autoconnect on start up. "
            "The setting can be overridden by the -a (--account) command line option.",
        {
            { "set <account>", "Connect with account on start up." },
            { "off",           "Disable autoconnect." },
            END_ARGS },
        {
            "/autoconnect set jc@stuntteam.org",
            "/autoconnect off",
            NULL } }
        },

    { "/vercheck",
        cmd_vercheck, parse_args, 0, 1, NULL,
        { NULL, NULL, { NULL },
        {
            "/vercheck on|off",
            NULL
        },
            "Check for new versions when Profanity starts, and when the /about command is run.",
        {
            { "on|off", "Enable or disable the version check." },
            END_ARGS },
        {
            NULL } }
        },

    { "/titlebar",
        cmd_titlebar, parse_args, 2, 2, &cons_titlebar_setting,
        { NULL, NULL, { NULL },
        {
            "/titlebar show on|off",
            "/titlebar goodbye on|off",
            NULL
        },
            "Allow Profanity to modify the window title bar.",
        {
            { "show on|off",    "Show current logged in user, and unread messages as the window title." },
            { "goodbye on|off", "Show a message in the title when exiting profanity." },
            END_ARGS },
        {
            NULL } }
        },


    // OLD STYLE


    { "/alias",
        cmd_alias, parse_args_with_freetext, 1, 3, NULL,
        { "/alias add|remove|list [name value]", "Add your own command aliases.",
        { "/alias add|remove|list [name value]",
          "-----------------------------------",
          "Add, remove or show command aliases.",
          "",
          "add name value : Add a new command alias.",
          "remove name    : Remove a command alias.",
          "list           : List all aliases.",
          "",
          "Example: /alias add friends /who online friends",
          "Example: /alias add /q /quit",
          "Example: /alias a /away \"I'm in a meeting.\"",
          "Example: /alias remove q",
          "Example: /alias list",
          "",
          "The above aliases will be available as /friends and /a",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/chlog",
        cmd_chlog, parse_args, 1, 1, &cons_chlog_setting,
        { "/chlog on|off", "Chat logging to file.",
        { "/chlog on|off",
          "-------------",
          "Switch chat logging on or off.",
          "This setting will be enabled if /history is set to on.",
          "When disabling this option, /history will also be disabled.",
          "See the /grlog setting for enabling logging of chat room (groupchat) messages.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/grlog",
        cmd_grlog, parse_args, 1, 1, &cons_grlog_setting,
        { "/grlog on|off", "Chat logging of chat rooms to file.",
        { "/grlog on|off",
          "-------------",
          "Switch chat room logging on or off.",
          "See the /chlog setting for enabling logging of one to one chat.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/states",
        cmd_states, parse_args, 1, 1, &cons_states_setting,
        { "/states on|off", "Send chat states during a chat session.",
        { "/states on|off",
          "--------------",
          "Send chat state notifications during chat sessions.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/pgp",
        cmd_pgp, parse_args, 1, 3, NULL,
        { "/pgp command [args..]", "Open PGP commands.",
        { "/pgp command [args..]",
          "---------------------",
          "Open PGP commands.",
          "",
          "keys                 : List all keys.",
          "libver               : Show which version of the libgpgme library is being used.",
          "fps                  : Show known fingerprints.",
          "setkey contact keyid : Manually associate a key ID with a JID.",
          "start [contact]      : Start PGP encrypted chat, current contact will be used if not specified.",
          "end                  : End PGP encrypted chat with the current recipient.",
          "log on|off|redact    : PGP message logging, default: redact.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/otr",
        cmd_otr, parse_args, 1, 3, NULL,
        { "/otr command [args..]", "Off The Record encryption commands.",
        { "/otr command [args..]",
          "---------------------",
          "Off The Record encryption commands.",
          "",
          "gen                                : Generate your private key.",
          "myfp                               : Show your fingerprint.",
          "theirfp                            : Show contacts fingerprint.",
          "start [contact]                    : Start an OTR session with contact, or current recipient if omitted.",
          "end                                : End the current OTR session,",
          "trust                              : Indicate that you have verified the contact's fingerprint.",
          "untrust                            : Indicate the the contact's fingerprint is not verified,",
          "log on|off|redact                  : OTR message logging, default: redact.",
          "warn on|off                        : Show in the titlebar when unencrypted messaging is being used.",
          "libver                             : Show which version of the libotr library is being used.",
          "policy manual|opportunistic|always : Set the global OTR policy.",
          "secret [secret]                    : Verify a contacts identity using a shared secret.",
          "question [question] [answer]       : Verify a contacts identity using a question and expected answer.",
          "answer [answer]                    : Respond to a question answer verification request with your answer.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/outtype",
        cmd_outtype, parse_args, 1, 1, &cons_outtype_setting,
        { "/outtype on|off", "Send typing notification to recipient.",
        { "/outtype on|off",
          "---------------",
          "Send typing notifications, chat states (/states) will be enabled if this setting is set.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/gone",
        cmd_gone, parse_args, 1, 1, &cons_gone_setting,
        { "/gone minutes", "Send 'gone' state to recipient after a period.",
        { "/gone minutes",
          "-------------",
          "Send a 'gone' state to the recipient after the specified number of minutes.",
          "A value of 0 will disable sending this chat state.",
          "Chat states (/states) will be enabled if this setting is set.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/history",
        cmd_history, parse_args, 1, 1, &cons_history_setting,
        { "/history on|off", "Chat history in message windows.",
        { "/history on|off",
          "---------------",
          "Switch chat history on or off, /chlog will automatically be enabled when this setting is on.",
          "When history is enabled, previous messages are shown in chat windows.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/log",
        cmd_log, parse_args, 1, 2, &cons_log_setting,
        { "/log where|rotate|maxsize|shared [value]", "Manage system logging settings.",
        { "/log where|rotate|maxsize|shared [value]",
          "----------------------------------------",
          "Manage profanity logging settings.",
          "",
          "where         : Show the current log file location.",
          "rotate on|off : Rotate log, default on.",
          "maxsize bytes : With rotate enabled, specifies the max log size, defaults to 1048580 (1MB).",
          "shared on|off : Share logs between all instances, default: on.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/carbons",
      cmd_carbons, parse_args, 1, 1, &cons_carbons_setting,
      { "/carbons on|off", "Message carbons.",
      { "/carbons on|off",
        "---------------",
        "Enable or disable message carbons.",
        "The message carbons feature ensures that both sides of all conversations are shared with all the user's clients that implement this protocol.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/receipts",
      cmd_receipts, parse_args, 2, 2, &cons_receipts_setting,
      { "/receipts send|request on|off", "Message delivery receipts.",
      { "/receipts send|request on|off",
        "-----------------------------",
        "Enable or disable message delivery receipts. The interface will indicate when a message has been received.",
        "",
        "send on|off    : Enable or disable sending of delivery receipts.",
        "request on|off : Enable or disable sending of delivery receipt requests.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/reconnect",
        cmd_reconnect, parse_args, 1, 1, &cons_reconnect_setting,
        { "/reconnect seconds", "Set reconnect interval.",
        { "/reconnect seconds",
          "------------------",
          "Set the reconnect attempt interval in seconds for when the connection is lost.",
          "A value of 0 will switch off reconnect attempts.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/autoping",
        cmd_autoping, parse_args, 1, 1, &cons_autoping_setting,
        { "/autoping seconds", "Server ping interval.",
        { "/autoping seconds",
          "-----------------",
          "Set the number of seconds between server pings, so ensure connection kept alive.",
          "A value of 0 will switch off autopinging the server.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/ping",
        cmd_ping, parse_args, 0, 1, NULL,
        { "/ping [target]", "Send ping IQ request.",
        { "/ping [target]",
          "--------------",
          "Sends an IQ ping stanza to the specified target.",
          "If no target is supplied, your chat server will be pinged.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/autoaway",
        cmd_autoaway, parse_args_with_freetext, 2, 2, &cons_autoaway_setting,
        { "/autoaway mode|time|message|check value", "Set auto idle/away properties.",
        { "/autoaway mode|time|message|check value",
          "---------------------------------------",
          "Manage autoway properties.",
          "",
          "mode idle        : Sends idle time, status remains online.",
          "mode away        : Sends an away presence.",
          "mode off         : Disabled (default).",
          "time minutes     : Number of minutes before the presence change is sent, default: 15.",
          "message text|off : Optional message to send with the presence change, default: off (disabled).",
          "check on|off     : When enabled, checks for activity and sends online presence, default: on.",
          "",
          "Example: /autoaway mode idle",
          "Example: /autoaway time 30",
          "Example: /autoaway message I'm not really doing much",
          "Example: /autoaway check off",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/priority",
        cmd_priority, parse_args, 1, 1, &cons_priority_setting,
        { "/priority value", "Set priority for the current account.",
        { "/priority value",
          "---------------",
          "Set priority for the current account.",
          "",
          "value : Number between -128 and 127, default: 0.",
          "",
          "See the /account command for specific priority settings per presence status.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/account",
        cmd_account, parse_args, 0, 4, NULL,
        { "/account [command] [account] [property] [value]", "Manage accounts.",
        { "/account [command] [account] [property] [value]",
          "-----------------------------------------------",
          "Commands for creating and managing accounts.",
          "",
          "list                         : List all accounts.",
          "show account                 : Show information about an account.",
          "enable account               : Enable the account, it will be used for autocomplete.",
          "disable account              : Disable the account.",
          "default [set|off] [account]  : Set the default account.",
          "add account                  : Create a new account.",
          "remove account               : Remove an account.",
          "rename account newname       : Rename account to newname.",
          "set account property value   : Set 'property' of 'account' to 'value'.",
          "clear account property value : Clear 'property' of 'account'.",
          "",
          "Account properties.",
          "",
          "jid                     : The Jabber ID of the account, account name will be used if not set.",
          "server                  : The chat server, if different to the domainpart of the JID.",
          "port                    : The port used for connecting if not the default (5222, or 5223 for SSL).",
          "status                  : The presence status to use on login, use 'last' to use your last status before logging out.",
          "online|chat|away|xa|dnd : Priority for the specified presence.",
          "resource                : The resource to be used.",
          "password                : Password for the account, note this is currently stored in plaintext if set.",
          "eval_password           : Shell command evaluated to retrieve password for the account. Can be used to retrieve password from keyring.",
          "muc                     : The default MUC chat service to use.",
          "nick                    : The default nickname to use when joining chat rooms.",
          "otr                     : Override global OTR policy for this account: manual, opportunistic or always.",
          "",
          "Example: /account add me",
          "Example: /account set me jid me@chatty",
          "Example: /account set me server talk.chat.com",
          "Example: /account set me port 5111",
          "Example: /account set me muc chatservice.mycompany.com",
          "Example: /account set me nick dennis",
          "Example: /account set me status dnd",
          "Example: /account set me dnd -1",
          "Example: /account rename me gtalk",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/prefs",
        cmd_prefs, parse_args, 0, 1, NULL,
        { "/prefs [ui|desktop|chat|log|conn|presence]", "Show configuration.",
        { "/prefs [ui|desktop|chat|log|conn|presence]",
          "------------------------------------------",
          "Show preferences for different areas of functionality.",
          "",
          "ui       : User interface preferences.",
          "desktop  : Desktop notification preferences.",
          "chat     : Chat state preferences.",
          "log      : Logging preferences.",
          "conn     : Connection handling preferences.",
          "presence : Chat presence preferences.",
          "",
          "No argument shows all preferences.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/theme",
        cmd_theme, parse_args, 1, 2, &cons_theme_setting,
        { "/theme list|load|colours [theme-name]", "Change colour theme.",
        { "/theme list|load|colours [theme-name]",
          "-------------------------------------",
          "Load a theme, includes colours and UI options.",
          "",
          "list            : List all available themes.",
          "load theme-name : Load the named theme. 'default' will reset to the default theme.",
          "colours         : Show the colour values as rendered by the terminal.",
          "",
          "Example: /theme list",
          "Example: /theme load mycooltheme",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/statuses",
        cmd_statuses, parse_args, 2, 2, &cons_statuses_setting,
        { "/statuses console|chat|muc setting", "Set preferences for presence change messages.",
        { "/statuses console|chat|muc setting",
          "----------------------------------",
          "Configure which presence changes are displayed in various windows.",
          "",
          "console : Configure what is displayed in the console window.",
          "chat    : Configure what is displayed in chat windows.",
          "muc     : Configure what is displayed in chat room windows.",
          "",
          "Available options are:",
          "",
          "all    : Show all presence changes.",
          "online : Show only online/offline changes.",
          "none   : Don't show any presence changes.",
          "",
          "The default is 'all' for all windows.",
          "",
          "Example: /statuses console none",
          "Example: /statuses chat online",
          "Example: /statuses muc all",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/xmlconsole",
        cmd_xmlconsole, parse_args, 0, 0, NULL,
        { "/xmlconsole", "Open the XML console",
        { "/xmlconsole",
          "-----------",
          "Open the XML console to view incoming and outgoing XMPP traffic.",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/away",
        cmd_away, parse_args_with_freetext, 0, 1, NULL,
        { "/away [message]", "Set status to away.",
        { "/away [message]",
          "---------------",
          "Set your status to 'away' with the optional message.",
          "",
          "Example: /away Gone for lunch",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/chat",
        cmd_chat, parse_args_with_freetext, 0, 1, NULL,
        { "/chat [message]", "Set status to chat (available for chat).",
        { "/chat [message]",
          "---------------",
          "Set your status to 'chat', meaning 'available for chat', with the optional message.",
          "",
          "Example: /chat Please talk to me!",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/dnd",
        cmd_dnd, parse_args_with_freetext, 0, 1, NULL,
        { "/dnd [message]", "Set status to dnd (do not disturb).",
        { "/dnd [message]",
          "--------------",
          "Set your status to 'dnd', meaning 'do not disturb', with the optional message.",
          "",
          "Example: /dnd I'm in the zone",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/online",
        cmd_online, parse_args_with_freetext, 0, 1, NULL,
        { "/online [message]", "Set status to online.",
        { "/online [message]",
          "-----------------",
          "Set your status to 'online' with the optional message.",
          "",
          "Example: /online Up the Irons!",
          NULL,
          NULL, NULL, NULL, NULL } } },

    { "/xa",
        cmd_xa, parse_args_with_freetext, 0, 1, NULL,
        { "/xa [message]", "Set status to xa (extended away).",
        { "/xa [message]",
          "-------------",
          "Set your status to 'xa', meaning 'extended away', with the optional message.",
          "",
          "Example: /xa This meeting is going to be a long one",
          NULL,
          NULL, NULL, NULL, NULL } } },
};

static Autocomplete commands_ac;
static Autocomplete who_room_ac;
static Autocomplete who_roster_ac;
static Autocomplete help_ac;
static Autocomplete notify_ac;
static Autocomplete notify_room_ac;
static Autocomplete notify_message_ac;
static Autocomplete notify_typing_ac;
static Autocomplete prefs_ac;
static Autocomplete sub_ac;
static Autocomplete log_ac;
static Autocomplete autoaway_ac;
static Autocomplete autoaway_mode_ac;
static Autocomplete autoconnect_ac;
static Autocomplete titlebar_ac;
static Autocomplete theme_ac;
static Autocomplete theme_load_ac;
static Autocomplete account_ac;
static Autocomplete account_set_ac;
static Autocomplete account_clear_ac;
static Autocomplete account_default_ac;
static Autocomplete disco_ac;
static Autocomplete close_ac;
static Autocomplete wins_ac;
static Autocomplete roster_ac;
static Autocomplete roster_option_ac;
static Autocomplete roster_by_ac;
static Autocomplete roster_remove_all_ac;
static Autocomplete group_ac;
static Autocomplete bookmark_ac;
static Autocomplete bookmark_property_ac;
static Autocomplete otr_ac;
static Autocomplete otr_log_ac;
static Autocomplete otr_policy_ac;
static Autocomplete connect_property_ac;
static Autocomplete statuses_ac;
static Autocomplete statuses_setting_ac;
static Autocomplete alias_ac;
static Autocomplete aliases_ac;
static Autocomplete join_property_ac;
static Autocomplete room_ac;
static Autocomplete affiliation_ac;
static Autocomplete role_ac;
static Autocomplete privilege_cmd_ac;
static Autocomplete subject_ac;
static Autocomplete form_ac;
static Autocomplete form_field_multi_ac;
static Autocomplete occupants_ac;
static Autocomplete occupants_default_ac;
static Autocomplete occupants_show_ac;
static Autocomplete time_ac;
static Autocomplete time_format_ac;
static Autocomplete resource_ac;
static Autocomplete inpblock_ac;
static Autocomplete receipts_ac;
static Autocomplete pgp_ac;
static Autocomplete pgp_log_ac;

/*
 * Initialise command autocompleter and history
 */
void
cmd_init(void)
{
    log_info("Initialising commands");

    commands_ac = autocomplete_new();
    aliases_ac = autocomplete_new();

    help_ac = autocomplete_new();
    autocomplete_add(help_ac, "commands");
    autocomplete_add(help_ac, "basic");
    autocomplete_add(help_ac, "chatting");
    autocomplete_add(help_ac, "groupchat");
    autocomplete_add(help_ac, "presences");
    autocomplete_add(help_ac, "contacts");
    autocomplete_add(help_ac, "service");
    autocomplete_add(help_ac, "settings");
    autocomplete_add(help_ac, "navigation");

    // load command defs into hash table
    commands = g_hash_table_new(g_str_hash, g_str_equal);
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(command_defs); i++) {
        Command *pcmd = command_defs+i;

        // add to hash
        g_hash_table_insert(commands, pcmd->cmd, pcmd);

        // add to commands and help autocompleters
        autocomplete_add(commands_ac, pcmd->cmd);
        autocomplete_add(help_ac, pcmd->cmd+1);
    }

    // load aliases
    GList *aliases = prefs_get_aliases();
    GList *curr = aliases;
    while (curr) {
        ProfAlias *alias = curr->data;
        GString *ac_alias = g_string_new("/");
        g_string_append(ac_alias, alias->name);
        autocomplete_add(commands_ac, ac_alias->str);
        autocomplete_add(aliases_ac, alias->name);
        g_string_free(ac_alias, TRUE);
        curr = g_list_next(curr);
    }
    prefs_free_aliases(aliases);

    prefs_ac = autocomplete_new();
    autocomplete_add(prefs_ac, "ui");
    autocomplete_add(prefs_ac, "desktop");
    autocomplete_add(prefs_ac, "chat");
    autocomplete_add(prefs_ac, "log");
    autocomplete_add(prefs_ac, "conn");
    autocomplete_add(prefs_ac, "presence");
    autocomplete_add(prefs_ac, "otr");
    autocomplete_add(prefs_ac, "pgp");

    notify_ac = autocomplete_new();
    autocomplete_add(notify_ac, "message");
    autocomplete_add(notify_ac, "room");
    autocomplete_add(notify_ac, "typing");
    autocomplete_add(notify_ac, "remind");
    autocomplete_add(notify_ac, "invite");
    autocomplete_add(notify_ac, "sub");

    notify_message_ac = autocomplete_new();
    autocomplete_add(notify_message_ac, "on");
    autocomplete_add(notify_message_ac, "off");
    autocomplete_add(notify_message_ac, "current");
    autocomplete_add(notify_message_ac, "text");

    notify_room_ac = autocomplete_new();
    autocomplete_add(notify_room_ac, "on");
    autocomplete_add(notify_room_ac, "off");
    autocomplete_add(notify_room_ac, "mention");
    autocomplete_add(notify_room_ac, "current");
    autocomplete_add(notify_room_ac, "text");

    notify_typing_ac = autocomplete_new();
    autocomplete_add(notify_typing_ac, "on");
    autocomplete_add(notify_typing_ac, "off");
    autocomplete_add(notify_typing_ac, "current");

    sub_ac = autocomplete_new();
    autocomplete_add(sub_ac, "request");
    autocomplete_add(sub_ac, "allow");
    autocomplete_add(sub_ac, "deny");
    autocomplete_add(sub_ac, "show");
    autocomplete_add(sub_ac, "sent");
    autocomplete_add(sub_ac, "received");

    titlebar_ac = autocomplete_new();
    autocomplete_add(titlebar_ac, "show");
    autocomplete_add(titlebar_ac, "goodbye");

    log_ac = autocomplete_new();
    autocomplete_add(log_ac, "maxsize");
    autocomplete_add(log_ac, "rotate");
    autocomplete_add(log_ac, "shared");
    autocomplete_add(log_ac, "where");

    autoaway_ac = autocomplete_new();
    autocomplete_add(autoaway_ac, "mode");
    autocomplete_add(autoaway_ac, "time");
    autocomplete_add(autoaway_ac, "message");
    autocomplete_add(autoaway_ac, "check");

    autoaway_mode_ac = autocomplete_new();
    autocomplete_add(autoaway_mode_ac, "away");
    autocomplete_add(autoaway_mode_ac, "idle");
    autocomplete_add(autoaway_mode_ac, "off");

    autoconnect_ac = autocomplete_new();
    autocomplete_add(autoconnect_ac, "set");
    autocomplete_add(autoconnect_ac, "off");

    theme_ac = autocomplete_new();
    autocomplete_add(theme_ac, "load");
    autocomplete_add(theme_ac, "list");
    autocomplete_add(theme_ac, "colours");

    disco_ac = autocomplete_new();
    autocomplete_add(disco_ac, "info");
    autocomplete_add(disco_ac, "items");

    account_ac = autocomplete_new();
    autocomplete_add(account_ac, "list");
    autocomplete_add(account_ac, "show");
    autocomplete_add(account_ac, "add");
    autocomplete_add(account_ac, "remove");
    autocomplete_add(account_ac, "enable");
    autocomplete_add(account_ac, "disable");
    autocomplete_add(account_ac, "default");
    autocomplete_add(account_ac, "rename");
    autocomplete_add(account_ac, "set");
    autocomplete_add(account_ac, "clear");

    account_set_ac = autocomplete_new();
    autocomplete_add(account_set_ac, "jid");
    autocomplete_add(account_set_ac, "server");
    autocomplete_add(account_set_ac, "port");
    autocomplete_add(account_set_ac, "status");
    autocomplete_add(account_set_ac, "online");
    autocomplete_add(account_set_ac, "chat");
    autocomplete_add(account_set_ac, "away");
    autocomplete_add(account_set_ac, "xa");
    autocomplete_add(account_set_ac, "dnd");
    autocomplete_add(account_set_ac, "resource");
    autocomplete_add(account_set_ac, "password");
    autocomplete_add(account_set_ac, "eval_password");
    autocomplete_add(account_set_ac, "muc");
    autocomplete_add(account_set_ac, "nick");
    autocomplete_add(account_set_ac, "otr");
    autocomplete_add(account_set_ac, "pgpkeyid");

    account_clear_ac = autocomplete_new();
    autocomplete_add(account_clear_ac, "password");
    autocomplete_add(account_clear_ac, "eval_password");
    autocomplete_add(account_clear_ac, "server");
    autocomplete_add(account_clear_ac, "port");
    autocomplete_add(account_clear_ac, "otr");
    autocomplete_add(account_clear_ac, "pgpkeyid");

    account_default_ac = autocomplete_new();
    autocomplete_add(account_default_ac, "set");
    autocomplete_add(account_default_ac, "off");

    close_ac = autocomplete_new();
    autocomplete_add(close_ac, "read");
    autocomplete_add(close_ac, "all");

    wins_ac = autocomplete_new();
    autocomplete_add(wins_ac, "prune");
    autocomplete_add(wins_ac, "tidy");
    autocomplete_add(wins_ac, "swap");

    roster_ac = autocomplete_new();
    autocomplete_add(roster_ac, "add");
    autocomplete_add(roster_ac, "online");
    autocomplete_add(roster_ac, "nick");
    autocomplete_add(roster_ac, "clearnick");
    autocomplete_add(roster_ac, "remove");
    autocomplete_add(roster_ac, "remove_all");
    autocomplete_add(roster_ac, "show");
    autocomplete_add(roster_ac, "hide");
    autocomplete_add(roster_ac, "by");
    autocomplete_add(roster_ac, "size");

    roster_option_ac = autocomplete_new();
    autocomplete_add(roster_option_ac, "offline");
    autocomplete_add(roster_option_ac, "resource");
    autocomplete_add(roster_option_ac, "empty");

    roster_by_ac = autocomplete_new();
    autocomplete_add(roster_by_ac, "group");
    autocomplete_add(roster_by_ac, "presence");
    autocomplete_add(roster_by_ac, "none");

    roster_remove_all_ac = autocomplete_new();
    autocomplete_add(roster_remove_all_ac, "contacts");

    group_ac = autocomplete_new();
    autocomplete_add(group_ac, "show");
    autocomplete_add(group_ac, "add");
    autocomplete_add(group_ac, "remove");

    theme_load_ac = NULL;

    who_roster_ac = autocomplete_new();
    autocomplete_add(who_roster_ac, "chat");
    autocomplete_add(who_roster_ac, "online");
    autocomplete_add(who_roster_ac, "away");
    autocomplete_add(who_roster_ac, "xa");
    autocomplete_add(who_roster_ac, "dnd");
    autocomplete_add(who_roster_ac, "offline");
    autocomplete_add(who_roster_ac, "available");
    autocomplete_add(who_roster_ac, "unavailable");
    autocomplete_add(who_roster_ac, "any");

    who_room_ac = autocomplete_new();
    autocomplete_add(who_room_ac, "chat");
    autocomplete_add(who_room_ac, "online");
    autocomplete_add(who_room_ac, "away");
    autocomplete_add(who_room_ac, "xa");
    autocomplete_add(who_room_ac, "dnd");
    autocomplete_add(who_room_ac, "available");
    autocomplete_add(who_room_ac, "unavailable");
    autocomplete_add(who_room_ac, "moderator");
    autocomplete_add(who_room_ac, "participant");
    autocomplete_add(who_room_ac, "visitor");
    autocomplete_add(who_room_ac, "owner");
    autocomplete_add(who_room_ac, "admin");
    autocomplete_add(who_room_ac, "member");

    bookmark_ac = autocomplete_new();
    autocomplete_add(bookmark_ac, "list");
    autocomplete_add(bookmark_ac, "add");
    autocomplete_add(bookmark_ac, "update");
    autocomplete_add(bookmark_ac, "remove");
    autocomplete_add(bookmark_ac, "join");

    bookmark_property_ac = autocomplete_new();
    autocomplete_add(bookmark_property_ac, "nick");
    autocomplete_add(bookmark_property_ac, "password");
    autocomplete_add(bookmark_property_ac, "autojoin");

    otr_ac = autocomplete_new();
    autocomplete_add(otr_ac, "gen");
    autocomplete_add(otr_ac, "start");
    autocomplete_add(otr_ac, "end");
    autocomplete_add(otr_ac, "myfp");
    autocomplete_add(otr_ac, "theirfp");
    autocomplete_add(otr_ac, "trust");
    autocomplete_add(otr_ac, "untrust");
    autocomplete_add(otr_ac, "secret");
    autocomplete_add(otr_ac, "log");
    autocomplete_add(otr_ac, "libver");
    autocomplete_add(otr_ac, "policy");
    autocomplete_add(otr_ac, "question");
    autocomplete_add(otr_ac, "answer");

    otr_log_ac = autocomplete_new();
    autocomplete_add(otr_log_ac, "on");
    autocomplete_add(otr_log_ac, "off");
    autocomplete_add(otr_log_ac, "redact");

    otr_policy_ac = autocomplete_new();
    autocomplete_add(otr_policy_ac, "manual");
    autocomplete_add(otr_policy_ac, "opportunistic");
    autocomplete_add(otr_policy_ac, "always");

    connect_property_ac = autocomplete_new();
    autocomplete_add(connect_property_ac, "server");
    autocomplete_add(connect_property_ac, "port");

    join_property_ac = autocomplete_new();
    autocomplete_add(join_property_ac, "nick");
    autocomplete_add(join_property_ac, "password");

    statuses_ac = autocomplete_new();
    autocomplete_add(statuses_ac, "console");
    autocomplete_add(statuses_ac, "chat");
    autocomplete_add(statuses_ac, "muc");

    statuses_setting_ac = autocomplete_new();
    autocomplete_add(statuses_setting_ac, "all");
    autocomplete_add(statuses_setting_ac, "online");
    autocomplete_add(statuses_setting_ac, "none");

    alias_ac = autocomplete_new();
    autocomplete_add(alias_ac, "add");
    autocomplete_add(alias_ac, "remove");
    autocomplete_add(alias_ac, "list");

    room_ac = autocomplete_new();
    autocomplete_add(room_ac, "accept");
    autocomplete_add(room_ac, "destroy");
    autocomplete_add(room_ac, "config");

    affiliation_ac = autocomplete_new();
    autocomplete_add(affiliation_ac, "owner");
    autocomplete_add(affiliation_ac, "admin");
    autocomplete_add(affiliation_ac, "member");
    autocomplete_add(affiliation_ac, "none");
    autocomplete_add(affiliation_ac, "outcast");

    role_ac = autocomplete_new();
    autocomplete_add(role_ac, "moderator");
    autocomplete_add(role_ac, "participant");
    autocomplete_add(role_ac, "visitor");
    autocomplete_add(role_ac, "none");

    privilege_cmd_ac = autocomplete_new();
    autocomplete_add(privilege_cmd_ac, "list");
    autocomplete_add(privilege_cmd_ac, "set");

    subject_ac = autocomplete_new();
    autocomplete_add(subject_ac, "set");
    autocomplete_add(subject_ac, "clear");

    form_ac = autocomplete_new();
    autocomplete_add(form_ac, "submit");
    autocomplete_add(form_ac, "cancel");
    autocomplete_add(form_ac, "show");
    autocomplete_add(form_ac, "help");

    form_field_multi_ac = autocomplete_new();
    autocomplete_add(form_field_multi_ac, "add");
    autocomplete_add(form_field_multi_ac, "remove");

    occupants_ac = autocomplete_new();
    autocomplete_add(occupants_ac, "show");
    autocomplete_add(occupants_ac, "hide");
    autocomplete_add(occupants_ac, "default");
    autocomplete_add(occupants_ac, "size");

    occupants_default_ac = autocomplete_new();
    autocomplete_add(occupants_default_ac, "show");
    autocomplete_add(occupants_default_ac, "hide");

    occupants_show_ac = autocomplete_new();
    autocomplete_add(occupants_show_ac, "jid");

    time_ac = autocomplete_new();
    autocomplete_add(time_ac, "main");
    autocomplete_add(time_ac, "statusbar");

    time_format_ac = autocomplete_new();
    autocomplete_add(time_format_ac, "set");
    autocomplete_add(time_format_ac, "off");

    resource_ac = autocomplete_new();
    autocomplete_add(resource_ac, "set");
    autocomplete_add(resource_ac, "off");
    autocomplete_add(resource_ac, "title");
    autocomplete_add(resource_ac, "message");

    inpblock_ac = autocomplete_new();
    autocomplete_add(inpblock_ac, "timeout");
    autocomplete_add(inpblock_ac, "dynamic");

    receipts_ac = autocomplete_new();
    autocomplete_add(receipts_ac, "send");
    autocomplete_add(receipts_ac, "request");

    pgp_ac = autocomplete_new();
    autocomplete_add(pgp_ac, "keys");
    autocomplete_add(pgp_ac, "fps");
    autocomplete_add(pgp_ac, "setkey");
    autocomplete_add(pgp_ac, "libver");
    autocomplete_add(pgp_ac, "start");
    autocomplete_add(pgp_ac, "end");
    autocomplete_add(pgp_ac, "log");

    pgp_log_ac = autocomplete_new();
    autocomplete_add(pgp_log_ac, "on");
    autocomplete_add(pgp_log_ac, "off");
    autocomplete_add(pgp_log_ac, "redact");
}

void
cmd_uninit(void)
{
    autocomplete_free(commands_ac);
    autocomplete_free(who_room_ac);
    autocomplete_free(who_roster_ac);
    autocomplete_free(help_ac);
    autocomplete_free(notify_ac);
    autocomplete_free(notify_message_ac);
    autocomplete_free(notify_room_ac);
    autocomplete_free(notify_typing_ac);
    autocomplete_free(sub_ac);
    autocomplete_free(titlebar_ac);
    autocomplete_free(log_ac);
    autocomplete_free(prefs_ac);
    autocomplete_free(autoaway_ac);
    autocomplete_free(autoaway_mode_ac);
    autocomplete_free(autoconnect_ac);
    autocomplete_free(theme_ac);
    autocomplete_free(theme_load_ac);
    autocomplete_free(account_ac);
    autocomplete_free(account_set_ac);
    autocomplete_free(account_clear_ac);
    autocomplete_free(account_default_ac);
    autocomplete_free(disco_ac);
    autocomplete_free(close_ac);
    autocomplete_free(wins_ac);
    autocomplete_free(roster_ac);
    autocomplete_free(roster_option_ac);
    autocomplete_free(roster_by_ac);
    autocomplete_free(roster_remove_all_ac);
    autocomplete_free(group_ac);
    autocomplete_free(bookmark_ac);
    autocomplete_free(bookmark_property_ac);
    autocomplete_free(otr_ac);
    autocomplete_free(otr_log_ac);
    autocomplete_free(otr_policy_ac);
    autocomplete_free(connect_property_ac);
    autocomplete_free(statuses_ac);
    autocomplete_free(statuses_setting_ac);
    autocomplete_free(alias_ac);
    autocomplete_free(aliases_ac);
    autocomplete_free(join_property_ac);
    autocomplete_free(room_ac);
    autocomplete_free(affiliation_ac);
    autocomplete_free(role_ac);
    autocomplete_free(privilege_cmd_ac);
    autocomplete_free(subject_ac);
    autocomplete_free(form_ac);
    autocomplete_free(form_field_multi_ac);
    autocomplete_free(occupants_ac);
    autocomplete_free(occupants_default_ac);
    autocomplete_free(occupants_show_ac);
    autocomplete_free(time_ac);
    autocomplete_free(time_format_ac);
    autocomplete_free(resource_ac);
    autocomplete_free(inpblock_ac);
    autocomplete_free(receipts_ac);
    autocomplete_free(pgp_ac);
    autocomplete_free(pgp_log_ac);
}

gboolean
cmd_exists(char *cmd)
{
    if (commands_ac == NULL) {
        return FALSE;
    } else {
        return autocomplete_contains(commands_ac, cmd);
    }
}

void
cmd_autocomplete_add(char *value)
{
    if (commands_ac) {
        autocomplete_add(commands_ac, value);
    }
}

void
cmd_autocomplete_add_form_fields(DataForm *form)
{
    if (form == NULL) {
        return;
    }

    GSList *fields = autocomplete_create_list(form->tag_ac);
    GSList *curr_field = fields;
    while (curr_field) {
        GString *field_str = g_string_new("/");
        g_string_append(field_str, curr_field->data);
        cmd_autocomplete_add(field_str->str);
        g_string_free(field_str, TRUE);
        curr_field = g_slist_next(curr_field);
    }
    g_slist_free_full(fields, free);
}

void
cmd_autocomplete_remove_form_fields(DataForm *form)
{
    if (form == NULL) {
        return;
    }

    GSList *fields = autocomplete_create_list(form->tag_ac);
    GSList *curr_field = fields;
    while (curr_field) {
        GString *field_str = g_string_new("/");
        g_string_append(field_str, curr_field->data);
        cmd_autocomplete_remove(field_str->str);
        g_string_free(field_str, TRUE);
        curr_field = g_slist_next(curr_field);
    }
    g_slist_free_full(fields, free);
}

void
cmd_autocomplete_remove(char *value)
{
    if (commands_ac) {
        autocomplete_remove(commands_ac, value);
    }
}

void
cmd_alias_add(char *value)
{
    if (aliases_ac) {
        autocomplete_add(aliases_ac, value);
    }
}

void
cmd_alias_remove(char *value)
{
    if (aliases_ac) {
        autocomplete_remove(aliases_ac, value);
    }
}

// Command autocompletion functions
char*
cmd_autocomplete(ProfWin *window, const char * const input)
{
    // autocomplete command
    if ((strncmp(input, "/", 1) == 0) && (!str_contains(input, strlen(input), ' '))) {
        char *found = NULL;
        found = autocomplete_complete(commands_ac, input, TRUE);
        if (found) {
            return found;
        }

    // autocomplete parameters
    } else {
        char *found = _cmd_complete_parameters(window, input);
        if (found) {
            return found;
        }
    }

    return NULL;
}

void
cmd_reset_autocomplete(ProfWin *window)
{
    roster_reset_search_attempts();
    muc_invites_reset_ac();
    accounts_reset_all_search();
    accounts_reset_enabled_search();
    prefs_reset_boolean_choice();
    presence_reset_sub_request_search();
    autocomplete_reset(help_ac);
    autocomplete_reset(notify_ac);
    autocomplete_reset(notify_message_ac);
    autocomplete_reset(notify_room_ac);
    autocomplete_reset(notify_typing_ac);
    autocomplete_reset(sub_ac);

    autocomplete_reset(who_room_ac);
    autocomplete_reset(who_roster_ac);
    autocomplete_reset(prefs_ac);
    autocomplete_reset(log_ac);
    autocomplete_reset(commands_ac);
    autocomplete_reset(autoaway_ac);
    autocomplete_reset(autoaway_mode_ac);
    autocomplete_reset(autoconnect_ac);
    autocomplete_reset(theme_ac);
    if (theme_load_ac) {
        autocomplete_free(theme_load_ac);
        theme_load_ac = NULL;
    }
    autocomplete_reset(account_ac);
    autocomplete_reset(account_set_ac);
    autocomplete_reset(account_clear_ac);
    autocomplete_reset(account_default_ac);
    autocomplete_reset(disco_ac);
    autocomplete_reset(close_ac);
    autocomplete_reset(wins_ac);
    autocomplete_reset(roster_ac);
    autocomplete_reset(roster_option_ac);
    autocomplete_reset(roster_by_ac);
    autocomplete_reset(roster_remove_all_ac);
    autocomplete_reset(group_ac);
    autocomplete_reset(titlebar_ac);
    autocomplete_reset(bookmark_ac);
    autocomplete_reset(bookmark_property_ac);
    autocomplete_reset(otr_ac);
    autocomplete_reset(otr_log_ac);
    autocomplete_reset(otr_policy_ac);
    autocomplete_reset(connect_property_ac);
    autocomplete_reset(statuses_ac);
    autocomplete_reset(statuses_setting_ac);
    autocomplete_reset(alias_ac);
    autocomplete_reset(aliases_ac);
    autocomplete_reset(join_property_ac);
    autocomplete_reset(room_ac);
    autocomplete_reset(affiliation_ac);
    autocomplete_reset(role_ac);
    autocomplete_reset(privilege_cmd_ac);
    autocomplete_reset(subject_ac);
    autocomplete_reset(form_ac);
    autocomplete_reset(form_field_multi_ac);
    autocomplete_reset(occupants_ac);
    autocomplete_reset(occupants_default_ac);
    autocomplete_reset(occupants_show_ac);
    autocomplete_reset(time_ac);
    autocomplete_reset(time_format_ac);
    autocomplete_reset(resource_ac);
    autocomplete_reset(inpblock_ac);
    autocomplete_reset(receipts_ac);
    autocomplete_reset(pgp_ac);
    autocomplete_reset(pgp_log_ac);

    if (window->type == WIN_CHAT) {
        ProfChatWin *chatwin = (ProfChatWin*)window;
        assert(chatwin->memcheck == PROFCHATWIN_MEMCHECK);
        PContact contact = roster_get_contact(chatwin->barejid);
        if (contact) {
            p_contact_resource_ac_reset(contact);
        }
    }

    if (window->type == WIN_MUC) {
        ProfMucWin *mucwin = (ProfMucWin*)window;
        assert(mucwin->memcheck == PROFMUCWIN_MEMCHECK);
        muc_autocomplete_reset(mucwin->roomjid);
        muc_jid_autocomplete_reset(mucwin->roomjid);
    }

    if (window->type == WIN_MUC_CONFIG) {
        ProfMucConfWin *confwin = (ProfMucConfWin*)window;
        assert(confwin->memcheck == PROFCONFWIN_MEMCHECK);
        if (confwin->form) {
            form_reset_autocompleters(confwin->form);
        }
    }

    bookmark_autocomplete_reset();
}

/*
 * Take a line of input and process it, return TRUE if profanity is to
 * continue, FALSE otherwise
 */
gboolean
cmd_process_input(ProfWin *window, char *inp)
{
    log_debug("Input received: %s", inp);
    gboolean result = FALSE;
    g_strchomp(inp);

    // just carry on if no input
    if (strlen(inp) == 0) {
        result = TRUE;

    // handle command if input starts with a '/'
    } else if (inp[0] == '/') {
        char *inp_cpy = strdup(inp);
        char *command = strtok(inp_cpy, " ");
        result = _cmd_execute(window, command, inp);
        free(inp_cpy);

    // call a default handler if input didn't start with '/'
    } else {
        result = cmd_execute_default(window, inp);
    }

    return result;
}

// Command execution

void
cmd_execute_connect(ProfWin *window, const char * const account)
{
    GString *command = g_string_new("/connect ");
    g_string_append(command, account);
    cmd_process_input(window, command->str);
    g_string_free(command, TRUE);
}

static gboolean
_cmd_execute(ProfWin *window, const char * const command, const char * const inp)
{
    if (g_str_has_prefix(command, "/field") && window->type == WIN_MUC_CONFIG) {
        gboolean result = FALSE;
        gchar **args = parse_args_with_freetext(inp, 1, 2, &result);
        if (!result) {
            ui_current_print_formatted_line('!', 0, "Invalid command, see /form help");
            result = TRUE;
        } else {
            gchar **tokens = g_strsplit(inp, " ", 2);
            char *field = tokens[0] + 1;
            result = cmd_form_field(window, field, args);
            g_strfreev(tokens);
        }

        g_strfreev(args);
        return result;
    }

    Command *cmd = g_hash_table_lookup(commands, command);
    gboolean result = FALSE;

    if (cmd) {
        gchar **args = cmd->parser(inp, cmd->min_args, cmd->max_args, &result);
        if (result == FALSE) {
            ui_invalid_command_usage(cmd->help.usage, cmd->setting_func);
            return TRUE;
        } else {
            gboolean result = cmd->func(window, args, cmd->help);
            g_strfreev(args);
            return result;
        }
    } else {
        gboolean ran_alias = FALSE;
        gboolean alias_result = cmd_execute_alias(window, inp, &ran_alias);
        if (!ran_alias) {
            return cmd_execute_default(window, inp);
        } else {
            return alias_result;
        }
    }
}

static char *
_cmd_complete_parameters(ProfWin *window, const char * const input)
{
    int i;
    char *result = NULL;

    // autocomplete boolean settings
    gchar *boolean_choices[] = { "/beep", "/intype", "/states", "/outtype",
        "/flash", "/splash", "/chlog", "/grlog", "/history", "/vercheck",
        "/privileges", "/presence", "/wrap", "/winstidy", "/carbons", "/encwarn" };

    for (i = 0; i < ARRAY_SIZE(boolean_choices); i++) {
        result = autocomplete_param_with_func(input, boolean_choices[i], prefs_autocomplete_boolean_choice);
        if (result) {
            return result;
        }
    }

    // autocomplete nickname in chat rooms
    if (window->type == WIN_MUC) {
        ProfMucWin *mucwin = (ProfMucWin*)window;
        assert(mucwin->memcheck == PROFMUCWIN_MEMCHECK);
        Autocomplete nick_ac = muc_roster_ac(mucwin->roomjid);
        if (nick_ac) {
            gchar *nick_choices[] = { "/msg", "/info", "/caps", "/status", "/software" } ;

            // Remove quote character before and after names when doing autocomplete
            char *unquoted = strip_arg_quotes(input);
            for (i = 0; i < ARRAY_SIZE(nick_choices); i++) {
                result = autocomplete_param_with_ac(unquoted, nick_choices[i], nick_ac, TRUE);
                if (result) {
                    free(unquoted);
                    return result;
                }
            }
            free(unquoted);
        }

    // otherwise autocomplete using roster
    } else {
        gchar *contact_choices[] = { "/msg", "/info", "/status" };
        // Remove quote character before and after names when doing autocomplete
        char *unquoted = strip_arg_quotes(input);
        for (i = 0; i < ARRAY_SIZE(contact_choices); i++) {
            result = autocomplete_param_with_func(unquoted, contact_choices[i], roster_contact_autocomplete);
            if (result) {
                free(unquoted);
                return result;
            }
        }
        free(unquoted);

        gchar *resource_choices[] = { "/caps", "/software", "/ping" };
        for (i = 0; i < ARRAY_SIZE(resource_choices); i++) {
            result = autocomplete_param_with_func(input, resource_choices[i], roster_fulljid_autocomplete);
            if (result) {
                return result;
            }
        }
    }

    result = autocomplete_param_with_func(input, "/invite", roster_contact_autocomplete);
    if (result) {
        return result;
    }

    gchar *invite_choices[] = { "/decline", "/join" };
    for (i = 0; i < ARRAY_SIZE(invite_choices); i++) {
        result = autocomplete_param_with_func(input, invite_choices[i], muc_invites_find);
        if (result) {
            return result;
        }
    }

    gchar *cmds[] = { "/help", "/prefs", "/disco", "/close", "/wins", "/subject", "/room" };
    Autocomplete completers[] = { help_ac, prefs_ac, disco_ac, close_ac, wins_ac, subject_ac, room_ac };

    for (i = 0; i < ARRAY_SIZE(cmds); i++) {
        result = autocomplete_param_with_ac(input, cmds[i], completers[i], TRUE);
        if (result) {
            return result;
        }
    }

    GHashTable *ac_funcs = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(ac_funcs, "/who",           _who_autocomplete);
    g_hash_table_insert(ac_funcs, "/sub",           _sub_autocomplete);
    g_hash_table_insert(ac_funcs, "/notify",        _notify_autocomplete);
    g_hash_table_insert(ac_funcs, "/autoaway",      _autoaway_autocomplete);
    g_hash_table_insert(ac_funcs, "/theme",         _theme_autocomplete);
    g_hash_table_insert(ac_funcs, "/log",           _log_autocomplete);
    g_hash_table_insert(ac_funcs, "/account",       _account_autocomplete);
    g_hash_table_insert(ac_funcs, "/roster",        _roster_autocomplete);
    g_hash_table_insert(ac_funcs, "/group",         _group_autocomplete);
    g_hash_table_insert(ac_funcs, "/bookmark",      _bookmark_autocomplete);
    g_hash_table_insert(ac_funcs, "/autoconnect",   _autoconnect_autocomplete);
    g_hash_table_insert(ac_funcs, "/otr",           _otr_autocomplete);
    g_hash_table_insert(ac_funcs, "/pgp",           _pgp_autocomplete);
    g_hash_table_insert(ac_funcs, "/connect",       _connect_autocomplete);
    g_hash_table_insert(ac_funcs, "/statuses",      _statuses_autocomplete);
    g_hash_table_insert(ac_funcs, "/alias",         _alias_autocomplete);
    g_hash_table_insert(ac_funcs, "/join",          _join_autocomplete);
    g_hash_table_insert(ac_funcs, "/form",          _form_autocomplete);
    g_hash_table_insert(ac_funcs, "/occupants",     _occupants_autocomplete);
    g_hash_table_insert(ac_funcs, "/kick",          _kick_autocomplete);
    g_hash_table_insert(ac_funcs, "/ban",           _ban_autocomplete);
    g_hash_table_insert(ac_funcs, "/affiliation",   _affiliation_autocomplete);
    g_hash_table_insert(ac_funcs, "/role",          _role_autocomplete);
    g_hash_table_insert(ac_funcs, "/resource",      _resource_autocomplete);
    g_hash_table_insert(ac_funcs, "/titlebar",      _titlebar_autocomplete);
    g_hash_table_insert(ac_funcs, "/inpblock",      _inpblock_autocomplete);
    g_hash_table_insert(ac_funcs, "/time",          _time_autocomplete);
    g_hash_table_insert(ac_funcs, "/receipts",      _receipts_autocomplete);

    int len = strlen(input);
    char parsed[len+1];
    i = 0;
    while (i < len) {
        if (input[i] == ' ') {
            break;
        } else {
            parsed[i] = input[i];
        }
        i++;
    }
    parsed[i] = '\0';

    char * (*ac_func)(ProfWin*, const char * const) = g_hash_table_lookup(ac_funcs, parsed);
    if (ac_func) {
        result = ac_func(window, input);
        if (result) {
            g_hash_table_destroy(ac_funcs);
            return result;
        }
    }
    g_hash_table_destroy(ac_funcs);

    if (g_str_has_prefix(input, "/field")) {
        result = _form_field_autocomplete(window, input);
        if (result) {
            return result;
        }
    }

    return NULL;
}

static char *
_sub_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;
    result = autocomplete_param_with_func(input, "/sub allow", presence_sub_request_find);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_func(input, "/sub deny", presence_sub_request_find);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/sub", sub_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_who_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    if (window->type == WIN_MUC) {
        result = autocomplete_param_with_ac(input, "/who", who_room_ac, TRUE);
        if (result) {
            return result;
        }
    } else {
        int i = 0;
        gchar *group_commands[] = { "/who any", "/who online", "/who offline",
            "/who chat", "/who away", "/who xa", "/who dnd", "/who available",
            "/who unavailable" };

        for (i = 0; i < ARRAY_SIZE(group_commands); i++) {
            result = autocomplete_param_with_func(input, group_commands[i], roster_group_autocomplete);
            if (result) {
                return result;
            }
        }

        result = autocomplete_param_with_ac(input, "/who", who_roster_ac, TRUE);
        if (result) {
            return result;
        }
    }

    return NULL;
}

static char *
_roster_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;
    result = autocomplete_param_with_func(input, "/roster nick", roster_barejid_autocomplete);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_func(input, "/roster clearnick", roster_barejid_autocomplete);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_func(input, "/roster remove", roster_barejid_autocomplete);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/roster remove_all", roster_remove_all_ac, TRUE);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/roster show", roster_option_ac, TRUE);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/roster hide", roster_option_ac, TRUE);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/roster by", roster_by_ac, TRUE);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/roster", roster_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_group_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;
    result = autocomplete_param_with_func(input, "/group show", roster_group_autocomplete);
    if (result) {
        return result;
    }

    result = autocomplete_param_no_with_func(input, "/group add", 4, roster_contact_autocomplete);
    if (result) {
        return result;
    }
    result = autocomplete_param_no_with_func(input, "/group remove", 4, roster_contact_autocomplete);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_func(input, "/group add", roster_group_autocomplete);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_func(input, "/group remove", roster_group_autocomplete);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/group", group_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_bookmark_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;

    gboolean result;
    gchar **args = parse_args(input, 3, 8, &result);
    gboolean handle_options = result && (g_strv_length(args) > 2);

    if (handle_options && ((strcmp(args[0], "add") == 0) || (strcmp(args[0], "update") == 0)) ) {
        GString *beginning = g_string_new("/bookmark");
        gboolean autojoin = FALSE;
        int num_args = g_strv_length(args);

        g_string_append(beginning, " ");
        g_string_append(beginning, args[0]);
        g_string_append(beginning, " ");
        g_string_append(beginning, args[1]);
        if (num_args == 4 && g_strcmp0(args[2], "autojoin") == 0) {
            g_string_append(beginning, " ");
            g_string_append(beginning, args[2]);
            autojoin = TRUE;
        }

        if (num_args > 4) {
            g_string_append(beginning, " ");
            g_string_append(beginning, args[2]);
            g_string_append(beginning, " ");
            g_string_append(beginning, args[3]);
            if (num_args == 6 && g_strcmp0(args[4], "autojoin") == 0) {
                g_string_append(beginning, " ");
                g_string_append(beginning, args[4]);
                autojoin = TRUE;
            }
        }

        if (num_args > 6) {
            g_string_append(beginning, " ");
            g_string_append(beginning, args[4]);
            g_string_append(beginning, " ");
            g_string_append(beginning, args[5]);
            if (num_args == 8 && g_strcmp0(args[6], "autojoin") == 0) {
                g_string_append(beginning, " ");
                g_string_append(beginning, args[6]);
                autojoin = TRUE;
            }
        }

        if (autojoin) {
            found = autocomplete_param_with_func(input, beginning->str, prefs_autocomplete_boolean_choice);
        } else {
            found = autocomplete_param_with_ac(input, beginning->str, bookmark_property_ac, TRUE);
        }
        g_string_free(beginning, TRUE);
        if (found) {
            g_strfreev(args);
            return found;
        }
    }

    g_strfreev(args);

    found = autocomplete_param_with_func(input, "/bookmark remove", bookmark_find);
    if (found) {
        return found;
    }
    found = autocomplete_param_with_func(input, "/bookmark join", bookmark_find);
    if (found) {
        return found;
    }
    found = autocomplete_param_with_func(input, "/bookmark update", bookmark_find);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/bookmark", bookmark_ac, TRUE);
    return found;
}

static char *
_notify_autocomplete(ProfWin *window, const char * const input)
{
    int i = 0;
    char *result = NULL;

    result = autocomplete_param_with_func(input, "/notify room current", prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_func(input, "/notify message current", prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_func(input, "/notify typing current", prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_func(input, "/notify room text", prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_func(input, "/notify message text", prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/notify room", notify_room_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/notify message", notify_message_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/notify typing", notify_typing_ac, TRUE);
    if (result) {
        return result;
    }

    gchar *boolean_choices[] = { "/notify invite", "/notify sub" };
    for (i = 0; i < ARRAY_SIZE(boolean_choices); i++) {
        result = autocomplete_param_with_func(input, boolean_choices[i],
            prefs_autocomplete_boolean_choice);
        if (result) {
            return result;
        }
    }

    result = autocomplete_param_with_ac(input, "/notify", notify_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_autoaway_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    result = autocomplete_param_with_ac(input, "/autoaway mode", autoaway_mode_ac, TRUE);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_func(input, "/autoaway check",
        prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/autoaway", autoaway_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_log_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    result = autocomplete_param_with_func(input, "/log rotate",
        prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_func(input, "/log shared",
        prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }
    result = autocomplete_param_with_ac(input, "/log", log_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_autoconnect_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    result = autocomplete_param_with_func(input, "/autoconnect set", accounts_find_enabled);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/autoconnect", autoconnect_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_otr_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;

    found = autocomplete_param_with_func(input, "/otr start", roster_contact_autocomplete);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/otr log", otr_log_ac, TRUE);
    if (found) {
        return found;
    }

    // /otr policy always user@server.com
    gboolean result;
    gchar **args = parse_args(input, 3, 3, &result);
    if (result && (strcmp(args[0], "policy") == 0)) {
        GString *beginning = g_string_new("/otr ");
        g_string_append(beginning, args[0]);
        g_string_append(beginning, " ");
        g_string_append(beginning, args[1]);

        found = autocomplete_param_with_func(input, beginning->str, roster_contact_autocomplete);
        g_string_free(beginning, TRUE);
        if (found) {
            g_strfreev(args);
            return found;
        }
    }

    g_strfreev(args);

    found = autocomplete_param_with_ac(input, "/otr policy", otr_policy_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/otr", otr_ac, TRUE);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_pgp_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;

    found = autocomplete_param_with_func(input, "/pgp start", roster_contact_autocomplete);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/pgp log", pgp_log_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_func(input, "/pgp setkey", roster_barejid_autocomplete);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/pgp", pgp_ac, TRUE);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_theme_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;
    if ((strncmp(input, "/theme load ", 12) == 0) && (strlen(input) > 12)) {
        if (theme_load_ac == NULL) {
            theme_load_ac = autocomplete_new();
            GSList *themes = theme_list();
            GSList *curr = themes;
            while (curr) {
                autocomplete_add(theme_load_ac, curr->data);
                curr = g_slist_next(curr);
            }
            g_slist_free_full(themes, g_free);
            autocomplete_add(theme_load_ac, "default");
        }
        result = autocomplete_param_with_ac(input, "/theme load", theme_load_ac, TRUE);
        if (result) {
            return result;
        }
    }
    result = autocomplete_param_with_ac(input, "/theme", theme_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_resource_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;

    if (window->type == WIN_CHAT) {
        ProfChatWin *chatwin = (ProfChatWin*)window;
        assert(chatwin->memcheck == PROFCHATWIN_MEMCHECK);
        PContact contact = roster_get_contact(chatwin->barejid);
        if (contact) {
            Autocomplete ac = p_contact_resource_ac(contact);
            found = autocomplete_param_with_ac(input, "/resource set", ac, FALSE);
            if (found) {
                return found;
            }
        }
    }

    found = autocomplete_param_with_func(input, "/resource title", prefs_autocomplete_boolean_choice);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_func(input, "/resource message", prefs_autocomplete_boolean_choice);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/resource", resource_ac, FALSE);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_titlebar_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;

    found = autocomplete_param_with_func(input, "/titlebar show", prefs_autocomplete_boolean_choice);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_func(input, "/titlebar goodbye", prefs_autocomplete_boolean_choice);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/titlebar", titlebar_ac, FALSE);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_inpblock_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;

    found = autocomplete_param_with_func(input, "/inpblock dynamic", prefs_autocomplete_boolean_choice);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/inpblock", inpblock_ac, FALSE);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_form_autocomplete(ProfWin *window, const char * const input)
{
    if (window->type != WIN_MUC_CONFIG) {
        return NULL;
    }

    char *found = NULL;

    ProfMucConfWin *confwin = (ProfMucConfWin*)window;
    DataForm *form = confwin->form;
    if (form) {
        found = autocomplete_param_with_ac(input, "/form help", form->tag_ac, TRUE);
        if (found) {
            return found;
        }
    }

    found = autocomplete_param_with_ac(input, "/form", form_ac, TRUE);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_form_field_autocomplete(ProfWin *window, const char * const input)
{
    if (window->type != WIN_MUC_CONFIG) {
        return NULL;
    }

    char *found = NULL;

    ProfMucConfWin *confwin = (ProfMucConfWin*)window;
    DataForm *form = confwin->form;
    if (form == NULL) {
        return NULL;
    }

    gchar **split = g_strsplit(input, " ", 0);

    if (g_strv_length(split) == 3) {
        char *field_tag = split[0]+1;
        if (form_tag_exists(form, field_tag)) {
            form_field_type_t field_type = form_get_field_type(form, field_tag);
            Autocomplete value_ac = form_get_value_ac(form, field_tag);;
            GString *beginning = g_string_new(split[0]);
            g_string_append(beginning, " ");
            g_string_append(beginning, split[1]);

            if (((g_strcmp0(split[1], "add") == 0) || (g_strcmp0(split[1], "remove") == 0))
                    && field_type == FIELD_LIST_MULTI) {
                found = autocomplete_param_with_ac(input, beginning->str, value_ac, TRUE);

            } else if ((g_strcmp0(split[1], "remove") == 0) && field_type == FIELD_TEXT_MULTI) {
                found = autocomplete_param_with_ac(input, beginning->str, value_ac, TRUE);

            } else if ((g_strcmp0(split[1], "remove") == 0) && field_type == FIELD_JID_MULTI) {
                found = autocomplete_param_with_ac(input, beginning->str, value_ac, TRUE);
            }

            g_string_free(beginning, TRUE);
        }

    } else if (g_strv_length(split) == 2) {
        char *field_tag = split[0]+1;
        if (form_tag_exists(form, field_tag)) {
            form_field_type_t field_type = form_get_field_type(form, field_tag);
            Autocomplete value_ac = form_get_value_ac(form, field_tag);;

            switch (field_type)
            {
                case FIELD_BOOLEAN:
                    found = autocomplete_param_with_func(input, split[0], prefs_autocomplete_boolean_choice);
                    break;
                case FIELD_LIST_SINGLE:
                    found = autocomplete_param_with_ac(input, split[0], value_ac, TRUE);
                    break;
                case FIELD_LIST_MULTI:
                case FIELD_JID_MULTI:
                case FIELD_TEXT_MULTI:
                    found = autocomplete_param_with_ac(input, split[0], form_field_multi_ac, TRUE);
                    break;
                default:
                    break;
            }
        }
    }

    g_strfreev(split);

    return found;
}

static char *
_occupants_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;

    found = autocomplete_param_with_ac(input, "/occupants default show", occupants_show_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/occupants default hide", occupants_show_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/occupants default", occupants_default_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/occupants show", occupants_show_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/occupants hide", occupants_show_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/occupants", occupants_ac, TRUE);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_time_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;

    found = autocomplete_param_with_ac(input, "/time statusbar", time_format_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/time main", time_format_ac, TRUE);
    if (found) {
        return found;
    }

    found = autocomplete_param_with_ac(input, "/time", time_ac, TRUE);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_kick_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    if (window->type == WIN_MUC) {
        ProfMucWin *mucwin = (ProfMucWin*)window;
        assert(mucwin->memcheck == PROFMUCWIN_MEMCHECK);
        Autocomplete nick_ac = muc_roster_ac(mucwin->roomjid);

        if (nick_ac) {
            result = autocomplete_param_with_ac(input, "/kick", nick_ac, TRUE);
            if (result) {
                return result;
            }
        }
    }

    return result;
}

static char *
_ban_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    if (window->type == WIN_MUC) {
        ProfMucWin *mucwin = (ProfMucWin*)window;
        assert(mucwin->memcheck == PROFMUCWIN_MEMCHECK);
        Autocomplete jid_ac = muc_roster_jid_ac(mucwin->roomjid);

        if (jid_ac) {
            result = autocomplete_param_with_ac(input, "/ban", jid_ac, TRUE);
            if (result) {
                return result;
            }
        }
    }

    return result;
}

static char *
_affiliation_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    if (window->type == WIN_MUC) {
        ProfMucWin *mucwin = (ProfMucWin*)window;
        assert(mucwin->memcheck == PROFMUCWIN_MEMCHECK);
        gboolean parse_result;
        Autocomplete jid_ac = muc_roster_jid_ac(mucwin->roomjid);

        gchar **args = parse_args(input, 3, 3, &parse_result);

        if ((strncmp(input, "/affiliation", 12) == 0) && (parse_result == TRUE)) {
            GString *beginning = g_string_new("/affiliation ");
            g_string_append(beginning, args[0]);
            g_string_append(beginning, " ");
            g_string_append(beginning, args[1]);

            result = autocomplete_param_with_ac(input, beginning->str, jid_ac, TRUE);
            g_string_free(beginning, TRUE);
            if (result) {
                g_strfreev(args);
                return result;
            }
        }

        g_strfreev(args);
    }

    result = autocomplete_param_with_ac(input, "/affiliation set", affiliation_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/affiliation list", affiliation_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/affiliation", privilege_cmd_ac, TRUE);
    if (result) {
        return result;
    }

    return result;
}

static char *
_role_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    if (window->type == WIN_MUC) {
        ProfMucWin *mucwin = (ProfMucWin*)window;
        assert(mucwin->memcheck == PROFMUCWIN_MEMCHECK);
        gboolean parse_result;
        Autocomplete nick_ac = muc_roster_ac(mucwin->roomjid);

        gchar **args = parse_args(input, 3, 3, &parse_result);

        if ((strncmp(input, "/role", 5) == 0) && (parse_result == TRUE)) {
            GString *beginning = g_string_new("/role ");
            g_string_append(beginning, args[0]);
            g_string_append(beginning, " ");
            g_string_append(beginning, args[1]);

            result = autocomplete_param_with_ac(input, beginning->str, nick_ac, TRUE);
            g_string_free(beginning, TRUE);
            if (result) {
                g_strfreev(args);
                return result;
            }
        }

        g_strfreev(args);
    }

    result = autocomplete_param_with_ac(input, "/role set", role_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/role list", role_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/role", privilege_cmd_ac, TRUE);
    if (result) {
        return result;
    }

    return result;
}

static char *
_statuses_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    result = autocomplete_param_with_ac(input, "/statuses console", statuses_setting_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/statuses chat", statuses_setting_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/statuses muc", statuses_setting_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/statuses", statuses_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_receipts_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    result = autocomplete_param_with_func(input, "/receipts send", prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_func(input, "/receipts request", prefs_autocomplete_boolean_choice);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/receipts", receipts_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_alias_autocomplete(ProfWin *window, const char * const input)
{
    char *result = NULL;

    result = autocomplete_param_with_ac(input, "/alias remove", aliases_ac, TRUE);
    if (result) {
        return result;
    }

    result = autocomplete_param_with_ac(input, "/alias", alias_ac, TRUE);
    if (result) {
        return result;
    }

    return NULL;
}

static char *
_connect_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;
    gboolean result = FALSE;

    gchar **args = parse_args(input, 2, 4, &result);

    if ((strncmp(input, "/connect", 8) == 0) && (result == TRUE)) {
        GString *beginning = g_string_new("/connect ");
        g_string_append(beginning, args[0]);
        if (args[1] && args[2]) {
            g_string_append(beginning, " ");
            g_string_append(beginning, args[1]);
            g_string_append(beginning, " ");
            g_string_append(beginning, args[2]);
        }
        found = autocomplete_param_with_ac(input, beginning->str, connect_property_ac, TRUE);
        g_string_free(beginning, TRUE);
        if (found) {
            g_strfreev(args);
            return found;
        }
    }

    g_strfreev(args);

    found = autocomplete_param_with_func(input, "/connect", accounts_find_enabled);
    if (found) {
        return found;
    }

    return NULL;
}

static char *
_join_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;
    gboolean result = FALSE;

    found = autocomplete_param_with_func(input, "/join", bookmark_find);
    if (found) {
        return found;
    }

    gchar **args = parse_args(input, 2, 4, &result);

    if ((strncmp(input, "/join", 5) == 0) && (result == TRUE)) {
        GString *beginning = g_string_new("/join ");
        g_string_append(beginning, args[0]);
        if (args[1] && args[2]) {
            g_string_append(beginning, " ");
            g_string_append(beginning, args[1]);
            g_string_append(beginning, " ");
            g_string_append(beginning, args[2]);
        }
        found = autocomplete_param_with_ac(input, beginning->str, join_property_ac, TRUE);
        g_string_free(beginning, TRUE);
        if (found) {
            g_strfreev(args);
            return found;
        }
    }

    g_strfreev(args);

    return NULL;
}

static char *
_account_autocomplete(ProfWin *window, const char * const input)
{
    char *found = NULL;
    gboolean result = FALSE;

    gchar **args = parse_args(input, 3, 4, &result);

    if ((strncmp(input, "/account set", 12) == 0) && (result == TRUE)) {
        GString *beginning = g_string_new("/account set ");
        g_string_append(beginning, args[1]);
        if ((g_strv_length(args) > 3) && (g_strcmp0(args[2], "otr")) == 0) {
            g_string_append(beginning, " ");
            g_string_append(beginning, args[2]);
            found = autocomplete_param_with_ac(input, beginning->str, otr_policy_ac, TRUE);
            g_string_free(beginning, TRUE);
            if (found) {
                g_strfreev(args);
                return found;
            }
        } else {
            found = autocomplete_param_with_ac(input, beginning->str, account_set_ac, TRUE);
            g_string_free(beginning, TRUE);
            if (found) {
                g_strfreev(args);
                return found;
            }
        }
    }

    if ((strncmp(input, "/account clear", 14) == 0) && (result == TRUE)) {
        GString *beginning = g_string_new("/account clear ");
        g_string_append(beginning, args[1]);
        found = autocomplete_param_with_ac(input, beginning->str, account_clear_ac, TRUE);
        g_string_free(beginning, TRUE);
        if (found) {
            g_strfreev(args);
            return found;
        }
    }

    g_strfreev(args);

    found = autocomplete_param_with_ac(input, "/account default", account_default_ac, TRUE);
    if(found){
        return found;
    }

    int i = 0;
    gchar *account_choice[] = { "/account set", "/account show", "/account enable",
        "/account disable", "/account rename", "/account clear", "/account remove",
        "/account default set" };

    for (i = 0; i < ARRAY_SIZE(account_choice); i++) {
        found = autocomplete_param_with_func(input, account_choice[i], accounts_find_all);
        if (found) {
            return found;
        }
    }

    found = autocomplete_param_with_ac(input, "/account", account_ac, TRUE);
    return found;
}

static int
_cmp_command(Command *cmd1, Command *cmd2)
{
    return g_strcmp0(cmd1->cmd, cmd2->cmd);
}

void
command_docgen(void)
{
    GList *cmds = NULL;
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(command_defs); i++) {
        Command *pcmd = command_defs+i;
        cmds = g_list_insert_sorted(cmds, pcmd, (GCompareFunc)_cmp_command);
    }

    FILE *toc_fragment = fopen("toc_fragment.html", "w");
    FILE *main_fragment = fopen("main_fragment.html", "w");

    fputs("<ul><li><ul><li>\n", toc_fragment);
    fputs("<hr>\n", main_fragment);

    GList *curr = cmds;
    while (curr) {
        Command *pcmd = curr->data;


        // old style
        if (pcmd->help.usage) {
//            fprintf(toc_fragment, "<a href=\"#%s\">%s</a>,\n", &pcmd->cmd[1], pcmd->cmd);
//            fprintf(main_fragment, "<a name=\"%s\"></a>\n", &pcmd->cmd[1]);
//            fprintf(main_fragment, "<h4>%s</h4>\n", pcmd->cmd);
//            fputs("<p>Usage:</p>\n", main_fragment);
//            fprintf(main_fragment, "<p><pre><code>%s</code></pre></p>\n", pcmd->help.usage);
//
//            fputs("<p>Details:</p>\n", main_fragment);
//            fputs("<p><pre><code>", main_fragment);
//            int i = 2;
//            while (pcmd->help.long_help[i]) {
//                fprintf(main_fragment, "%s\n", pcmd->help.long_help[i++]);
//            }
//            fputs("</code></pre></p>\n<a href=\"#top\"><h5>back to top</h5></a><br><hr>\n", main_fragment);
//            fputs("\n", main_fragment);

        // new style
        } else {
            fprintf(toc_fragment, "<a href=\"#%s\">%s</a>,\n", &pcmd->cmd[1], pcmd->cmd);
            fprintf(main_fragment, "<a name=\"%s\"></a>\n", &pcmd->cmd[1]);
            fprintf(main_fragment, "<h4>%s</h4>\n", pcmd->cmd);

            fputs("<p><b>Synopsis</b></p>\n", main_fragment);
            fputs("<p><pre><code>", main_fragment);
            int i = 0;
            while (pcmd->help.synopsis[i]) {
                char *str1 = str_replace(pcmd->help.synopsis[i], "<", "&lt;");
                char *str2 = str_replace(str1, ">", "&gt;");
                fprintf(main_fragment, "%s\n", str2);
                i++;
            }
            fputs("</code></pre></p>\n", main_fragment);

            fputs("<p><b>Description</b></p>\n", main_fragment);
            fputs("<p>", main_fragment);
            fprintf(main_fragment, "%s\n", pcmd->help.desc);
            fputs("</p>\n", main_fragment);

            if (pcmd->help.args[0][0] != NULL) {
                fputs("<p><b>Arguments</b></p>\n", main_fragment);
                fputs("<table>", main_fragment);
                for (i = 0; pcmd->help.args[i][0] != NULL; i++) {
                    fputs("<tr>", main_fragment);
                    fputs("<td>", main_fragment);
                    fputs("<code>", main_fragment);
                    char *str1 = str_replace(pcmd->help.args[i][0], "<", "&lt;");
                    char *str2 = str_replace(str1, ">", "&gt;");
                    fprintf(main_fragment, "%s", str2);
                    fputs("</code>", main_fragment);
                    fputs("</td>", main_fragment);
                    fputs("<td>", main_fragment);
                    fprintf(main_fragment, "%s", pcmd->help.args[i][1]);
                    fputs("</td>", main_fragment);
                    fputs("</tr>", main_fragment);
                }
                fputs("</table>\n", main_fragment);
            }

            if (pcmd->help.examples[0] != NULL) {
                fputs("<p><b>Examples</b></p>\n", main_fragment);
                fputs("<p><pre><code>", main_fragment);
                int i = 0;
                while (pcmd->help.examples[i]) {
                    fprintf(main_fragment, "%s\n", pcmd->help.examples[i]);
                    i++;
                }
                fputs("</code></pre></p>\n", main_fragment);
            }

            fputs("<a href=\"#top\"><h5>back to top</h5></a><br><hr>\n", main_fragment);
            fputs("\n", main_fragment);
        }
        curr = g_list_next(curr);
    }

    fputs("</ul></ul>\n", toc_fragment);

    fclose(toc_fragment);
    fclose(main_fragment);
    g_list_free(cmds);
}
