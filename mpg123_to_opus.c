#include <stdio.h>
#include <strings.h>
#include <mpg123.h>
#include <opus/opusenc.h>
#include <id3v2lib.h>

#define FIELD_TITLE "TITLE"
#define FIELD_ARTIST "ARTIST"
#define FIELD_ALBUM "ALBUM"
#define FIELD_ALBUMARTIST "ALBUMARTIST"
#define FIELD_TRACKNUMBER "TRACKNUMBER"
#define FIELD_GENRE "GENRE"
#define FIELD_DATE "DATE"

void usage()
{
	printf("Usage: mpg123_to_opus <input> <output>\n");
	exit(99);
}

void cleanup(mpg123_handle *mh)
{
	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();
}

int main(int argc, char *argv[])
{
	mpg123_handle *mh = NULL;
	size_t done = 0;
	int  channels = 0, encoding = 0;
	long rate = 0;
	int  err  = MPG123_OK;
	off_t samples = 0;

	OggOpusComments *comments;
	OggOpusEnc *enc;

	if (argc<3) {
		usage();
	}
	printf( "Input file: %s\n", argv[1]);
	printf( "Output file: %s\n", argv[2]);

	err = mpg123_init();
	mh = mpg123_new(NULL, &err);
	err = mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_PICTURE, 1);
	if(err != MPG123_OK || mh == NULL)
	{
		fprintf(stderr, "Basic setup goes wrong: %s", mpg123_plain_strerror(err));
		cleanup(mh);
		return -1;
	}

	/* Simple hack to enable floating point output. */
	// if(argc >= 4 && !strcmp(argv[3], "f32")) mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT, 0.);

	/* Let mpg123 work with the file, that excludes MPG123_NEED_MORE messages. */
	if(mpg123_open(mh, argv[1]) != MPG123_OK
	    /* Peek into track and get first output format. */
	    || mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK )
	{
		fprintf( stderr, "Trouble with mpg123: %s\n", mpg123_strerror(mh) );
		cleanup(mh);
		return -1;
	}

	printf("Sampling rate: %lu\n", rate);
	printf("Channels: %d\n", channels);
	printf("Encoding: %d\n", encoding);
	comments = ope_comments_create();

	mpg123_id3v1 *v1 = NULL;
	mpg123_id3v2 *v2 = NULL;
	mpg123_id3(mh, &v1, &v2);

	if (v2) {
		if(v2->title) {
			ope_comments_add(comments, FIELD_TITLE, v2->title->p);
		}
		if(v2->artist) {
			ope_comments_add(comments, FIELD_ARTIST, v2->artist->p);
		}
		if(v2->album) {
			ope_comments_add(comments, FIELD_ALBUM, v2->album->p);
		}
		if(v2->album_artist) {
			ope_comments_add(comments, FIELD_ALBUMARTIST, v2->album_artist->p);
		}
		if(v2->track_no) {
			ope_comments_add(comments, FIELD_TRACKNUMBER, v2->track_no->p);
		}
		if(v2->genre) {
			ope_comments_add(comments, FIELD_GENRE, v2->genre->p);
		}
		if(v2->year) {
			ope_comments_add(comments, FIELD_DATE, v2->year->p);
		}
		if(v2->picture) {
			ope_comments_add_picture_from_memory(comments, v2->picture->data, v2->picture->size, -1, NULL);
		}
		// ope_comments_add(comments, "DESCRIPTION", "hello world");
	}
	else if (v1) {
		// handle id3v1 data
	}
	else {
		fprintf(stderr, "No ID3 metadata\n");
	}
	

	if(encoding != MPG123_ENC_SIGNED_16 && encoding != MPG123_ENC_FLOAT_32)
	{ /* Signed 16 is the default output format anyways; it would actually by only different if we forced it.
	     So this check is here just for this explanation. */
		cleanup(mh);
		fprintf(stderr, "Bad encoding: 0x%x!\n", encoding);
		return -2;
	}
	/* Ensure that this output format will not change (it could, when we allow it). */
	mpg123_format_none(mh);
	mpg123_format(mh, rate, channels, encoding);

	enc = ope_encoder_create_file(argv[2], comments, rate, channels, 0, NULL);
	if (!enc) {
		printf("ope encoder create file error\n");
		return -1;
	}

	size_t buf_size = mpg123_outblock(mh);
	short buf[2*buf_size];

	puts("start run");
	do
	{
		err = mpg123_read(mh, buf, sizeof(buf), &done);
		ope_encoder_write(enc, buf, buf_size);
		samples += done;
	} while (err==MPG123_OK);

	if(err != MPG123_DONE)
	fprintf( stderr, "Warning: Decoding ended prematurely because: %s\n",
	         err == MPG123_ERR ? mpg123_strerror(mh) : mpg123_plain_strerror(err) );

	samples /= channels;
	printf("%li samples written.\n", (long)samples);
	cleanup(mh);
	ope_comments_destroy(comments);
	ope_encoder_drain(enc);
	ope_encoder_destroy(enc);
	return 0;
}
