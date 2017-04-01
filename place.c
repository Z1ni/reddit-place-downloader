/*
 * Reddit /r/place image downloader
 * Copyright (c) 2017 zini
 * 
 * Requires libcurl and libpng
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <fcntl.h>
#include <png.h>
#include <curl/curl.h>

size_t total_bytes = 0;
size_t total_pixels = 0;
size_t image_data_len = 0;

size_t image_row = 0;
size_t image_col = 0;
png_bytep * image = NULL;

const int colors[16] = {
	0xFFFFFF,
	0xE4E4E4,
	0x888888,
	0x222222,
	0xFFA7D1,
	0xE50000,
	0xE59500,
	0xA06A42,
	0xE5D900,
	0x94E044,
	0x02BE01,
	0x00D3DD,
	0x0083C7,
	0x0000EA,
	0xCF6EE4,
	0x820080
};

const char status[4] = {
	'|', '/', '-', '\\'
};

int status_updates = 0;

void add_to_image_data(int color) {

	if (image_row >= 1000) return;

	size_t real_pos = image_col * 3;
	image[image_row][real_pos]   = (colors[color] >> 16) & 0xFF;	// Red
	image[image_row][real_pos+1] = (colors[color] >> 8) & 0xFF;		// Green
	image[image_row][real_pos+2] = (colors[color] & 0xFF);			// Blue
	image_col++;

	total_pixels++;

	if (image_col > 999) {
		image_col = 0;
		image_row++;
		if (image_row % 5 == 0) {
			printf("Downloading and converting image data... %c\r",
				status[status_updates++ % 4]);
			fflush(stdout);
		}
	}
}

size_t dl_write_callback(char *ptr, size_t size, size_t nmemb, void *user) {
	// Parse received bytes
	// Ignore all data arrived after first 500000 bytes
	if (total_bytes >= 0x7A120) return size * nmemb;

	size_t written = 0;
	for (size_t i = 0; i < (size * nmemb); i++) {
		// One byte contains two pixels
		// One nibble is an index to palette color
		int pix1 = ptr[i] >> 4;
		int pix2 = ptr[i] & 0xF;
		add_to_image_data(pix1);
		add_to_image_data(pix2);
		total_bytes++;
		written++;
		if (image_row > 1000) break;
	}
	return written;
}

int write_image(const char *filename) {
	FILE *f = fopen(filename, "wb");
	if (f == NULL) {
		fprintf(stderr, "File open failed\n");
		return 1;
	}

	png_structp png_ptr = png_create_write_struct(
		PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL
	);
	if (png_ptr == NULL) {
		fprintf(stderr, "libpng init failed\n");
		return 1;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "libpng image creation failed\n");
		return 1;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);

	png_init_io(png_ptr, f);
	png_set_IHDR(
		png_ptr,
		info_ptr,
		1000,	// Width
		1000,	// Height
		8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);

	png_write_info(png_ptr, info_ptr);

	for (size_t i = 0; i < 1000; i++) {
		png_write_row(png_ptr, image[i]);
		if (i % 20 != 0) continue;
		printf("Saving image... %.2f %%\r", (i / 1000.0) * 100.0);
		fflush(stdout);
	}

	//png_write_image(png_ptr, image);
	png_write_end(png_ptr, NULL);
	fclose(f);
	png_destroy_write_struct(&png_ptr, NULL);

	return 0;
}

int main(int argc, char *argv[]) {

	bool silent = false;
	char *filename = NULL;

	int ch = 0;
	while ((ch = getopt(argc, argv, "shf:")) != -1) {
		switch (ch) {
			case 's':
				silent = true;
				break;

			case 'f':
				filename = optarg;	// strdup?
				break;

			case '?':
				// Fall through
			
			case 'h':
			default:
				printf("Reddit /r/place image downloader\nzini 2017\n\n");
				printf("Usage: %s [options] [filename]\n", argv[0]);
				printf("  options:\n");
				printf("    -h       Help\n");
				printf("    -s       Silent (don't print anything)\n");
				printf("    -f file  Saves the downloaded image as 'file'\n");
				return 0;
		}
	}

	if (optind < argc) {
		// Optional filename is supplied
		filename = argv[optind];
	}

	if (filename == NULL) {
		filename = "place.png";
	}

	if (silent) {
		// Redirect stdout to /dev/null
		int dev_null = open("/dev/null", O_WRONLY);
		if (dup2(dev_null, STDOUT_FILENO) == -1) {
			fprintf(stderr, "dup2 failed\n");
			return 1;
		}
	}

	printf("Reddit /r/place image downloader\nzini 2017\n\n");

	printf("Saving to file: %s\n", filename);

	// Start doing work =======================================================

	image = calloc(1000, sizeof(png_bytep));
	for (size_t i = 0; i < 1000; i++) {
		image[i] = calloc(1000 * 3, sizeof(png_byte));	// RGB, 3 bytes per pix
	}

	printf("CURL init...\r");
	fflush(stdout);

	CURL *curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "curl init failed\n");
		return 1;
	}

	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);	// For debugging
	curl_easy_setopt(curl, CURLOPT_URL, 
		"https://www.reddit.com/api/place/board-bitmap");
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dl_write_callback);

	printf("Downloading and converting image data...\r");
	fflush(stdout);
	CURLcode c = curl_easy_perform(curl);
	if (c != CURLE_OK) {
		fprintf(stderr, "curl error: %s\n", curl_easy_strerror(c));
	}

	printf("Saving image...                                     \r");
	fflush(stdout);

	if (write_image(filename) == -1) {
		fprintf(stderr, "Image saving failed!\n");
	} else {
		printf("Image saved             \nSaved as \"place.png\"\n");
	}

	curl_easy_cleanup(curl);

	for (size_t i = 0; i < 1000; i++) {
		free(image[i]);
	}
	free(image);

	return 0;
}
