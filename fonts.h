
#ifndef EZQUAKE_FONTS_HEADER
#define EZQUAKE_FONTS_HEADER

float FontCharacterWidthWide(wchar ch);
qbool FontAlterCharCoordsWide(int* x, int* y, wchar ch, qbool bigchar, float scale);
void FontAdvanceCharCoordsWide(int* x, int* y, wchar ch, qbool bigchar, float scale, int char_gap);

float FontCharacterWidth(char ch);
qbool FontAlterCharCoords(int* x, int* y, char ch, qbool bigchar, float scale);
void FontAdvanceCharCoords(int* x, int* y, char ch, qbool bigchar, float scale, int char_gap);

void FontCreate(const char* path);

#endif
