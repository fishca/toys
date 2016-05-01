/*
	Image crossfading --- SSE implementation, $Revision: 1.10 $

	Author: Wojciech Mu�a
	e-mail: wojciech_mula@poczta.onet.pl
	www:    http://0x80.pl/

	License: BSD

	initial release 5-06-2008, last update $Date: 2008-06-21 22:15:11 $

	----------------------------------------------------------------------

	Comparison of three crossfading procedures:
	1. x86    --- naive C implementation
	2. sse4   --- SSE implementation
	3. sse4-2 --- SSE4 implementation, that use new instruction PMADDUBSW 
*/

#ifndef _XOPEN_SOURCE
#	define _XOPEN_SOURCE 600
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <immintrin.h>
#include <emmintrin.h>

#ifdef USE_Xscr
#include "../Xscr/Xscr.h"
#include "../loadppm/load_ppm.h"
#endif


//=== global variables ===================================================
int width, height, maxval;
uint8_t *imgA, *imgB, *data;
uint8_t alpha;

int function = 0;


#define OPT_COUNT 3

static char* function_name[OPT_COUNT] = {
	"x86", "SSE4", "SSE4-2"
};


static char* function_name_abbr[OPT_COUNT] = {
	"x86", "SSE4", "SSE4-2"
};



//=== common functions ===================================================
uint32_t getTime(void) {
	static struct timeval T;
	gettimeofday(&T, NULL);
	return (T.tv_sec * 1000000) + T.tv_usec;
}



void die(const char* fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	putchar('\n');
	va_end(ap);

	exit(2);
}



//=== crossfading implementations ========================================
void SSE4_blend() {
	size_t n = width * height * 4;

    __m128i alpha_pos = _mm_set1_epi16(256 * (uint16_t)alpha);
    __m128i alpha_neg = _mm_set1_epi16(256 * (uint16_t)(~alpha));

    for (size_t i=0; i < n; i += 16) {
        __m128i A = _mm_load_si128((__m128i*)(imgA + i));
        __m128i B = _mm_load_si128((__m128i*)(imgB + i));

        __m128i A0 = _mm_cvtepu8_epi16(A);
        __m128i B0 = _mm_cvtepu8_epi16(B);
        __m128i A1 = _mm_cvtepu8_epi16(_mm_srli_si128(A, 8));
        __m128i B1 = _mm_cvtepu8_epi16(_mm_srli_si128(B, 8));

        A0 = _mm_mulhi_epu16(A0, alpha_pos);
        A1 = _mm_mulhi_epu16(A1, alpha_pos);
        B0 = _mm_mulhi_epu16(B0, alpha_neg);
        B1 = _mm_mulhi_epu16(B1, alpha_neg);

        A  = _mm_packus_epi16(A0, A1);
        B  = _mm_packus_epi16(B0, B1);

        _mm_store_si128((__m128i*)(data + i), _mm_adds_epu8(A, B));
    }
}


void SSE42_blend() {
	int n = width * height * 4;
	int dummy __attribute__((unused));

    const uint8_t alpha2 = alpha/2;
    __m128i alpha_np = _mm_set1_epi16(alpha2 | ((uint16_t)(alpha2 ^ 0x7f) << 8));

    for (size_t i=0; i < n; i += 32) {
        __m128i A0 = _mm_load_si128((__m128i*)(imgA + i));
        __m128i B0 = _mm_load_si128((__m128i*)(imgB + i));

        __m128i A1 = _mm_load_si128((__m128i*)(imgA + i + 16));
        __m128i B1 = _mm_load_si128((__m128i*)(imgB + i + 16));

        __m128i lo0 = _mm_unpacklo_epi8(A0, B0);
        __m128i hi0 = _mm_unpackhi_epi8(A0, B0);

        __m128i lo1 = _mm_unpacklo_epi8(A1, B1);
        __m128i hi1 = _mm_unpackhi_epi8(A1, B1);

        lo0 = _mm_maddubs_epi16(lo0, alpha_np);
        lo1 = _mm_maddubs_epi16(lo1, alpha_np);
        hi0 = _mm_maddubs_epi16(hi0, alpha_np);
        hi1 = _mm_maddubs_epi16(hi1, alpha_np);

        lo0 = _mm_srli_epi16(lo0, 7);
        lo1 = _mm_srli_epi16(lo1, 7);
        hi0 = _mm_srli_epi16(hi0, 7);
        hi1 = _mm_srli_epi16(hi1, 7);

        __m128i res0 = _mm_packus_epi16(lo0, hi0);
        __m128i res1 = _mm_packus_epi16(lo1, hi1);

        _mm_store_si128((__m128i*)(data + i +  0), res0);
        _mm_store_si128((__m128i*)(data + i + 16), res1);
    }
}


void blend() {
	int n = width * height * 4;

	uint8_t *iA = imgA;
	uint8_t *iB = imgB;
	uint8_t *iD = data;

	uint32_t a1 = alpha;
	uint32_t a2 = 255 - alpha;

	uint32_t R, G, B;
	uint32_t R1, G1, B1;
	uint32_t R2, G2, B2;

	while (n--) {
		R1 =  (*iA) & 0xff;
		G1 = ((*iA) >>  8) & 0xff;
		B1 = ((*iA) >> 16) & 0xff;
		
		R2 =  (*iB) & 0xff;
		G2 = ((*iB) >>  8) & 0xff;
		B2 = ((*iB) >> 16) & 0xff;

		R = (R1*a1 + R2*a2) >> 8;
		G = (G1*a1 + G2*a2) >> 8;
		B = (B1*a1 + B2*a2) >> 8;

		if (R > 255) R = 255;
		if (G > 255) G = 255;
		if (B > 255) B = 255;

		(*iD) = R | (G << 8) | (B << 16);

		iA++;
		iB++;
		iD++;
	}
}


//=== X11 procedures =====================================================
#ifdef USE_Xscr
static struct {
	uint32_t sum;
	int32_t n;
} frequency[OPT_COUNT];


void motion(
	int x, int y, Time t,
	unsigned int kb_mask
) {
	static uint8_t old_alpha = 255;

	alpha = (255*x/width);
	if (alpha != old_alpha) {
		uint32_t t1, t2;

		t1 = getTime();
		switch (function) {
			case 0:
				printf("%s", function_name[function]);
				blend();
				break;
			case 1:
				printf("%s", function_name[function]);
				SSE4_blend();
				break;
			case 2:
				printf("%s", function_name[function]);
				SSE42_blend();
				break;
			default:
				return;
		}
		t2 = getTime();
		printf(" = %d us\n", t2-t1);
		Xscr_redraw();
		old_alpha = alpha;

		frequency[function].n   += 1;
		frequency[function].sum += (t2 - t1);
	}
}


void keyboard(
	int x, int y, Time t,
	KeySym c,
	KeyOrButtonState s,
	unsigned int state
) {

	if (s != Pressed)
		return;

	switch (c) {
		case XK_Tab:
		case XK_Return:
		case XK_space:
			function = (function + 1) % OPT_COUNT;
			break;

		case XK_1:
			function = 0;
			break;

		case XK_2:
			function = 1;
			break;

		case XK_3:
			function = 2;
			break;

		case XK_q:
		case XK_Q:
			Xscr_quit();
			break;
	}
}


void view(const char* file1, const char* file2) {
	FILE *f;
	int err;

	int w1, h1, w2, h2;

	f = fopen(file1, "rb");
	if (f == NULL)
		die("Can't open %s", file1);
	err = ppm_load_32bpp(f, &w1, &h1, &maxval, &imgA, 32);
	if (err < 0)
		die("Can't read %s: %s", file1, PPM_errormsg[-err]);
	else
	if ((((uintptr_t)imgA) & 0x0f) != 0x00)
		die("Compile load_ppm library with -DPPM_ALIGN_MALLOC=16.");
	fclose(f);

	f = fopen(file2, "rb");
	if (f == NULL)
		die("Can't open %s", file2);
	ppm_load_32bpp(f, &w2, &h2, &maxval, &imgB, 32);
	if (err < 0)
		die("Can't read %s: %s", file2, PPM_errormsg[-err]);
	else
	if ((((uintptr_t)imgB) & 0x0f) != 0x00)
		die("Compile load_ppm library with -DPPM_ALIGN_MALLOC=16.");
	fclose(f);


	printf("imgA = %d(%d) x %d, imgB = %d(%d) x %d\n",
		w1,
		ppm_bytes_per_line(w1, 4, 32),
		h1,
		w2,
		ppm_bytes_per_line(w2, 4, 32),
		h2
	);

	
	if (ppm_bytes_per_line(w1, 4, 32) != ppm_bytes_per_line(w2, 4, 32)) {
		die("Images should have similar width.");
	}

	width  = ppm_bytes_per_line(w1, 4, 16)/4;
	height = h1 < h2 ? h1 : h2;

	printf("%d x %d\n", width, height);

	if (posix_memalign((void*)&data, 16, width*height*4))
		die("No free memory");

	memcpy(data, imgA, width*height*4);

	int i;
	for (i=0; i < OPT_COUNT; i++) {
		frequency[i].n = 0;
		frequency[i].sum = 0;
	}

	// run mainloop
	err = Xscr_mainloop(
		width, height, DEPTH_32bpp, False, data, 
		keyboard, motion, NULL,
		"Image mixing demo"
	);

	// check exit status
	if (err < 0) {
		printf("Xscr error: %s\n", Xscr_error_str(err));
	}
	else {
		for (i=0; i < OPT_COUNT; i++) {
			printf("function %-6s called %5d time(s) ",
				function_name[i], frequency[i].n
			);
			if (frequency[i].n > 0)
				printf("average time %d us\n",
					frequency[i].sum / frequency[i].n
				);
			else
				putchar('\n');
		}
	}

	free(imgA);
	free(imgB);
	free(data);
}
#endif


//=== test procedure =====================================================
void measure(int function, int repeat_count) {
	uint32_t t1, t2;
	int n = repeat_count;
	
	printf(
		"function %s, called %d times; image %d x %d\n",
		function_name[function],
		repeat_count,
		width, height
	);

	switch (function) {
		case 0:
			t1 = getTime();
			while (n--)
				blend();
			t2 = getTime();
			break;
		
		case 1:
			t1 = getTime();
			while (n--)
				SSE4_blend();
			t2 = getTime();
			break;
		
		case 2:
			t1 = getTime();
			while (n--)
				SSE42_blend();
			t2 = getTime();
			break;
		default:
			return;
	}

	printf("time = %d us\n", t2 - t1);
}


//=== main program =======================================================
void help(char* progname) {
	int i;

	printf("1. %s measure %s", progname, function_name_abbr[0]);

	for (i=1; i < OPT_COUNT; i++)
		printf("|%s", function_name_abbr[i]);
	
	printf(" repeat-count\n");
#ifdef USE_Xscr
	printf("2. %s view file1.ppm file2.ppm\n", progname);
#endif

	exit(1);
}


int main(int argc, char* argv[]) {
	int function;
	int repeat_count;

	// parse args
	if (argc < 2)
		help(argv[0]);

	if (strcasecmp(argv[1], "measure") == 0) {
		if (argc < 4)
			help(argv[0]);

		repeat_count = atoi(argv[3]);
		if (repeat_count < 0)
			help(argv[0]);

		width  = 1024;
		height = 768;
		alpha  = 127;

		imgA = NULL;
		imgB = NULL;
		data = NULL;

		posix_memalign((void*)&imgA, 16, width*height*4);
		if (imgA == NULL) die("No free memory");

		posix_memalign((void*)&imgB, 16, width*height*4);
		if (imgB == NULL) die("No free memory");

		posix_memalign((void*)&data, 16, width*height*4);
		if (data == NULL) die("No free memory");

		for (function=0; function < OPT_COUNT; function++) {
			if (strcasecmp(argv[2], function_name_abbr[function]) == 0) {
				measure(function, repeat_count);
				break;
			}
		}

		free(imgA);
		free(imgB);
		free(data);
		
		if (function == OPT_COUNT)
			help(argv[0]);
		else
			return 0;
	}
#ifdef USE_Xscr
	else
	if (strcasecmp(argv[1], "view") == 0) {
		if (argc < 4)
			help(argv[0]);
		else
			view(argv[2], argv[3]);
	}
#endif
	else
		help(argv[0]);

	
	return 0;
}


// eof
