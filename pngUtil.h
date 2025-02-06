typedef struct PNG_INFO_S
{	
	/// the image's width
	int m_width;

	/// the image's height
	int m_height;

	/// the color values (blue, green, red, alpha)
	unsigned char* m_bgra;

} PNG_INFO_S;

bool pngCheckFileHeader(const char* Path);

PNG_INFO_S *pngLoadFromFile(const char *Path);

void pngDestroy(PNG_INFO_S *Info);

