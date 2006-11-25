#ifndef __EX_MISC__H__
#define __EX_MISC__H__


// clamp values (macro)
#define clamp(a,b,c) (a = min(max(a, b), c))

// make any intermediate directories for given filename path
//void MakeIntermediatePath(char *);


// copies text to clipboard
void CopyToClipboard(const char *);


// reads from clipboard
char *ReadFromClipboard(void);


// compares fun strings
int funcmp(const char *s1, const char *s2);


// case insensitive strstr with boolean return
//int stristr(const char *s, const char *sub);

// find view entity
//int GetViewEntity(void);

// separate chat
#define  CHAT_MM1   1
#define  CHAT_MM2   2
#define  CHAT_SPEC  3
int SeparateChat(char *chat, int *out_type, char **out_msg);

// uncolor text
// void myRemoveColors(char *str);

// int GetUserNum(char *t);

// overwrites, always generates same length or shorter string
void StripName(char *);

// get float value of string, false on error
// qbool getFloatValue(char *string, float *val);

// get integer value of string, false on error
// qbool getIntegerValue(char *string, int *val);


#endif //  __EX_MISC__H__
