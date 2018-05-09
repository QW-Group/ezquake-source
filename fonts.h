
#ifndef EZQUAKE_FONTS_HEADER
#define EZQUAKE_FONTS_HEADER

#ifdef EZ_FREETYPE_SUPPORT
float FontCharacterWidthWide(wchar ch, float scale, qbool proportional);
float FontCharacterWidth(char ch, qbool proportional);

void FontInitialise(void);
int FontFixedWidth(int max_length, float scale, qbool digits_only, qbool proportional);
#else
#define FontCharacterWidthWide(ch, scale, proportional) (8.0f * scale)
#define FontCharacterWidth(ch, proportional) (8.0f)

#define FontFixedWidth(max_length, scale, digits_only, proportional) (max_length * 8.0f * scale)
#endif

qbool FontAlterCharCoordsWide(float* x, float* y, wchar ch, qbool bigchar, float scale, qbool proportional);
qbool FontAlterCharCoords(float* x, float* y, char ch, qbool bigchar, float scale, qbool proportional);

#endif
