/*
Copyright (C) 2011-2015 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

wchar *decode_string (const char *s);
char *encode_say (wchar *in);

wchar TextEncodingDecodeUTF8(char* str, int* index);

// Make sure the renderer can display all Unicode chars in the string,
// otherwise try to replace them with Latin equivalents
wchar *maybe_transliterate (wchar *src);
