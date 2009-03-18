/*
 * Copyright © 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Ian Romanick <ian.d.romanick@intel.com>
 */

/**
 * \file occlusion_query.c
 * Simple test for GL_ARB_occlusion_query.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "piglit-util.h"

static int Width = 180, Height = 100;
static int Automatic = 0;

#define MAX_QUERIES 5
static GLuint occ_queries[MAX_QUERIES];
static PFNGLGENQUERIESPROC gen_queries = NULL;
static PFNGLBEGINQUERYPROC begin_query = NULL;
static PFNGLENDQUERYPROC end_query = NULL;
static PFNGLGETQUERYOBJECTIVPROC get_query_objectiv = NULL;


static void draw_box(float x, float y, float z, float w, float h)
{
	glBegin(GL_QUADS);
	glVertex3f(x, y, z);
	glVertex3f(x + w, y, z);
	glVertex3f(x + w, y + h, z);
	glVertex3f(x, y + h, z);
	glEnd();
}


static int check_result(GLint passed, GLint expected)
{
	printf("samples passed = %d, expected = %d\n", passed, expected);
	return (expected == passed) ? 1 : 0;
}


static int do_test(float x, int all_at_once)
{
	static const struct {
		float x;
		float y;
		float z;
		float w;
		float h;
		int expected;
		GLubyte color[3];
	} tests[MAX_QUERIES] = {
		{
			25.0f, 25.0f, 0.2f, 20.0f, 20.0f, 20 * 20,
			{ 0x00, 0xff, 0x00 }
		},
		{
			45.0f, 45.0f, -0.2f, 20.0f, 20.0f, 0,
			{ 0x00, 0x7f, 0xf0 }
		},
		{
			10.0f, 10.0f, -0.3f, 75.0f, 75.0f,
			(75 * 75) - (55 * 55),
			{ 0x00, 0x00, 0xff }
		},
		{
			20.0f, 20.0f, -0.1f, 55.0f, 55.0f, 0,
			{ 0x7f, 0x7f, 0x00 }
		},
		{
			50.0f, 25.0f, 0.2f, 20.0f, 20.0f, 20 * 20,
			{ 0x00, 0x7f, 0xf0 }
		},
	};
	GLint passed;
	int test_pass = 1;
	unsigned i;


	/* Draw an initial red box that is 2500 pixels.  All of the occlusion
	 * query measurements are relative to this box.
	 */
	glColor3ub(0xff, 0x00, 0x00);
	draw_box(x + 20.0f, 20.0f, 0.0f, 55.0f, 55.0f);

	for (i = 0; i < MAX_QUERIES; i++) {
		(*begin_query)(GL_SAMPLES_PASSED, occ_queries[i]);
		glColor3ubv(tests[i].color);
		draw_box(x + tests[i].x, tests[i].y, tests[i].z,
			 tests[i].w, tests[i].h);
		(*end_query)(GL_SAMPLES_PASSED);

		if (! all_at_once) {
			(*get_query_objectiv)(occ_queries[i],
					      GL_QUERY_RESULT, &passed);
			test_pass &= check_result(passed, tests[i].expected);
		}
	}


	if (all_at_once) {
		for (i = 0; i < MAX_QUERIES; i++) {
			(*get_query_objectiv)(occ_queries[i], GL_QUERY_RESULT,
					      &passed);
			test_pass &= check_result(passed, tests[i].expected);
		}
	}

	printf("\n");
	return test_pass;
}

static void Redisplay(void)
{
	int test_pass;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	test_pass = do_test(0.0f, 0);
	test_pass &= do_test(85.0f, 1);

	glutSwapBuffers();

	if (Automatic) {
		piglit_report_result(test_pass
				     ? PIGLIT_SUCCESS : PIGLIT_FAILURE);
	}
}


static void Reshape(int width, int height)
{
	Width = width;
	Height = height;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}


static void Init(void)
{
	glewInit();

	Reshape(Width,Height);

	glClearColor(0.0, 0.2, 0.3, 0.0);
	glClearDepth(1.0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);


	if (GLEW_VERSION_1_5) {
		gen_queries = GLEW_GET_FUN(__glewGenQueries);
		begin_query = GLEW_GET_FUN(__glewBeginQuery);
		end_query = GLEW_GET_FUN(__glewEndQuery);
		get_query_objectiv = GLEW_GET_FUN(__glewGetQueryObjectiv);
	} else if (GLEW_ARB_occlusion_query) {
		gen_queries = GLEW_GET_FUN(__glewGenQueriesARB);
		begin_query = GLEW_GET_FUN(__glewBeginQueryARB);
		end_query = GLEW_GET_FUN(__glewEndQueryARB);
		get_query_objectiv = GLEW_GET_FUN(__glewGetQueryObjectivARB);
	} else {
		piglit_report_result(PIGLIT_SKIP);
		exit(1);
	}

	(*gen_queries)(MAX_QUERIES, occ_queries);
}

static void Key(unsigned char key, int x, int y)
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


int main(int argc, char *argv[])
{
	int i;
	glutInit(&argc, argv);
	if (argc == 2 && !strcmp(argv[1], "-auto"))
		Automatic = 1;
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(Width, Height);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutCreateWindow(argv[0]);
	glutReshapeFunc(Reshape);
	glutDisplayFunc(Redisplay);
	if (!Automatic)
		glutKeyboardFunc(Key);
	Init();
	glutMainLoop();
	return 0;
}

