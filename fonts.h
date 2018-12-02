
#ifndef EZQUAKE_FONTS_HEADER
#define EZQUAKE_FONTS_HEADER

#ifdef EZ_FREETYPE_SUPPORT
void FontInitialise(void);
int FontFixedWidth(int max_length, float scale, qbool digits_only, qbool proportional);
#else
#define FontFixedWidth(max_length, scale, digits_only, proportional) (max_length * 8.0f * scale)
#endif

qbool FontAlterCharCoordsWide(float* x, float* y, wchar ch, qbool bigchar, float scale, qbool proportional);
float FontCharacterWidthWide(wchar ch, float scale, qbool proportional);
float FontCharacterWidth(char ch, float scale, qbool proportional);

#endif
