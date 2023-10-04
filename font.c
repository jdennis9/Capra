/*
   Copyright 2023 Jamie Dennis

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include <libdrm/drm_fourcc.h>
#include <freetype/freetype.h>
#include <wlr/render/drm_format_set.h>
#include <wchar.h>

#define MAX_CHAR_CODES 16384

typedef struct {
	struct wlr_texture *texture;
	int8_t x_offset, y_offset, advance, width, height;
	int8_t padding_[3];
} Glyph;

static struct {
	// Index is the glyph unicode value
	Glyph glyphs[MAX_CHAR_CODES];
} bar_font;

static FT_Library freetype;

static void setup_font_rendering() {
	FT_Init_FreeType(&freetype);
}

static size_t get_string_glyphs(const char *utf8_string, Glyph *glyphs, size_t *string_length);
static void load_font(const char *path, int px_size);
static size_t render_glyphs(float *matrix, Glyph *glyphs, int count, int x, int y);

static size_t get_string_glyphs(const char *string, Glyph *glyphs, size_t *string_length) {
	size_t ret = 0;
	size_t length = 0;
	mbstate_t mbstate;
	wchar_t unicode;
    
	for (; *string; ++glyphs, ++length) {
		size_t s = mbrtowc(&unicode, string, sizeof(wchar_t), &mbstate);
		if (s == 0 || s == (size_t)-2 || s == (size_t)-1) break;
		if (unicode >= MAX_CHAR_CODES) *glyphs = bar_font.glyphs[' '];
		else *glyphs = bar_font.glyphs[unicode];
		ret += glyphs->advance;
		string += s;
	}
    
	if (string_length) *string_length = length;
	return ret;
}

static void load_font(const char *path, int px_size) {
	FT_Face face;
    
	if (FT_New_Face(freetype, path, 0, &face)) {
		printf("Failed to load font \"%s\"\n", path);
		return;
	}
	else if (FT_Select_Charmap(face, FT_ENCODING_UNICODE)) {
		printf("%s is not a unicode font\n", path);
		return;
	}
    
	printf("Loading font %s\n", path);
    
	FT_Set_Pixel_Sizes(face, px_size, 0);
    
	struct {
		uint8_t *memory;
		size_t size;
	} conversion_buffer = {NULL, 0};
	
	// Copy the glyphs to wlr_texture objects
	// @TODO: Figure out a way to store glyphs in a texture atlas because this
	// is very memory-inefficient
	for (uint16_t char_code = 0; char_code < MAX_CHAR_CODES; ++char_code) {
		uint32_t glyph_index = FT_Get_Char_Index(face, char_code);
		Glyph *glyph = &bar_font.glyphs[char_code];
        
		if (glyph->texture || glyph->advance) continue;
		if (!glyph_index) continue;
		
		int width, height, pixel_count, output_pixel_data_size;
		uint8_t *input_buffer;
        
		FT_Load_Glyph(face, glyph_index, 0);
		FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		
		width = face->glyph->bitmap.width;
		height = face->glyph->bitmap.rows;
		pixel_count = width*height;
		output_pixel_data_size = pixel_count * 4;
        
		glyph->x_offset = face->glyph->metrics.horiBearingX >> 6;
		glyph->y_offset = face->glyph->metrics.horiBearingY >> 6;
		glyph->advance = face->glyph->metrics.horiAdvance >> 6;
		glyph->width = width;
		glyph->height = height;
        
		// Check if glyph is a space
		if (!pixel_count) {
			continue;
		}
		
		input_buffer = face->glyph->bitmap.buffer;
		
		if (!conversion_buffer.memory || (conversion_buffer.size < output_pixel_data_size)) {
			conversion_buffer.memory = realloc(conversion_buffer.memory, output_pixel_data_size);
			conversion_buffer.size = output_pixel_data_size;
		}
		
		// Convert r8 to rgba8888
		uint8_t *conversion_buffer_iterator = conversion_buffer.memory;
		for (int i = 0; i < pixel_count; ++i) {
			conversion_buffer_iterator[0] = input_buffer[0];
			conversion_buffer_iterator[1] = input_buffer[0];
			conversion_buffer_iterator[2] = input_buffer[0];
			conversion_buffer_iterator[3] = input_buffer[0];
			input_buffer += 1;
			conversion_buffer_iterator += 4;
		}
		
		glyph->texture = 
			wlr_texture_from_pixels(server.renderer, DRM_FORMAT_ABGR8888, width * 4, 
                                    width, height, conversion_buffer.memory);
        
	}
	free(conversion_buffer.memory);
}

static size_t render_glyphs(float *matrix, Glyph *glyphs, int count, int x, int y) {
	Glyph *glyph = glyphs;
	size_t ret = 0;
    
	for (int i = 0; i < count; ++i, ++glyph) {
		if (glyph->texture) {
			wlr_render_texture(server.renderer, glyph->texture, matrix,
                               x + glyph->x_offset, y - glyph->y_offset, 1.f);
		}
        
		x += glyph->advance;
		ret += glyph->advance;
	}
    
	return ret;
}

