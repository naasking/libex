
#include <stdio.h>
#include "libex.h"

static FILE* st;

static int sem_init(FILE **f, int i, int j) {
	return -1;
}

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
			MAYBE(fp = fopen("dummy-file.txt", "w"), errno);
		} IN {
			TRY(FILE* sock) {
				if (randBomb()) THROW(EUnrecoverable)
				printf("Open a socket.\n");
				MAYBE(sock = fopen("foo", "r"), errno);
			} IN {
				/*...*/
				TRY() {
					if (randBomb()) THROW(EUnrecoverable)
					printf("Create/grab a semaphore.\n");
					if (-1 == sem_init(&st, 0, 1)) THROW(errno)
				} IN {
					/* ... */
				} HANDLE CATCH(EUnrecoverable) {
					fprintf(stderr, "Random exception after shmat()!\n");
				} OTHERWISE {
					/* semaphore's only available to this process and children. */
					fprintf(stderr, "sem_init() failed.\n");
				} FINALLY {
					fclose(sock);
					printf("Socket closed!\n");
				}
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
