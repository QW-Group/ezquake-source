#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#include <windows.h>
#endif

#include "EX_misc.h"
#include "EX_FunNames.h"

#if defined (__linux__)
#define CLIPBOARDSIZE 1024
static char clipboard[CLIPBOARDSIZE] = "\0";    // for clipboard implementation
#endif

// make any intermediate directories for given filename path    
void MakeIntermediatePath(char *_name)
{
    char *slash;
    char name[2000];
    char buf[2000];

    strcpy(name, _name);

    for (slash = name; *slash; slash++)
        if (*slash == '/'  ||  *slash == '\\')
            *slash = PATH_SEPARATOR;

    slash = strchr(name, PATH_SEPARATOR);


    while (slash != NULL)
    {
        strncpy(buf, name, slash-name);
        buf[slash-name] = 0;
    Sys_mkdir(buf);
        slash = strchr(slash+1, PATH_SEPARATOR);
    }
}

// copies given text to clipboard
void CopyToClipboard(const char *text)
{
#ifdef _WIN32
    if (OpenClipboard(NULL))
    {
        HANDLE i;
        LPTSTR  lptstrCopy;
        HGLOBAL hglbCopy;

        EmptyClipboard();
        hglbCopy = GlobalAlloc(GMEM_DDESHARE, strlen(text)+1);
        lptstrCopy = GlobalLock(hglbCopy);
        strcpy((char *)lptstrCopy, text);
        GlobalUnlock(hglbCopy);
        i = SetClipboardData(CF_TEXT, hglbCopy);

        CloseClipboard();
    }
#else
    strncpy(clipboard, text, CLIPBOARDSIZE);
    clipboard[CLIPBOARDSIZE-1] = 0;
#endif
}

// reads from clipboard
char *ReadFromClipboard(void)
{
#ifdef _WIN32
    static char clipbuf[1024];
    int     i;
    HANDLE  th;
    char    *clipText;

    clipbuf[0] = 0;

    if (OpenClipboard(NULL))
    {
        th = GetClipboardData(CF_TEXT);
        if (th)
        {
            clipText = GlobalLock(th);
            if (clipText)
            {
                strncpy(clipbuf, clipText, 1023);
                clipbuf[1023] = 0;
                for (i=0; i < strlen(clipbuf); i++)
                    if (clipbuf[i]=='\n' || clipbuf[i]=='\t' || clipbuf[i]=='\b')
                        clipbuf[i] = ' ';
            }
            GlobalUnlock(th);
        }
        CloseClipboard();
    }
    return clipbuf;
#else
    return clipboard;
#endif
}


// compares two fun strings
int funcmp(const char *s1, const char *s2)
{
    char *t1, *t2;
    int ret;

    if (s1 == NULL  &&  s2 == NULL)
        return 0;

    if (s1 == NULL)
        return -1;

    if (s2 == NULL)
        return 1;

    t1 = strdup(s1);
    t2 = strdup(s2);

    FunToSort(t1);
    FunToSort(t2);

    ret = strcmp(t1, t2);

    free(t1);
    free(t2);

    return ret;
}

//-------------------------------------
//
// case insensitive strstr (bool return)
// assumes sub is lowercase
//

int stristr(const char *s, const char *sub)
{
    int i;
    char *tmp;

    tmp = strdup(s);
    for (i=0; i < strlen(tmp); i++)
        tmp[i] = tolower(tmp[i]);

    i = (int)strstr(tmp, sub);

    free(tmp);
    return i;
}

//-------------------------------------
//
// find view entity
// = playernum+1 if player, or tracked person if spectator
// minus if free track
//

int GetViewEntity(void)
{
    extern cvar_t cl_chasecam;

    if (cl.spectator)
    {
        if (WhoIsSpectated() < 0  ||  cl_chasecam.value == 0)
            return -1;
        else
            return WhoIsSpectated() + 1;
    }
    else
        return cl.playernum + 1;
}

//-------------------------------------
//
// separate chat message
// finds, who said that, where is the message and type of it
// returns client number (or MAX_CLIENTS for 'console' user)
//

void Tmp_MakeRed(char *s)
{
    while (*s)
        *s++ |= 128;
}

int SeparateChat(char *chat, int *out_type, char **out_msg)
{
    int server_cut = 31;    // maximum characters sent in nick by server

    int i;
    int classified = -1;
    int type;
    char *msg;
    for (i=0; i <= MAX_CLIENTS; i++)
    {
        int client = -1;
        char buf[512];

        if (i == MAX_CLIENTS)
        {
            strcpy(buf, "console: ");
            //Tmp_MakeRed(buf);
            if (!strncmp(chat, buf, strlen(buf)))
            {
                client = i;
                type = CHAT_MM1;
                msg = chat + strlen(buf);
            }
        }
        else
        {
            if (!cl.players[i].name[0])
                continue;

            sprintf(buf, "%.*s: ", server_cut, Info_ValueForKey(cl.players[i].userinfo, "name"));
            //Tmp_MakeRed(buf);
            if (!strncmp(chat, buf, strlen(buf)))
            {
                client = i;
                type = CHAT_MM1;
                msg = chat + strlen(buf);
            }

            sprintf(buf, "(%.*s): ", server_cut, Info_ValueForKey(cl.players[i].userinfo, "name"));
            //Tmp_MakeRed(buf);
            if (!strncmp(chat, buf, strlen(buf)))
            {
                client = i;
                type = CHAT_MM2;
                msg = chat + strlen(buf);
            }

            sprintf(buf, "[SPEC] %.*s: ", server_cut, Info_ValueForKey(cl.players[i].userinfo, "name"));
            //Tmp_MakeRed(buf);
            if (!strncmp(chat, buf, strlen(buf)))
            {
                client = i;
                type = CHAT_SPEC;
                msg = chat + strlen(buf);
            }
        }

        if (client >= 0)
        {
            if (classified < 0)
                classified = client;
            else
                return -1;
        }
    }
    
    if (classified < 0)
        return -1;

    if (out_msg)
        *out_msg = msg;
    if (out_type)
        *out_type = type;

    return classified;
}

//-------------------------------------
//
// find user number by string, string can be:
//   #num - user number
//   num  - user id
//   XXX  - exact client name
//

int GetUserNum(char *t)
{
    int client = -1;

    if (*t == '#')
        client = atoi(t+1);
    else
    {
        int uid, i;
        uid = atoi(t);
        for (i=0; i < MAX_CLIENTS; i++)
            if (cl.players[i].name[0]  &&  cl.players[i].userid == uid)
            {
                client = i;
                break;
            }
        if (client < 0)
            for (i=0; i < MAX_CLIENTS; i++)
                if (cl.players[i].name[0]  &&  !strcmp(cl.players[i].name, t))
                {
                    client = i;
                    break;
                }
    }

    if (client >= 0  &&  cl.players[client].name[0])
        return client;
    else
        return -1;
}

// ----------------------------------------------
//
// myRemoveColors
// convert string to non-coloured form
// alse remove characters, which cannot be in a filename
//

void myRemoveColors(char *str)
{
    //char dest[2000];
    char *s = str;

    if (str[0] == 0)
        return;

    while (*s)
    {
        *s &= 127;  // remove red colour

        if (*s < 16) *s = '_';  // bochomazy
        else if (*s == 16) *s = '[';
        else if (*s == 17) *s = ']';
        else if (*s >= 18 && *s <= 27) *s += 30;
        else if (*s >= 28 && *s <= 31) *s = '_';
        else if (*s == 127) *s = '_';

        // remove disallowed characters

        if (*s=='\\' || *s=='/' || *s==':' || *s=='*' || *s=='?' ||
            *s=='\"' || *s=='<' || *s=='>' || *s=='|')
                *s = '_';

        s++;
    }

    // remove spaces from the beginning
    s = str;
    while (*s == '_'  ||  *s == ' ')
        s++;
    memmove(str, s, strlen(s) + 1);

    // remove spaces from the end
    s = str + strlen(str);
    while (s > str  &&  (*(s-1) == '_' || *(s-1) == ' '))
        s--;
    *s = 0;

    if (str[0] == 0)
        strcpy(str, "_");
    // done
}

// overwrites, always generates same length or shorter string
void StripName(char *namebuf)
{
    int     i;
    char    *s;

    for (i=0; i < strlen(namebuf); i++)
        if (namebuf[i] == '_')
            namebuf[i] = ' ';

    // remove spaces from the beginning
    s = namebuf;
    while (*s == ' ')
        s++;
    memmove(namebuf, s, strlen(s) + 1);

    // remove spaces from the end
    s = namebuf + strlen(namebuf);
    while (s > namebuf  &&  *(s-1) == ' ')
        s--;
    *s = 0;

    // remove few spaces in a row
    s = namebuf;
    while (s < namebuf + strlen(namebuf))
    {
        if (*s == ' ')
        {
            char *q = s+1;
            while (*q == ' ')
                q++;

            if (q > s+1)
                memmove(s+1, q, strlen(q)+1);
        }
        s++;
    }

    if (namebuf[0] == 0)
        strcpy(namebuf, "_");

    // replace all non-standard characters with '_'
    for (i=0; i < strlen(namebuf); i++)
    {
        if (namebuf[i] < ' '  ||  namebuf[i] > '~')
            namebuf[i] = '_';
    }
}


// get float value of string, false on error
qboolean getFloatValue(char *string, float *val)
{
    // [sign] [digits] [.digits] [ {d | D | e | E }[sign]digits]

    char *s;
    qboolean percent = false;

    s = string;

    //
    // first validate
    //

    // skip white spaces
    while (isspace2(*s))
        s++;

    if (*s == 0)    // empty string
        return false;

    // sign
    if (*s == '-'  ||  *s == '+')
        s++;

    // digits
    if ((*s >= 0  &&  *s <= '9')  ||  *s == '.')
    {
        while (*s >= '0'  &&  *s <= '9')
            s++;

        // .digits
        if (*s == '.')
        {
            s++;

            if (*s >= '0'  &&  *s <= '9')
            {
                while (*s >= '0'  &&  *s <= '9')
                    s++;
            }
            else
                return false;   // no digits after '.' ?
        }
    }
    else
        return false;   // no value?

    // [ {d | D | e | E }[sign]digits]
    if (tolower(*s) == 'd'  ||  tolower(*s) == 'e')
    {
        s++;

        if (*s == '-'  ||  *s == '+')
            s++;

        if (*s >= '0'  &&  *s <= '9')
        {
            while (*s >= '0'  &&  *s <= '9')
                s++;
        }
        else
            return false;
    }

    // check for percent sign
    if (*s == '%')
    {
        percent = true;
        s++;
    }

    // skip white spaces
    while (isspace2(*s))
        s++;

    // we should be at the end now
    if (*s != 0)
        return false;

    //
    // all OK, calculate value
    //

    *val = atof(string);
    if (percent)
        *val /= 100.0;

    return true;
}

// get integer value of string, false on error
qboolean getIntegerValue(char *string, int *val)
{
    // [sign]digits  ||  "0x" hex-digits

    char *s;

    s = string;

    //
    // first validate
    //

    // skip white spaces
    while (isspace2(*s))
        s++;

    if (*s == 0)    // empty string
        return false;

    if (*s == '0'  &&  tolower(*(s+1)) == 'x')
    {
        // hexadecimal
        s += 2;

        // hex digits
        if ((*s >= '0'  &&  *s <= '9')  ||  (tolower(*s) >= 'a'  &&  tolower(*s) <= 'f'))
        {
            while ((*s >= '0'  &&  *s <= '9')  ||  (tolower(*s) >= 'a'  &&  tolower(*s) <= 'f'))
                s++;
        }
        else
            return false;   // at least one required

        // skip white spaces
        while (isspace2(*s))
            s++;

        // we should be at the end now
        if (*s != 0)
            return false;

        //
        // calculate value
        //

        sscanf(string+2, "%x", val);
    }
    else
    {
        // decimal

        // sign
        if (*s == '-'  ||  *s == '+')
            s++;

        // digits
        if (*s >= '0'  &&  *s <= '9')
        {
            while (*s >= '0'  &&  *s <= '9')
                s++;
        }
        else
            return false;   // at least one digit required

        // skip white spaces
        while (isspace2(*s))
            s++;

        // we should be at the end now
        if (*s != 0)
            return false;

        //
        // calculate value
        //

        *val = atoi(string);
    }

    return true;
}
