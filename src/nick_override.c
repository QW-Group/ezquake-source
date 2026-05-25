#include "quakedef.h"
#include "cmd.h"
#include "keys.h"
#include "nick_override.h"
#include "utils.h"

#include <ctype.h>

#define MAX_NICK_OVERRIDE_MATCHES       16
#define MAX_NICK_OVERRIDE_MATCH_STRING 256

typedef struct nick_match_s {
	// A single match entry can either target a normalized name or a userid.
	char original[MAX_SCOREBOARDNAME];
	char normalized[MAX_SCOREBOARDNAME];
	int userid;
	qbool match_userid;
} nick_match_t;

typedef struct nick_override_s {
	struct nick_override_s *next;
	char alias[MAX_SCOREBOARDNAME];
	char matchlist_raw[MAX_NICK_OVERRIDE_MATCH_STRING];
	nick_match_t matches[MAX_NICK_OVERRIDE_MATCHES];
	int match_count;
} nick_override_t;

typedef struct nick_parsed_list_s {
	char raw[MAX_NICK_OVERRIDE_MATCH_STRING];
	nick_match_t matches[MAX_NICK_OVERRIDE_MATCHES];
	int count;
} nick_parsed_list_t;

static nick_override_t *nick_overrides = NULL;

// Strips leading/trailing whitespace from the token we are currently parsing.
static void Nick_TrimWhitespace(char *text)
{
	char *start = text;
	char *end;

	if (!text) {
		return;
	}

	while (*start && isspace((unsigned char)*start)) {
		start++;
	}

	end = start + strlen(start);
	while (end > start && isspace((unsigned char)*(end - 1))) {
		end--;
	}

	if (start != text) {
		memmove(text, start, end - start);
	}

	text[end - start] = '\0';
}

// Returns true if the token is purely digits and therefore should match by userid.
static qbool Nick_IsUseridToken(const char *text, int *userid_out)
{
	size_t len;
	size_t i;
	int userid;

	if (!text || !text[0]) {
		return false;
	}

	len = strlen(text);
	for (i = 0; i < len; ++i) {
		if (!isdigit((unsigned char)text[i])) {
			return false;
		}
	}

	userid = atoi(text);
	if (userid <= 0) {
		return false;
	}

	if (userid_out) {
		*userid_out = userid;
	}

	return true;
}

// If the token is wrapped in braces, remove them so we can force an exact name match.
static qbool Nick_UnwrapForcedName(char *text)
{
	size_t len;

	if (!text) {
		return false;
	}

	len = strlen(text);
	if (len < 2 || text[0] != '{' || text[len - 1] != '}') {
		return false;
	}

	text[len - 1] = '\0';
	memmove(text, text + 1, len - 1);
	Nick_TrimWhitespace(text);
	return true;
}

// Helper used while parsing to avoid storing duplicate matches.
static qbool Nick_ParsedListContains(const nick_parsed_list_t *parsed, qbool match_userid, const char *normalized, int userid)
{
	int i;

	for (i = 0; i < parsed->count; ++i) {
		const nick_match_t *match = &parsed->matches[i];

		if (match->match_userid != match_userid) {
			continue;
		}

		if (match_userid) {
			if (match->userid == userid) {
				return true;
			}
		} else {
			if (!strcmp(match->normalized, normalized)) {
				return true;
			}
		}
	}

	return false;
}

static nick_override_t *Nick_FindByAlias(const char *alias)
{
	nick_override_t *entry;

	if (!alias || !alias[0]) {
		return NULL;
	}

	for (entry = nick_overrides; entry; entry = entry->next) {
		if (!strcasecmp(entry->alias, alias)) {
			return entry;
		}
	}

	return NULL;
}

// Produces a lowercase, colorless version of a player name for comparison.
static void Nick_NormalizeName(const char *input, char *output, size_t output_size)
{
	size_t len = 0;
	char *stripped;
	const char *start, *end;

	if (!output_size) {
		return;
	}

	output[0] = '\0';
	if (!input || !input[0]) {
		return;
	}

	stripped = Player_StripNameColor(input);
	if (!stripped) {
		return;
	}

	start = stripped;
	while (*start && isspace((unsigned char)*start)) {
		start++;
	}

	end = stripped + strlen(stripped);
	while (end > start && isspace((unsigned char)*(end - 1))) {
		end--;
	}

	while (start < end && len + 1 < output_size) {
		unsigned char ch = (unsigned char)*start++;
		if (ch == '\r' || ch == '\n') {
			continue;
		}
		output[len++] = (char)tolower(ch);
	}
	output[len] = '\0';

	Q_free(stripped);
}

// Searches the override list for either a userid match or a normalized name match.
static nick_override_t *Nick_FindMatchingEntryForPlayer(const player_info_t *player)
{
	char normalized[MAX_SCOREBOARDNAME];
	int i;
	nick_override_t *entry;
	int userid;

	if (!player) {
		return NULL;
	}

	Nick_NormalizeName(player->name, normalized, sizeof(normalized));
	userid = player->userid;

	for (entry = nick_overrides; entry; entry = entry->next) {
		for (i = 0; i < entry->match_count; ++i) {
			if (entry->matches[i].match_userid) {
				if (userid > 0 && entry->matches[i].userid == userid) {
					return entry;
				}
			}
			else {
				if (!strcmp(entry->matches[i].normalized, normalized)) {
					return entry;
				}
			}
		}
	}

	return NULL;
}

const char *Nick_OverrideForPlayer(const player_info_t *player)
{
	nick_override_t *entry = Nick_FindMatchingEntryForPlayer(player);
	return entry ? entry->alias : NULL;
}

const char *Nick_PlayerDisplayName(const player_info_t *player)
{
	const char *alias;

	if (!player) {
		return "";
	}

	alias = Nick_OverrideForPlayer(player);
	return alias ? alias : player->name;
}

qbool Nick_HasOverrideForPlayer(const player_info_t *player)
{
	return Nick_FindMatchingEntryForPlayer(player) != NULL;
}

void Nick_RefreshShortnames(void)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; ++i) {
		if (cl.players[i].name[0]) {
			CL_RemovePrefixFromName(i);
		} else {
			cl.players[i].shortname[0] = '\0';
		}
	}
}

static void Nick_FreeEntry(nick_override_t *entry)
{
	if (!entry) {
		return;
	}
	Q_free(entry);
}

void Nick_ClearOverrides(void)
{
	nick_override_t *entry, *next;

	for (entry = nick_overrides; entry; entry = next) {
		next = entry->next;
		Nick_FreeEntry(entry);
	}

	nick_overrides = NULL;
	Nick_RefreshShortnames();
}

static void Nick_ReportEntry(const nick_override_t *entry)
{
	if (!entry) {
		return;
	}

	if (entry->matchlist_raw[0]) {
		Com_Printf("%s => %s\n", entry->alias, entry->matchlist_raw);
	} else {
		Com_Printf("%s => (no matches)\n", entry->alias);
	}
}

// Tokenizes the argument list for /nick and converts it into match entries.
static qbool Nick_ParseMatchString(const char *match_string, nick_parsed_list_t *parsed)
{
	char buffer[MAX_NICK_OVERRIDE_MATCH_STRING];
	char token[MAX_NICK_OVERRIDE_MATCH_STRING];
	char *cursor;
	int count = 0;

	if (!match_string || !match_string[0]) {
		return false;
	}

	strlcpy(buffer, match_string, sizeof(buffer));
	cursor = buffer;
	parsed->count = 0;
	parsed->raw[0] = '\0';

	while (*cursor && parsed->count < MAX_NICK_OVERRIDE_MATCHES) {
		size_t token_len = 0;
		char processed[MAX_NICK_OVERRIDE_MATCH_STRING];
		char normalized[MAX_SCOREBOARDNAME];
		char display_token[MAX_NICK_OVERRIDE_MATCH_STRING];
		qbool forced_name;
		qbool is_userid;
		int userid = 0;
		nick_match_t *out_match;

		while (*cursor && (isspace((unsigned char)*cursor) || *cursor == ',' || *cursor == ';')) {
			cursor++;
		}
		if (!*cursor) {
			break;
		}

		while (*cursor && *cursor != ',' && *cursor != ';') {
			if (token_len + 1 < sizeof(token)) {
				token[token_len++] = *cursor;
			}
			cursor++;
		}
		token[token_len] = '\0';

		strlcpy(processed, token, sizeof(processed));
		Nick_TrimWhitespace(processed);
		if (!processed[0]) {
			continue;
		}

		strlcpy(display_token, processed, sizeof(display_token));
		forced_name = Nick_UnwrapForcedName(processed);

		if (!processed[0]) {
			continue;
		}

		is_userid = !forced_name && Nick_IsUseridToken(processed, &userid);
		out_match = &parsed->matches[parsed->count];

		if (is_userid) {
			if (Nick_ParsedListContains(parsed, true, NULL, userid)) {
				continue;
			}

			out_match->match_userid = true;
			out_match->userid = userid;
			out_match->normalized[0] = '\0';
		} else {
			Nick_NormalizeName(processed, normalized, sizeof(normalized));
			if (!normalized[0] || Nick_ParsedListContains(parsed, false, normalized, 0)) {
				continue;
			}

			out_match->match_userid = false;
			out_match->userid = 0;
			strlcpy(out_match->normalized, normalized, sizeof(out_match->normalized));
		}

		strlcpy(out_match->original, display_token, sizeof(out_match->original));
		parsed->count++;
	}

	if (!parsed->count) {
		return false;
	}

	for (count = 0; count < parsed->count; ++count) {
		if (count) {
			strlcat(parsed->raw, ", ", sizeof(parsed->raw));
		}
		strlcat(parsed->raw, parsed->matches[count].original, sizeof(parsed->raw));
	}

	return true;
}

static void Nick_ApplyParsedMatches(nick_override_t *entry, const nick_parsed_list_t *parsed)
{
	int i;

	entry->match_count = parsed->count;
	strlcpy(entry->matchlist_raw, parsed->raw, sizeof(entry->matchlist_raw));

	for (i = 0; i < parsed->count; ++i) {
		strlcpy(entry->matches[i].original, parsed->matches[i].original, sizeof(entry->matches[i].original));
		strlcpy(entry->matches[i].normalized, parsed->matches[i].normalized, sizeof(entry->matches[i].normalized));
		entry->matches[i].match_userid = parsed->matches[i].match_userid;
		entry->matches[i].userid = parsed->matches[i].userid;
	}
}

static void Nick_AddOrUpdate(const char *alias, const char *match_string)
{
	nick_override_t *entry;
	nick_parsed_list_t parsed;

	if (!alias || !alias[0]) {
		Com_Printf("Usage: nick \"alias\" \"name[, other]\"\n");
		return;
	}

	if (strlen(alias) >= MAX_SCOREBOARDNAME) {
		Com_Printf("nick: alias is too long (max %d)\n", MAX_SCOREBOARDNAME - 1);
		return;
	}

	if (!Nick_ParseMatchString(match_string, &parsed)) {
		Com_Printf("nick: please provide at least one valid name to match\n");
		return;
	}

	entry = Nick_FindByAlias(alias);
	if (!entry) {
		entry = (nick_override_t *)Q_malloc(sizeof(*entry));
		memset(entry, 0, sizeof(*entry));
		strlcpy(entry->alias, alias, sizeof(entry->alias));
		entry->next = nick_overrides;
		nick_overrides = entry;
	}

	Nick_ApplyParsedMatches(entry, &parsed);
	Nick_ReportEntry(entry);
	Nick_RefreshShortnames();
}

static qbool Nick_RemoveEntry(const char *alias)
{
	nick_override_t *entry = nick_overrides;
	nick_override_t *prev = NULL;

	while (entry) {
		if (!strcasecmp(entry->alias, alias)) {
			if (prev) {
				prev->next = entry->next;
			} else {
				nick_overrides = entry->next;
			}
			Nick_FreeEntry(entry);
			Nick_RefreshShortnames();
			return true;
		}
		prev = entry;
		entry = entry->next;
	}

	return false;
}

static void Cmd_Nick_f(void)
{
	int argc = Cmd_Argc();
	const char *alias;

	if (argc == 1) {
		Com_Printf("nick <alias> \"name[, name2]\" : create or update a scoreboard nickname override\n");
		Com_Printf("nick <alias> : show existing override\n");
		return;
	}

	alias = Cmd_Argv(1);
	if (argc == 2) {
		nick_override_t *entry = Nick_FindByAlias(alias);

		if (!entry) {
			Com_Printf("nick: unknown alias \"%s\"\n", alias);
			return;
		}

		Nick_ReportEntry(entry);
		return;
	}

	Nick_AddOrUpdate(alias, Cmd_MakeArgs(2));
}

static void Cmd_UnNick_f(void)
{
	int argc = Cmd_Argc();
	int i;

	if (argc < 2) {
		Com_Printf("nick_clear <alias> [alias2 ...] : remove nickname overrides\n");
		return;
	}

	for (i = 1; i < argc; ++i) {
		const char *alias = Cmd_Argv(i);
		if (!Nick_RemoveEntry(alias)) {
			Com_Printf("nick_clear: unknown alias \"%s\"\n", alias);
		} else {
			Com_Printf("Removed nick override \"%s\"\n", alias);
		}
	}
}

static void Cmd_NickList_f(void)
{
	nick_override_t *entry;

	if (!nick_overrides) {
		Com_Printf("No nick overrides defined\n");
		return;
	}

	for (entry = nick_overrides; entry; entry = entry->next) {
		Nick_ReportEntry(entry);
	}
}

static void Cmd_NickClear_f(void)
{
	if (!nick_overrides) {
		return;
	}

	Nick_ClearOverrides();
	Com_Printf("Cleared all nick overrides\n");
}

static void Cmd_NickEdit_f(void)
{
	const char *alias;
	const char *matches;
	nick_override_t *entry;
	char final_string[MAXCMDLINE - 1];
	size_t cursor = 0;

	if (Cmd_Argc() == 1) {
		Com_Printf("%s <alias> : edit an existing nick override\n", Cmd_Argv(0));
		Com_Printf("nicklist : list all nick overrides\n");
		return;
	}

	alias = Cmd_Argv(1);
	if (!alias[0]) {
		Com_Printf("nickedit: alias name must be specified\n");
		return;
	}

	entry = Nick_FindByAlias(alias);
	if (!entry) {
		Com_Printf("nickedit: unknown alias \"%s\"\n", alias);
		return;
	}

	matches = entry->matchlist_raw;

	strlcpy(final_string, "/nick \"", sizeof(final_string));
	strlcat(final_string, alias, sizeof(final_string));
	strlcat(final_string, "\" \"", sizeof(final_string));
	strlcat(final_string, matches, sizeof(final_string));
	strlcat(final_string, "\"", sizeof(final_string));

	Key_ClearTyping();
	memcpy(key_lines[edit_line] + 1, str2wcs(final_string), (strlen(final_string) + 1) * sizeof(wchar));

	{
		const char *value_marker = strstr(final_string, "\" \"");
		if (value_marker) {
			cursor = (size_t)(value_marker - final_string) + 3;
		} else {
			cursor = strlen(final_string);
		}
	}

	key_linepos = min((int)cursor + 1, MAXCMDLINE - 1);
}

void Nick_WriteOverrides(FILE *f)
{
	nick_override_t *entry;

	if (!nick_overrides) {
		fprintf(f, "// no nick overrides\n");
		return;
	}

	for (entry = nick_overrides; entry; entry = entry->next) {
		fprintf(f, "nick \"%s\" \"%s\"\n", entry->alias, entry->matchlist_raw);
	}
}

void Nick_Init(void)
{
	nick_overrides = NULL;
	Cmd_AddCommand("nick", Cmd_Nick_f);
	Cmd_AddCommand("nicklist", Cmd_NickList_f);
	Cmd_AddCommand("nick_clear", Cmd_UnNick_f);
	Cmd_AddCommand("nick_clearall", Cmd_NickClear_f);
	Cmd_AddCommand("nickedit", Cmd_NickEdit_f);
}

void Nick_Shutdown(void)
{
	Nick_ClearOverrides();
}
