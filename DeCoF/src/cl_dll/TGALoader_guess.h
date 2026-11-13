// Cry of Fear TGA loader (the loader of the NeHe lesson 33 tutorial, in a
// class). Nothing in the client calls it; the linker keeps it.

#ifndef GUESS_TGALOADER_H
#define GUESS_TGALOADER_H

typedef struct
{
	GLubyte	*imageData_guess;
	GLuint	bpp_guess;
	GLuint	width_guess;
	GLuint	height_guess;
	GLuint	texID_guess;
	GLuint	type_guess;
} Texture_guess;

typedef struct
{
	GLubyte	Header_guess[12];
} TGAHeader_guess;

typedef struct
{
	GLubyte	header_guess[6];
	GLuint	bytesPerPixel_guess;
	GLuint	imageSize_guess;
	GLuint	temp_guess;
	GLuint	type_guess;
	GLuint	Height_guess;
	GLuint	Width_guess;
	GLuint	Bpp_guess;
} TGA_guess;

class CTGALoader_guess
{
public:
	bool LoadTGA_guess( Texture_guess *texture, char *filename );
	bool LoadUncompressedTGA_guess( Texture_guess *texture, char *filename, FILE *fTGA );
	bool LoadCompressedTGA_guess( Texture_guess *texture, char *filename, FILE *fTGA );

	TGAHeader_guess	tgaheader_guess;
	TGA_guess		tga_guess;
	GLubyte			uTGAcompare_guess[12];
	GLubyte			cTGAcompare_guess[12];
};

#endif // GUESS_TGALOADER_H
