
#include <stdio.h>
#include "libex.h"

int randBomb(void)
{
    static int seeded = 0;
    if (!seeded)
    {
        srand(time(NULL));
        seeded = 1;
    }

    /* Flip a coin. */
    return (rand() % 5) / 4;
}

exc_type test() {
	THROWS(EOutOfMemory)

	TRY(char *memBuf) {
		printf("Allocate a memory buffer.\n");
		MAYBE(memBuf = (char *) malloc(256 * 1024));
	} IN {
		TRY(FILE *fp) {
			/* Fifty-bomb. */
			if (randBomb()) THROW(EUnrecoverable)
			MAYBE(fp = fopen("dummy-file.txt", "w"));
		} IN {
			TRY(FILE* sock) {
				if (randBomb()) THROW(EUnrecoverable)
				printf("Open a socket.\n");
				MAYBE(sock = fopen("foo", "r"));
			} IN {
				/*...*/
			} HANDLE CATCH (EUnrecoverable) {
		        fprintf(stderr, "Random exception after fopen()!\n");
			} OTHERWISE {
				fprintf(stderr, "socket() failed.\n");
			} FINALLY {
				fclose(fp);
				printf("File handle closed!\n");
			}
		} HANDLE CATCH (EUnrecoverable) {
			fprintf(stderr, "Random exception after malloc()!\n");
		} OTHERWISE {
			fprintf(stderr, "fopen() failed.\n");
		} FINALLY {
			free(memBuf);
			printf("Memory buffer freed!\n");
		}
	} HANDLE OTHERWISE {
        fprintf(stderr, "malloc() failed.\n");
    } FINALLY {
		printf("retVal: %d\n", THROWS);
	}
	EXIT
}

static int main(char ** argv, size_t argc) {
	//printf("unit: %d\n", sizeof(unit));
	//printf("unit: %d\n", sizeof());
	scanf("\n");
	return 0;
}
