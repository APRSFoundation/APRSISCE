#include <stdio.h>

#include <stdlib.h>

#include <lpng/source/png.h>

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

#include "TraceLog.h"

#include "pngUtil.h"

bool pngCheckFileHeader(const char* Path)
{
	// try to open file
	FILE* file = fopen(Path, "rb");

	// unable to open
	if (file == 0)
	{
TraceLogThread("OSM", TRUE, "pngCheckFileHeader(%s) fopen Failed\n", Path);
		return false;
	}

	// the number of bytes to read
	const int nread = 8;

	unsigned char buffer[nread];

	// try to read header
	if (fread(buffer, 1, nread, file) != nread)
	{
TraceLogThread("OSM", TRUE, "pngCheckFileHeader(%s) fread Failed\n", Path);
		fclose(file);
		return 0;
	}

	// close file
	fclose(file);

	// compare headers
	int result = png_sig_cmp(buffer, 0, nread);

if (result) TraceLogThread("OSM", TRUE, "pngCheckFileHeader(%s) png_sig_cmp Failed with %ld\n", Path, result);

	return(result == 0);
}

// -------------
// doConvertRGB8
// -------------
/**
 * Gets the data from an 8-bit rgb image.
 *
 * @par Memory
 * - The returned pointer is created by using the new[] operator.
 * - You have to free the allocated memory yourself.
 *
 * @par Structure
 * - The color-sequence is Blue-Green-Red-Alpha (8 bit each).
 * - The first 4 values (RGBA) are located in the top-left corner of the image.
 * - The last 4 values (RGBA) are located in the bottom-right corner of the image.
 */
static unsigned char* pngConvertRGB8(png_structp& PngPtr, png_infop& InfoPtr)
{
	// get dimensions
	int m_width = png_get_image_width(PngPtr, InfoPtr);
    int m_height = png_get_image_height(PngPtr, InfoPtr);

	// calculate needed memory
	int size = m_height * m_width * 4;

	// allocate memory
	unsigned char* bgra = new unsigned char[size];

	// get image rows
	png_bytep* row_pointers = png_get_rows(PngPtr, InfoPtr);

	int pos = 0;

	// get color values
	for(int i = 0; i < m_height; i++)
	{
		for(int j = 0; j < (3 * m_width); j += 3)
		{
			bgra[pos++] = row_pointers[i][j + 2];	// blue
			bgra[pos++] = row_pointers[i][j + 1];	// green
			bgra[pos++] = row_pointers[i][j];		// red
			bgra[pos++] = 0;						// alpha
		}
	}

	return bgra;
}


// --------------
// doConvertRGBA8
// --------------
/**
 * Gets the data from an 8-bit rgb image with alpha values.
 *
 * @par Memory
 * - The returned pointer is created by using the new[] operator.
 * - You have to free the allocated memory yourself.
 *
 * @par Structure
 * - The color-sequence is Blue-Green-Red-Alpha (8 bit each).
 * - The first 4 values (RGBA) are located in the top-left corner of the image.
 * - The last 4 values (RGBA) are located in the bottom-right corner of the image.
 */
static unsigned char* pngConvertRGBA8(png_structp& PngPtr, png_infop& InfoPtr)
{
	// get dimensions
	int m_width = png_get_image_width(PngPtr, InfoPtr);
    int m_height = png_get_image_height(PngPtr, InfoPtr);

	// calculate needed memory
	int size = m_height * m_width * 4;

	// allocate memory
	unsigned char* bgra = new unsigned char[size];

	// get image rows
	png_bytep* row_pointers = png_get_rows(PngPtr, InfoPtr);

	int pos = 0;

	// get color values
	for(int i = 0; i < m_height; i++)
	{
		for(int j = 0; j < (4 * m_width); j += 4)
		{
			bgra[pos++] = row_pointers[i][j + 2];	// blue
			bgra[pos++] = row_pointers[i][j + 1];	// green
			bgra[pos++] = row_pointers[i][j];		// red
			bgra[pos++] = row_pointers[i][j + 3];	// alpha
		}
	}

	return bgra;
}


// --------------
// doConvertGrey8
// --------------
/**
 * Gets the data from an 8-bit monochrome image.
 *
 * @par Memory
 * - The returned pointer is created by using the new[] operator.
 * - You have to free the allocated memory yourself.
 *
 * @par Structure
 * - The color-sequence is Blue-Green-Red-Alpha (8 bit each).
 * - The first 4 values (RGBA) are located in the top-left corner of the image.
 * - The last 4 values (RGBA) are located in the bottom-right corner of the image.
 */
static unsigned char* pngConvertGrey8(png_structp& PngPtr, png_infop& InfoPtr)
{
	// get dimensions
	int m_width = png_get_image_width(PngPtr, InfoPtr);
    int m_height = png_get_image_height(PngPtr, InfoPtr);

	// calculate needed memory
	int size = m_height * m_width * 4;

	// allocate memory
	unsigned char* bgra = new unsigned char[size];

	// get image rows
	png_bytep* row_pointers = png_get_rows(PngPtr, InfoPtr);

	int pos = 0;

	// get color values
	for(int i = 0; i < m_height; i++)
	{
		for(int j = 0; j < m_width; j++)
		{
			bgra[pos++] = row_pointers[i][j];	// blue
			bgra[pos++] = row_pointers[i][j];	// green
			bgra[pos++] = row_pointers[i][j];	// red
			bgra[pos++] = 0;					// alpha
		}
	}

	return bgra;
}


// ---------------
// doConvertGreyA8
// ---------------
/**
 * Gets the data from an 8-bit monochrome image with alpha values.
 */
static unsigned char* pngConvertGreyA8(png_structp& PngPtr, png_infop& InfoPtr)
{
	// get dimensions
	int m_width = png_get_image_width(PngPtr, InfoPtr);
    int m_height = png_get_image_height(PngPtr, InfoPtr);

	// calculate needed memory
	int size = m_height * m_width * 4;

	// allocate memory
	unsigned char* bgra = new unsigned char[size];

	// get image rows
	png_bytep* row_pointers = png_get_rows(PngPtr, InfoPtr);

	int pos = 0;

	// get color values
	for(int i = 0; i < m_height; i++)
	{
		for(int j = 0; j < (2 * m_width); j += 2)
		{
			bgra[pos++] = row_pointers[i][j];		// blue
			bgra[pos++] = row_pointers[i][j];		// green
			bgra[pos++] = row_pointers[i][j];		// red
			bgra[pos++] = row_pointers[i][j + 1];	// alpha
		}
	}

	return bgra;
}

static unsigned char* pngExtractCanonicData(png_structp& PngPtr, png_infop& InfoPtr)
{
	// get dimensions
	int m_width = png_get_image_width(PngPtr, InfoPtr);
    int m_height = png_get_image_height(PngPtr, InfoPtr);

	// get color information
	int color_type = png_get_color_type(PngPtr, InfoPtr);

	/// the color values (blue, green, red, alpha)
	unsigned char* m_bgra;

	// rgb
	if (color_type == PNG_COLOR_TYPE_RGB)
	{
		m_bgra = pngConvertRGB8(PngPtr, InfoPtr);
	}

	// rgb with opacity
	else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
	{
		m_bgra = pngConvertRGBA8(PngPtr, InfoPtr);
	}

	// 256 grey values
	else if (color_type == PNG_COLOR_TYPE_GRAY)
	{
		m_bgra = pngConvertGrey8(PngPtr, InfoPtr);
	}

	// 256 grey values with opacity
	else if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
		m_bgra = pngConvertGreyA8(PngPtr, InfoPtr);
	}

	// check pointer
	return m_bgra;
}

PNG_INFO_S *pngLoadFromFile(const char *Path)
{	PNG_INFO_S *Info;

	// check filetype
	if (!pngCheckFileHeader(Path))
	{
TraceLogThread("OSM", TRUE, "pngLoadFromFile:pngCheckFileHeader(%s) Failed\n", Path);
		return NULL;
	}

	// try to open file
	FILE* file = fopen(Path, "rb");

	// unable to open
	if (file == 0)
	{
TraceLogThread("OSM", TRUE, "pngLoadFromFile:fopen(%s) Failed\n", Path);
		return NULL;
	}
	// create read struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);

	// check pointer
	if (png_ptr == 0)
	{
TraceLogThread("OSM", TRUE, "pngLoadFromFile:png_create_read_struct(%s) Failed\n", Path);
		fclose(file);
		return NULL;
	}

	// create info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);

	// check pointer
    if (info_ptr == 0)
    {
TraceLogThread("OSM", TRUE, "pngLoadFromFile:png_create_info_struct(%s) Failed\n", Path);
        png_destroy_read_struct(&png_ptr, 0, 0);
		fclose(file);
        return NULL;
    }

	// set error handling
	if (setjmp(png_jmpbuf(png_ptr)))
	{
TraceLogThread("OSM", TRUE, "pngLoadFromFile:setjmp(%s) Failed\n", Path);
		png_destroy_read_struct(&png_ptr, &info_ptr, 0);
		fclose(file);
		return NULL;
	}

	// I/O initialization using standard C streams
	png_init_io(png_ptr, file);

	// read entire image (high level)
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);

	Info = (PNG_INFO_S *) calloc(1,sizeof(*Info));

	// convert the png bytes to BGRA
	if (!(Info->m_bgra = pngExtractCanonicData(png_ptr, info_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, 0);

		fclose(file);
		free(Info);
		return NULL;
	}

	Info->m_width = png_get_image_width(png_ptr, info_ptr);
    Info->m_height = png_get_image_height(png_ptr, info_ptr);

	// free memory
	png_destroy_read_struct(&png_ptr, &info_ptr, 0);

	// close file
	fclose(file);

	return Info;
}

void pngDestroy(PNG_INFO_S *Info)
{	if (Info)
	{	delete[]  Info->m_bgra;
		free(Info);
	}
}

