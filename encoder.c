/**
 * @file encoder.c
 * @author Andreas Waidler
 * @version 1.1.0
 * @date 2017-10-12
 *
 * Base64 Encoder in C99
 *
 * Takes one file name as parameter and encodes its contents as base64.
 *
 * Concept taken from [Wikipedia](https://en.wikipedia.org/wiki/Base64),
 * retrieved on 2017-10-11 14:21.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(FILE *out, FILE *in);
void base64_encode_block(char *buf_out, uint8_t *buf_in, size_t len_in);

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "No file name passed.\n");
		return 1;
	}

	FILE *fin = fopen(argv[1], "r");
	if (!fin) {
		perror("Failed to open file");
		return 2;
	}

	base64_encode(stdout, fin);

	if (fclose(fin)) {
		perror("Failed to close file");
		return 3;
	}

	return 0;
}

/**
 * Maps values 0..63 to a set of 64 (byte-sized) characters.
 *
 * The Base64 encoding input alphabet contains 64 values (surprise),
 * i.e. input character size is lb(64) = 6 bit. However, we can only
 * read in chunks of 8 bit. Thus, we read chunks in the size of the
 * least common multiple, which is 24 (i.e. 3 bytes in, 4 bytes out)
 * and store it in a 32 bit word.
 *
 * Wikipedia says we have to add CR+LF line breaks after 76 characters
 * (i.e. 19 blocks) be complient with MIME (RFC 2045).
 */
void base64_encode(FILE *out, FILE *in)
{
	uint8_t buf_in[3];            // Read blocks that size from input file
	char    buf_out[4 + 1] = {0}; // Contains trailing \0

	bool eof = feof(in);
	while (!eof) {
		for (size_t i = 0; i < 19 && !eof; ++i) {
			size_t nread = fread(buf_in, 1, sizeof buf_in, in);

			if (nread == 0) { // No input block anymore
				assert(feof(in));
				eof = true;
				break;
			}

			base64_encode_block(buf_out, buf_in, nread);

			/* Top byte is zero anyways, so that's our implicit
			 * trailing \0 when treated as string. */
			fprintf(out, "%s", buf_out);
		}
		fprintf(out, "\r\n");
	}
}

void base64_encode_block(char *buf_out, uint8_t *buf_in, size_t len_in)
{
	/* Store input bytes in BIG endian order and works on 24-bit blocks.
	 * That way we do not have to keep track of "leftover" bits.
	 * [Wikipedia (2017-10-11 14:21)](https://en.wikipedia.org/wiki/Base64)
	 * has a neat graphical representation of how the block-based encoding
	 * works.
	 *
	 * Note that we're actually using buffers of 32 bit width.
	 *
	 * We could write these bytes directly to memory adresses and interpret
	 * the bundle as an integer. However, that approach would raise the
	 * issue of big/little endianness. Thus, we rather use arithmetics
	 * and let the compiler do the work.
	 */

	const size_t len_out = (8*len_in + 5)/6; // round up

	uint32_t buf = 0;

	/* Stage 1: Format the buffer properly. */
	for (size_t i = 0; i < len_in; ++i) {
		buf |= ((uint32_t)buf_in[i]) << (8 * (3-i-1));
	}

	/* Stage 2: Do the encoding. */
	uint32_t msk = ((uint32_t)0x3F) << (3*6);
	for (size_t i = 0; i < len_out; ++i, msk >>= 6) {
		buf_out[i] = base64_table[(buf & msk) >> (6*(4-i-1))];
	}

	/* Stage 2.5: Padding if input was "too short". */
	for (size_t i = len_out; i < 4; ++i) {
		buf_out[i] = '=';
	}
}
