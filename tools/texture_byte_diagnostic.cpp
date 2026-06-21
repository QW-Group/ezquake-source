// Standalone byte-level diagnostic for ezQuake HUD/charset texture uploads.
//
// Build from the repository root, using the x64-linux vcpkg dependencies:
//   g++ -std=c++17 -O2 tools/texture_byte_diagnostic.cpp \
//     -I build-static/vcpkg_installed/x64-linux/include \
//     build-static/vcpkg_installed/x64-linux/lib/libpng16.a \
//     build-static/vcpkg_installed/x64-linux/lib/libz.a \
//     -o build/texture_byte_diagnostic
//
// Examples:
//   build/texture_byte_diagnostic ~/nquake/ezquake/ezquake.pk3:textures/charsets/povo5.png
//   build/texture_byte_diagnostic ~/nquake/qw/textures/charsets/1.png

#include <png.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Image {
	int width = 0;
	int height = 0;
	int original_bit_depth = 0;
	int original_color_type = 0;
	int output_channels = 0;
	bool original_has_trns = false;
	bool loaded_with_engine_order = false;
	std::vector<uint8_t> rgba;
};

struct MemoryReader {
	const uint8_t* data = nullptr;
	size_t size = 0;
	size_t offset = 0;
};

struct Stats {
	uint64_t pixels = 0;
	uint8_t min_r = 255;
	uint8_t min_g = 255;
	uint8_t min_b = 255;
	uint8_t min_a = 255;
	uint8_t max_r = 0;
	uint8_t max_g = 0;
	uint8_t max_b = 0;
	uint8_t max_a = 0;
	uint64_t alpha_0 = 0;
	uint64_t alpha_mid = 0;
	uint64_t alpha_255 = 0;
	uint64_t alpha_bucket_1_63 = 0;
	uint64_t alpha_bucket_64_127 = 0;
	uint64_t alpha_bucket_128_191 = 0;
	uint64_t alpha_bucket_192_254 = 0;
	uint64_t alpha0_nonzero_rgb = 0;
	uint64_t alpha0_zero_rgb = 0;
	uint64_t alpha255_zero_rgb = 0;
	std::array<uint64_t, 256> alpha_hist{};
	std::map<uint32_t, uint64_t> rgba_frequency;
};

static std::string ShellQuote(const std::string& text)
{
	std::string out = "'";
	for (char c : text) {
		if (c == '\'') {
			out += "'\\''";
		}
		else {
			out += c;
		}
	}
	out += "'";
	return out;
}

static bool EndsWithCaseInsensitive(const std::string& value, const std::string& suffix)
{
	if (suffix.size() > value.size()) {
		return false;
	}
	size_t offset = value.size() - suffix.size();
	for (size_t i = 0; i < suffix.size(); ++i) {
		if (std::tolower((unsigned char)value[offset + i]) != std::tolower((unsigned char)suffix[i])) {
			return false;
		}
	}
	return true;
}

static std::pair<std::string, std::string> SplitArchiveSpec(const std::string& spec)
{
	for (const char* ext : { ".pk3:", ".zip:" }) {
		size_t pos = spec.find(ext);
		if (pos != std::string::npos) {
			pos += std::strlen(ext) - 1;
			return { spec.substr(0, pos), spec.substr(pos + 1) };
		}
	}
	return { spec, "" };
}

static std::vector<uint8_t> ReadFile(const std::string& path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		throw std::runtime_error("failed to open " + path);
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static std::vector<uint8_t> ReadArchiveMember(const std::string& archive, const std::string& member)
{
	std::string command = "unzip -p " + ShellQuote(archive) + " " + ShellQuote(member);
	FILE* pipe = popen(command.c_str(), "r");
	if (!pipe) {
		throw std::runtime_error("failed to run unzip");
	}

	std::vector<uint8_t> data;
	std::array<uint8_t, 16384> buffer{};
	while (true) {
		size_t got = fread(buffer.data(), 1, buffer.size(), pipe);
		if (got) {
			data.insert(data.end(), buffer.begin(), buffer.begin() + got);
		}
		if (got < buffer.size()) {
			if (feof(pipe)) {
				break;
			}
			if (ferror(pipe)) {
				pclose(pipe);
				throw std::runtime_error("failed while reading unzip output");
			}
		}
	}

	int status = pclose(pipe);
	if (status != 0 || data.empty()) {
		throw std::runtime_error("unzip did not return data for " + archive + ":" + member);
	}
	return data;
}

static std::vector<uint8_t> ReadInputSpec(const std::string& spec)
{
	auto [outer, inner] = SplitArchiveSpec(spec);
	if (!inner.empty()) {
		return ReadArchiveMember(outer, inner);
	}
	return ReadFile(outer);
}

static void PngReadMemory(png_structp png_ptr, png_bytep out, png_size_t count)
{
	MemoryReader* reader = static_cast<MemoryReader*>(png_get_io_ptr(png_ptr));
	if (!reader || reader->offset + count > reader->size) {
		png_error(png_ptr, "PNG read overrun");
	}
	std::memcpy(out, reader->data + reader->offset, count);
	reader->offset += count;
}

static const char* ColorTypeName(int color_type)
{
	switch (color_type) {
	case PNG_COLOR_TYPE_GRAY:
		return "GRAY";
	case PNG_COLOR_TYPE_PALETTE:
		return "PALETTE";
	case PNG_COLOR_TYPE_RGB:
		return "RGB";
	case PNG_COLOR_TYPE_RGB_ALPHA:
		return "RGBA";
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		return "GRAY_ALPHA";
	default:
		return "unknown";
	}
}

static Image LoadPng(const std::vector<uint8_t>& bytes, bool engine_order)
{
	if (bytes.size() < 8 || png_sig_cmp(bytes.data(), 0, 8)) {
		throw std::runtime_error("input is not a PNG");
	}

	MemoryReader reader;
	reader.data = bytes.data();
	reader.size = bytes.size();
	reader.offset = 8;

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png) {
		throw std::runtime_error("png_create_read_struct failed");
	}
	png_infop info = png_create_info_struct(png);
	if (!info) {
		png_destroy_read_struct(&png, nullptr, nullptr);
		throw std::runtime_error("png_create_info_struct failed");
	}

	if (setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &info, nullptr);
		throw std::runtime_error("libpng failed to decode input");
	}

	png_set_read_fn(png, &reader, PngReadMemory);
	png_set_sig_bytes(png, 8);
	png_read_info(png, info);

	png_uint_32 width = 0;
	png_uint_32 height = 0;
	int bit_depth = 0;
	int color_type = 0;
	int interlace = 0;
	int compression = 0;
	int filter = 0;
	png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, &interlace, &compression, &filter);
	bool has_trns = png_get_valid(png, info, PNG_INFO_tRNS) != 0;

	if (engine_order) {
		// Mirrors src/image.c:Image_LoadPNG_All as closely as possible.
		if (color_type == PNG_COLOR_TYPE_PALETTE) {
			png_set_palette_to_rgb(png);
			png_set_filler(png, 255, PNG_FILLER_AFTER);
		}
		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
			png_set_expand_gray_1_2_4_to_8(png);
		}
		if (has_trns) {
			png_set_tRNS_to_alpha(png);
		}
		if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
			png_set_gray_to_rgb(png);
		}
		if (color_type != PNG_COLOR_TYPE_RGBA) {
			png_set_filler(png, 255, PNG_FILLER_AFTER);
		}
		if (bit_depth < 8) {
			png_set_expand(png);
		}
		else if (bit_depth == 16) {
			png_set_strip_16(png);
		}
	}
	else {
		bool has_alpha = (color_type & PNG_COLOR_MASK_ALPHA) || has_trns;
		if (bit_depth == 16) {
			png_set_strip_16(png);
		}
		if (color_type == PNG_COLOR_TYPE_PALETTE) {
			png_set_palette_to_rgb(png);
		}
		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
			png_set_expand_gray_1_2_4_to_8(png);
		}
		if (has_trns) {
			png_set_tRNS_to_alpha(png);
		}
		if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
			png_set_gray_to_rgb(png);
		}
		if (!has_alpha) {
			png_set_filler(png, 255, PNG_FILLER_AFTER);
		}
		if (bit_depth < 8) {
			png_set_expand(png);
		}
	}

	png_read_update_info(png, info);
	png_size_t rowbytes = png_get_rowbytes(png, info);
	int channels = png_get_channels(png, info);
	int out_bit_depth = png_get_bit_depth(png, info);

	if (out_bit_depth != 8 || channels != 4) {
		std::ostringstream msg;
		msg << "unsupported converted PNG: bit_depth=" << out_bit_depth
		    << " channels=" << channels << " rowbytes=" << rowbytes;
		png_destroy_read_struct(&png, &info, nullptr);
		throw std::runtime_error(msg.str());
	}

	Image image;
	image.width = static_cast<int>(width);
	image.height = static_cast<int>(height);
	image.original_bit_depth = bit_depth;
	image.original_color_type = color_type;
	image.original_has_trns = has_trns;
	image.output_channels = channels;
	image.loaded_with_engine_order = engine_order;
	image.rgba.resize(static_cast<size_t>(height) * static_cast<size_t>(rowbytes));

	std::vector<png_bytep> rows(height);
	for (png_uint_32 y = 0; y < height; ++y) {
		rows[y] = image.rgba.data() + static_cast<size_t>(y) * rowbytes;
	}
	png_read_image(png, rows.data());
	png_read_end(png, info);
	png_destroy_read_struct(&png, &info, nullptr);

	return image;
}

static uint32_t PackRgba(const uint8_t* p)
{
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

static Stats AnalyzeRgba(const std::vector<uint8_t>& rgba)
{
	if (rgba.size() % 4 != 0) {
		throw std::runtime_error("RGBA buffer size is not divisible by 4");
	}

	Stats s;
	s.pixels = rgba.size() / 4;
	for (size_t i = 0; i < rgba.size(); i += 4) {
		uint8_t r = rgba[i + 0];
		uint8_t g = rgba[i + 1];
		uint8_t b = rgba[i + 2];
		uint8_t a = rgba[i + 3];

		s.min_r = std::min(s.min_r, r);
		s.min_g = std::min(s.min_g, g);
		s.min_b = std::min(s.min_b, b);
		s.min_a = std::min(s.min_a, a);
		s.max_r = std::max(s.max_r, r);
		s.max_g = std::max(s.max_g, g);
		s.max_b = std::max(s.max_b, b);
		s.max_a = std::max(s.max_a, a);
		++s.alpha_hist[a];

		if (a == 0) {
			++s.alpha_0;
			if (r || g || b) {
				++s.alpha0_nonzero_rgb;
			}
			else {
				++s.alpha0_zero_rgb;
			}
		}
		else if (a == 255) {
			++s.alpha_255;
			if (!r && !g && !b) {
				++s.alpha255_zero_rgb;
			}
		}
		else {
			++s.alpha_mid;
			if (a < 64) {
				++s.alpha_bucket_1_63;
			}
			else if (a < 128) {
				++s.alpha_bucket_64_127;
			}
			else if (a < 192) {
				++s.alpha_bucket_128_191;
			}
			else {
				++s.alpha_bucket_192_254;
			}
		}

		++s.rgba_frequency[PackRgba(&rgba[i])];
	}
	return s;
}

static std::vector<uint8_t> PremultiplyAlpha(std::vector<uint8_t> rgba)
{
	for (size_t i = 0; i < rgba.size(); i += 4) {
		uint8_t a = rgba[i + 3];
		rgba[i + 0] = static_cast<uint8_t>((uint16_t(rgba[i + 0]) * a) / 255);
		rgba[i + 1] = static_cast<uint8_t>((uint16_t(rgba[i + 1]) * a) / 255);
		rgba[i + 2] = static_cast<uint8_t>((uint16_t(rgba[i + 2]) * a) / 255);
	}
	return rgba;
}

static std::vector<uint8_t> ExpandCharsetLikeEngine(const Image& image, const std::vector<uint8_t>& premultiplied)
{
	if (image.width <= 0 || image.height <= 0 || image.width % 16 != 0 || image.height % 16 != 0) {
		throw std::runtime_error("charset dimensions must be divisible by 16");
	}
	if (premultiplied.size() != static_cast<size_t>(image.width) * image.height * 4) {
		throw std::runtime_error("premultiplied buffer size does not match image dimensions");
	}

	const int out_width = image.width * 2;
	const int out_height = image.height * 2;
	const int cell_w = image.width >> 4;
	const int cell_h = image.height >> 4;
	std::vector<uint8_t> out(static_cast<size_t>(out_width) * out_height * 4, 0);

	for (int y = 0; y < image.height; ++y) {
		for (int x = 0; x < image.width; ++x) {
			int x_offset = (x / cell_w) * cell_w;
			int y_offset = (y / cell_h) * cell_h;
			size_t src = (static_cast<size_t>(x) + static_cast<size_t>(image.width) * y) * 4;
			size_t dst = (static_cast<size_t>(x + x_offset) + static_cast<size_t>(out_width) * (y + y_offset)) * 4;
			std::memcpy(out.data() + dst, premultiplied.data() + src, 4);
		}
	}
	return out;
}

static void PrintImageHeader(const std::string& title, const Image& img)
{
	std::cout << "\n== " << title << " ==\n";
	std::cout << "source png: " << img.width << "x" << img.height
	          << ", original_color=" << ColorTypeName(img.original_color_type)
	          << ", original_bit_depth=" << img.original_bit_depth
	          << ", original_tRNS=" << (img.original_has_trns ? "yes" : "no")
	          << ", output_channels=" << img.output_channels << "\n";
	std::cout << "vulkan expected upload format: VK_FORMAT_R8G8B8A8_UNORM bytes [R,G,B,A]\n";
}

static std::string Percent(uint64_t part, uint64_t whole)
{
	std::ostringstream out;
	if (!whole) {
		out << "0.00%";
	}
	else {
		out << std::fixed << std::setprecision(2) << (100.0 * double(part) / double(whole)) << "%";
	}
	return out.str();
}

static void PrintTopColors(const Stats& s, int count)
{
	std::vector<std::pair<uint32_t, uint64_t>> values(s.rgba_frequency.begin(), s.rgba_frequency.end());
	std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
		if (a.second != b.second) {
			return a.second > b.second;
		}
		return a.first < b.first;
	});

	std::cout << "top rgba values:\n";
	for (int i = 0; i < count && i < static_cast<int>(values.size()); ++i) {
		uint32_t rgba = values[i].first;
		uint8_t r = (rgba >> 24) & 0xff;
		uint8_t g = (rgba >> 16) & 0xff;
		uint8_t b = (rgba >> 8) & 0xff;
		uint8_t a = rgba & 0xff;
		std::cout << "  #" << std::setw(2) << std::setfill('0') << i + 1 << std::setfill(' ')
		          << " count=" << values[i].second << " (" << Percent(values[i].second, s.pixels) << ")"
		          << " rgba=(" << int(r) << "," << int(g) << "," << int(b) << "," << int(a) << ")\n";
	}
}

static void PrintStats(const std::string& title, const Stats& s)
{
	std::cout << "\n-- " << title << " --\n";
	std::cout << "pixels: " << s.pixels << "\n";
	std::cout << "R range: " << int(s.min_r) << ".." << int(s.max_r)
	          << "  G range: " << int(s.min_g) << ".." << int(s.max_g)
	          << "  B range: " << int(s.min_b) << ".." << int(s.max_b)
	          << "  A range: " << int(s.min_a) << ".." << int(s.max_a) << "\n";
	std::cout << "alpha == 0:   " << s.alpha_0 << " (" << Percent(s.alpha_0, s.pixels) << ")\n";
	std::cout << "alpha 1..254: " << s.alpha_mid << " (" << Percent(s.alpha_mid, s.pixels) << ")\n";
	std::cout << "alpha == 255: " << s.alpha_255 << " (" << Percent(s.alpha_255, s.pixels) << ")\n";
	std::cout << "alpha buckets 1..63 / 64..127 / 128..191 / 192..254: "
	          << s.alpha_bucket_1_63 << " / "
	          << s.alpha_bucket_64_127 << " / "
	          << s.alpha_bucket_128_191 << " / "
	          << s.alpha_bucket_192_254 << "\n";
	std::cout << "transparent pixels with nonzero RGB: " << s.alpha0_nonzero_rgb << "\n";
	std::cout << "transparent pixels with zero RGB:    " << s.alpha0_zero_rgb << "\n";
	std::cout << "opaque black pixels:                 " << s.alpha255_zero_rgb << "\n";

	std::cout << "distinct alpha values:";
	int printed = 0;
	for (int i = 0; i < 256; ++i) {
		if (s.alpha_hist[i]) {
			std::cout << " " << i << ":" << s.alpha_hist[i];
			if (++printed >= 24) {
				std::cout << " ...";
				break;
			}
		}
	}
	if (!printed) {
		std::cout << " none";
	}
	std::cout << "\n";
	PrintTopColors(s, 10);
}

static void PrintAlphaGrid(const std::vector<uint8_t>& rgba, int width, int height, int max_width, int max_height)
{
	std::cout << "\nalpha preview (#=opaque, +=partial, .=transparent), top-left "
	          << std::min(width, max_width) << "x" << std::min(height, max_height) << ":\n";
	for (int y = 0; y < std::min(height, max_height); ++y) {
		std::cout << "  ";
		for (int x = 0; x < std::min(width, max_width); ++x) {
			uint8_t a = rgba[(static_cast<size_t>(x) + static_cast<size_t>(width) * y) * 4 + 3];
			char c = (a == 0) ? '.' : (a == 255) ? '#' : '+';
			std::cout << c;
		}
		std::cout << "\n";
	}
}

static bool AlphaCompatible(const Stats& s)
{
	return s.alpha_0 > 0 || s.alpha_mid > 0;
}

static void PrintDiagnosis(const Image& engine_img, const Stats& engine, const Stats& reference,
	const Stats& engine_premul, const Stats& engine_charset)
{
	std::cout << "\n== Diagnosis ==\n";
	if (engine_img.original_has_trns && !AlphaCompatible(engine) && AlphaCompatible(reference)) {
		std::cout << "LIKELY BUG: the PNG has tRNS transparency, but the engine-order decode produced only opaque alpha.\n";
		std::cout << "This matches solid colored rectangles: Vulkan alpha-test sees alpha 255 everywhere.\n";
		std::cout << "The reference decode preserves alpha, so inspect src/image.c PNG transform order.\n";
		std::cout << "Do not add a filler alpha channel before png_set_tRNS_to_alpha for palette PNGs.\n";
	}
	else if (!AlphaCompatible(engine)) {
		std::cout << "The decoded texture is fully opaque. Alpha-test cannot cut glyph/background holes.\n";
		std::cout << "If this is a charset, either the source has no alpha or decode lost it before upload.\n";
	}
	else if (engine_premul.alpha0_nonzero_rgb) {
		std::cout << "Premultiply simulation still has transparent pixels with nonzero RGB. That would break premultiplied blending.\n";
	}
	else if (!AlphaCompatible(engine_charset)) {
		std::cout << "The expanded charset upload buffer is fully opaque. The problem happens before or during charset expansion.\n";
	}
	else {
		std::cout << "Decoded alpha exists and premultiply/charset expansion keeps it. If Android still shows blocks,\n";
		std::cout << "the next suspect is Vulkan sampling/format/descriptor or a different texture than this file.\n";
	}
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		std::cerr << "usage: " << argv[0] << " <png | archive.pk3:inner/path.png>\n";
		return 2;
	}

	try {
		std::string input = argv[1];
		std::vector<uint8_t> bytes = ReadInputSpec(input);

		std::cout << "input: " << input << "\n";
		std::cout << "input bytes: " << bytes.size() << "\n";

		Image engine = LoadPng(bytes, true);
		Image reference = LoadPng(bytes, false);

		PrintImageHeader("engine-order PNG decode", engine);
		Stats engine_stats = AnalyzeRgba(engine.rgba);
		PrintStats("engine-order RGBA bytes", engine_stats);
		PrintAlphaGrid(engine.rgba, engine.width, engine.height, 64, 24);

		PrintImageHeader("reference PNG decode", reference);
		Stats reference_stats = AnalyzeRgba(reference.rgba);
		PrintStats("reference RGBA bytes", reference_stats);

		std::vector<uint8_t> premul = PremultiplyAlpha(engine.rgba);
		Stats premul_stats = AnalyzeRgba(premul);
		PrintStats("engine RGBA after R_ImagePreMultiplyAlpha simulation", premul_stats);

		Stats charset_stats;
		if (engine.width % 16 == 0 && engine.height % 16 == 0) {
			std::vector<uint8_t> charset_upload = ExpandCharsetLikeEngine(engine, premul);
			charset_stats = AnalyzeRgba(charset_upload);
			PrintStats("R_LoadCharsetImage expanded upload bytes", charset_stats);
			PrintAlphaGrid(charset_upload, engine.width * 2, engine.height * 2, 64, 24);
		}
		else {
			std::cout << "\n-- R_LoadCharsetImage expanded upload bytes --\n";
			std::cout << "skipped: dimensions are not divisible by 16\n";
		}

		PrintDiagnosis(engine, engine_stats, reference_stats, premul_stats, charset_stats);
	}
	catch (const std::exception& ex) {
		std::cerr << "error: " << ex.what() << "\n";
		return 1;
	}

	return 0;
}
