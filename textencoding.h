// textencoding.h

wchar *decode_string (const char *s);
char *encode_say (wchar *in);

// Make sure the renderer can display all Unicode chars in the string,
// otherwise try to replace them with Latin equivalents
wchar *maybe_transliterate (wchar *src);
