/*
 * Copyright (c) The Piglit project 2007
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEM, IBM AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(_MSC_VER)
#include <windows.h>
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "piglit-util.h"

/* These texture coordinates should have 1 or -1 in the major axis selecting
 * the face, and a nearly-1-or-negative-1 value in the other two coordinates
 * which will be used to produce the s,t values used to sample that face's
 * image.
 */
GLfloat cube_face_texcoords[6][4][3] = {
	{ /* GL_TEXTURE_CUBE_MAP_POSITIVE_X */
		{1.0,  0.99,  0.99},
		{1.0,  0.99, -0.99},
		{1.0, -0.99, -0.99},
		{1.0, -0.99,  0.99},
	},
	{ /* GL_TEXTURE_CUBE_MAP_POSITIVE_Y */
		{-0.99, 1.0, -0.99},
		{ 0.99, 1.0, -0.99},
		{ 0.99, 1.0,  0.99},
		{-0.99, 1.0,  0.99},
	},
	{ /* GL_TEXTURE_CUBE_MAP_POSITIVE_Z */
		{-0.99,  0.99, 1.0},
		{-0.99, -0.99, 1.0},
		{ 0.99, -0.99, 1.0},
		{ 0.99,  0.99, 1.0},
	},
	{ /* GL_TEXTURE_CUBE_MAP_NEGATIVE_X */
		{-1.0,  0.99, -0.99},
		{-1.0,  0.99,  0.99},
		{-1.0, -0.99,  0.99},
		{-1.0, -0.99, -0.99},
	},
	{ /* GL_TEXTURE_CUBE_MAP_NEGATIVE_Y */
		{-0.99, -1.0,  0.99},
		{-0.99, -1.0, -0.99},
		{ 0.99, -1.0, -0.99},
		{ 0.99, -1.0,  0.99},
	},
	{ /* GL_TEXTURE_CUBE_MAP_NEGATIVE_Z */
		{ 0.99,  0.99, -1.0},
		{-0.99,  0.99, -1.0},
		{-0.99, -0.99, -1.0},
		{ 0.99, -0.99, -1.0},
	},
};

const char *cube_face_names[6] = {
	"POSITIVE_X",
	"POSITIVE_Y",
	"POSITIVE_Z",
	"NEGATIVE_X",
	"NEGATIVE_Y",
	"NEGATIVE_Z",
};

const GLenum cube_face_targets[6] = {
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

GLint piglit_ARBfp_pass_through = 0;

/** Returns the line in the program string given the character position. */
int FindLine(const char *program, int position)
{
	int i, line = 1;
	for (i = 0; i < position; i++) {
		if (program[i] == '0')
			return -1; /* unknown line */
		if (program[i] == '\n')
			line++;
	}
	return line;
}

void
piglit_report_result(enum piglit_result result)
{
	fflush(stderr);

	if (result == PIGLIT_SUCCESS) {
		printf("PIGLIT: {'result': 'pass' }\n");
		fflush(stdout);
		exit(0);
	} else if (result == PIGLIT_SKIP) {
		printf("PIGLIT: {'result': 'skip' }\n");
		fflush(stdout);
		exit(0);
	} else if (result == PIGLIT_WARN) {
		printf("PIGLIT: {'result': 'warn' }\n");
		fflush(stdout);
		exit(0);
	} else {
		printf("PIGLIT: {'result': 'fail' }\n");
		fflush(stdout);
		exit(1);
	}
}

void piglit_require_extension(const char *name)
{
	if (!glutExtensionSupported(name)) {
		printf("Test requires %s\n", name);
		piglit_report_result(PIGLIT_SKIP);
		exit(1);
	}
}

void piglit_require_not_extension(const char *name)
{
	if (glutExtensionSupported(name)) {
		piglit_report_result(PIGLIT_SKIP);
		exit(1);
	}
}

static float tolerance[4] = { 0.01, 0.01, 0.01, 0.01 };

void
piglit_set_tolerance_for_bits(int rbits, int gbits, int bbits, int abits)
{
	int bits[4] = {rbits, gbits, bbits, abits};
	int i;

	for (i = 0; i < 4; i++) {
		if (bits[i] < 2) {
			/* Don't try to validate channels when there's only 1
			 * bit of precision (or none).
			 */
			tolerance[i] = 1.0;
		} else {
			tolerance[i] = 3.0 / (1 << bits[i]);
		}
	}
}

/**
 * Read a pixel from the given location and compare its RGBA value to the
 * given expected values.
 *
 * Print a log message if the color value deviates from the expected value.
 * \return true if the color values match, false otherwise
 */
int piglit_probe_pixel_rgba(int x, int y, const float* expected)
{
	GLfloat probe[4];
	int i;
	GLboolean pass = GL_TRUE;

	glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, probe);

	for(i = 0; i < 4; ++i) {
		if (fabs(probe[i] - expected[i]) > tolerance[i]) {
			pass = GL_FALSE;
		}
	}

	if (pass)
		return 1;

	printf("Probe at (%i,%i)\n", x, y);
	printf("  Expected: %f %f %f %f\n", expected[0], expected[1], expected[2], expected[3]);
	printf("  Observed: %f %f %f %f\n", probe[0], probe[1], probe[2], probe[3]);

	return 0;
}

int
piglit_probe_rect_rgba(int x, int y, int w, int h, const float *expected)
{
	int i, j, p;
	GLfloat *probe;
	GLfloat *pixels = malloc(w*h*4*sizeof(float));

	glReadPixels(x, y, w, h, GL_RGBA, GL_FLOAT, pixels);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			probe = &pixels[(j*w+i)*4];

			for (p = 0; p < 4; ++p) {
				if (fabs(probe[p] - expected[p]) >= tolerance[p]) {
					printf("Probe at (%i,%i)\n", x+i, y+j);
					printf("  Expected: %f %f %f %f\n",
					       expected[0], expected[1], expected[2], expected[3]);
					printf("  Observed: %f %f %f %f\n",
					       probe[0], probe[1], probe[2], probe[3]);

					free(pixels);
					return 0;
				}
			}
		}
	}

	free(pixels);
	return 1;
}

/**
 * Read a pixel from the given location and compare its RGB value to the
 * given expected values.
 *
 * Print a log message if the color value deviates from the expected value.
 * \return true if the color values match, false otherwise
 */
int piglit_probe_pixel_rgb(int x, int y, const float* expected)
{
	GLfloat probe[3];
	int i;
	GLboolean pass = GL_TRUE;

	glReadPixels(x, y, 1, 1, GL_RGB, GL_FLOAT, probe);


	for(i = 0; i < 3; ++i) {
		if (fabs(probe[i] - expected[i]) > tolerance[i]) {
			pass = GL_FALSE;
		}
	}

	if (pass)
		return 1;

	printf("Probe at (%i,%i)\n", x, y);
	printf("  Expected: %f %f %f\n", expected[0], expected[1], expected[2]);
	printf("  Observed: %f %f %f\n", probe[0], probe[1], probe[2]);

	return 0;
}

int
piglit_probe_rect_rgb(int x, int y, int w, int h, const float *expected)
{
	int i, j, p;
	GLfloat *probe;
	GLfloat *pixels = malloc(w*h*3*sizeof(float));

	glReadPixels(x, y, w, h, GL_RGB, GL_FLOAT, pixels);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			probe = &pixels[(j*w+i)*3];

			for (p = 0; p < 3; ++p) {
				if (fabs(probe[p] - expected[p]) >= tolerance[p]) {
					printf("Probe at (%i,%i)\n", x+i, y+j);
					printf("  Expected: %f %f %f\n",
					       expected[0], expected[1], expected[2]);
					printf("  Observed: %f %f %f\n",
					       probe[0], probe[1], probe[2]);

					free(pixels);
					return 0;
				}
			}
		}
	}

	free(pixels);
	return 1;
}

/**
 * Read a pixel from the given location and compare its depth value to the
 * given expected value.
 *
 * Print a log message if the depth value deviates from the expected value.
 * \return true if the depth value matches, false otherwise
 */
int piglit_probe_pixel_depth(int x, int y, float expected)
{
	GLfloat probe;
	GLfloat delta;

	glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &probe);

	delta = probe - expected;
	if (fabs(delta) < 0.01)
		return 1;

	printf("Probe at (%i,%i)\n", x, y);
	printf("  Expected: %f\n", expected);
	printf("  Observed: %f\n", probe);

	return 0;
}

int piglit_probe_rect_depth(int x, int y, int w, int h, float expected)
{
	int i, j;
	GLfloat *probe;
	GLfloat *pixels = malloc(w*h*sizeof(float));

	glReadPixels(x, y, w, h, GL_DEPTH_COMPONENT, GL_FLOAT, pixels);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			probe = &pixels[j*w+i];

			if (fabs(*probe - expected) >= 0.01) {
				printf("Probe at (%i,%i)\n", x+i, y+j);
				printf("  Expected: %f\n", expected);
				printf("  Observed: %f\n", *probe);

				free(pixels);
				return 0;
			}
		}
	}

	free(pixels);
	return 1;
}

/**
 * Read a texel from the given location and compare its RGBA value to the
 * given expected values.
 *
 * Print a log message if the color value deviates from the expected value.
 * \return true if the color values match, false otherwise
 */
int piglit_probe_texel_rgba(int target, int level, int x, int y,
			    const float* expected)
{
	GLfloat *buffer;
	GLfloat *probe;
	GLfloat delta[4];
	GLfloat deltamax;
	int i;
	GLint width;
	GLint height;

	glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
	buffer = malloc(width * height * 4 * sizeof(GLfloat));

	glGetTexImage(target, level, GL_RGBA, GL_FLOAT, buffer);

	probe = &buffer[4 * ((width * y) + x)];
	deltamax = 0.0;
	for(i = 0; i < 4; ++i) {
		delta[i] = probe[i] - expected[i];
		if (fabs(delta[i]) > deltamax)
			deltamax = fabs(delta[i]);
	}

	if (deltamax < 0.01) {
		free(buffer);
		return 1;
	}

	printf("Probe at (%i,%i)\n", x, y);
	printf("  Expected: %f %f %f %f\n", expected[0], expected[1], expected[2], expected[3]);
	printf("  Observed: %f %f %f %f\n", probe[0], probe[1], probe[2], probe[3]);

	free(buffer);
	return 0;
}

/**
 * Read a texel from the given location and compare its RGBA value to the
 * given expected values.
 *
 * Print a log message if the color value deviates from the expected value.
 * \return true if the color values match, false otherwise
 */
int piglit_probe_texel_rgb(int target, int level, int x, int y,
			   const float* expected)
{
	GLfloat *buffer;
	GLfloat *probe;
	GLfloat delta[3];
	GLfloat deltamax;
	int i;
	GLint width;
	GLint height;

	glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
	buffer = malloc(width * height * 3 * sizeof(GLfloat));

	glGetTexImage(target, level, GL_RGB, GL_FLOAT, buffer);

	probe = &buffer[3 * ((width * y) + x)];
	deltamax = 0.0;
	for(i = 0; i < 3; ++i) {
		delta[i] = probe[i] - expected[i];
		if (fabs(delta[i]) > deltamax)
			deltamax = fabs(delta[i]);
	}

	if (deltamax < 0.01) {
		free(buffer);
		return 1;
	}

	printf("Probe at (%i,%i)\n", x, y);
	printf("  Expected: %f %f %f\n", expected[0], expected[1], expected[2]);
	printf("  Observed: %f %f %f\n", probe[0], probe[1], probe[2]);

	free(buffer);
	return 0;
}

int piglit_use_fragment_program(void)
{
	static const char source[] =
		"!!ARBfp1.0\n"
		"MOV	result.color, fragment.color;\n"
		"END\n"
		;

	glewInit();
	if (!GLEW_ARB_fragment_program)
		return 0;

	piglit_ARBfp_pass_through =
		piglit_compile_program(GL_FRAGMENT_PROGRAM_ARB, source);

	return (piglit_ARBfp_pass_through != 0);
}

void piglit_require_fragment_program(void)
{
	if (!piglit_use_fragment_program()) {
		printf("GL_ARB_fragment_program not supported.\n");
		piglit_report_result(PIGLIT_SKIP);
		exit(1);
	}
}

int piglit_use_vertex_program(void)
{
	glewInit();
	return GLEW_ARB_vertex_program;
}

void piglit_require_vertex_program(void)
{
	if (!piglit_use_vertex_program()) {
		printf("GL_ARB_vertex_program not supported.\n");
		piglit_report_result(PIGLIT_SKIP);
		exit(1);
	}
}

GLuint piglit_compile_program(GLenum target, const char* text)
{
	GLuint program;
	GLint errorPos;

	glGenProgramsARB(1, &program);
	glBindProgramARB(target, program);
	glProgramStringARB(
			target,
			GL_PROGRAM_FORMAT_ASCII_ARB,
			strlen(text),
			(const GLubyte *)text);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
	if (glGetError() != GL_NO_ERROR || errorPos != -1) {
		int l = FindLine(text, errorPos);
		int a;

		fprintf(stderr, "Compiler Error (pos=%d line=%d): %s\n",
			errorPos, l,
			(char *) glGetString(GL_PROGRAM_ERROR_STRING_ARB));

		for (a=-10; a<10; a++)
		{
			if (errorPos+a < 0)
				continue;
			if (errorPos+a >= strlen(text))
				break;
			fprintf(stderr, "%c", text[errorPos+a]);
		}
		fprintf(stderr, "\nin program:\n%s", text);
		piglit_report_result(PIGLIT_FAILURE);
	}
	if (!glIsProgramARB(program)) {
		fprintf(stderr, "glIsProgramARB failed\n");
		piglit_report_result(PIGLIT_FAILURE);
	}

	return program;
}

/**
 * Convenience function to compile a GLSL shader from a file.
 */
GLuint
piglit_compile_shader(GLenum target, char *filename)
{
	GLuint prog;
	struct stat st;
	int err;
	GLchar *prog_string;
	FILE *f;
	const char *source_dir;
	char filename_with_path[FILENAME_MAX];

	source_dir = getenv("PIGLIT_SOURCE_DIR");
	if (source_dir == NULL) {
		source_dir = SOURCE_DIR;
	}

	snprintf(filename_with_path, FILENAME_MAX - 1,
		 "%s/tests/%s", source_dir, filename);
	filename_with_path[FILENAME_MAX - 1] = 0;

	err = stat(filename_with_path, &st);
	if (err == -1) {
		fprintf(stderr, "Couldn't stat program %s: %s\n", filename, strerror(errno));
		fprintf(stderr, "You can override the source dir by setting the PIGLIT_SOURCE_DIR environment variable.\n");
		exit(1);
	}

	prog_string = malloc(st.st_size + 1);
	if (prog_string == NULL) {
		fprintf(stderr, "malloc\n");
		exit(1);
	}

	f = fopen(filename_with_path, "r");
	if (f == NULL) {
		fprintf(stderr, "Couldn't open program: %s\n", strerror(errno));
		exit(1);
	}
	fread(prog_string, 1, st.st_size, f);
	prog_string[st.st_size] = '\0';
	fclose(f);

	prog = piglit_compile_shader_text(target, prog_string);

	free(prog_string);

	return prog;
}

/**
 * Convenience function to compile a GLSL shader.
 */
GLuint
piglit_compile_shader_text(GLenum target, const char *text)
{
	GLuint prog;
	GLint ok;

	prog = glCreateShader(target);
	glShaderSource(prog, 1, (const GLchar **) &text, NULL);
	glCompileShader(prog);

	glGetShaderiv(prog, GL_COMPILE_STATUS, &ok);

	{
		GLchar *info;
		GLint size;

		glGetShaderiv(prog, GL_INFO_LOG_LENGTH, &size);
		info = malloc(size);

		glGetShaderInfoLog(prog, size, NULL, info);
		if (!ok) {
			fprintf(stderr, "Failed to compile %s: %s\n",
				target == GL_FRAGMENT_SHADER ? "FS" : "VS",
				info);
		}
		else if (0) {
			/* Enable this to get extra compilation info.
			 * Even if there's no compilation errors, the info
			 * log may have some remarks.
			 */
			fprintf(stderr, "Shader compiler warning: %s\n", info);
		}
		free(info);
	}

	return prog;
}

GLboolean
piglit_link_check_status(GLint prog)
{
	GLchar *info;
	GLint size;
	GLint ok;

	glGetProgramiv(prog, GL_LINK_STATUS, &ok);

	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &size);
	info = malloc(size);

	glGetProgramInfoLog(prog, size, NULL, info);
	if (!ok) {
		fprintf(stderr, "Failed to link: %s\n", info);
	}
	else if (0) {
		/* Enable this to get extra linking info.
		 * Even if there's no link errors, the info log may
		 * have some remarks.
		 */
		fprintf(stderr, "Linker warning: %s\n", info);
	}

	free(info);

	return ok;
}

GLint piglit_link_simple_program(GLint vs, GLint fs)
{
	GLint prog;

	prog = glCreateProgram();
	if (fs)
		glAttachShader(prog, fs);
	if (vs)
		glAttachShader(prog, vs);
	glLinkProgram(prog);

	piglit_link_check_status(prog);

	return prog;
}

void
piglit_escape_exit_key(unsigned char key, int x, int y)
{
	(void) x;
	(void) y;
	switch (key) {
		case 27:
			exit(0);
			break;
	}
	glutPostRedisplay();
}

/**
 * Convenience function to draw an axis-aligned rectangle.
 */
GLvoid
piglit_draw_rect(float x, float y, float w, float h)
{
	float verts[4][4];

	verts[0][0] = x;
	verts[0][1] = y;
	verts[0][2] = 0.0;
	verts[0][3] = 1.0;
	verts[1][0] = x + w;
	verts[1][1] = y;
	verts[1][2] = 0.0;
	verts[1][3] = 1.0;
	verts[2][0] = x + w;
	verts[2][1] = y + h;
	verts[2][2] = 0.0;
	verts[2][3] = 1.0;
	verts[3][0] = x;
	verts[3][1] = y + h;
	verts[3][2] = 0.0;
	verts[3][3] = 1.0;

	glVertexPointer(4, GL_FLOAT, 0, verts);
	glEnableClientState(GL_VERTEX_ARRAY);

	glDrawArrays(GL_QUADS, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);
}

/**
 * Convenience function to draw an axis-aligned rectangle.
 */
GLvoid
piglit_draw_rect_z(float z, float x, float y, float w, float h)
{
	float verts[4][4];

	verts[0][0] = x;
	verts[0][1] = y;
	verts[0][2] = z;
	verts[0][3] = 1.0;
	verts[1][0] = x + w;
	verts[1][1] = y;
	verts[1][2] = z;
	verts[1][3] = 1.0;
	verts[2][0] = x + w;
	verts[2][1] = y + h;
	verts[2][2] = z;
	verts[2][3] = 1.0;
	verts[3][0] = x;
	verts[3][1] = y + h;
	verts[3][2] = z;
	verts[3][3] = 1.0;

	glVertexPointer(4, GL_FLOAT, 0, verts);
	glEnableClientState(GL_VERTEX_ARRAY);

	glDrawArrays(GL_QUADS, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);
}

/**
 * Convenience function to draw an axis-aligned rectangle
 * with texture coordinates.
 */
GLvoid
piglit_draw_rect_tex(float x, float y, float w, float h,
                     float tx, float ty, float tw, float th)
{
	float verts[4][4];
	float tex[4][2];

	verts[0][0] = x;
	verts[0][1] = y;
	verts[0][2] = 0.0;
	verts[0][3] = 1.0;
	tex[0][0] = tx;
	tex[0][1] = ty;
	verts[1][0] = x + w;
	verts[1][1] = y;
	verts[1][2] = 0.0;
	verts[1][3] = 1.0;
	tex[1][0] = tx + tw;
	tex[1][1] = ty;
	verts[2][0] = x + w;
	verts[2][1] = y + h;
	verts[2][2] = 0.0;
	verts[2][3] = 1.0;
	tex[2][0] = tx + tw;
	tex[2][1] = ty + th;
	verts[3][0] = x;
	verts[3][1] = y + h;
	verts[3][2] = 0.0;
	verts[3][3] = 1.0;
	tex[3][0] = tx;
	tex[3][1] = ty + th;

	glVertexPointer(4, GL_FLOAT, 0, verts);
	glTexCoordPointer(2, GL_FLOAT, 0, tex);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glDrawArrays(GL_QUADS, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}


/**
 * Convenience function to configure projection matrix for window coordinates
 */
void
piglit_ortho_projection(int w, int h, GLboolean push)
{
        /* Set up projection matrix so we can just draw using window
         * coordinates.
         */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
	if (push)
		glPushMatrix();
        glOrtho(0, w, 0, h, -1, 1);

        glMatrixMode(GL_MODELVIEW);
	if (push)
		glPushMatrix();
        glLoadIdentity();
}


/**
 * Generate a checkerboard texture
 *
 * \param tex                Name of the texture to be used.  If \c tex is
 *                           zero, a new texture name will be generated.
 * \param level              Mipmap level the checkerboard should be written to
 * \param width              Width of the texture image
 * \param height             Height of the texture image
 * \param horiz_square_size  Size of each checkerboard tile along the X axis
 * \param vert_square_size   Size of each checkerboard tile along the Y axis
 * \param black              RGBA color to be used for "black" tiles
 * \param white              RGBA color to be used for "white" tiles
 *
 * A texture with alternating black and white squares in a checkerboard
 * pattern is generated.  The texture data is written to LOD \c level of
 * the texture \c tex.
 *
 * If \c tex is zero, a new texture created.  This texture will have several
 * texture parameters set to non-default values:
 *
 *  - S and T wrap modes will be set to \c GL_CLAMP_TO_BORDER.
 *  - Border color will be set to { 1.0, 0.0, 0.0, 1.0 }.
 *  - Min and mag filter will be set to \c GL_NEAREST.
 *
 * \return
 * Name of the texture.  In addition, this texture will be bound to the
 * \c GL_TEXTURE_2D target of the currently active texture unit.
 */
GLuint
piglit_checkerboard_texture(GLuint tex, unsigned level,
			    unsigned width, unsigned height,
			    unsigned horiz_square_size,
			    unsigned vert_square_size,
			    const float *black, const float *white)
{
	static const GLfloat border_color[4] = { 1.0, 0.0, 0.0, 1.0 };
	unsigned i;
	unsigned j;

	float *const tex_data = malloc(width * height * (4 * sizeof(float)));
	float *texel = tex_data;

	for (i = 0; i < height; i++) {
		const unsigned row = i / vert_square_size;

		for (j = 0; j < width; j++) {
			const unsigned col = j / horiz_square_size;

			if ((row ^ col) & 1) {
				memcpy(texel, white, 4 * sizeof(float));
			} else {
				memcpy(texel, black, 4 * sizeof(float));
			}

			texel += 4;
		}
	}


	if (tex == 0) {
		glGenTextures(1, &tex);

		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
				GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
				GL_CLAMP_TO_BORDER);
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
				 border_color);
	} else {
		glBindTexture(GL_TEXTURE_2D, tex);
	}
		
	glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA,
		     GL_FLOAT, tex_data);

	return tex;
}

/**
 * Generates a texture with the given internalFormat, w, h with a
 * teximage of r, g, b w quadrants.
 *
 * Note that for compressed teximages, where the blocking would be
 * problematic, we assign the whole layers at w == 4 to red, w == 2 to
 * green, and w == 1 to blue.
 */
GLuint
piglit_rgbw_texture(GLenum format, int w, int h, GLboolean mip,
		    GLboolean alpha)
{
	GLfloat *data;
	int size, x, y, level;
	GLuint tex;
	float red[4]   = {1.0, 0.0, 0.0, 0.0};
	float green[4] = {0.0, 1.0, 0.0, 0.25};
	float blue[4]  = {0.0, 0.0, 1.0, 0.5};
	float white[4] = {1.0, 1.0, 1.0, 1.0};

	if (!alpha) {
		red[3] = 1.0;
		green[3] = 1.0;
		blue[3] = 1.0;
		white[3] = 1.0;
	}

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (mip) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_LINEAR_MIPMAP_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_NEAREST);
	}
	data = malloc(w * h * 4 * sizeof(GLfloat));

	/* XXX: Do we want non-square textures?  Surely some day. */
	assert(w == h);

	for (level = 0, size = w; size > 0; level++, size >>= 1) {
		for (y = 0; y < size; y++) {
			for (x = 0; x < size; x++) {
				const float *color;

				if (x < size / 2 && y < size / 2)
					color = red;
				else if (y < size / 2)
					color = green;
				else if (x < size / 2)
					color = blue;
				else
					color = white;

				switch (format) {
				case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
				case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
				case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
				case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
				case GL_COMPRESSED_RGB_FXT1_3DFX:
				case GL_COMPRESSED_RGBA_FXT1_3DFX:
					if (size == 4)
						color = red;
					else if (size == 2)
						color = green;
					else if (size == 1)
						color = blue;
					break;
				default:
					break;
				}

				memcpy(data + (y * size + x) * 4, color,
				       4 * sizeof(float));
			}
		}
		glTexImage2D(GL_TEXTURE_2D, level,
			     format,
			     size, size, 0,
			     GL_RGBA, GL_FLOAT, data);

		if (!mip)
			break;
	}
	free(data);
	return tex;
}

GLuint
piglit_depth_texture(GLenum internalformat, int w, int h, GLboolean mip)
{
	void *data;
	float *f = NULL;
	unsigned int  *i = NULL;
	int size, x, y, level;
	GLuint tex;
	GLenum type, format;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (mip) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_LINEAR_MIPMAP_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_NEAREST);
	}
	data = malloc(w * h * 4 * sizeof(GLfloat));

	/* XXX: Do we want non-square textures?  Surely some day. */
	assert(w == h);

	if (internalformat == GL_DEPTH_STENCIL_EXT ||
	    internalformat == GL_DEPTH24_STENCIL8_EXT) {
		format = GL_DEPTH_STENCIL_EXT;
		type = GL_UNSIGNED_INT_24_8_EXT;
		i = data;
	} else {
		format = GL_DEPTH_COMPONENT;
		type = GL_FLOAT;
		f = data;
	}

	for (level = 0, size = w; size > 0; level++, size >>= 1) {
		for (y = 0; y < size; y++) {
			for (x = 0; x < size; x++) {
				float val = (float)(x) / (w - 1);
				if (f)
					f[y * size + x] = val;
				else
					i[y * size + x] = 0xffffff00 * val;
			}
		}
		glTexImage2D(GL_TEXTURE_2D, level,
			     internalformat,
			     size, size, 0,
			     format, type, data);

		if (!mip)
			break;
	}
	free(data);
	return tex;
}

#ifndef HAVE_STRCHRNUL
char *strchrnul(const char *s, int c)
{
	char *t = strchr(s, c);

	return (t == NULL) ? ((char *) s + strlen(s)) : t;
}
#endif
