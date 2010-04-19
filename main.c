/**
 * BitTorrent spec requires dictionaries to be sorted!
 */
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <libgen.h>       // Don't use GNU version of basename
#include <openssl/sha.h>
#include <dirent.h>
#include <time.h>

#include "config.h"

/* to prevent buffer overflows */
#define MAX_RECURSION 29
#define MAX_ANNOUNCE  100

void help_message();
int create_announce( char** const, int, char** const, int, const char*, const char*, const char*, int );
int create_from_file( const char*, FILE*, long long, int );
int create_from_directory( const char*, FILE*, int );
int create_from_assortment( char** const, int, FILE*, int );
void write_name( const char*, FILE* );

int files = 0;
char* comment = NULL;
int inclusive = 0;
int private = 0;
int use_dht = 0;
char* sha = NULL;
int shasize = 0;
int bytesin = 0;
char* buf = NULL;

int main( int argc, char** const argv)
{
	int i;
	int optidx = 0;
	char** announce;
	int no_announce = 0;
	char** src = NULL;
	int no_src = 0;
	char* outputfile = NULL;
	char* port = "6881";
	char* path = "/announce";
	int piecelen = 256 * 1024;
	char dht[60];

	announce = (char**)malloc(sizeof(char*) * MAX_ANNOUNCE);
	announce[0] = NULL;
	while (1) {
		static struct option options[] = {
			{ "announce",	 1, 0, 'a' }, // announce
			{ "help",	 0, 0, 'h' },
			{ "version",	 0, 0, 'V' },
			{ "port",	 1, 0, 'p' },
			{ "path",	 1, 0, 'P' },
			{ "piecelength", 1, 0, 'l' },
			{ "comment",	 1, 0, 'c' },
			{ "inclusive",	 0, 0, 'i' },
			{ "private",	 0, 0, 'x' },
			{ "dht",	 0, 0, 'd' },
			{ 0,		 0, 0, 0   }
		};
		int c = getopt_long(argc, argv, "a:dhixVp:P:l:c:", options, &optidx);
		if (c == -1)
			break;
		switch (c) {
		case '?':
			return 1;
		case 'c':
			comment = optarg;
			break;
		case 'i':
			inclusive = 1;
			break;
		case 'x':
			private = 1;
			break;
		case 'P':
			path = optarg;
			break;
		case 'a':
			if (no_announce <= MAX_ANNOUNCE - 1)
				announce[no_announce++] = optarg;
			else
				fprintf(stderr, "warning: more than %d announce urls. The rest will be ignored.\n", MAX_ANNOUNCE);
			break;
		case 'd':
			use_dht = 1;
			break;
		case 'p':
			port = optarg;
			break;
		case 'h':
			help_message();
			return 0;
		case 'V':
			printf("createtorrent %s\n", VERSION);
			printf("  by: Daniel Etzold <detzold@gmx.de>\n");
			printf("      James M. Leddy <jm.leddy@gmail.com>\n");

			return 0;
		case 'l':
			piecelen = atoi(optarg);
			break;
		}
	}

	if (piecelen <= 0) {
		fprintf( stderr, "error: piece length has to be > 0.\n" );
		return 1;
	}

	if (argc - optind >= 2) {
		no_src = argc - optind - 1;
		src = argv + optind;
		outputfile = argv[ optind + no_src ];
	}

	if (use_dht) {
		if ( no_announce > 0 && announce[0] )
			fprintf(stderr, "warning: both --announce and --dht specified. --dht will be ignored.\n");
		else if ( RAND_MAX < 2147483647 ) {
			fprintf(stderr, "error: random number generator isn't sufficient on this system for --dht.\n"
				"       Use --anounce instead.\n");
			return 1;
		} else {
			strcpy(&dht[0], "dht://");
			srand(time(0));
			for (i = 0; i < 40; i += 8)
				snprintf(&dht[6 + i], 9, "%0X", rand());
			strcat(&dht[0], ".dht/announce");

			no_announce = 1;
			announce[0] = &dht[0];
		}
	}


	if (no_announce > 0 && announce[0] && src &&
	    outputfile && port && path)
		return create_announce(announce, no_announce, src, no_src, outputfile, port, path, piecelen);

	free(announce);

	fprintf(stderr, "Invalid arguments. Use -h for help.\n");
	return 1;
}

int create_announce(char** const announce, int no_announce, char** const src, int no_src, const char* output, const char* port, const char* path, int piecelen)
{
	struct stat s;
	const char* onesrc = *src;
	char *tok;
	int ret = 0;
	int p = atoi(port);
	int i;
	char *creationtag = "createtorrent/" VERSION;


	// "announce", "comment", "creation date", "info"

	FILE* f = fopen( output, "w" );

	if ( f ) {
		// Open main dictionary and write "announce"
		// If announce has a more than 3 slashes, use new method
		tok = strdup( announce[0] );
		strtok(tok, "/");
		strtok(NULL, "/");
		if ( strtok(NULL, "/") == NULL) {
			fprintf(stderr, "warning: deprecated use of announce. Use the full path of the tracker url.\n");
			fprintf(stderr, "     ex: http://example.com/announce O http://example.com:6881/announce.php\n");
			fprintf(f, "d8:announce%d:%s:%s%s",
				strlen(announce[0]) + strlen(path )
				+ strlen(port) + 1, announce[0], port,
				path);
			if (no_announce > 1)
				fprintf(stderr, "warning: cannot use multiple announce with legacy port and path tags\n");
		} else {
			fprintf(f, "d8:announce%d:%s",
				strlen(announce[0]), announce[0]);

			// "announce-list" - BEP 12
			if (no_announce > 1) {
				fprintf(f, "13:announce-listl");
				for (i = 0; i < no_announce; i++) {
					fprintf(f, "l%d:%se",
						strlen( announce[i]),
						announce[i]);
				}
				fputs("e", f);
			}
		}

		// "comment"
		if (comment)
			fprintf(f, "7:comment%d:%s",
				strlen( comment ), comment);

		// "created by"
		fprintf(f, "10:created by%d:%s", strlen( creationtag ),
			creationtag);

		// "creation date"
		fprintf(f, "13:creation datei%de", (unsigned)time(NULL));

		// "info" dictionary
		fputs("4:infod", f);

		// write file info
		if (no_src == 1) {
			if (!stat(onesrc, &s)) {
				if (S_ISDIR(s.st_mode))
					ret = create_from_directory(onesrc, f, piecelen);
				else if (S_ISREG( s.st_mode))
					ret = create_from_file(onesrc, f, s.st_size, piecelen);
				else {
					fprintf(stderr, "not a file or directory: %s\n", onesrc);
					ret = 1;
				}
			} else {
				fprintf(stderr, "could not access %s: %m\n", onesrc);
				ret = 1;
			}
		} else
			create_from_assortment(src, no_src, f, piecelen);


		// private torrent
		if (private)
			fprintf(f, "7:privatei1e", f);

		// Close info and main dictionary
		fputs("ee", f);
		fclose(f);
		if (ret)
			unlink(output);
	} else {
		fprintf(stderr, "could not open destination file %s\n", output);
		ret = 1;
	}
	return ret;
}

void write_name(const char* name, FILE* f)
{
	char* q;
	char* p = strdup(name);

	q = basename(p);

	fprintf(f, "4:name%d:%s", strlen(q), q);
	free(p);
}

int create_from_file(const char* src, FILE* f, long long fsize, int piecelen)
{
	char* sha;
	char* buf;
	int r, shalen;
	FILE* fs = fopen(src, "rb");

	// "length", "name", "piece length", "pieces"

	fprintf(f, "6:lengthi%llde", fsize);
	write_name( src, f );
	fprintf(f, "12:piece lengthi%de", piecelen);

	shalen = fsize / piecelen;
	if (fsize % piecelen)
		shalen++;
	fprintf(f, "6:pieces%d:", shalen * 20);

	if (fs) {
		sha = (char*)malloc(sizeof(char) * SHA_DIGEST_LENGTH);
		buf = (char*)malloc(piecelen);
		printf("computing sha1... ");
		fflush(stdout);
		while ((r = fread( buf, 1, piecelen, fs))) {
			SHA1(buf, r, sha);
			fwrite(sha, 1, 20, f);
		}
		free(buf);
		free(sha);
		fclose(fs);
		printf("done\n");
	} else {
		fprintf(stderr, "could not open file %s\n", src);
		return 1;
	}
	return 0;
}

/**
 * ByteEncode file name:
 *   name = "CD1/Sample/myfile.avi"
 *       -> "pathl3:CD16:Sample10:myfile.avi"
 **/
void format_path(char *in, char *out)
{
	char *s;
	char t[1024];

	memcpy(out, "pathl", 5);
	s = strtok(in, "/");
	while (s) {
		snprintf(t, sizeof(t), "%i:%s", strlen(s), s);
		strcat(out, t);
		s = strtok(NULL, "/");
	}
}

int add_file(FILE *torrent, long long fsize, char *fname, int piecelen, const char *strip)
{
	FILE* fs = fopen( fname, "rb" );

	// "length", "path"

	printf( "adding %s\n", fname );
	if ( fs ) {
		int r;
		char fname_t[8092];
		memset(fname_t, '\0', sizeof(fname_t));
		format_path( fname + strlen(strip) + 1, fname_t );
		fprintf( torrent, "d6:lengthi%llde4:%see", fsize, fname_t );
		while ( ( r = fread( buf + bytesin, 1, piecelen - bytesin, fs ) ) ) {
			bytesin += r;
			if ( bytesin == piecelen ) {
				sha = (char*)realloc( sha, shasize + 20 );
				SHA1( buf, bytesin, sha + shasize );
				shasize += 20;
				bytesin = 0;
			}
		}
		fclose( fs );
		return 1;
	} else {
		fprintf( stderr, "could not open file %s\n", fname );
		return 0;
	}
}

/* recursively scan directories & add files */
int process_directory(DIR *d, FILE *torrent, char *p, int level, int piecelen, const char* strip)
{
	char path[ 8092 ];
	struct dirent* n;
	DIR* dir;

	while ((n = readdir(d))) {
		struct stat s;
		char fbuf[ 8092 ];
		snprintf(fbuf, sizeof(fbuf), "%s/%s", p, n->d_name);
		if (!stat(fbuf, &s)) {
			if (S_ISREG(s.st_mode)) {
				if (n->d_name[0] == '.' && !inclusive)
					continue;
				if ( add_file(torrent, (unsigned int)s.st_size, fbuf, piecelen, strip) )
					files++;
			} else if ( S_ISDIR(s.st_mode) ) {
				if ( strcmp(n->d_name, ".") && strcmp(n->d_name, "..") ) {
					if (++level > MAX_RECURSION)
						return 1;
					snprintf( path, sizeof(path), "%s/%s", p, n->d_name );
					dir = opendir( path );
					if (!dir)
						printf("skipping directory %s (%m)\n", path);
					else
						process_directory(dir, torrent, path, level, piecelen, strip);
				}
			} else
				printf( "ignoring %s (no directory or regular file)\n", fbuf );
		} else
			fprintf( stderr, "could not stat file %s\n", fbuf );
	}
	return 0;
}

int create_from_directory( const char* src, FILE* f, int piecelen )
{
	int ret = 0;
	DIR* dir = opendir( src );

	// "files", "name", "piece length", "pieces"

	if ( dir ) {
		char src_p[1024];
		buf = (char*)malloc( piecelen );

		// start of the "files" list
		fputs( "5:filesl", f );
		snprintf(src_p, sizeof(src_p), "%s", src);
		process_directory(dir, f, src_p, 0, piecelen, src);
		fputs( "e", f );  // mark end of "files" list

		// "name"
		write_name( src, f );

		// "piece length"
		fprintf( f, "12:piece lengthi%de", piecelen );

		// "pieces"
		if ( shasize || bytesin ) {
			// first e comes from file list
			fprintf( f, "6:pieces%d:", shasize + ( ( bytesin > 0 ) ? 20 : 0 ) );
			if ( sha )
				fwrite( sha, 1, shasize, f );
			if ( bytesin ) {
				char sha[ 20 ];
				SHA1( buf, bytesin, sha );
				fwrite( sha, 1, 20, f );
			}
		}
		if ( sha )
			free( sha );
		free( buf );
		if ( !files )
			fprintf( stderr, "warning: no files found in directory\n" );
		closedir( dir );
	} else {
		fprintf( stderr, "could not read directory %s\n", src );
		ret = 1;
	}
	return ret;
}

int create_from_assortment( char** const src, int no_src, FILE* f, int piecelen )
{
	int ret = 0;
	int i, j;
	char *p;
	char *q;
	char *common;
	char **filename;

	filename = malloc( sizeof( char* ) * no_src );
	// check for dupes
	for ( i = 0; i < no_src; i++ ) {
		filename[i] = (char*)canonicalize_file_name( src[i] );
		if ( !filename[i] ) {
			fprintf( stderr, "could not canonicalize %s\n", src[i] );
			ret = 1;
			break;
		}
		for ( j = 0; j < i; j++ )
			if ( !strcmp( filename[i], filename[j] ) )
				break;
		if ( j != i ) {
			fprintf( stderr, "can't include file \"%s\" twice\n",
				 filename[i] );
			for ( j = 0; j < i; j++ ) {
				free( filename[j] );
				filename[j] = 0;
			}
			filename[i] = 0;
			ret = 1;
			break;
		}
	}

	if ( filename[0] ) {
		common = strdup( filename[0] );
		// find common directory
		for ( i = 1; i < no_src; i++ ) {
			p = common;
			char* start;
			start = filename[i];
			q = start;
			while ( p && q ) {
				if ( *p != *q )
					break;
				p++;
				q++;
			}
			*p = '\0';
		}
		printf("using base directory \"%s\"\n", common);
		if ( strlen( common ) > 1 )
			common[ strlen( common ) - 1 ] = '\0';
		else {
			// the files have no common directories
			fprintf( stderr, "warning: these files have no directory in common\n" );
			free( common );
			common = "root";
		}

		buf = (char*)malloc( piecelen );

		// start of "files" list
		fputs( "5:filesl", f );
		for ( i = 0; i <  no_src; i++ ) {
			struct stat s;
			DIR *dir;

			q = filename[i];
			if ( !stat( q, &s ) ) {
				if ( S_ISREG( s.st_mode ) ) {

					if ( add_file(f, (unsigned int)s.st_size,  q, piecelen, common) )
						files++;
				} else if ( S_ISDIR(s.st_mode) ) {
					dir = opendir( q );
					if (!dir)
						printf("skipping directory %s (%m)\n", src[i]);
					else {
						process_directory(dir, f, q, 0, piecelen, common);
						closedir( dir );
					}
				} else
					printf( "ignoring %s (no directory or regular file)\n", src[i] );
			}
		}
		fputs( "e", f );  // mark end of "files" list

		// "name"
		write_name( common, f );

		// "piece length"
		fprintf( f, "12:piece lengthi%de", piecelen );

		// "pieces"
		if ( shasize || bytesin ) {
			// first e comes from file list
			fprintf( f, "6:pieces%d:", shasize + ( ( bytesin > 0 ) ? 20 : 0 ) );
			if ( sha )
				fwrite( sha, 1, shasize, f );
			if ( bytesin ) {
				char sha[ 20 ];
				SHA1( buf, bytesin, sha );
				fwrite( sha, 1, 20, f );
			}
		}
		if ( sha )
			free( sha );
		free( buf );

		if ( !files )
			fprintf( stderr, "warning: no files found in directory\n" );

		free( common );

	} else
		ret = 1;

	free( (char**)filename );
	return ret;
}

void help_message()
{
	printf( "Usage: createtorrent [OPTIONS] -a announce <input file or directory> <output torrent>\n\n"
		"options:\n"
		"--announce    -a  <announceurl> : announce url\n"
		"--piecelength -l  <piecelen>    : sets the piece length in bytes, default: 262144\n"
		"--comment     -c  <comment>     : adds an optional comment to the torrent file\n"
		"--inclusive   -i                : include hidden *nix files (starting with '.')\n"
		"--private     -x                : create a private torrent\n"
		"--version     -V                : version of createtorrent\n"
		"--help        -h                : this help screen\n"
		);
}

