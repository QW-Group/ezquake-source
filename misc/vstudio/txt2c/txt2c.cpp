/*
Copyright (C) 2016-2020 ezQuake team (https://github.com/ezquake/).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"
#include <string.h>
#include <ctype.h>

int main(int argc, const char* argv[])
{
	FILE* input;
	FILE* output = stdout;
	const char* name;
	const char* prefix = "";

	if (argc < 2) {
		printf("Usage: %s <file> <output>\n", argv[0]);
		return 0;
	}

	if (fopen_s(&input, argv[1], "rb") != 0) {
		printf("ERROR: cannot open %s\n", argv[1]);
		return 1;
	}

	if (argc >= 3) {
		if (fopen_s(&output, argv[2], "wt")) {
			printf("ERROR: cannot open %s for writing\n", argv[2]);
			return 1;
		}
	}

	if (argc >= 4) {
		prefix = argv[3];
	}

	{
		const char* next;

		name = argv[1];
		while (next = strstr(name, "\\")) {
			name = next + 1;
		}
	}

	fprintf(output, "unsigned char %s", prefix);
	for (size_t i = 0; i < strlen(name); ++i) {
		if (!isalnum(name[i])) {
			fprintf(output, "_");
		}
		else {
			fprintf(output, "%c", name[i]);
		}
	}
	fprintf(output, "[] = {");

	int bytes = 0;
	while (!feof(input)) {
		unsigned char ch = (unsigned char)fgetc(input);
		if (feof(input)) {
			break;
		}

		if (bytes != 0) {
			fprintf(output, ",");
		}
		if (bytes % 12 == 0) {
			fprintf(output, "\n  ");
		}
		fprintf(output, "0x%02X", ch);
		++bytes;
	}
	fclose(input);
	input = NULL;

	fprintf(output, "\n};\n");
	fprintf(output, "unsigned int %s", prefix);
	for (size_t i = 0; i < strlen(name); ++i) {
		if (name[i] == '.') {
			fprintf(output, "_");
		}
		else {
			fprintf(output, "%c", name[i]);
		}
	}
	fprintf(output, "_len = %d;\n", bytes);

	return 0;
}

